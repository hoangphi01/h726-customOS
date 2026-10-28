# RE: observable signals for blind (no-UART) bring-up

The hardest part of bringing up a from-source kernel on this projector is not writing drivers,
it is **seeing anything at all** while the kernel runs. There is no serial console, and the
device denies the usual blind-debug channels. This note records what output channels exist, what
each one is actually wired to, and which ones are usable as a boot "beacon". It is written from
live investigation on the rooted stock device (2026-10-28); the verdicts are hard-won.

## The constraints (measured)

- **No UART adapter / pads reachable.** The one real unlock is still a 3.3V USB-UART; absent that,
  everything below is a workaround.
- **No `/dev/mem`** (`ls /dev/mem` -> No such file). So userspace cannot poke MMIO to reproduce an
  early-kernel register write for validation.
- **No `/sys/class/gpio`** (legacy sysfs GPIO not built). So userspace cannot toggle a raw GPIO the
  way the kernel beacon does, either.
- The kernel beacon itself (our from-source kernel) *can* poke MMIO directly - that is the whole
  point - but we cannot pre-validate a candidate register from userspace because of the two above.

## Signal 1: the focus/stepper motor - NOT a usable visible beacon

The vendor exposes a stepper-motor driver at `/sys/devices/platform/motor_ctr/` (module
`motor_control` + `motor_limiter`, bound to pinctrl at `0x2000000`). Full register-free interface:

- **Phases = four PH-bank GPIOs**, from `/sys/kernel/debug/gpio`:
  `gpio-237 = motor-gpio-phase0 = PH13`, `gpio-242 = phase1 = PH18`,
  `gpio-230 = phase2 = PH6`, `gpio-231 = phase3 = PH7` (all `out`, driven directly - no enable IC).
- **Drive is a half-step 8-entry table** (from the device tree `motor_ctr/motor-cw-table`):
  `01 09 08 0a 02 06 04 05` (CCW is the reverse). Each byte is a phase bitmask, bit0=phase0(PH13),
  bit1=phase1(PH18), bit2=phase2(PH6), bit3=phase3(PH7). So `0x09 = PH13+PH7`, etc. DT also gives
  `motor-phase-num=4`, `motor-step-num=8`, `motor-cycle=3`, `motor-step-type=4`,
  `motor-phase-udelay=1`, `motor-step-mdelay=1`.
- sysfs attrs: `motor_ctrl`, `motor_ctrl_no_limit`, `motor_step_num`, `motor_step_delay`,
  `motor_back_step`, `motor_cycle`, `motor_limit`, `motor_ctrl_time`. `cat motor_ctrl` -> `00.400.01`
  (looks like a `DIR.STEPS.SPEED` template; writes to it returned rc=0 but the readback never
  changed, so the write format was not confirmed).

**Verdict: not usable as a beacon on this unit.** Two independent reasons:
1. **Optical focus on this model is MANUAL** - the user manual (page 42) labels a physical
   "Bánh xe tập trung" (focus wheel) on the lens barrel, and a "QUAY" (rotate) swivel base. So the
   autofocus motor does not visibly change the projected image's focus; driving it produced **no
   visible change** on the image.
2. The motor is **inaudible over the cooling fan**, which runs loud the whole time, so an audible
   tick is useless too.

A kernel beacon that only tapped one coil (PH13) therefore has no observable effect here. The phase
map above is still valuable reference for a future `motor_ctr` driver.

## Signal 2: `/sys/class/prj/*` - rich, but reading it REBOOTS the device

The vendor "projector" HID/reg driver (`hidtvreg_dev`, class `prj`) exposes a lot of what we want:
`bl_duty` (backlight/LED brightness), `fanpolicy` / `fan2power` / `fanrunstatus` / `speedlevel`
(fan), `ledtemp` / `lcdtemp` / `avdtemp` / `lcdadc` (sensors), `CameraGpio`, `gpios`, `mode`.

**Verdict: do not touch from userspace.** Reading several of these attrs (at least `bl_duty` and the
temp/adc sensors) **reboots the device** - reproduced twice. The read handlers evidently do a
fragile hardware/I2C/ADC access that faults. It is useful only as a hint that a **backlight-brightness
PWM exists** (a dim/bright pulse would be an ideal visible + thermally-safe beacon, since dimming
*reduces* heat); the PWM channel/register must be found from the **static device tree** (`/sys/firmware/fdt`),
never from this driver.

