# U-Boot console bring-up loop (H726 / HY300 Ultra)

A **bidirectional UART** turns the stock Allwinner U-Boot into an interactive bring-up tool: load a
test kernel into RAM and `bootm` it **without ever overwriting the stock `boot_a`**. This is the
sustainable alternative to flashing `boot_a` (which, on a kernel hang, costs a FEL + PhoenixSuit
recovery and wipes `/data`; see [../TROUBLESHOOTING.md](../TROUBLESHOOTING.md)).

Status (2026-11-06): the console is **working** (TX confirmed, commands execute). But `bootm` here is
**Android-only** and faults on eMMC in the slot-b default-env state, so neither the bare boot image nor a
uImage can be `bootm`-ed from `=>` (both tried, both crash U-Boot; see below). The working path to *run* a
test kernel is to **clone slot a to slot b** and let the stock flow boot it (see "The fix" below); the
`=>` console remains useful for inspection.

## Wiring (bidirectional, add TX to the read-only setup)

The read-only UART (see [uart.md](uart.md)) is **adapter RXD -> board TX** (the middle pad of the
3-pad LED-connector cluster) + GND. To drive U-Boot, add the third wire:

- **adapter TXD -> board RX** (the **top** pad of the 3-pad cluster)
- adapter RXD -> board TX (middle pad) *(unchanged)*
- adapter GND -> board GND (bottom pad) **or the HDMI shell** (frees a hand; the shell is ground)

Level safety: the **CP2102 is 3.3 V logic**. Measured its TXD idle-high at **3.56 V**, which is below
the SoC's input-protection-diode threshold (VDDIO ~3.3 V + ~0.5 V ~= 3.8 V), so it will not forward-bias
the diode or latch up, and 3.56 V is a clean logic HIGH into PH1. A ~1 kOhm series resistor on TX is
cheap extra insurance. Do **not** use the CH340G for TX unless you confirm it is 3.3 V (many are 5 V).

Three hand-held contacts are hard to hold steady through a multi-command session; tape the pins down
(GND to the HDMI shell, TX/RX to their pads) or solder three short wires for a stable loop.

## Getting to the `=>` prompt

Arming the **A/B one-shot** for slot b and rebooting drops straight to the U-Boot prompt, because
**slot b is not fully provisioned**: its environment (`env_b`, `mmcblk0p4`) fails CRC, so U-Boot falls
back to the *default* environment, where `bootcmd=run distro_bootcmd` but **`distro_bootcmd` is not
defined** -> autoboot aborts to `=>`. (This, not a dead kernel, is what "boot_b never comes up" was.)

```
[..]ANDROID: Attempting slot b, tries remaining 1
*** Warning - bad CRC, using default environment
...
## Error: "distro_bootcmd" not defined
=>
```

The one-shot is `/data`-safe: `misc` slot_b tries decrement 1->0, so a **power-cycle falls back to
stock `boot_a`** (adb returns). No FEL, no `/data` wipe. (On a normal slot-a boot you can instead hit a
key at "Hit any key to stop autoboot" to reach `=>`, which needs the TX wire too.)

## Driving U-Boot (what this stock build has)

- Storage is **not** the standard `mmc` command (`mmc list` -> "No MMC device available"). Allwinner
  uses its own: **`sunxi_flash read <mem_addr> <part_name> [size]`** (and `write`). Partition names come
  from the `partitions=` env: `boot_a@mmcblk0p5`, `boot_b@mmcblk0p6`, `misc@mmcblk0p12`, `vendor_boot_*`,
  `init_boot_*`, `super@mmcblk0p11`, etc.
- Boot command is **`bootm` only** - there is **no `bootz` / `booti` / `boota`**. So a raw `zImage` cannot
  be booted directly; it must be wrapped in a format `bootm` recognises (legacy uImage or FIT).
- Also present: `ab_select`, `fastboot`, `efex` (enter FEL), `fdt`, `md`/`mw`/`cp`, `reset`, `printenv`.
- Safe scratch RAM for staging an image: **`0x50000000`** (clear of the reserved regions - BL31
  `0x48000000`, OP-TEE `0x48600000`, CMA `0x7c000000`, relocated U-Boot ~`0x7ff00000`).

