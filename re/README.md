# Reverse-engineering notes (Phase B1)

Register and architecture maps of the vendor drivers that mainline Linux cannot supply, reverse-engineered to drive the hardware from an open kernel (Track B). No vendor binaries are committed here (see [../RULE.md](../RULE.md)); these notes are our own analysis. Method is ARM `objdump` (and Ghidra where decompiled C is needed), anchored on the device-tree register bases, reproducible via `scratchpad/trackb/disasm.sh`.

All the vendor display drivers are loadable modules, not built into `vmlinux`; the kernel is not stripped, so `/proc/kallsyms` names everything and no signature matching is needed. See [kallsyms-map.md](kallsyms-map.md) for the module / built-in boundary and the one built-in piece (the CCU clock tree, a B2 item).

## The display stack, in bring-up order

| Driver | Note | Role | Port difficulty |
|---|---|---|---|
| `sunxi_tvtop` | [tvtop.md](tvtop.md) | TV-subsystem top: shared clocks, resets, ADC, sub-block gate registers | low (standard clk/reset + a few gate writes) |
| `tvpanel` (DE top + TCON + LVDS + DSI) | [tvpanel.md](tvpanel.md) | the output stack in one module: `tvdisp_top` routing/mux + PLL, TCON timing, LVDS PHY (`buf`/`wm_top`/`tx20`), DSI D-PHY | medium (port-and-diff against mainline sun50i tcon/lvds/dsi) |
| `z20hd720m` / `panel_lvds_gen` | [panel.md](panel.md) | thin DT-driven LVDS panel glue (timings + GPIO power) | trivial (DT data) |
| `vs-display` | [vs-display.md](vs-display.md) | orchestrator: maps five register windows (soc/afbd/cap/svp/panel), 9 IRQs, misc dev + DMA-BUF pool. Pixel processing is in the userspace blob | kernel conduit low; userspace pixel path reuse-or-RE |
| `vs-osd` | [vs-osd.md](vs-osd.md) | fbdev + CRTC / UI plane ("wce"), register programming in-kernel, routed through keystone | medium (maps to a DRM CRTC + primary plane) |
| `sunxi_ksc` | [ksc.md](ksc.md) | hardware keystone: LUT-based geometric warp + antialias/sharpen | medium (register file ~0x100 mapped) |
| `sunxi-ve` | [ve.md](ve.md) | standalone Cedar VE char driver (conduit); reuse the CedarX blobs | low (reuse blob) |

## The shape of the pipeline

`sunxi_tvtop` owns the shared clocks and resets. The DE top in `tvpanel` (`tvdisp_top`, `tvdisp@5500000`) is the routing/mux + PLL: its registers (AFBD `0x04`, SVP `0x40`, TCON/DSI `0x8C`+`0x138`, KSC `0xA0`) gate each sub-block onto the panel, driven by `vs-display`/`vs-osd` through the exported `sunxi_tvdisp_top_*` symbols. `vs-display` maps and sequences the capture + scaler/deinterlace/NR (`svp`) + AFBD windows; the output goes through TCON -> LVDS (`tvpanel`) to the `z20hd720m` imager. `vs-osd` composites the UI framebuffer as a CRTC/plane. Both the video path and the UI plane route through the `ksc` keystone warp before the panel, so the whole projected image is geometry-corrected. The one genuinely proprietary, not-in-kernel part is the `svp`/`cap` pixel-processing programming inside `vs-display`, which runs from the userspace display blob.

See [../STATUS.md](../STATUS.md) for the project state and [../PLAN.md](../PLAN.md) for the roadmap.
