# RE: `sunxi_ksc` (hardware keystone engine)

Register and behaviour map for the keystone-correction block, reverse-engineered from the vendor module `sunxi_ksc.ko` (not redistributed; see [RULE.md](../RULE.md)). This is Phase B1 work: enough to drive the block from an open driver, cross-checked against the device tree.

## Source and method

- Module: `sunxi_ksc.ko`, `AllWinner`, `license=GPL`, `compatible = allwinner,ksc100` (also `ksc110`), `depends=tvpanel`, `import_ns=DMA_BUF`. Platform driver, `alias=platform:ksc`.
- Device-tree anchor (from the extracted DTS): `ksc@5900000`, `reg = <0x5900000 0x100000>`, one interrupt (`0x69`), one clock (`clk0` at ~480 MHz, `clk0_freq = 0x1c9c3800`). So the register base is **`0x5900000`**; the active register file is about the first `0x100` bytes.
- Method: `arm-linux-gnueabihf-objdump -d -r sunxi_ksc.ko`, read the `ksc_reg_*` / `is_*` functions, map `ldr`/`str [base, #offset]` against the DT base. First pass (status and control registers firmly decoded; the bulk programming functions enumerated, full bitfields pending a Ghidra decompile). Reproduce with `scratchpad/trackb/disasm.sh`.

## Register map (base `0x5900000`)

Firmly decoded from the small accessor functions:

| Offset | Access | Field | Meaning |
|---|---|---|---|
| `0x08` | RW, W1C | IRQ status + control | The main interrupt/commit register. `ksc_reg_irq_query` builds the IRQ bitmap from bits 4, 8, 10, 15 (FE/BE finish and error, LUT-over-span), masks with `0x11110`, and writes the value back to acknowledge. Bit **24** (`0x1000000`) is an error flag; bit **20** (`0x100000`) is "register update finished" / commit-done (`is_reg_update_finish` returns bit 20 and sets it to arm the next commit). |
| `0x0C` | R (16-bit) | version | `ksc_reg_get_version` returns the half-word here. |
| `0x14` | RW | secondary control | `is_reg_update_finish` sets bit **8** (`0x100`) here as part of arming a commit, gated by a mode bit. |
| `0x1B` | R (byte) | core status | `is_ksc_core_busy` returns bit **0** = core busy. |
| `0x01` | R (byte) | mode/config | A byte whose bit **1** selects a path (checked before touching `0x14`). |

## Register file footprint (offsets written by the programming functions)

Enumerated (4-byte aligned, so 32-bit MMIO registers), semantics to be filled in by the Ghidra decompile:

- `ksc_reg_update_all` (the full commit, 2252 bytes): writes `0x04, 0x10, 0x14, 0x1C, 0x20, 0x24, 0x28, 0x2C, 0x30, 0x34, 0x3C, 0x40, 0x54, 0x58, 0x5C, 0x64, 0x68, 0x6C, 0x74, 0x78, 0x84, 0x94, 0x9C, 0xE8, 0xEC, 0xF0`. It programs field masks such as `0xdfff_fcff`, `0x41_8930`, `0xffff_efef`, `0xe000_fff0`, `0x0010_1000` (geometry/scale coefficients and enables).
- `ksc_reg_pq_setting` (picture quality, 908 bytes): writes `0x88, 0x8C, 0xC0, 0xC4, 0xC8, 0xCC, 0xD0, 0xD4, 0xD8, 0xDC, 0xE0`. This is the antialias (range 0..128) and sharpen (level 0..4) block.
- `ksc_reg_other_setting` (660 bytes): writes `0x38, 0x44, 0x48, 0x4C, 0xB4, 0xB8, 0xBC, 0xDC, 0xE4, 0xF4, 0xF8, 0xFC, 0x100`.

So the KSC register window in use is roughly `0x00`..`0x100`.

## LUT double-buffer (software state, not MMIO)

`ksc_reg_switch_lut_buf(dev, ctx)` is bookkeeping over a context struct, not register writes: it compares `ctx+0x188` against `dev+0x84` and selects one of three LUT buffer descriptors (`ctx+0x184/0x18c/0x190/0x194/0x198`), storing the chosen pair into `ctx+0x178` / `ctx+0x17c`. This matches the three LUT banks (`Memory0/1/2`) and the live double-buffer swap seen in the strings pass; the actual warp table is uploaded via `mmap` + DMA-BUF and committed through register `0x08` bit 20.

## How keystone works (confirmed)

A LUT-based geometric warp, not a matrix. Userspace computes a corner-geometry lookup table, maps it in through the char device (`ksc_open` / `ksc_ioctl` / `ksc_mmap`, DMA-BUF import), the driver points the hardware at the active LUT bank (`ksc_reg_switch_lut_buf`), programs geometry and PQ (`ksc_reg_update_all` + `ksc_reg_pq_setting`), then commits via `0x08` and waits on the commit/finish bit. On-chip it also does antialias (0..128) and sharpen (0..4). The display pipeline switches it on inline through the exported `ksc_online_enable` (`EXPORT_SYMBOL_GPL`) and `tvdisp_ksc_enable`; FE/BE finish and error interrupts gate the frame handoff from the SVP path.

## For the clean build

Keystone needs: this register block driven from an open driver (or the vendor `.ko` ported), its `tvpanel` dependency, the userspace table generator (the vendor keystone app), and `ksc_online_enable` wired into the clean kernel's display path. The warp table format is produced by the userspace app, so the kernel side is mostly: map the LUT banks, program `0x04`..`0x100`, commit on `0x08`, service the IRQ.

## Next

- Ghidra headless decompile of `ksc_reg_update_all` / `ksc_reg_pq_setting` for the exact bitfields of `0x04`..`0x100` and the geometry-coefficient layout.
- Then `sunxi_tvtop` (the shared clk/reset/ADC top the display path hooks into).
