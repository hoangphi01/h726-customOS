# Kernel (Phase B2): a buildable base for sun50iw15 (H726)

This directory holds **our deltas** for building a clean, from-source kernel for the H726: the device tree, defconfig, any CCU/pinctrl additions and patches. It does not vendor a full kernel tree or any vendor binary (see [../RULE.md](../RULE.md)); the base kernel and the anchor project are cloned locally and referenced here.

## The base decision

- **Mainline Linux (6.16.x), not a vendor BSP.** There is no public Allwinner 5.15 BSP for the sun50iw family (the H616 BSP is 4.9, Radxa's 5.15 targets the A527/sun55iw3), so there is no clean BSP tree to extend. Mainline is also the trustworthy, auditable base this project is for.
- **Anchored on [shift/sun50iw12p1-research](https://github.com/shift/sun50iw12p1-research)**, a mainline port for the **H713 (sun50iw12p1)** on the same HY300 projector family. It builds plain mainline Linux (6.16.7) plus a board device tree and the standard Allwinner H6 drivers, treating the SoC as H6-compatible: its DTS is `compatible = "...", "allwinner,sun50i-h713", "allwinner,sun50i-h6"`, so the mainline `sun50i-h6-ccu` and `pinctrl-sun50i-h6` bind. We reuse that method, its toolchain recipe, and the reusable driver/patch pieces. Credit to that project; its code stays under its own license and is not copied wholesale here.

## H713 vs H726: where we diverge

The H713 port and ours share the boring half and differ on the half that matters:

- **Reused from the H713 approach:** the overall mainline-port method, the DRAM-from-boot0 idea, the U-Boot "H6 base", Panfrost for the Mali-Midgard GPU, and the mainline peripheral drivers (MMC, USB, I2C, SPI, GMAC/`dwmac-sun8i`, PWM, thermal, crypto). Note the H713 could reuse the H6 CCU/pinctrl; sun50iw15 cannot (it is the A523 generation), so we wrote our own, which is the main extra work.
- **Not shared, the display:** the H713 HY300 drives its display through a **MIPS co-processor** (`sunxi-mipsloader` + `display.bin`); shift leaves TCON/LVDS/DSI/DE "not started". Our **H726 has no display co-processor** (the device tree has no MIPS/remoteproc node and no loader module); the display is direct hardware, already reverse-engineered in [../re/](../re/). So our display bring-up is our own work from the B1 register maps, not inherited, and we drop the MIPS pieces.

## Build (no Nix required)

`build.sh` is self-contained: it installs an ordinary cross toolchain on WSL Ubuntu, fetches mainline 6.16.7, drops in our device tree + CCU/pinctrl drivers, configures, and builds. The default target is **32-bit** (`TARGET=arm`, `zImage`), to match the vendor AArch32 boot chain; `TARGET=arm64` builds the reference `Image`.

```sh
HEARTBEAT=1 bash build.sh      # 32-bit zImage with the gate-#0 heartbeat
bash build.sh                  # 32-bit zImage
TARGET=arm64 bash build.sh     # 64-bit Image (reference; not bootable by the vendor chain)
```

Verified end to end:

- `sun50i-h726-hy300ultra.dtb`: **9276 bytes**, compiles with no errors or warnings (both arches).
- `arch/arm/boot/zImage`: **~11 MB**, with our CCU + pinctrl drivers built in (and the heartbeat with `HEARTBEAT=1`).
- `arch/arm64/boot/Image`: **~41 MB** via `TARGET=arm64`, kept for reference.

## sun50iw15 is not H6 (the key finding)

Unlike the H713, which the anchor could treat as H6-compatible, the H726's core controllers are **relocated** in the address map, so the mainline H6 `ccu`/`pinctrl` drivers do not simply apply. Comparing our extracted device tree against H6:

| Block | H6 / H713 | H726 (sun50iw15) |
|---|---|---|
| CCU | `0x3001000` | `0x2002000` (+ `cpupll@2001000`, `pll-dram@4830000`, `rtc-ccu@7090000`) |
| GIC | `0x3021000` | `0x3400000` |
| pinctrl | `0x300b000` | `0x2000000` |
| watchdog | `0x30090a0` | `0x2051000` |
| UART | `0x5000000` | `0x2600000` |
| THS | `0x5070400` | `0x7070400` |
| IOMMU | `0x30f0000` | `0x2010000` |
| eMMC / SID / R_CCU / R_PIO | same | `0x4022000` / `0x3006000` / `0x7010000` / `0x7022000` (match) |

So the device tree names the controllers as `allwinner,sun50iw15-ccu` / `-pinctrl` / `-r-ccu`. The genuinely new kernel work is a **sun50iw15 CCU and pinctrl driver**, which mainline does not have. These are now written (`ccu/ccu-sun50iw15.c`, `pinctrl/pinctrl-sun50iw15.c`, modelled on the sun55i A523 generation from the live clock tree in [../re/ccu-sun50iw15.md](../re/ccu-sun50iw15.md)) and compile into the kernel; their register offsets are an A523 hypothesis to be hardware-confirmed.

## Status

Phase B2 is done (DTS + defconfig + CCU/pinctrl drivers + build scripts, all verified to compile into the kernel), and gate #0 (the boot handoff) is **passed on hardware**. Phase B3b is the current work. The full reasoning is in **[bringup-strategy.md](bringup-strategy.md)**.

**Gate #0, the boot handoff: build 32-bit - passed (2026-10-28).** The stock kernel is AArch32 (verified by inspecting `boot_a`), the whole vendor stack runs the Cortex-A53 in 32-bit mode, and entering AArch64 on this SoC is unsolved (`sunxi-fel` reports `0x1911` unknown, `sunxi-fel uboot` fails `Unexpected SCTLR`, the RVBAR warm-reset to AArch64 hangs). So the kernel is built **32-bit (`ARCH=arm`)** to match the vendor chain (Branch 1); `build.sh` defaults to this. A GPIO beacon in the `zImage` decompressor ran on the device, confirming the vendor U-Boot enters our kernel. **B3b now walks the kernel to init with a ladder of checkpoint beacons and an authoritative appended device tree** ([heartbeat.md](heartbeat.md)). The loader is PhoenixSuit over FEL (`sunxi-fel` is unsupported on `0x1911`); a UART console is the real unlock. See [../STATUS.md](../STATUS.md).

## Planned contents

- `sun50i-h726-hy300ultra.dts` - our board DTS, converted from the extracted vendor device tree, H6-compatible where the hardware matches.
- `h726_defconfig` - from the extracted `config.gz`, filtered to mainline options.
- `sun50iw15-ccu.h` / pinctrl tables - the sun50iw15 deltas over the H6 drivers, where the clock/pin layout differs (spec: the DTS clock/pin nodes and the CCU clock inventory in [../re/kallsyms-map.md](../re/kallsyms-map.md)).
- `patches/` - our patches to mainline (and the reused ones from the anchor, kept by reference).
