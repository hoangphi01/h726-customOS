# Kernel (Phase B2): a buildable base for sun50iw15 (H726)

This directory holds **our deltas** for building a clean, from-source kernel for the H726: the device tree, defconfig, any CCU/pinctrl additions and patches. It does not vendor a full kernel tree or any vendor binary (see [../RULE.md](../RULE.md)); the base kernel and the anchor project are cloned locally and referenced here.

## The base decision

- **Mainline Linux (6.16.x), not a vendor BSP.** There is no public Allwinner 5.15 BSP for the sun50iw family (the H616 BSP is 4.9, Radxa's 5.15 targets the A527/sun55iw3), so there is no clean BSP tree to extend. Mainline is also the trustworthy, auditable base this project is for.
- **Anchored on [shift/sun50iw12p1-research](https://github.com/shift/sun50iw12p1-research)**, a mainline port for the **H713 (sun50iw12p1)** on the same HY300 projector family. It builds plain mainline Linux (6.16.7) plus a board device tree and the standard Allwinner H6 drivers, treating the SoC as H6-compatible: its DTS is `compatible = "...", "allwinner,sun50i-h713", "allwinner,sun50i-h6"`, so the mainline `sun50i-h6-ccu` and `pinctrl-sun50i-h6` bind. We reuse that method, its toolchain recipe, and the reusable driver/patch pieces. Credit to that project; its code stays under its own license and is not copied wholesale here.

## H713 vs H726: where we diverge

The H713 port and ours share the boring half and differ on the half that matters:

- **Reused from the H713 approach:** the H6-compatible CCU and pinctrl binding, the DRAM-from-boot0 method, the U-Boot "H6 base", Panfrost for the Mali-Midgard GPU, and the mainline peripheral drivers (MMC, USB, I2C, SPI, GMAC/`dwmac-sun8i`, PWM, thermal, crypto).
- **Not shared, the display:** the H713 HY300 drives its display through a **MIPS co-processor** (`sunxi-mipsloader` + `display.bin`); shift leaves TCON/LVDS/DSI/DE "not started". Our **H726 has no display co-processor** (the device tree has no MIPS/remoteproc node and no loader module); the display is direct hardware, already reverse-engineered in [../re/](../re/). So our display bring-up is our own work from the B1 register maps, not inherited, and we drop the MIPS pieces.

## Build host (no Nix required)

shift's `flake.nix` only provides a dev shell (an aarch64 cross toolchain, `dtc`, `sunxi-tools`, `bison`/`flex`/`bc`/`openssl`); it does not pin the kernel source (plain mainline is downloaded and configured by script). So the same build works with an ordinary toolchain:

- WSL Ubuntu, `gcc-aarch64-linux-gnu`, `device-tree-compiler`, `bc bison flex libssl-dev`, `u-boot-tools`, `sunxi-tools`.
- `ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu-`.

## Build recipe (verified)

Toolchain and base confirmed working end to end, no Nix:

```sh
# toolchain (WSL Ubuntu)
sudo apt-get install -y gcc-aarch64-linux-gnu build-essential \
    device-tree-compiler bc bison flex libssl-dev u-boot-tools libncurses-dev

# mainline base (matches the anchor's 6.16.7)
wget https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-6.16.7.tar.xz
tar xf linux-6.16.7.tar.xz && cd linux-6.16.7
export ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu-
make defconfig            # generic arm64, includes CONFIG_ARCH_SUNXI
make allwinner/<board>.dtb
```

**Sanity pass done:** dropping the anchor's `sun50i-h713-hy300.dts` into `arch/arm64/boot/dts/allwinner/` and building it produces a valid 14 KB DTB (only benign `wifi@1` reg-format warnings). So the toolchain, the mainline tree and the H6-compatible DTS approach all work; our H726 work plugs into the same pipeline.

## Status

Phase B2 in progress. Done: the base decision (above), the build host, and the anchor's H713 DTB built as a sanity check. Next: convert the extracted vendor device tree into `sun50i-h726-hy300ultra.dts` (H6-compatible, using the anchor's H713 DTS as the structural template), assess how far the H6 `ccu`/`pinctrl` cover sun50iw15, and get our DTS + an `h726_defconfig` building, then the kernel `Image`. Booting is Phase B3 (needs the no-serial debug harness, since there is no UART yet). See [../STATUS.md](../STATUS.md).

## Planned contents

- `sun50i-h726-hy300ultra.dts` - our board DTS, converted from the extracted vendor device tree, H6-compatible where the hardware matches.
- `h726_defconfig` - from the extracted `config.gz`, filtered to mainline options.
- `sun50iw15-ccu.h` / pinctrl tables - the sun50iw15 deltas over the H6 drivers, where the clock/pin layout differs (spec: the DTS clock/pin nodes and the CCU clock inventory in [../re/kallsyms-map.md](../re/kallsyms-map.md)).
- `patches/` - our patches to mainline (and the reused ones from the anchor, kept by reference).
