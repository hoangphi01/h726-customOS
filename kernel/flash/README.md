# Flashing the heartbeat test (step by step)

How to put the 32-bit `zImage` (with the gate-#0 red-LED heartbeat) onto the device and read the result. The goal of this run is one yes/no answer: **does the vendor U-Boot enter our kernel?** See [../bringup-strategy.md](../bringup-strategy.md) and [../heartbeat.md](../heartbeat.md).

Nothing here can brick the device: FEL stays available and the stock / rooted image reflashes.

## The one rule that matters: how PhoenixSuit verification works

This is the lesson from rooting this device, and it is easy to get wrong.

- Inside an Allwinner **IMAGEWTY**, every partition payload is followed by a 16-byte **verify file** named `V<name>.fex` (first 4 bytes = an `add_sum` checksum: pad the payload to a multiple of 4, then sum all little-endian u32 words mod 2^32).
- If you edit a payload (e.g. the kernel in `boot`) and do **not** update its `V<name>.fex`, **PhoenixSuit rejects the whole image**: *"Please ensure the firmware file is valid."*
- The method PhoenixSuit **accepts** is **minimal diff**: copy the base image whole, overwrite only the one payload at its exact offset and the **same size**, patch only that payload's `V` file, and **never touch `dlinfo`, `sys_partition` or the header**. The "surgical dlinfo" approach (trimming the partition list) is **rejected**; do not use it.
- A full flash is safe anyway: `dlinfo` lists 13 firmware partitions (bootloader, env, boot, vendor_boot, init_boot, super, misc, vbmeta*, dtbo, media_data, Reserve0). **`userdata` is not in the list, so `/data` is never flashed**, and Allwinner **skips** the `Reserve0` calibration partition. So customizations in `/data` and the per-unit calibration survive a flash; only `super` is reset to the base image's.

The cleanest way to avoid this problem entirely is not to use an IMAGEWTY at all, below.

## Method A (recommended): `dd` the boot_a partition on the rooted device

The device is rooted, so write `boot_a` directly. `dd` copies raw bytes; U-Boot reads the partition directly (verity is disabled), so **there is no verify file and nothing to reject**, and **only `boot_a` is touched** (not even `super`).

**1. Build the boot_a image** (a stock Android boot image with our zImage as the kernel):

```sh
HEARTBEAT=1 bash kernel/build.sh                       # builds the zImage
python3 kernel/flash/build_heartbeat_boot.py \
    /path/to/HY300Ultra_H726_Android14_STOCK_Magcubic.img \
    ~/h726kernel/linux-6.16.7/arch/arm/boot/zImage \
    boot_hb_dd.img
```

(`boot_hb_dd.img` is 64 MB, the same size as `boot_a`. You can also dump your own `boot_a` from the device and pass that instead of the stock image.)

**2. Back up the current boot_a**, then write ours:

```sh
adb shell su -c 'dd if=/dev/block/by-name/boot_a of=/sdcard/boot_a_backup.img'
adb pull /sdcard/boot_a_backup.img            # keep this safe

adb push boot_hb_dd.img /data/local/tmp/boot_hb.img
adb shell su -c 'dd if=/data/local/tmp/boot_hb.img of=/dev/block/by-name/boot_a'
adb reboot
```

**3. Watch** the red LED in the first second or two after it reboots.

## Reading the result

| What you see | What it means | Next |
|---|---|---|
| Red LED blinks ~5 times | **Gate #0 passed.** U-Boot entered our 32-bit kernel; the CPU is running it. | On to the CCU/pinctrl offsets. |
| No blink, device does not reach Android | The pin guess is wrong, or U-Boot did not enter the kernel. | Recover, then retarget the pin and retry. |

The device will **not** boot into Android with our kernel (the CCU is not corrected yet, so it hangs after the heartbeat). That is expected; the heartbeat is the whole signal for this run.

## Recovery

Because the heartbeat kernel hangs, Android will not come up to `dd` the backup back. Recover over FEL:

1. Enter FEL: hold Vol+ on the IR remote at power-on (or the FEL pad); the PC sees `1f3a:efe8`.
2. PhoenixSuit V1.19 -> flash the **unmodified** rooted image `root-tools/HY300Ultra_H726_ROOTED_v2.img` (or the stock image). Unmodified images already carry correct verify files, so there is nothing to repack and nothing to reject.
3. `/data` and calibration are preserved (not flashed); the device boots the rooted Android again.

## Method B (alternative): PhoenixSuit, minimal-diff image

If you prefer to flash an IMAGEWTY, build one with `kernel/flash/pack_heartbeat_boot.py` using the **rooted v2 image as the base** (so you stay rooted). It follows the accepted minimal-diff method: it overwrites only `boot.fex` and recomputes only `Vboot.fex`, and confirms the `add_sum` algorithm against the base before writing; `dlinfo` and everything else are untouched.

```sh
python3 kernel/flash/pack_heartbeat_boot.py \
    root-tools/HY300Ultra_H726_ROOTED_v2.img \
    ~/h726kernel/linux-6.16.7/arch/arm/boot/zImage \
    boot_hb_imagewty.img
```

Then flash `boot_hb_imagewty.img` with PhoenixSuit over FEL, the same flow used to root. Recovery is the same as above (flash the unmodified v2 image).

## If it does not blink: retarget the pin

The red LED is `PL0`, a best guess from the H713 sibling; this unit may differ. Edit [../heartbeat/heartbeat-h726.S](../heartbeat/heartbeat-h726.S) (the register math and alternatives are in [../heartbeat.md](../heartbeat.md)), rebuild step 1, and retry. Each attempt is safe.

A 3.3V USB-UART on the debug pads makes all of this unnecessary: with `earlycon` the kernel prints from its assembly head, so you see exactly where it is instead of guessing from a blink.
