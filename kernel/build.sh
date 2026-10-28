#!/bin/bash
# Build a mainline kernel + device tree for the HY300 Ultra (H726 / sun50iw15).
# Self-contained: fetches mainline Linux, drops in our DTS + drivers, builds.
# Runs on Ubuntu/WSL.
#
#   bash kernel/build.sh [workdir]
#
# Default target is 32-bit ARM (zImage), to match the vendor AArch32 boot
# chain (see bringup-strategy.md). Set TARGET=arm64 for the 64-bit Image
# (does not boot via the vendor chain yet; kept for reference).
#
set -e

KVER=6.16.7
WORK="${1:-$HOME/h726kernel}"
HERE="$(cd "$(dirname "$0")" && pwd)"   # the kernel/ dir of this repo
TARGET="${TARGET:-arm}"

if [ "$TARGET" = "arm64" ]; then
	ARCH=arm64; CROSS=aarch64-linux-gnu-; GCCPKG=gcc-aarch64-linux-gnu
	DTSDIR=arch/arm64/boot/dts/allwinner; IMG=arch/arm64/boot/Image; DEFCFG=defconfig
else
	ARCH=arm; CROSS=arm-linux-gnueabihf-; GCCPKG=gcc-arm-linux-gnueabihf
	DTSDIR=arch/arm/boot/dts/allwinner; IMG=arch/arm/boot/zImage; DEFCFG=multi_v7_defconfig
fi

echo ">> toolchain ($ARCH)"
if ! command -v "${CROSS}gcc" >/dev/null; then
	sudo apt-get update -qq
	sudo apt-get install -y "$GCCPKG" build-essential \
		device-tree-compiler bc bison flex libssl-dev u-boot-tools libncurses-dev
fi

echo ">> mainline linux-$KVER"
mkdir -p "$WORK" && cd "$WORK"
if [ ! -d "linux-$KVER" ]; then
	[ -f "linux-$KVER.tar.xz" ] || \
		wget -q "https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-$KVER.tar.xz"
	tar xf "linux-$KVER.tar.xz"
fi
cd "linux-$KVER"

export ARCH CROSS_COMPILE="$CROSS"

echo ">> sun50iw15 CCU + pinctrl drivers"
install -m644 "$HERE/ccu/sun50iw15-ccu.h"       include/dt-bindings/clock/sun50iw15-ccu.h
install -m644 "$HERE/ccu/sun50iw15-ccu-reset.h" include/dt-bindings/reset/sun50iw15-ccu.h
cp "$HERE/ccu/ccu-sun50iw15.c" "$HERE/ccu/sun50iw15-ccu.h" "$HERE/ccu/sun50iw15-ccu-reset.h" \
	drivers/clk/sunxi-ng/
grep -q 'ccu-sun50iw15.o' drivers/clk/sunxi-ng/Makefile || \
	echo 'obj-$(CONFIG_SUNXI_CCU) += ccu-sun50iw15.o' >> drivers/clk/sunxi-ng/Makefile
cp "$HERE/pinctrl/pinctrl-sun50iw15.c" drivers/pinctrl/sunxi/
grep -q 'pinctrl-sun50iw15.o' drivers/pinctrl/sunxi/Makefile || \
	echo 'obj-$(CONFIG_PINCTRL_SUNXI) += pinctrl-sun50iw15.o' >> drivers/pinctrl/sunxi/Makefile

echo ">> device tree"
cp "$HERE/sun50i-h726-hy300ultra.dts" "$DTSDIR/"
grep -q 'sun50i-h726-hy300ultra.dtb' "$DTSDIR/Makefile" || \
	echo 'dtb-$(CONFIG_ARCH_SUNXI) += sun50i-h726-hy300ultra.dtb' >> "$DTSDIR/Makefile"

if [ "$TARGET" != "arm64" ] && [ "${HEARTBEAT:-0}" = "1" ]; then
	echo ">> gate-#0 GPIO heartbeat (decompressor)"
	CH=arch/arm/boot/compressed
	cp "$HERE/heartbeat/heartbeat-h726.S" "$CH/"
	sed -i '/heartbeat-h726.S/d' "$CH/head.S"
	sed -i 's@^\( AR_CLASS(\tmrs\tr9, cpsr\t)\)@#include "heartbeat-h726.S"\n\1@' "$CH/head.S"
fi

echo ">> config"
make "$DEFCFG"
if [ "$TARGET" = "arm64" ]; then
	bash "$HERE/configure-h726.sh"
else
	# multi_v7 already carries most sunxi options; ensure ours are on.
	./scripts/config --enable CONFIG_ARCH_SUNXI \
		--enable CONFIG_SUNXI_CCU --enable CONFIG_PINCTRL_SUNXI \
		--enable CONFIG_PSTORE --enable CONFIG_PSTORE_RAM
fi
make olddefconfig

echo ">> build"
if [ "$TARGET" = "arm64" ]; then
	make -j"$(nproc)" Image dtbs
else
	make -j"$(nproc)" zImage dtbs
fi

echo
echo ">> artifacts:"
ls -la "$IMG" "$DTSDIR/sun50i-h726-hy300ultra.dtb"
