#!/bin/bash
# Build a mainline kernel + device tree for the HY300 Ultra (H726 / sun50iw15).
# Self-contained: fetches mainline Linux, drops in our DTS, configures, builds.
# Runs on Ubuntu/WSL. Booting is Phase B3; this produces the build artifacts.
#
#   bash kernel/build.sh [workdir]
#
set -e

KVER=6.16.7
WORK="${1:-$HOME/h726kernel}"
HERE="$(cd "$(dirname "$0")" && pwd)"   # the kernel/ dir of this repo

echo ">> toolchain"
if ! command -v aarch64-linux-gnu-gcc >/dev/null; then
	sudo apt-get update -qq
	sudo apt-get install -y gcc-aarch64-linux-gnu build-essential \
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

export ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu-

echo ">> sun50iw15 CCU + pinctrl drivers"
# dt-bindings headers (referenced by the device tree)
install -m644 "$HERE/ccu/sun50iw15-ccu.h"       include/dt-bindings/clock/sun50iw15-ccu.h
install -m644 "$HERE/ccu/sun50iw15-ccu-reset.h" include/dt-bindings/reset/sun50iw15-ccu.h
# CCU driver
cp "$HERE/ccu/ccu-sun50iw15.c"         drivers/clk/sunxi-ng/
cp "$HERE/ccu/sun50iw15-ccu.h"         drivers/clk/sunxi-ng/
cp "$HERE/ccu/sun50iw15-ccu-reset.h"   drivers/clk/sunxi-ng/
grep -q 'ccu-sun50iw15.o' drivers/clk/sunxi-ng/Makefile || \
	echo 'obj-$(CONFIG_SUNXI_CCU) += ccu-sun50iw15.o' >> drivers/clk/sunxi-ng/Makefile
# pinctrl driver
cp "$HERE/pinctrl/pinctrl-sun50iw15.c" drivers/pinctrl/sunxi/
grep -q 'pinctrl-sun50iw15.o' drivers/pinctrl/sunxi/Makefile || \
	echo 'obj-$(CONFIG_PINCTRL_SUNXI) += pinctrl-sun50iw15.o' >> drivers/pinctrl/sunxi/Makefile

echo ">> device tree"
cp "$HERE/sun50i-h726-hy300ultra.dts" arch/arm64/boot/dts/allwinner/
grep -q 'sun50i-h726-hy300ultra.dtb' arch/arm64/boot/dts/allwinner/Makefile || \
	echo 'dtb-$(CONFIG_ARCH_SUNXI) += sun50i-h726-hy300ultra.dtb' \
		>> arch/arm64/boot/dts/allwinner/Makefile

echo ">> config"
make defconfig
bash "$HERE/configure-h726.sh"
make olddefconfig

echo ">> build"
make -j"$(nproc)" Image dtbs

echo
echo ">> artifacts:"
ls -la arch/arm64/boot/Image \
	arch/arm64/boot/dts/allwinner/sun50i-h726-hy300ultra.dtb
