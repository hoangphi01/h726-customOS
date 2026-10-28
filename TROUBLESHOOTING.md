# Troubleshooting and hard-won lessons

Real failures hit on this device, and exactly how to get out of them. Read the two cautions first; they are the ones that can damage hardware or strand you.

> [!CAUTION]
> **Never drive the cooling fan in a kernel bring-up test, and never leave a hung kernel sitting.**
> On this projector the light engine is on during the U-Boot splash. In the first gate-#0 test the heartbeat toggled `fan-power-gpio` (PL7) off; the kernel then hung with the fan off and the light engine on, and after about **15 seconds the imager panel overheated** - a dark bloom spread from the centre of the projected image. Power was cut and it recovered once cool, but it was avoidable. Use the **focus motor** (no thermal role) or a UART console as the test signal, cut power the instant you see the fan stop, and recover promptly.

> [!WARNING]
> **If a test kernel hangs, `adb reboot efex` will not work - you need the hardware FEL button. Prepare it before you flash.**
> `adb reboot efex` needs a booted Android, which a hung kernel never reaches. The hardware FEL entry on this unit is a **recessed FLASH button next to the HDMI port**: press and hold it with a **paperclip while you apply power**, keep holding a few seconds. The PC then shows the FEL device `1f3a:efe8`. **Vol+ on the IR remote does not work** (the IR receiver is not active that early in the BootROM). Know where this button is before flashing anything.

## Recovery: a test kernel is flashed and the device will not boot

1. Do not keep power-cycling hoping for an A/B fallback: the active slot is usually already marked "successful" from normal use, so the bootloader keeps retrying it and does not switch to the other slot.
2. Enter FEL with the **paperclip FLASH button next to HDMI**: hold it, apply power, keep holding ~3-5 s. Watch the PC for `1f3a:efe8` (a new USB device). In FEL the kernel does not run, so the fan is safe.
3. In PhoenixSuit V1.19, select the known-good image (`HY300Ultra_H726_ROOTED_v2.img`), click **Upgrade**, confirm **"Do Mandatory Upgrade?"** with **OK**, then **unplug/replug the USB cable** to make the device enumerate - PhoenixSuit catches FEL at that moment and flashes. (Skip the "hold a key, press power 10x" box; that is for tablets. The device is already in FEL.)
4. It boots back to rooted Android (Magisk **core** runs; the Magisk app and su policy may need re-doing, and your `su` over adb will not work until you set Superuser = "Apps and ADB" + grant Shell in the app). The per-unit calibration (Reserve0) is preserved.

> [!CAUTION]
> **A PhoenixSuit recovery flash WIPES `/data` (factory reset).** Even though `userdata` is not in `dlinfo`, the flash rewrites `vbmeta`/`super` to stock, so the AVB/encryption state no longer matches the old encrypted `/data` and Android **force-formats it** on the next boot. Measured: after a v2 recovery flash, all pushed files under `/data/local/tmp` and all user apps (launcher, SmartTube, Aurora, the Hub, Magisk app) were gone. So any PhoenixSuit recovery costs your customizations; only the calibration survives.
>
> **To keep `/data`, do not get into a state that needs a PhoenixSuit flash.** The only way to replace just `boot_a` without a full flash is `dd` on a *running* Android (`dd if=boot_a_backup.img of=/dev/block/by-name/boot_a`) - which a hung kernel denies you. So: keep a bootable fallback alive (the other A/B slot, or a UART so you never have to blind-flash), always back up `boot_a` first, and treat a kernel that can hang as "this may cost /data."

## PhoenixSuit rejects the image: "Please ensure the firmware file is valid"

Allwinner IMAGEWTY images carry a per-partition **verify file** `V<name>.fex` (16 bytes, first 4 = an `add_sum` checksum: pad the payload to a multiple of 4, sum all little-endian u32 words mod 2^32). Edit a payload and not its `V` file and PhoenixSuit rejects the whole image.

- The accepted edit is **minimal diff**: copy the base image whole, overwrite only the one payload at its exact offset and the same size, recompute only that payload's `V` file, and **never touch `dlinfo`, `sys_partition` or the header**.
- **Rewriting `dlinfo` (the "surgical" single-partition trick) is rejected.** Do not do it; a full flash is safe anyway.
- A full flash skips the `Reserve0` calibration partition (it survives). But although `userdata` is not in `dlinfo`, a full flash still rewrites `vbmeta`/`super`, which force-formats `/data` on the next boot (see the recovery caution above). So a full flash does **not** preserve your customizations - only the calibration.
- Simplest of all: on a rooted device, skip IMAGEWTY entirely and **`dd` the partition** - raw bytes, no verify files, only that partition touched.

## Flashing on this SoC (`0x1911` / sun50iw15)

- `sunxi-fel` does **not** support `0x1911` (it reports `soc=00001911(unknown)`; `sunxi-fel uboot` fails `Unexpected SCTLR`). The working loader is **PhoenixSuit over FEL**, which uses the stock `fes1.fex` from the firmware to init DRAM.
- A failed/partial flash does **not** brick the device: if the boot chain is broken the BootROM drops into FEL on its own (`1f3a:efe8`), always recoverable. (Our case was different - the boot chain was intact and only the kernel hung, so it did not auto-FEL; hence the paperclip button.)

## adb path mangling on Windows (git-bash)

git-bash rewrites remote adb paths (e.g. `/data/local/tmp/...` becomes `C:/Program Files/Git/data/...`), so `adb push`/`adb pull`/`adb shell 'dd ...'` with a `/data` path fails or lands in the wrong place. Run adb from **PowerShell** (no path rewriting), or set `MSYS_NO_PATHCONV=1` (but then Windows local paths must be written `C:\...`).

## Keep handy before any on-device test

- A backup of `boot_a` (`dd` it out and pull it).
- The stock and rooted IMAGEWTY images for PhoenixSuit recovery.
- The location of the paperclip FLASH button next to HDMI.
- A dark room and a hand on the power, in case the kernel hangs with the light engine on.
