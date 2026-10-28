# RE: `sunxi-ve` (Cedar video engine)

Short note on the video-engine driver, from `sunxi-ve.ko` (not redistributed; see [RULE.md](../RULE.md)). It is deliberately shallow: this block is reused via blobs, not reimplemented.

## What it is

`sunxi-ve.ko`, `description = User mode CEDAR device interface`, no `depends`. It is the standard Allwinner Cedar VE kernel driver: a char device that hands the VE register region and reserved memory to the userspace CedarX stack, plus clock/reset/frequency management. DT: `ve@1c0e000` (`allwinner,sunxi-cedar-ve` and `...-ve1`, a dual VE), under power domain `pd_ve`.

Functions: `cedardev_open` / `cedardev_release` / `cedardev_mmap` and `_compat_cedardev_ioctl` (7648 bytes, the big ioctl dispatch) are the userspace surface; `enable_cedar_hw_clk` / `disable_cedar_hw_clk` / `setup_ve_freq` / `enable_cedar_hw_clk_module` manage the VE clocks and DVFS; `sunxi_cedar_probe` (2700 bytes) maps the engine and wires the interrupt; `ve_info_show` dumps state. So, like `vs-display`, the kernel driver is a conduit: the actual codec logic runs in userspace.

## Reuse, do not reimplement

The decode/encode intelligence is in the proprietary userspace libraries on the device: `libVE.so`, `libcedarse.so`, `libvdecoder.so` / `libvdecVcs.so` / `lib_dp_vdec.so`, `libvenc_{base,common,h264,h265,jpeg}.so` / `libvencoder.so`, `libawcodec2_vndk.so` (the Codec2 VNDK shim), plus the VE firmware in `/vendor/etc/firmware`. For the clean build:

- **Reuse path (default):** keep the vendor `sunxi-ve.ko` (or port its thin kernel glue) and the CedarX userspace blobs as-is. This is the standalone, lowest-risk option (the VE is decoupled from the display port, confirmed in [vs-display.md](vs-display.md) and the dependency graph).
- **Open path (optional, later):** mainline has the `cedrus` VPU driver for sunxi, but it covers fewer codecs than CedarX and would drop the vendor userspace, so it is only worth it if a fully-open decode path is a goal. Not a B1 concern.

## Status

No deep register RE done or needed for B1. This closes the B1 driver pass: the display path (`tvtop`, `tvpanel`, `vs-display`, `vs-osd`, `ksc`) is mapped for porting, and the VE is slated for blob reuse.
