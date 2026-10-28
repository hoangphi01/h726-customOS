# Checkpoint beacons: debugging the boot with no serial console

How we see where a from-source kernel gets to on the H726 when there is no UART. The
only output channel is a GPIO: a direct `writel` to the pin controller at `0x02000000`
(always-on MMIO, needs no clock), which ticks the **focus motor** (phase PH13). Code
dropped in at successive boot stages ticks the motor a distinct number of times; the
highest count observed is the furthest the boot got. A steady repeating tick is the
"it booted" signal. This supersedes the single gate-#0 heartbeat (which is now just the
first beacon).

## !!! Safety: never drive the cooling fan !!!

Every beacon ticks the **focus motor (PH13)** only, never the cooling fan (PL7 / PWM0).
The first gate-#0 test blinked the fan (PL7) off; with the U-Boot splash still projecting
(light engine on) the imager panel overheated in ~15 s (a dark bloom from the centre).
The motor has no thermal role. Note the kernel proper never drives cooling either: our
device tree keeps `pwm-fan` disabled, so the fan stays in the state U-Boot left it (on)
for the whole run, which is thermally safe even if the kernel hangs. See
[bringup-strategy.md](bringup-strategy.md).

## The ladder (Build A)

| Ticks | Where | Proves |
|---|---|---|
| 1 | decompressor entry (`arch/arm/boot/compressed/head.S`) | **Gate #0:** U-Boot entered our kernel (already passed 2026-10-28). |
| 2 | kernel proper, after `__create_page_tables` (`arch/arm/kernel/head.S`, MMU still off) | The decompressor handed off into `vmlinux`. |
| 3 | `early_initcall` (C) | Reached the init thread: `setup_arch` + memory + scheduler + timers are up. |
| steady ~1 Hz, forever | `late_initcall` kthread | **Kernel fully alive:** the scheduler and timekeeping are running. |

So: count the ticks, then watch for the steady heartbeat. You saw bursts 1,2,3 then nothing
means it hung between reaching init and late init; a steady heartbeat means success. Counts
2 and 3 are close by ear, so the two unambiguous reads are "did it get past 1 (gate #0)" and
"did it reach the steady heartbeat".

## How it is built

- [checkpoint/h726-blink.h](checkpoint/h726-blink.h): an assembly macro `H726_BLINK n, rb, rt, rc, rd`
  that ticks PH13 `n` times using only the four registers it is handed, so it drops into both
  the decompressor (registers free) and kernel-proper `head.S` (only `r0/r3/r6/r7` free). It uses
  only `movw/movt` immediates and PC-relative branches, so it is position-independent with the MMU
  on or off.
- [checkpoint/h726-checkpoint.c](checkpoint/h726-checkpoint.c): the C side. An `early_initcall`
  ticks 3 times; a `late_initcall` starts a kthread that ticks ~1 Hz forever. It `ioremap`s the PIO
  and pokes PH13 directly (not through the gpio/pinctrl API) so it works before any driver probes.
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
