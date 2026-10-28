# RE: the LVDS panel (`z20hd720m` / `panel_lvds_gen`) and where TCON/LVDS live

Notes on the panel layer of the display stack, from `z20hd720m.ko` and `panel_lvds_gen.ko` (not redistributed; see [RULE.md](../RULE.md)).

## The panel modules are thin, generic, DT-driven

`z20hd720m.ko` and `panel_lvds_gen.ko` share the exact same code (identical `panel_lvds_*` function set); `z20hd720m` is just `panel_lvds_gen` registered for the specific panel compatible. Both `depends=tvpanel`.

Functions: `panel_lvds_parse_dt` (1424 bytes, the bulk), `panel_lvds_probe`, `panel_lvds_prepare` / `panel_lvds_enable` / `panel_lvds_disable` / `panel_lvds_unprepare`, `panel_lvds_get_native_timing`, `panel_lvds_get_extra_para`.

- `panel_lvds_parse_dt` reads the standard `display-timings` (`of_get_display_timings`) plus the `panel_*` extra properties the DTS lists (`panel_lane_num`, `panel_bitwidth`, `panel_lvds0_pol`, the `enable0..4-gpios`, the `*_delay` values).
- It understands the interface enums `LCD_IF_LVDS` (6 / 8 / 10 bit) and the DSI modes (`LCD_DSI_{1..4}LANE`, burst / command / video), confirming the same driver can drive either interface; our unit uses LVDS 8-bit, 4-lane.
- `prepare` / `enable` sequence the panel power GPIOs (the `enable0..4-gpios`) with the parsed delays and hand the timing to the LVDS/TCON layer.

So the panel is trivial to port: it is DT data plus GPIO power sequencing. For the clean build the `z20hd720m` timing (1280x720, pixel clock ~61.8 MHz, the porches from the DTS) goes straight into a DT panel node; no proprietary register programming lives here.

## The real work is in TCON/LVDS, which is built into the kernel (not a `.ko`)

The actual LVDS/TCON PHY programming (the `allwinner,tcon` and `allwinner,lvds0` DT nodes at `0x5501000` and `0x5800000`) is **not** shipped as a loadable module: the config has `CONFIG_AW_TVPANEL_LVDS=y` (built-in), and there is no `tcon.ko` / `lvds0.ko` among the loaded modules. So to reverse the TCON/LVDS register programming we must extract it from the kernel image itself (`boot_a` -> `vmlinux`), not from a standalone `.ko`.

- Deferred to a later B1/B2 step: unpack `boot_a`, decompress the kernel, and either locate the `tcon` / `lvds` symbols (if the image is not stripped) or RE the region the DTS reg bases point at. Mainline linux-sunxi already has sun8i/sun50i `tcon` and `lvds` drivers, so this is more likely a port-and-diff than a from-scratch RE.
- The panel module above tells us the interface parameters the TCON/LVDS driver must be programmed with (8-bit, 4-lane, polarities, the timing), which is the useful half for bring-up.

## Next

- Extract `vmlinux` from `boot_a` to reach the built-in `tcon` / `lvds0` code; diff against mainline sun50i tcon/lvds.