## Signal 3: the cooling fan (PL7 / PWM0) - proven, but thermal

This is what confirmed gate #0 (the fan cut out = our code ran), so it is **proven visible/audible**.
But it is dangerous: the light engine is on during the U-Boot splash, and leaving the fan off ~15 s
overheated the imager panel (dark bloom). A *safe* fan beacon is possible - very short OFF pulses
(~150 ms) heavily biased to ON, with every beacon/beat ending ON, so a hang almost always leaves the
fan running and the ~15 s thermal margin covers the rest - but it must be designed with that
discipline, and the operator watches for the fan going *fully* silent (hung mid-pulse) and cuts power.
See [../kernel/bringup-strategy.md](../kernel/bringup-strategy.md).

## Where this leaves the beacon

| Signal | Visible over the fan? | Safe? | Early-kernel drivable? | Verdict |
|---|---|---|---|---|
| Focus motor (PH13/18/6/7) | **No** (manual focus) | yes | yes (raw GPIO) | dead |
| Status LED | n/a | yes | **no** (PMIC/MCU, not a SoC GPIO) | dead |
| `/sys/class/prj` attrs | (reboots) | **no** | n/a (vendor only) | dead |
| Backlight PWM (`bl_duty`) | **yes** (image dim/bright) | yes (dim = less heat) | maybe (if U-Boot left the PWM clocked) | **investigate via static FDT** |
| Cooling fan (PL7/PWM0) | yes (audible stutter) | risky (needs short-pulse discipline) | yes | fallback if backlight fails |

**Method lesson:** validate that a candidate actually moves something observable *before* building a
kernel beacon on it. We built a focus-motor beacon first and only then found the focus is manual, so
the beacon could never have been seen. The backlight PWM is the next candidate; the fan (safely
pulsed) is the proven fallback.

## The companion MCU and the ~1-2 s reset window (the real blocker)

Live-testing a bring-up kernel exposed two power-management facts that dominate everything:

- **There is a companion MCU/power controller.** The front **LED is MCU-driven: red = standby/off,
  blue = on.** The MCU powers the SoC and the light engine and watches for the SoC to finish the
  vendor boot (a handshake our bare kernel never does). When it does not, the MCU drops the whole
  device to **standby (red LED, fan off, light off)**. This is *good* for safety: standby turns the
  light engine off, so a hung bring-up kernel cannot overheat the panel, and recovery is just a
  power-cycle (see the A/B loop below). The vendor fan GPIO (PL7) is still ours to toggle while the
  device is on - the MCU does not fight it.
- **Something resets/stops the SoC ~1-2 s after our kernel starts.** A fan beacon confirmed the
  kernel runs (one clear fan event = CP1 in the decompressor executed, so **gate #0 passes on the
  appended-DTB build too**), but then the device goes to standby almost immediately - we never get
  more than about one beacon cycle. Prime suspect: the vendor U-Boot leaves a **watchdog** running
  (sun6i-a31 watchdog at `0x02051000`) that a bring-up kernel does not service, so it resets at its
  short timeout; the MCU then sees the SoC reset and latches to standby. **Caution:** a direct
  `writel` to the watchdog MODE register from the kernel made things *worse* (no beacon at all),
  which suggests the watchdog may be **secure-world / OP-TEE owned** (a normal-world write aborts) or
  needs the key field - do not blindly poke it. Confirming and extending this window is the current
  blocker; until it is solved, any beacon has only ~1-2 s to produce its signal, so the signal must
  be a single clear event early (the fan audibly stopping), not a long count.

**Signal shape that works in a short window:** turn the fan fully OFF for ~1 s then ON for ~2 s,
repeated. The fan audibly stopping is unmistakable even over itself, and starting with OFF puts the
first clear event inside the window. (Quick sub-second "stutters" are too easy to miss, and a long
multi-cycle pattern never completes before the reset.)

### The cutoff is EXTERNAL, confirmed by a spin test (2026-10-29)

