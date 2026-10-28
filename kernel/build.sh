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
