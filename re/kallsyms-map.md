# kallsyms: the module / built-in boundary

Using the running kernel's symbol table to settle what is a loadable module versus compiled into `vmlinux`, so the RE work targets the right files. This is our analysis; the raw `/proc/kallsyms` dump (vendor-specific) is not committed.

## The kernel is not stripped

`CONFIG_KALLSYMS=y`, `CONFIG_KALLSYMS_ALL=y`, `CONFIG_KALLSYMS_BASE_RELATIVE=y`; `/proc/kallsyms` lists 182,889 symbols, each tagged with its owning module in `[brackets]` when it belongs to one. So **no signature / pattern matching is needed to isolate a built-in driver's functions**: the symbol table names them directly. (Addresses read as `00000000` because `kptr_restrict=2`; the names and module tags are what matter. Real addresses, if ever needed to disassemble a specific built-in function, require `kptr_restrict=0`, a deliberate and reversible root action, not done here because B1 does not need it.)

## Every vendor display driver is a loadable module

The symbol tags confirm the whole pipeline is modular, so B1 reverses `.ko` files we already have, not `vmlinux`:

| Driver | Module tag | Note |
|---|---|---|
| DE top + TCON + LVDS + DSI | `[tvpanel]` | one module, [tvpanel.md](tvpanel.md) |
| display orchestrator | `[vs_display]` | [vs-display.md](vs-display.md) |
| OSD / UI plane | `[vs_osd]` | [vs-osd.md](vs-osd.md) |
| keystone | `[sunxi_ksc]` | [ksc.md](ksc.md) |
| TV-subsystem top | `[sunxi_tvtop]` | [tvtop.md](tvtop.md) |
| video engine | `[sunxi_ve]` | [ve.md](ve.md) |
| panel glue | `[z20hd720m]` / `[panel_lvds_gen]` | [panel.md](panel.md) |

## What is genuinely built into `vmlinux`

Searching for display-related symbols with **no** module tag turns up only two kinds, neither of which is a vendor driver:

1. **Standard mainline DRM / MIPI-DSI helpers** (text): `drm_of_lvds_get_dual_link_pixel_order`, `mipi_dsi_host_register`, `mipi_dsi_attach`, and the rest of the generic DRM panel/DSI core. These come from upstream Linux, not Allwinner, so there is nothing to reverse.
2. **The CCU clock tree** (data symbols): the sun50iw15 clock-controller definitions. These are the one built-in piece the display drivers depend on, and they are a **B2** item (port/extend the sun50iw15 CCU from the sun50iw12 bindings), not a B1 RE target.

### CCU display / VE / keystone clock inventory (for B2)

Clock and parent-list symbols the display, VE and keystone blocks consume, to be provided by the ported sun50iw15 CCU:

- Display: `disp_bus_clk`, `lvds_clk`, `dsi_clk` / `dsi_parents`, `afbd_clk` / `afbd_parents`, `tvdisp_ahb_sw_cfg_bus_clk`, `tvdisp_mbus_sw_cfg_bus_clk`.
- Keystone: `ksc_clk` / `ksc_parents`.
- Video engine: `ve_clk`, `ve_bus_clk`, `ve_ahb_sw_cfg_bus_clk`, `ve_mbus_sw_cfg_bus_clk`, `ve_parents`.
- Plus the two display PLLs the DE selects between (`pll0` / `pll1`, each `clk_hs` / `clk_ls`; see `tvdisp_top_pll_config` in [tvpanel.md](tvpanel.md)).

## Consequence

B1 needs no `vmlinux` extraction: the entire display/keystone/VE driver surface is in modules, already staged and reversed. The kernel image is only interesting for the standard built-in blocks (CCU, pinctrl, DRAM), which B2 ports from mainline anyway.