Interacting over the serial line: fully drain any stale output first, send the command, then read until
the `=>` prompt returns (a fresh port opened per command races with the previous command's output).

## Why `bootm` of the boot image crashes (and the fix)

Our `boot_hb.img` is the stock `boot_a` Android image with only the kernel region replaced. It is an
**Android boot image v3/v4**, which is *split*: the **kernel** lives in `boot`, but the **ramdisk + DTB**
live in a separate **`vendor_boot`** partition. A bare `bootm <addr>` on the boot image alone cannot
assemble it:

```
=> bootm 0x50000000
vendor magic err
[mmc]: mmc 2 data timeout ... give up reinit
prefetch abort          <- U-Boot itself crashes (pc = garbage)
```

It errors on the missing vendor magic, tries to pull `vendor_boot` off eMMC, the raw `mmc` access faults,
and U-Boot aborts. (Flashing the same image to `boot_a` boots fine only because the *stock* `env_a` boot
flow loads `vendor_boot_a` too.)

### uImage via `bootm` does NOT work here (tried 2026-11-06)

Wrapping `zImage-dtb` as a legacy uImage (`mkimage -A arm -O linux -T kernel -C none -a/-e 0x40008000`),
`dd`-ing it to `boot_b`, and `sunxi_flash read 0x50000000 boot_b; bootm 0x50000000` gives the **same crash**
(`vendor magic err` -> eMMC faults -> prefetch abort). This Allwinner `bootm` is **customised to boot only
Android images**: it ignores the legacy uImage magic at the given address and always runs its android path
(load `vendor_boot`, do its own mmc init). In the slot-b default-env state that mmc init faults. So neither
a raw zImage, a uImage, nor the bare boot image can be `bootm`-ed from this `=>` state. (There is also no
`bootz`/`booti`/`go`-with-cache-control to fall back to.)

### The fix that uses the proven path: clone slot a to slot b

Our kernel is known to boot via the **stock slot-a android flow** (it booted when `boot_hb.img` was flashed
to `boot_a`). So make **slot b an exact clone of slot a**, but with our kernel in `boot_b`. Then slot b
boots our kernel through the identical, working flow - no `=>` interaction, and reading needs only **RX**
(drop the TX wire):

```
# (Android, adb+su) clone the slot-a boot chain to slot b, leave boot_a stock:
su -c 'dd if=/dev/block/by-name/env_a         of=/dev/block/by-name/env_b'
su -c 'dd if=/dev/block/by-name/vendor_boot_a of=/dev/block/by-name/vendor_boot_b'
su -c 'dd if=/dev/block/by-name/init_boot_a   of=/dev/block/by-name/init_boot_b'
su -c 'dd if=/dev/block/by-name/vbmeta_a      of=/dev/block/by-name/vbmeta_b'
su -c 'dd if=/dev/block/by-name/dtbo_a        of=/dev/block/by-name/dtbo_b'
# our kernel (Android image with keep_bootcon kernel) into boot_b:
su -c 'dd if=/data/local/tmp/boot_hb.img      of=/dev/block/by-name/boot_b'
#   ... arm the misc one-shot for slot b (misc_bc.bin) ...
adb reboot                      # slot b now boots our kernel via the stock flow

# read the UART (RX only) for the earlycon + keep_bootcon output.
# hang? power-cycle -> stock boot_a (tries=0). No FEL, no /data wipe.
```

`env_b` must be a valid copy (its bad CRC was what dropped slot b to `=>`); `vendor_boot_b`/`init_boot_b`
give `bootm` the ramdisk it demands; `vbmeta_b` carries the same verity-disabled (orange) state that let
our modified kernel boot on slot a. `boot_a`, `/data` and the calibration are never touched.

The interactive `=>` console (above) is still the tool for one-off U-Boot inspection (`printenv`,
`sunxi_flash`, memory peeks); the slot-clone is the hands-off way to actually *run* a test kernel.

See [uart.md](uart.md) for the read-only pad/pinout, [../STATUS.md](../STATUS.md) for state, and
[../TROUBLESHOOTING.md](../TROUBLESHOOTING.md) for FEL recovery.
