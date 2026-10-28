# Reverse-engineering notes (Phase B1)

Register and architecture maps of the vendor drivers that mainline Linux cannot supply, reverse-engineered to drive the hardware from an open kernel (Track B). No vendor binaries are committed here (see [../RULE.md](../RULE.md)); these notes are our own analysis. Method is ARM `objdump` (and Ghidra where decompiled C is needed), anchored on the device-tree register bases, reproducible via `scratchpad/trackb/disasm.sh`.

## The display stack, in bring-up order

| Driver | Note | Role | Port difficulty |
|---|---|---|---|
| `sunxi_tvtop` | [tvtop.md](tvtop.md) | TV-subsystem top: shared clocks, resets, ADC, sub-block gate registers | low (standard clk/reset + a few gate writes) |
| `tcon` / `lvds0` | [panel.md](panel.md) | LVDS/TCON PHY timing and output. Built into `vmlinux` (not a `.ko`), extract and diff against mainline sun50i | medium (port-and-diff from mainline) |
| `z20hd720m` / `panel_lvds_gen` | [panel.md](panel.md) | thin DT-driven LVDS panel glue (timings + GPIO power) | trivial (DT data) |
| `vs-display` | [vs-display.md](vs-display.md) | orchestrator: maps five register windows (soc/afbd/cap/svp/panel), 9 IRQs, misc dev + DMA-BUF pool. Pixel processing is in the userspace blob | kernel conduit low; userspace pixel path reuse-or-RE |
| `vs-osd` | [vs-osd.md](vs-osd.md) | fbdev + CRTC / UI plane ("wce"), register programming in-kernel, routed through keystone | medium (maps to a DRM CRTC + primary plane) |
| `sunxi_ksc` | [ksc.md](ksc.md) | hardware keystone: LUT-based geometric warp + antialias/sharpen | medium (register file ~0x100 mapped) |
| `sunxi-ve` | pending | standalone user-mode CEDAR interface; reuse the CedarX blobs | low (reuse blob) |

## The shape of the pipeline

`tvtop` owns the shared clocks and resets. `vs-display` maps and sequences the capture + scaler/deinterlace/NR (`svp`) + AFBD windows and drives the panel through `tvpanel` -> `tcon`/`lvds0` to the `z20hd720m` imager. `vs-osd` composites the UI framebuffer as a CRTC/plane. Both the video path (`vs-display`) and the UI plane (`vs-osd`) are routed through the `ksc` keystone warp before the panel, so the whole projected image is geometry-corrected. The genuinely proprietary, not-in-kernel part is the `svp`/`cap` pixel-processing programming inside `vs-display`, which runs from the userspace display blob.

See [../STATUS.md](../STATUS.md) for the project state and [../PLAN.md](../PLAN.md) for the roadmap.
