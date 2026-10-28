#!/usr/bin/env python3
# Pack our 32-bit zImage into the stock boot_a and produce a flashable
# IMAGEWTY that PhoenixSuit writes to boot_a ONLY (surgical), so the rooted
# system, /data and the calibration partition are left untouched.
#
#   python3 pack_heartbeat_boot.py STOCK_IMAGEWTY.img zImage out_boot_hb.img
#
# How it stays safe and correct:
#  - it finds boot.fex / Vboot.fex by walking the IMAGEWTY file table (no
#    hardcoded offsets);
#  - it overwrites ONLY the kernel region inside boot_a's Android boot image,
#    padded to the original kernel size, so boot_a's total size is unchanged
#    (a minimal same-size swap, the pattern proven by the root build);
#  - it recomputes the add_sum verify-file, after first confirming the
#    algorithm reproduces the stock boot's stored checksum (aborts if not);
#  - it rewrites dlinfo to list only boot_a, so nothing else is flashed.
#
# Recovery is always available: re-flash the stock IMAGEWTY over FEL.
import sys, struct, shutil, os

STOCK, ZIMG, OUT = sys.argv[1], sys.argv[2], sys.argv[3]
DLINFO_OFF, DLINFO_LEN = 0x0031e2b0, 16384
DL_MAGIC = b"\xef\xa4\x10\xcb"


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


with open(STOCK, "rb") as f:
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
if os.path.exists(OUT) and os.path.getsize(OUT) == os.path.getsize(STOCK):
    print("output exists with the right size, reusing the copy")
else:
    print("copying stock image (2 GB, a moment)...")
    shutil.copyfile(STOCK, OUT)

f = open(OUT, "r+b")
f.seek(b_off); f.write(boot)
f.seek(v_off); f.write(struct.pack("<I", new_sum) + b"\x00" * (v_stored - 4))

# Surgical dlinfo: keep only the boot_a entry so PhoenixSuit flashes boot_a only.
f.seek(DLINFO_OFF); dl = bytearray(f.read(DLINFO_LEN))
assert dl[:4] == DL_MAGIC, "dlinfo magic mismatch"
count, = struct.unpack_from("<I", dl, 0x10)
ENTRY, BASE = 0x48, 0x20


def find_dl(name):
    for i in range(count):
        o = BASE + i * ENTRY
        nm = dl[o:o + 16].split(b"\x00")[0].decode("latin1")
        if nm == name:
            return bytes(dl[o:o + ENTRY])
    raise KeyError(name)


boot_entry = find_dl("boot_a")
new = bytearray(DLINFO_LEN)
new[0:0x20] = dl[0:0x20]
struct.pack_into("<I", new, 0x10, 1)          # count = 1
new[BASE:BASE + ENTRY] = boot_entry
f.seek(DLINFO_OFF); f.write(new)
f.close()
print("dlinfo rewritten: count=1 (boot_a only); nothing else is flashed.")
print("DONE:", OUT)
