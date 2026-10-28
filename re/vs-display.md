# RE: `vs-display` (the display orchestrator)

Structural map of the top display driver, from `vs-display.ko` (not redistributed; see [RULE.md](../RULE.md)). This is the `vs,display` node, the orchestrator the rest of the pipeline hangs off.

## Source and method

- Module: `vs-display.ko`, author `Display Team`, `license=GPL v2`, `name=vs_display`, `depends=tvpanel, sunxi_tvtop`. No `EXPORT_SYMBOL` (it is a leaf: it calls down into tvtop and tvpanel, nothing links against it).
- DT anchor (from the DTS): `vs-display`, `reg-names = "soc", "afbd", "cap", "svp", "panel"`, five windows: `soc`=`0x6e00000`/`0x1000`, `afbd`=`0x5600000`/`0x1000`, `cap`=`0x6800000`/`0x1c1000`, `svp`=`0x5000000`/`0x341000`, `panel`=`0x5800000`/`0x100000`; nine interrupts (`cap`, `deint`, `meter`, `vnr`, `panel0`, `panel1`, `proc`, `scaler`, `afbd_dec`); twenty clocks.
- Method: ARM objdump (`vsdp_probe`, `fops_ioctl`, the `dp_buf_*` and `init_interrupt` set) plus the `.modinfo` / rodata strings. (Ghidra 12.2 headless was set up but rejects the available JDK 21; objdump was clearer here anyway. Ghidra can be revisited with a matching JDK for the pixel-processing math.)

## Register model: five named segments

The driver keeps its registers as named segments (`dp_reg_segment_t`, enum `dp_reg_segment_id_t`), which map one-to-one onto the DT `reg-names`:

| Segment id | DT window | Base / size | Role |
|---|---|---|---|
| `kDpRegGid_Soc` | `soc` | `0x6e00000` / `0x1000` | display SoC-level control (shared with tvtop's second window at `0x6e00000`) |
| `kDpRegGid_Afbd` | `afbd` | `0x5600000` / `0x1000` | AFBD (frame-buffer decompression) decoder |
| `kDpRegGid_Cap` | `cap` | `0x6800000` / `0x1c1000` | capture engine (HDMI / TV-in) |
| `kDpRegGid_Svp` | `svp` | `0x5000000` / `0x341000` | the scaler / video-processing pipeline (deinterlace, noise reduction, meter, scaler, proc) |
| `kDpRegGid_Panel` | `panel` | `0x5800000` / `0x100000` | panel / LVDS output (same window as `lvds0`) |

`vsdp_probe` iterates a global `reg_names[]`, and for each calls `platform_get_resource_byname(pdev, IORESOURCE_MEM, name)` then `devm_ioremap`, storing per segment: size, phys start, and the ioremapped base (a 12-byte entry per segment in the device struct, starting at `+0x98`). So the driver's whole job at probe is to map these five windows, wire the nine interrupts (`init_interrupt` / `get_sub_interrupt` per source), register a misc char device, and set up the buffer pool.

## Key architectural finding: the kernel driver is mostly a conduit

The heavy pixel-processing logic (how the scaler / deinterlace / NR / AFBD registers in the `svp` and `cap` windows are actually programmed) does **not** live in this `.ko`. The driver exposes the mapped register windows and DMA buffers to userspace:

- `DP_IOCTL_GET_REGISTERS` (`ioctl_get_registers`, `dp_ioctl_registers_t`): hands a register segment's phys start / size to userspace.
- `fops_mmap` / `vs display mmap`: maps a segment or a buffer into userspace (`[0x%lx : 0x%lx] prot ... offset ... size`).
- `dp_buf_allocate` / `dp_buf_map_fd` / `dp_buf_free`: a DMA-BUF-backed buffer pool shared with userspace and the pipeline.
- a misc char device (`misc_register`, "VS display could not register to misc driver") is the control node.

So the real display algorithms run in the **userspace blob** (the display HAL / `libdisplayconfig.so` and friends) writing these mapped registers. For the clean build this is good news and bad news: the kernel side (`vs-display`) is a tractable conduit (map 5 windows, 9 IRQs, a misc dev, a DMA-BUF pool, and the calls down to tvtop/tvpanel/afbd/svp enable), but the proprietary register programming is in userspace, so either the userspace display blob is reused as-is (reuse-with-blob) or that programming is RE'd separately.

## Calls down into the rest of the pipeline

From the import list (see [tvtop.md](tvtop.md) and the dependency graph): `sunxi_tvtop_clk_get/put`, `sunxi_tvpanel_prepare/enable/wait_init_done`, `sunxi_tvdisp_top_afbd_enable`, `sunxi_tvdisp_top_svp_enable`, `sunxi_enable_device_iommu`. So vs-display's enable path is: take the tvtop clocks, enable the panel, enable AFBD and SVP on the display top, and bind the IOMMU, then run under userspace control.

## For the clean build

- Kernel side is portable: a DRM/KMS or platform driver that maps the five windows, services the nine IRQs, registers the buffer pool and misc/ioctl/mmap surface, and sequences tvtop + tvpanel + afbd + svp + iommu. This is also what a stable Track A GSI needs (the open DRM/KMS path that replaces the vendor HWC's frame-commit handshake).
- Pixel-processing registers (`svp` / `cap` / `afbd`) are programmed from userspace; reuse the vendor display blob first, RE the userspace programming only if a fully-open path is wanted.

## Next

- `vs-osd` (the OSD layer; depends on `sunxi_ksc`, so the UI plane is keystone-warped), then extracting `tcon`/`lvds` from `vmlinux` (see [panel.md](panel.md)).
