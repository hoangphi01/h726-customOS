#!/usr/bin/env python3
# Pack our 32-bit zImage into the boot_a of a base IMAGEWTY (MINIMAL DIFF).
#
#   python3 pack_heartbeat_boot.py BASE_IMAGEWTY.img zImage out_boot_hb.img
#
# Use the rooted image as the base (root-tools/HY300Ultra_H726_ROOTED_v2.img)
# so flashing keeps root and recovery is just re-flashing that same image.
#
# This follows EXACTLY the method PhoenixSuit accepts (the root "v2" lesson,
# docs.md "BAI HOC VERIFY CHECKSUM"): copy the base image whole, overwrite
# ONLY one payload at its exact offset and same size, patch ONLY that
# partition's add_sum verify-file, and touch NOTHING else - no dlinfo, no
# sys_partition, no header. (The earlier dlinfo-surgery approach was REJECTED
# by PhoenixSuit; a full flash is safe anyway because Allwinner skips the
# reserve/calibration partition.)
#
# Here the one payload is boot_a: we overwrite only the kernel region inside
# its Android boot image, padded to the original kernel size, so boot_a's
# total size is unchanged. We confirm the add_sum algorithm reproduces the
# base boot's stored checksum before writing (abort on mismatch).
import sys, struct, shutil, os

BASE, ZIMG, OUT = sys.argv[1], sys.argv[2], sys.argv[3]


def add_sum(b: bytes) -> int:
    if len(b) % 4:
        b = b + b"\x00" * (4 - len(b) % 4)
    s = 0
    for i in range(0, len(b), 4):
        s = (s + struct.unpack_from("<I", b, i)[0]) & 0xFFFFFFFF
    return s


def imagewty_table(f):
    """name -> (offset, stored, original)."""
    f.seek(0)
    h = f.read(0x400)
    assert h[:8] == b"IMAGEWTY", "not an IMAGEWTY image"
    image_header_size, = struct.unpack_from("<I", h, 0x20)
    num_files, = struct.unpack_from("<I", h, 0x3c)
    ents = {}
    for i in range(num_files):
        f.seek(image_header_size + i * 0x400)
        e = f.read(0x400)
        fn = e[0x24:0x24 + 256].split(b"\x00")[0].decode("latin1")
        stored, _p1, original, _p2, off = struct.unpack_from("<5I", e, 0x124)
        ents[fn] = (off, stored, original)
    return ents


with open(BASE, "rb") as f:
    ents = imagewty_table(f)
    assert "boot.fex" in ents and "Vboot.fex" in ents, "boot.fex / Vboot.fex not found"
    b_off, b_stored, _ = ents["boot.fex"]
    v_off, v_stored, _ = ents["Vboot.fex"]
    f.seek(b_off); boot = bytearray(f.read(b_stored))
    f.seek(v_off); vboot = f.read(v_stored)

assert boot[:8] == b"ANDROID!", "boot.fex is not an Android boot image"
# Android boot image v3/v4: kernel_size @ 8, page size fixed at 4096, kernel @ 4096.
kernel_size, = struct.unpack_from("<I", boot, 8)
KPAGE = 4096
print(f"boot.fex    @ {b_off:#010x}  size {b_stored}  kernel_size {kernel_size}")
print(f"Vboot.fex   @ {v_off:#010x}  size {v_stored}")

# Confirm the add_sum algorithm against the stock boot before trusting it.
stock_sum = add_sum(bytes(boot))
stored_sum = struct.unpack_from("<I", vboot, 0)[0]
print(f"add_sum(stock boot) = {stock_sum:#010x}   Vboot stored = {stored_sum:#010x}   "
      f"{'MATCH' if stock_sum == stored_sum else 'MISMATCH'}")
if stock_sum != stored_sum:
    sys.exit("ABORT: add_sum does not reproduce the stock checksum; do not flash.")

z = open(ZIMG, "rb").read()
if len(z) > kernel_size:
    sys.exit(f"ABORT: zImage ({len(z)}) is larger than the kernel slot ({kernel_size}).")
print(f"zImage {len(z)} bytes -> padded to {kernel_size} in the kernel slot")

# Overwrite only the kernel region [4096 : 4096+kernel_size]; ramdisk untouched.
boot[KPAGE:KPAGE + kernel_size] = z + b"\x00" * (kernel_size - len(z))
assert len(boot) == b_stored, "boot size changed"
new_sum = add_sum(bytes(boot))
print(f"new add_sum(boot) = {new_sum:#010x}")

# Build the output image.
if os.path.exists(OUT) and os.path.getsize(OUT) == os.path.getsize(BASE):
    print("output exists with the right size, reusing the copy")
else:
    print("copying stock image (2 GB, a moment)...")
    shutil.copyfile(BASE, OUT)

# Minimal diff: overwrite ONLY boot.fex and its Vboot.fex. dlinfo, header,
# every other byte: identical to the base (this is what PhoenixSuit accepts).
f = open(OUT, "r+b")
f.seek(b_off); f.write(boot)
f.seek(v_off); f.write(struct.pack("<I", new_sum) + b"\x00" * (v_stored - 4))
f.close()
assert os.path.getsize(OUT) == os.path.getsize(BASE), "output size changed"
print("minimal diff: only boot.fex + Vboot.fex changed; dlinfo and all else untouched.")
print("DONE:", OUT)
print("Flashing this writes the whole base image (boot_a now ours); the")
print("reserve/calibration partition is skipped by Allwinner, so it is safe.")
