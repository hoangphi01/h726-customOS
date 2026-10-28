# RE: `tvpanel` (the output stack: display-engine top + TCON + LVDS + DSI)

Map of the panel-output driver, from `tvpanel.ko` (not redistributed; see [RULE.md](../RULE.md)). This single 167 KB module (162 functions) contains the whole output side of the pipeline, which earlier notes had assumed was built into `vmlinux`.

## Correction: this is one module, not built-in

`CONFIG_AW_TVPANEL=m` with `CONFIG_AW_TVPANEL_{TOP,TCON,LVDS,DSI}=y` compiles the display-engine top, TCON, LVDS and DSI drivers all into `tvpanel.ko`. `/proc/kallsyms` confirms it: `tcon_show_info`, `lvds_show_info`, `dsi_irq_handler`, `tvdisp_top_show_info` all carry the `[tvpanel]` tag. So no `vmlinux` extraction is needed for the output path (see [kallsyms-map.md](kallsyms-map.md)); it is all in this module, RE'd with objdump like the others. This supersedes the earlier "TCON/LVDS is built-in" note in [panel.md](panel.md).

The vendor source tree (leaked in `vs-osd` strings) is `/mnt/hotack/Allwinner/H726-A14/vendor/...`; the register struct typedefs below (`__tv_disp_top_reg_t`, `__tv_tcon_reg_t`, `__tv_lvds_*_reg_t`, `dphy_*_reg_t`) are the vendor's own names, recovered from the `*_show_info` debug dumps.

## Display-engine top (`tvdisp_top`, base `tvdisp@5500000`)

The DE top is the routing/mux block that gates each sub-block onto the panel. It is what `vs-display` and `vs-osd` drive through the exported `sunxi_tvdisp_top_*` symbols. A global `tvdisp_top` holds the ioremapped base (set by `tvdisp_top_set_regbase`); the enable functions read-modify-write these registers (`__tv_disp_top_reg_t`):

| Reg | Field | Set by |
|---|---|---|
| `0x04` | AFBD enable | `tvdisp_top_afbd_enable` (exported `sunxi_tvdisp_top_afbd_enable`) |
| `0x40` | SVP enable (bits `0x10`, `0x100`, `0x1000` = the scaler/deint/NR sub-blocks) | `tvdisp_top_svp_enable` (exported `sunxi_tvdisp_top_svp_enable`) |
| `0x8C` | TCON / DSI path enable (TCON bit1 + bit5; DSI shares it) | `tvdisp_top_tcon_enable`, `tvdisp_top_dsi_enable` |
| `0xA0` | KSC (keystone) routing, bits 0..3 | `tvdisp_top_ksc_enable` -> the `ksc_online_enable` path (see [ksc.md](ksc.md)) |
| `0x138` | TCON / DSI mux (bit17) + enable (bit1) | `tvdisp_top_tcon_enable`, `tvdisp_top_dsi_enable` |

So the enable chain the display drivers walk is: AFBD (`0x04`) and SVP (`0x40`) on, then route TCON or DSI (`0x8C` + `0x138`), enable keystone (`0xA0`), out to the panel. This is the kernel-side half of what `vs-display`/`vs-osd` orchestrate.

**Display PLL:** `tvdisp_top_pll_config` (2968 bytes) + `tvdisp_top_get_pll_info` manage two PLLs (`pll0`, `pll1`), each with a high-speed and low-speed clock (`clk_hs` / `clk_ls`), and a "working pll index" selects which drives the panel. This is the pixel-clock source for the 1280x720 LVDS timing.

## TCON (`tcon0@5501000`, `__tv_tcon_reg_t`)

`tcon_set_regbase` installs the base; `tcon_config` programs the timing, `tcon_enable` turns it on, `tcon_irq_query` services the TCON interrupt, `tcon_set_colorbar` / `tcon_colorbar_style` drive the built-in test pattern. `tcon_show_info` is the register-dump path that enumerates the TCON register file (anchor for the full bitfields when needed).

## LVDS (`lvds0@5800000`, the active output)

The LVDS window (1 MB) holds several register banks, named in the debug dumps:
- `__tv_lvds_buf_reg_t` at **`0x80000`** (the pixel buffer / formatter).
- `__tv_lvds_wm_top_reg_t` at **`0xC0000`** (the LVDS "wm" top control).
- `__tv_lvds_tx20_core_reg_t` (the LVDS TX serializer PHY, "tx20").

`lvds_cfg` (1480 bytes) programs the PHY and formatting (8-bit, 4-lane, polarities from the DT), `lvds_combphy_open` / `lvds_combphy_close` bring the combo-PHY up/down, `sunxi_lvds_connector_{prepare,enable,disable,vrr,sw_enable}` are the connector lifecycle, `sunxi_lvds_get_timing` pulls the mode, and `lvds_test_pattern` / `lvds_show_info` are test and register-dump helpers.

## DSI (`dsi0@5506000`, present but not the active path)

A full MIPI-DSI block with a D-PHY: struct typedefs `dphy_ctl_reg_t`, `dphy_ana0..4_reg_t`, `dphy_int_en0..2_reg_t`, `dphy_dbg0..5_reg_t`, `combophy_status_reg_t`. Functions: `sunxi_dsi_connector_{prepare,enable,disable,vrr,sw_enable}`, `dsi_start`, `dsi_irq_{enable,disable,query}`, `dsi_set_reg_base` / `dsi_get_reg_base`, `sunxi_dsi_show_info`. Our unit outputs over LVDS, so DSI is carried but inactive; keep the driver for completeness.

## For the clean build

The output stack maps onto DRM as: the DE top (`tvdisp_top`) is the routing/mux + PLL that a CRTC programs; TCON is the timing generator; LVDS (with its `buf` / `wm_top` / `tx20` PHY banks) is the encoder/bridge to the panel; DSI is an alternate encoder. Mainline linux-sunxi has sun8i/sun50i `tcon`, `lvds` and `dsi`/`dphy` drivers, so this is a port-and-diff against those using the register banks and the enable chain above, not a from-scratch RE. The `sunxi_tvdisp_top_*` enable fan-out (0x04/0x40/0x8C/0xA0/0x138) is the H726-specific glue to reimplement so `vs-display`/`vs-osd` (or a DRM equivalent) can route the pipeline onto the panel.

## Next

- `sunxi-ve` (reuse the CedarX blobs; see [ve.md](ve.md)) closes the B1 pass.
