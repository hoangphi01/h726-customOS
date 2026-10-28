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
