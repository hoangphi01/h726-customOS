#!/usr/bin/env python3
# Build a boot_a image with our 32-bit zImage as the kernel, to flash by
# `dd` on the rooted device. No PhoenixSuit, no IMAGEWTY, no add_sum verify
# files: `dd` writes the raw partition and U-Boot reads it directly (verity
# is disabled), so the whole verify-checksum problem is sidestepped. Only
# boot_a is touched; /data, super, root and calibration are untouched.
#
#   python3 build_heartbeat_boot.py SOURCE zImage out_boot_a.img
#
# SOURCE is either the stock IMAGEWTY (we take its boot.fex) or a raw boot_a
# image you dumped from the device (`dd if=/dev/block/by-name/boot_a`).
#
# We keep the stock Android boot image whole and overwrite ONLY the kernel
# region, padded to the original kernel size, so the boot image is unchanged
# in size and layout (ramdisk, header, cmdline all intact). The heartbeat
# runs in the decompressor, before the device tree is read, so the stock
# header/DTB being "wrong" for our kernel does not matter for this test.
import sys, struct

SOURCE, ZIMG, OUT = sys.argv[1], sys.argv[2], sys.argv[3]
data = open(SOURCE, "rb").read(0x400)

if data[:8] == b"IMAGEWTY":
    # pull boot.fex out of the IMAGEWTY file table
    f = open(SOURCE, "rb")
    h = f.read(0x400)
    image_header_size, = struct.unpack_from("<I", h, 0x20)
    num_files, = struct.unpack_from("<I", h, 0x3c)
    boot = None
    for i in range(num_files):
        f.seek(image_header_size + i * 0x400)
        e = f.read(0x400)
        fn = e[0x24:0x24 + 256].split(b"\x00")[0].decode("latin1")
        if fn == "boot.fex":
            stored, _p1, _orig, _p2, off = struct.unpack_from("<5I", e, 0x124)
            f.seek(off); boot = bytearray(f.read(stored)); break
    if boot is None:
        sys.exit("boot.fex not found in the IMAGEWTY")
    print(f"took boot.fex from the IMAGEWTY ({len(boot)} bytes)")
else:
    boot = bytearray(open(SOURCE, "rb").read())
    print(f"using the raw boot image ({len(boot)} bytes)")

assert boot[:8] == b"ANDROID!", "source is not an Android boot image"
kernel_size, = struct.unpack_from("<I", boot, 8)   # v3/v4: kernel_size @ 8
KPAGE = 4096                                        # v3/v4 page size is fixed
print(f"stock kernel_size = {kernel_size}  (kernel at offset {KPAGE})")

z = open(ZIMG, "rb").read()
if len(z) > kernel_size:
    sys.exit(f"ABORT: zImage ({len(z)}) is larger than the kernel slot ({kernel_size}).")

# Overwrite ONLY the kernel region; ramdisk and everything else stay put.
boot[KPAGE:KPAGE + kernel_size] = z + b"\x00" * (kernel_size - len(z))
open(OUT, "wb").write(boot)
print(f"zImage {len(z)} bytes -> padded into the kernel slot")
print(f"DONE: {OUT}  ({len(boot)} bytes, same size as stock boot_a)")
print("Flash it on the device with:")
print("  adb push", OUT, "/data/local/tmp/boot_hb.img")
print("  adb shell su -c 'dd if=/data/local/tmp/boot_hb.img of=/dev/block/by-name/boot_a'")
