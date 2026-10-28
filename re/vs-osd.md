# RE: `vs-osd` (the OSD / UI plane and CRTC)

Structural map of the OSD driver, from `vs-osd.ko` (not redistributed; see [RULE.md](../RULE.md)). This is the `vs,osd` node, the plane that composites the UI framebuffer onto the panel.

## Source and method

- Module: `vs-osd.ko`, `name=vs_osd`, `depends=tvpanel, sunxi_ksc`. ARM objdump + strings.
- The strings leak the vendor source tree: `/mnt/hotack/Allwinner/H726-A14/vendor/vs/tv_osd_driver/{vs_osd.c, osd/drv_afbd.c, wce/crtc_read.c, wce/wce_panel.c, wce/wce_proc_r.c}`. So the vendor is **hotack** (the same name as the `hotacksu` backdoor), the tree is `H726-A14`, and the internal component is a "window compositing engine" (`wce`).

## What it is: a Linux fbdev + CRTC / plane driver

Unlike `vs-display` (a conduit that hands register windows to userspace), `vs-osd` programs its registers in-kernel:

- **fbdev**: `vs_osd_fb_init`, `fb_dealloc_cmap`, a classic framebuffer for the UI plane.
- **Window compositing engine (`wce`)**: `wce_set_path` (6024 bytes, the largest function, the composition path setup), `wce_panel_write_reg` (1512), `wce_crtc_set_timing`, `wce_panel_calc_window`, `wce_crtc_dump_reg`, `wce_proc_r_dump_reg`. This is the CRTC timing + panel-window + plane register programming, done in the kernel.
- **Pixel formats**: `drv_osd_set_pixel_format` / `drv_osd_get_pixel_format_bpp` (ARGB/RGB; "Invalid color format"), and AFBD compression on the UI plane (`drv_afbd_set_pixel_format`, `drv_afbd_set_block_size`).
- **vsync + fences**: `osd_fence_alloc`, `vsync_event_waitq`, `update_wait_queue`, guards "osd commit twice on a single vsync period" and "wait osd update finish timeout". A properly vsync-fenced display plane.
- **Memory**: ION (`__ion_alloc_memory`) and DMA-BUF (`osd_dmabuf_map`, "osd_dmabuf_cache not init yet").
- Userspace control via `fops_ioctl` (1768 bytes) + the framebuffer device.

## How it hooks the rest of the pipeline

Imports (confirming the dependency graph): `ksc_online_enable` (so the OSD plane is routed through the **keystone warp**; the UI is geometrically corrected along with video), `sunxi_tvpanel_prepare/enable/wait_init_done/get_timing`, `sunxi_tvpanel_top_clk_enable/disable`, `sunxi_tvdisp_top_afbd_enable`, `sunxi_tvdisp_top_svp_enable`. So its enable path: panel prepare + clk, afbd + svp enable, keystone enable, then composite the UI framebuffer at the panel timing.

## For the clean build

`vs-osd` maps naturally onto a DRM model as a CRTC plus the primary (UI) plane: the `wce` CRTC-timing and plane-register programming is in-kernel, so it is fully RE-able here (no userspace-hidden logic, unlike `vs-display`'s `svp`/`cap` pixel path). Keystone is wired in through `ksc_online_enable`, so an open driver must enable the KSC block (see [ksc.md](ksc.md)) when the OSD plane is active, or the UI will not be keystone-corrected. Reuse the AFBD and pixel-format helpers or reimplement from the register dumps (`wce_crtc_dump_reg` / `wce_proc_r_dump_reg` are the self-documenting register readback paths to lift the layout from).

## Next

- Extract `tcon` / `lvds0` from `vmlinux` (built-in, see [panel.md](panel.md)); `sunxi-ve` (standalone CedarX, reuse the blobs) last.
