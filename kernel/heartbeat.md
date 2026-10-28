# Checkpoint beacons: debugging the boot with no serial console

How we see where a from-source kernel gets to on the H726 when there is no UART. The
only output channel is a GPIO: a direct `writel` to the pin controller at `0x02000000`
(always-on MMIO, needs no clock), which drives the **focus motor**.

**The fan runs loud the whole time, so the signal must be seen, not heard.** The C
beacons step the focus motor through its four phases (PH13/PH18/PH6/PH7), which moves the
lens and makes the **projected image visibly defocus then refocus** - a "breath" that is
obvious over the fan. Watch the projected splash: a couple of focus breaths then nothing
means it reached init but hung; a never-ending slow focus breathing means it booted. (The
two early assembly beacons only tap one coil - a faint jerk, hard to see - so the C breaths
are the real signal; a later build can make the early ones sweep too.) This supersedes the
single gate-#0 heartbeat (now just the first beacon).

## !!! Safety: never drive the cooling fan !!!

Every beacon ticks the **focus motor (PH13)** only, never the cooling fan (PL7 / PWM0).
The first gate-#0 test blinked the fan (PL7) off; with the U-Boot splash still projecting
(light engine on) the imager panel overheated in ~15 s (a dark bloom from the centre).
The motor has no thermal role. Note the kernel proper never drives cooling either: our
device tree keeps `pwm-fan` disabled, so the fan stays in the state U-Boot left it (on)
for the whole run, which is thermally safe even if the kernel hangs. See
[bringup-strategy.md](bringup-strategy.md).

## The ladder (Build A)

| Signal | Where | Proves |
|---|---|---|
| CP1: one coil tap (faint) | decompressor entry (`arch/arm/boot/compressed/head.S`) | **Gate #0:** U-Boot entered our kernel (already passed 2026-10-28). |
| CP2: one coil tap (faint) | kernel proper, after `__create_page_tables` (`arch/arm/kernel/head.S`, MMU still off) | The decompressor handed off into `vmlinux`. |
| **2 focus breaths** (image defocus/refocus) | `early_initcall` (C) | Reached the init thread: `setup_arch` + memory + scheduler + timers are up. |
| **steady focus breath every ~1.5 s, forever** | `late_initcall` kthread | **Kernel fully alive:** the scheduler and timekeeping are running. |

So the reliable reads, watching the projected image: **no focus movement at all** = hung before
init (CP1/CP2 taps are too faint to see over the fan, so this just means "somewhere before init");
**two breaths then nothing** = reached init but hung at late init; **endless slow breathing** =
success. If the early/late split matters, a follow-up build makes CP1/CP2 sweep too.

## How it is built

- [checkpoint/h726-blink.h](checkpoint/h726-blink.h): an assembly macro `H726_BLINK n, rb, rt, rc, rd`
  that ticks PH13 `n` times using only the four registers it is handed, so it drops into both
  the decompressor (registers free) and kernel-proper `head.S` (only `r0/r3/r6/r7` free). It uses
  only `movw/movt` immediates and PC-relative branches, so it is position-independent with the MMU
  on or off.
- [checkpoint/h726-checkpoint.c](checkpoint/h726-checkpoint.c): the C side. It steps the four focus-motor
  phases (PH13/PH18/PH6/PH7) to sweep the lens out and back = one visible focus breath. An `early_initcall`
  does 2 breaths; a `late_initcall` kthread does one breath every ~1.5 s forever. It `ioremap`s the PIO and
  read-modify-writes only the four motor bits (not via the gpio/pinctrl API), so it works before any driver
  probes and disturbs nothing else.
- [build.sh](build.sh) injects CP1/CP2 into the two `head.S` files (idempotently) and builds the
  C file in, under `HEARTBEAT=1`.

The register math: main PIO `0x02000000`; PH = bank 7, base `0x02000000 + 7*0x24 = 0x020000FC`;
`CFG1` (pins 8-15) at `+0x04`, PH13 mux = bits [23:20] (set `0x1` = output); `DAT` at `+0x10`
bit 13.

## Build it

```sh
HEARTBEAT=1 bash kernel/build.sh
# -> arch/arm/boot/zImage-dtb  (zImage + appended bring-up DTB; FLASH THIS, not plain zImage)
```

`HEARTBEAT=1` also turns on `CONFIG_ARM_APPENDED_DTB` and appends our minimal
[sun50i-h726-hy300ultra-bringup.dts](sun50i-h726-hy300ultra-bringup.dts), so the kernel uses our
device tree and ignores whatever the vendor U-Boot passes. The cmdline is forced to `nr_cpus=1`
(skip SMP secondary bring-up via PSCI/SMC). Verified: the whole thing compiles clean and the
`zImage-dtb` (~11.8 MB) fits the stock `boot_a` kernel slot (~38.6 MB) with room to spare.

## Flash and observe

See [flash/README.md](flash/README.md). In short: pack `zImage-dtb` into `boot_a` and flash
(by `dd` if the device is rooted, or a PhoenixSuit boot-only minimal-diff image if not), power on,
count the motor ticks, and watch for the steady heartbeat.

## If a stage is silent: next build

If it hangs between two beacons, the next build adds a finer beacon in that gap (e.g. inside
`setup_arch` after `early_ioremap_init()`, the earliest a C beacon can `ioremap`). We batch as many
beacons as possible into each build because each on-device flash cycle is costly.

## The real unlock

A 3.3V USB-UART on the debug pads makes all of this unnecessary: with `earlycon` the kernel prints
from its assembly head, so every stage is text, not a guessed tick count. Until then, beacons are
the channel.
