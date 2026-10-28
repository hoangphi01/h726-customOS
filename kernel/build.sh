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
	echo ">> B3 checkpoint beacons (no-UART bring-up)"
	# Shared blink macro, copied next to each .S that includes it.
	CH=arch/arm/boot/compressed
	KH=arch/arm/kernel
	cp "$HERE/checkpoint/h726-blink.h" "$CH/"
	cp "$HERE/checkpoint/h726-blink.h" "$KH/"

	# CP1 (1 tick): decompressor entry = gate #0. Free registers here,
	# use r3-r6, preserve r0-r2 (kernel args). Also strip any legacy
	# heartbeat-h726.S include from an earlier build in a reused tree.
	sed -i '/h726-blink.h/d; /H726_BLINK/d; /H726_SPINTEST/d; /H726_BEEP/d; /heartbeat-h726.S/d' "$CH/head.S"
	# Default CP1 is the fan beacon. For diagnostics, swap H726_BLINK for
	# H726_SPINTEST (external-reset probe) or H726_BEEP (PA-toggle, proven
	# silent) - both defined in h726-blink.h.
	sed -i 's@^\( AR_CLASS(\tmrs\tr9, cpsr\t)\)@#include "h726-blink.h"\n\tH726_SPINTEST r3, r4, r5, r6\n\1@' "$CH/head.S"

	# CP2 (2 ticks): kernel-proper head.S, right after __create_page_tables
	# (MMU still off, physical). Only r0/r3/r6/r7 are free there.
	sed -i '/h726-blink.h/d; /H726_BLINK/d' "$KH/head.S"
	sed -i 's@^\(\tbl\t__create_page_tables\)@\1\n#include "h726-blink.h"\n\tH726_BLINK 2, r3, r0, r6, r7@' "$KH/head.S"

	# CP3 (3 ticks, early_initcall) + steady heartbeat (late_initcall), in C.
	cp "$HERE/checkpoint/h726-checkpoint.c" "$KH/"
	grep -q 'h726-checkpoint.o' "$KH/Makefile" || \
		echo 'obj-y += h726-checkpoint.o' >> "$KH/Makefile"

	# Minimal bring-up device tree (appended to the zImage below).
	cp "$HERE/sun50i-h726-hy300ultra-bringup.dts" "$DTSDIR/"
	grep -q 'sun50i-h726-hy300ultra-bringup.dtb' "$DTSDIR/Makefile" || \
		echo 'dtb-$(CONFIG_ARCH_SUNXI) += sun50i-h726-hy300ultra-bringup.dtb' >> "$DTSDIR/Makefile"
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
	if [ "${HEARTBEAT:-0}" = "1" ]; then
		# Our appended DTB must override the vendor U-Boot's DTB, and the
		# cmdline must be deterministic (no console we can read anyway).
		# nr_cpus=1 skips SMP secondary bring-up via PSCI/SMC. No earlycon:
		# an 8250 earlycon on an unclocked UART can busy-wait on THRE.
		./scripts/config --enable CONFIG_ARM_APPENDED_DTB \
			--enable CONFIG_ARM_ATAG_DTB_COMPAT \
			--enable CONFIG_CMDLINE_FORCE
		./scripts/config --set-str CONFIG_CMDLINE "nr_cpus=1 loglevel=7"
	fi
fi
make olddefconfig

echo ">> build"
if [ "$TARGET" = "arm64" ]; then
	make -j"$(nproc)" Image dtbs
else
	make -j"$(nproc)" zImage dtbs
fi

if [ "$TARGET" != "arm64" ] && [ "${HEARTBEAT:-0}" = "1" ]; then
	# Appended DTB: cat the bring-up DTB directly onto the zImage (no
	# padding, so CONFIG_ARM_APPENDED_DTB detects it at _edata). This is
	# the image to flash: it carries its own DT and overrides U-Boot's.
	BRINGUP_DTB="$DTSDIR/sun50i-h726-hy300ultra-bringup.dtb"
	cat "$IMG" "$BRINGUP_DTB" > "${IMG}-dtb"
	echo ">> appended-DTB image: ${IMG}-dtb ($(stat -c%s "${IMG}-dtb") bytes = zImage + bring-up DTB)"
fi

echo
echo ">> artifacts:"
ls -la "$IMG" "$DTSDIR/sun50i-h726-hy300ultra.dtb"
if [ "$TARGET" != "arm64" ] && [ "${HEARTBEAT:-0}" = "1" ]; then
	ls -la "${IMG}-dtb" "$DTSDIR/sun50i-h726-hy300ultra-bringup.dtb"
	echo ">> flash ${IMG}-dtb (NOT plain zImage) so the kernel uses our DTB."
fi
