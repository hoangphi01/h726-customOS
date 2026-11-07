# Interactive bring-up shell (initramfs + ttyS0)

How the mainline 6.16.7 kernel was walked from "boots to the rootfs-mount panic" to a **working
interactive shell over UART** on the HY300 Ultra (H726 / sun50iw15). Confirmed 2026-11-07: typed commands
over the serial console get real responses (`echo`, `uname -a`, `cat /proc/cpuinfo`, `ls /`); captured
transcript in [../re/h726-kernel-shell.txt](../re/h726-kernel-shell.txt). This is the first interactive
mainline Linux on this SoC.

Two pieces are needed: a userspace to run as init (there is no eMMC rootfs yet), and a real serial console
for input (earlycon is output only).

## 1. A userspace: static busybox + initramfs

Build a **static** armhf busybox (static so it needs no libc in the initramfs):

```
# busybox source from the github mirror (busybox.net is reachable but very slow here)
wget https://github.com/mirror/busybox/archive/refs/tags/1_36_1.tar.gz -O bb.tar.gz
tar xf bb.tar.gz && cd busybox-1_36_1
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- defconfig
sed -i 's/# CONFIG_STATIC is not set/CONFIG_STATIC=y/' .config
sed -i 's/^CONFIG_TC=y/# CONFIG_TC is not set/' .config    # tc breaks the static build
yes "" | make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- oldconfig
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- -j"$(nproc)"
# -> busybox: ELF 32-bit ARM, statically linked
```

Assemble the initramfs directory (the busybox binary is a build artifact and is not committed; the `init`
script is in [initramfs/init](initramfs/init)):

```
mkdir -p initramfs/{bin,sbin,proc,sys,dev,etc}
cp busybox            initramfs/bin/busybox
cp kernel/initramfs/init  initramfs/init      # chmod +x, LF line endings
ln -sf busybox        initramfs/bin/sh
mknod initramfs/dev/console c 5 1             # run the build as root so these are preserved
mknod initramfs/dev/null    c 1 3
```

Two gotchas that cost a boot each:

- **`/init` shebang must point at a binary that exists at exec time.** `#!/bin/sh` fails with
  `Failed to execute /init (error -2)` because `/bin/sh` is only created later by `busybox --install`.
  Use `#!/bin/busybox sh`.
- **`/dev/console` must be a static node in the initramfs.** `CONFIG_DEVTMPFS_MOUNT` does **not** mount
  devtmpfs for an initramfs, so without a static `/dev/console` the kernel prints
  `Warning: unable to open an initial console` and init has no stdio. `/init` then mounts devtmpfs itself
  for everything else.

## 2. A serial console: ttyS0 without the CCU

The UART0 module clock is CCU index 90 (vendor numbering), not modelled in the minimal CCU yet, so the
8250 cannot get a clock and `ttyS0` would not probe. `apb-uart` is in the 24 MHz domain
([../re/ccu-sun50iw15.md](../re/ccu-sun50iw15.md)), so the bring-up DTS gives the node a fixed
`clock-frequency = <24000000>` and drops `clocks`/`resets`/`pinctrl` - U-Boot already clocks and muxes
PH0/PH1, and the 8250 driver programs 115200 from the fixed rate. See the `serial@2600000` node in
[sun50i-h726-hy300ultra-bringup.dts](sun50i-h726-hy300ultra-bringup.dts).

## 3. build.sh glue

`HEARTBEAT=1 bash kernel/build.sh` wires it together ([build.sh](build.sh)):

- `CONFIG_BLK_DEV_INITRD`, `CONFIG_DEVTMPFS` + `_MOUNT`, and `CONFIG_INITRAMFS_SOURCE` = the initramfs dir
  (default `/root/initramfs`, override with `INITRAMFS=/path`). The kernel packs it into the image, so no
  separate initrd is flashed. Run the build **as root** so the `/dev/*` nodes are preserved in the cpio.
- cmdline adds `console=ttyS0,115200` (interactive console) on top of `earlycon ... keep_bootcon`.

With an initramfs there is no `root=` and no VFS-mount panic: the kernel runs `/init` from the initramfs.

## The loop

Build -> repack `boot_hb.img` -> `dd` to `boot_b` (slot cloned from a) -> reboot to slot b -> read/drive
`ttyS0` over the UART (bidirectional: adapter RXD->board TX, TXD->board RX, GND). Power-cycle returns to
stock `boot_a`. See [../re/uboot-console.md](../re/uboot-console.md) for the /data-safe slot-clone loop.
