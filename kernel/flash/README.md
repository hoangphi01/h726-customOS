# Flashing the B3 bring-up kernel (step by step)

How to put the 32-bit `zImage-dtb` (with the B3 checkpoint beacons and the appended bring-up
device tree) onto the device and read the result. The goal is to see **how far the kernel gets**:
count the motor ticks, then watch for the steady heartbeat (see [../heartbeat.md](../heartbeat.md)
for the ladder).

> [!CAUTION]
> **Never let a beacon drive the cooling fan, and never leave a hung kernel sitting.** The light
> engine is on during the U-Boot splash. The beacons only ever tick the **focus motor** (PH13); the
> kernel keeps `pwm-fan` disabled, so cooling stays in the U-Boot (on) state for the whole run. Still
> recover promptly if it hangs.

> [!WARNING]
> **Have the FEL path ready before you flash.** A bring-up kernel does not reach Android, so after you
> read the beacons you must reflash to get Android back, and if it hangs you cannot `dd`. FEL entry is
> the recessed **FLASH button next to HDMI** (paperclip + power → PC shows `1f3a:efe8`), not Vol+ IR.

## Build the image to flash

```sh
HEARTBEAT=1 bash kernel/build.sh
# -> arch/arm/boot/zImage-dtb   (zImage + appended bring-up DTB; flash THIS, not plain zImage)
```

Then wrap it in a `boot_a` image (a stock Android boot image with our `zImage-dtb` as the kernel):

```sh
python3 kernel/flash/build_heartbeat_boot.py \
    /path/to/HY300Ultra_H726_Android14_STOCK_Magcubic.img \
    ~/h726kernel/linux-6.16.7/arch/arm/boot/zImage-dtb \
    boot_hb_dd.img
```

The packer aborts if `zImage-dtb` does not fit the stock kernel slot. (Confirmed: ~11.8 MB into a
~38.6 MB slot, so `boot_hb_dd.img` is 64 MB, the same size as `boot_a`.)

## Which flash method

**`dd` needs root; a PhoenixSuit boot-only image does not.** Pick by the device's current state:

- **If root (`su`) works:** use Method A (`dd`) - fastest, touches only `boot_a`.
- **If root is down** (e.g. just after a recovery flash wiped `/data`, so the Magisk app / Superuser
  are not set up): use Method B (PhoenixSuit, boot-only minimal diff). No root needed.

Either way, only `boot_a` is written, so `/data` and calibration are untouched (see the verify rule
below). Keep a backup of the stock/rooted `boot_a` to restore.

## Method A: `dd` on the rooted device

```sh
adb shell su -c 'dd if=/dev/block/by-name/boot_a of=/sdcard/boot_a_backup.img'
adb pull /sdcard/boot_a_backup.img            # keep this safe (the restore image)

adb push boot_hb_dd.img /data/local/tmp/boot_hb.img
adb shell su -c 'dd if=/data/local/tmp/boot_hb.img of=/dev/block/by-name/boot_a'
adb reboot
```

(Run adb from **PowerShell**, not Git-bash, which mangles `/data/...` paths. See [../../LESSONS.md](../../LESSONS.md).)

## Method B: PhoenixSuit, boot-only minimal-diff image (no root)

Build an IMAGEWTY that overwrites only `boot.fex`, using the rooted v2 image as the base so you land
back in rooted Android:

```sh
python3 kernel/flash/pack_heartbeat_boot.py \
    root-tools/HY300Ultra_H726_ROOTED_v2.img \
    ~/h726kernel/linux-6.16.7/arch/arm/boot/zImage-dtb \
    boot_hb_imagewty.img
```

Enter FEL (paperclip FLASH button + power), then flash `boot_hb_imagewty.img` with PhoenixSuit V1.19,
the same flow used to root. Because it is a boot-only minimal diff (it never rewrites `vbmeta`/`super`),
it does **not** trip the AVB/encryption `/data` wipe that a full recovery flash does.

## The verify rule (why minimal-diff, and what a full flash costs)

- Inside an Allwinner **IMAGEWTY**, every partition payload is followed by a 16-byte verify file
  `V<name>.fex` (first 4 bytes = an `add_sum`: pad the payload to a multiple of 4, sum all LE u32 words
  mod 2^32). Edit a payload without recomputing its `V` file and PhoenixSuit rejects the whole image.
- The accepted edit is **minimal diff**: overwrite only the one payload at its exact offset and same
  size, recompute only that `V` file, and **never touch `dlinfo`, `sys_partition` or the header**. The
  "surgical dlinfo" trick is rejected.
- `userdata` is not in `dlinfo`, but a **full** flash still rewrites `vbmeta`/`super` to the base image,
  which changes the AVB/encryption state and **force-formats `/data`** on the next boot (measured). So a
  full recovery flash **wipes your customizations**; only `Reserve0` calibration survives. A **boot-only**
  minimal-diff flash or a `dd` of `boot_a` leaves `/data` intact.

## Reading the result

The fan runs loud, so **watch the projected image, do not listen.** The C beacons step the focus
motor, which visibly defocuses then refocuses the projected splash (a "breath"):

| What you see on the projected image | What it means | Next |
|---|---|---|
| No focus movement at all | Hung before init. (The two early coil-tap beacons are too faint to see over the fan, so this just means "somewhere before the init thread"; gate #0 itself already passed.) | Add a visible beacon earlier, retry. |
| **Two focus breaths, then nothing** | Reached the init thread (setup_arch + mm + sched + timers up), then hung at late init. | Instrument between early and late init. |
| **A slow focus breath every ~1.5 s, forever** | **Kernel fully alive.** Scheduler + timekeeping running. | Switch to the full DT, bring up a console/driver. |

The device will **not** boot into Android with this kernel (no rootfs/console yet); reflash to recover.

## Recover to Android (/data-safe)

To get Android back **without** wiping `/data`, restore just `boot_a`:

- **Rooted (if you still have `dd`):** `dd` your `boot_a_backup.img` back.
- **Over FEL (no root):** flash a **boot-only** minimal-diff image whose `boot.fex` is the stock/rooted
  `boot_a` (same `pack_heartbeat_boot.py`, but pass the stock `boot_a`/zImage you backed up), or a full
  stock/rooted IMAGEWTY if you accept the `/data` wipe. Prefer boot-only to keep `/data`.

## If a stage is silent: next build

The beacons pin the stage; fix or instrument that stage. The asm beacons and the register math are in
[../checkpoint/h726-blink.h](../checkpoint/h726-blink.h); the C beacons (and where to add a finer one,
e.g. inside `setup_arch` after `early_ioremap_init()`) are in
[../checkpoint/h726-checkpoint.c](../checkpoint/h726-checkpoint.c). A 3.3V USB-UART makes all of this
unnecessary: with `earlycon` the kernel prints from its assembly head.
