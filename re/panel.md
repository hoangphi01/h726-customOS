# RE: the LVDS panel (`z20hd720m` / `panel_lvds_gen`) and where TCON/LVDS live

Notes on the panel layer of the display stack, from `z20hd720m.ko` and `panel_lvds_gen.ko` (not redistributed; see [RULE.md](../RULE.md)).

## The panel modules are thin, generic, DT-driven

`z20hd720m.ko` and `panel_lvds_gen.ko` share the exact same code (identical `panel_lvds_*` function set); `z20hd720m` is just `panel_lvds_gen` registered for the specific panel compatible. Both `depends=tvpanel`.

Functions: `panel_lvds_parse_dt` (1424 bytes, the bulk), `panel_lvds_probe`, `panel_lvds_prepare` / `panel_lvds_enable` / `panel_lvds_disable` / `panel_lvds_unprepare`, `panel_lvds_get_native_timing`, `panel_lvds_get_extra_para`.

- `panel_lvds_parse_dt` reads the standard `display-timings` (`of_get_display_timings`) plus the `panel_*` extra properties the DTS lists (`panel_lane_num`, `panel_bitwidth`, `panel_lvds0_pol`, the `enable0..4-gpios`, the `*_delay` values).
- It understands the interface enums `LCD_IF_LVDS` (6 / 8 / 10 bit) and the DSI modes (`LCD_DSI_{1..4}LANE`, burst / command / video), confirming the same driver can drive either interface; our unit uses LVDS 8-bit, 4-lane.
- `prepare` / `enable` sequence the panel power GPIOs (the `enable0..4-gpios`) with the parsed delays and hand the timing to the LVDS/TCON layer.

So the panel is trivial to port: it is DT data plus GPIO power sequencing. For the clean build the `z20hd720m` timing (1280x720, pixel clock ~61.8 MHz, the porches from the DTS) goes straight into a DT panel node; no proprietary register programming lives here.

## Correction: TCON/LVDS/DSI are in `tvpanel.ko`, not built into the kernel

An earlier version of this note assumed the TCON/LVDS PHY programming was compiled into `vmlinux`. That was wrong. `CONFIG_AW_TVPANEL=m` with `CONFIG_AW_TVPANEL_{TOP,TCON,LVDS,DSI}=y` compiles the display-engine top, TCON, LVDS and DSI all into **`tvpanel.ko`** (which we already have), and `/proc/kallsyms` tags `tcon_show_info`, `lvds_show_info`, `dsi_irq_handler`, `tvdisp_top_show_info` with `[tvpanel]`. So no `vmlinux` extraction is needed: the full register map of TCON (`tcon0@5501000`), LVDS (`lvds0@5800000`), DSI (`dsi0@5506000`) and the DE top (`tvdisp@5500000`) is reversed from `tvpanel.ko` in **[tvpanel.md](tvpanel.md)**. The only genuinely built-in display-related symbols are CCU clocks, a B2 item (see [kallsyms-map.md](kallsyms-map.md)).

The panel-glue parameters above (LVDS 8-bit, 4-lane, the `z20hd720m` timing) are what that TCON/LVDS driver is programmed with.
