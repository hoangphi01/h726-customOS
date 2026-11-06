# U-Boot console bring-up loop (H726 / HY300 Ultra)

A **bidirectional UART** turns the stock Allwinner U-Boot into an interactive bring-up tool: load a
test kernel into RAM and `bootm` it **without ever overwriting the stock `boot_a`**. This is the
sustainable alternative to flashing `boot_a` (which, on a kernel hang, costs a FEL + PhoenixSuit
recovery and wipes `/data`; see [../TROUBLESHOOTING.md](../TROUBLESHOOTING.md)).

Status (2026-11-06): the console is **working** (TX confirmed, commands execute). The clean boot path
(uImage + `bootm`) is staged and being verified; the direct-`bootm`-of-the-Android-image path was
tried and **fails** (see "Why `bootm` of the boot image crashes" below).

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

**The fix: wrap the kernel as a self-contained legacy uImage** so `bootm` needs nothing else. Our kernel
carries its own DTB (`CONFIG_ARM_APPENDED_DTB`) and ignores the ramdisk at this stage, so a plain kernel
uImage is enough:

```
# on the build host (u-boot-tools):
mkimage -A arm -O linux -T kernel -C none -a 0x40008000 -e 0x40008000 \
        -n h726bringup -d zImage-dtb uImage
```

`-C none` because the payload (`zImage-dtb`) self-decompresses; `-a/-e 0x40008000` is the standard ARM
zImage load/exec (DRAM base + 0x8000; the kernel decompresses upward, clear of BL31 at `0x48000000`).

## The loop

```
# (Android, adb+su) stage the test kernel on slot b, leave boot_a stock:
adb push uImage /data/local/tmp/uImage
su -c 'dd if=/data/local/tmp/uImage of=/dev/block/by-name/boot_b'
#   ... arm the misc one-shot for slot b (see stage_b flow) ...
adb reboot                      # U-Boot tries slot b -> drops to =>

# (U-Boot =>, over UART TX)
sunxi_flash read 0x50000000 boot_b
bootm 0x50000000                # boots our kernel; earlycon + keep_bootcon print the whole way

# hang? power-cycle -> stock boot_a (tries=0). No FEL, no /data wipe.
```

Each iteration: rebuild -> `mkimage` -> `dd` to `boot_b` -> reboot to `=>` -> `sunxi_flash read` + `bootm`
-> read the UART. `boot_a` stays stock, so adb and `/data` always survive.

See [uart.md](uart.md) for the read-only pad/pinout, [../STATUS.md](../STATUS.md) for state, and
[../TROUBLESHOOTING.md](../TROUBLESHOOTING.md) for FEL recovery.