A diagnostic kernel did one GPIO blip and then `b .` (spin forever) in the decompressor, **without
touching the fan**, and went to **RED standby at ~2 s**. **Caveat (be honest):** that spin kernel used
the *silent* PA-beep as its blip, so we have no independent confirmation it ran - "red at ~2 s" could
be our spinning kernel being cut, OR U-Boot finishing with `boot_b` and the MCU timing out. The spin
test must be **re-run with the FAN blip** (`H726_SPINTEST`, which stutters PL7 audibly first): fan
stutter = our kernel definitely ran, then stay-on vs red answers external-vs-crash. **This was tried
(2026-10-29) and was inconclusive: no stutter was heard and the device went "straight to red, like a
shutdown".** A single brief fan off/on blip is at the very edge of audibility in the reboot noise, so
"no stutter" does not prove the kernel did not run. The fix is a **reliable, clearly-audible blip**:
hold the fan OFF for ~1 s timed by the **ARM generic timer** (read `CNTVCT` via `mrrc p15,1`, spin until
it advances ~24e6 ticks - accurate regardless of cache/CPU speed, and bounded so it is thermally safe),
then ON, then spin. A 1 s silence of the loud fan is unmissable. That is the #0 test next session. What *is* confirmed is that **`boot_b` runs our kernel** (a fan "stutter" was heard on an
earlier fan-beacon build - U-Boot never toggles the fan - and `misc` slot_b tries decremented 1->0), so
the slot is not locked. The external-cutoff reading below is the **leading hypothesis**, not yet proven:
the **watchdog**. A watchdog reset at ~2 s fits every
observation, including why the very first gate-#0 test (flashed to the *active* slot `boot_a`) seemed
to run ~15 s - it was **reset-looping** (boot_a is always bootable, so U-Boot kept re-entering it, the
MCU saw the SoC "active", and the fan toggled each loop until the panel overheated), whereas a
one-shot `boot_b` kernel resets once, exhausts its try, and the MCU then drops to standby.

**So the #1 blocker is the watchdog, and it must be handled correctly:** a plain `writel(0, MODE)` from
the kernel made things *worse* (no beacon at all), so it is likely **secure-world / OP-TEE owned or
key-protected**. The next move is to find the sun50iw15/sun55i watchdog's real register map + key and
either disable it from the earliest asm or, if it is secure, **pet it** periodically (write the
restart/key register) to keep the SoC alive past 2 s - or learn the vendor U-Boot handoff that stops
it. This is the gate before any further bring-up progress can be observed.

### Speaker beep (PA-enable toggle): does NOT work

The codec's power-amp enable is `pa-pin-0 = <&r_pio PL2 ...>` (active high; PL2 on the R_PIO at
`0x07022000`, same bank as the fan). Toggling PL2 as a square wave from the decompressor made **no
sound at all** - so the amp needs more than its enable line (its supply rail and/or a codec signal
path), which a bare kernel cannot bring up. Android itself would not play UI click sounds through the
speaker either (only media routes to it). Beep via a bare GPIO is therefore **not** a usable bring-up
signal here; the fan stays the only one.

## The A/B one-shot test loop (fast, /data-safe, no FEL)

The device is standard Android A/B, and `misc` holds a valid `bootloader_control` struct (AOSP magic
`BCAB`, CRC32 over the first 28 bytes - verified). This gives a **/data-safe, FEL-free test cycle**:

1. `dd` the test kernel into the **inactive** slot (`boot_b`), leaving `boot_a` (stock) and `/data`
   untouched.
2. Write `misc` @ offset 2048 to make `boot_b` win **once**: set slot_b `priority=15, tries=1,
   successful=0` and lower slot_a `priority` to 14, then fix the CRC32.
3. Reboot. U-Boot picks `boot_b`, **decrements its tries to 0** (confirmed by reading `misc` back),
   and runs our kernel.
4. Observe. Then power-cycle: `boot_b` now has tries=0/unsuccessful, so U-Boot falls back to
   `boot_a` = stock Android. **No FEL, no `/data` wipe, no re-root** - a loop measured in minutes.

Worst case (if a vendor U-Boot ignored the tries field) is being stuck in `boot_b` = the same FEL
recovery we already have, so there is no downside. This loop is what makes blind iteration bearable.
