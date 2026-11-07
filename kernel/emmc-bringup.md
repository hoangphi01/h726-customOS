# eMMC bring-up (sunxi-mmc) - coded, deploy pending

The next driver after the shell: the **eMMC** (`mmc2` at `0x4022000`), so there is a persistent rootfs
instead of the RAM initramfs. Code is in the tree; the build + flash + on-hardware test is the next step
(the register fields are an A523 hypothesis and are confirmed over the UART). Status: **staged, not yet
verified on hardware.**

## Why it is more than a DTS node

The sunxi-mmc driver `clk_set_rate()`s the "mmc" clock to make the card clock (400 kHz at init, up to the
HS rate). Our old `smhc2` was a plain **gate on osc24M** - not rate-settable - so the driver could not set
the card clock. It had to become a real mux+divider clock.

## What changed

1. **`kernel/ccu/ccu-sun50iw15.c`** - the `smhc2_clk` gate is replaced by a rate-settable MP clock
   (same struct name, so the clock arrays are unchanged):
   ```
   static const struct clk_parent_data mmc2_parents[] = {
       { .fw_name = "hosc" },                 /* 24 MHz, for the slow init */
       { .hw = &pll_peri0_clk.common.hw },    /* high speed */
   };
   static SUNXI_CCU_MP_MUX_GATE_POSTDIV_DUALDIV(smhc2_clk, "mmc2", mmc2_parents,
       0x838, 0,5 (M), 8,5 (P), 24,3 (mux), BIT(31) (gate), 2 (post-div), 0);
   ```
   Modelled on the A523 `mmc2` clock (`drivers/clk/sunxi-ng/ccu-sun55i-a523.c`, reg `0x838`). The A523 uses
   `pll-periph0-800M/600M` parents for HS200/HS400; ours is trimmed to `hosc` + `pll-peri0` for first
   enumeration (HS/DDR52). The bus gate (`bus_smhc2_clk`, CLK_BUS_SMHC2) and `RST_BUS_SMHC2` are unchanged.

2. **`kernel/sun50i-h726-hy300ultra-bringup.dts`** - new `mmc@4022000` node:
   `allwinner,sun50i-h6-emmc`, `clocks = <&ccu 53>, <&ccu 50>` (CLK_BUS_SMHC2 "ahb", CLK_SMHC2 "mmc"),
   `resets = <&ccu 10>` (RST_BUS_SMHC2), `bus-width 8`, `non-removable`, `cap-mmc-highspeed`,
   `max-frequency = <52000000>` (lowered for bring-up). **No pinctrl handle** - U-Boot already muxes the
   PC-bank eMMC pins (it booted from the card), same approach as the uart0 node. Mirrored into the full DTS.

3. **`kernel/build.sh`** - enable `CONFIG_MMC`, `CONFIG_MMC_BLOCK`, `CONFIG_MMC_SUNXI`.

## Deploy + test (next session)

```
HEARTBEAT=1 bash kernel/build.sh          # build the kernel with the mmc2 clock + node
# repack boot_hb.img, dd to boot_b (slot-clone loop), reboot, read UART
```

Look for, over the console:
- `sunxi-mmc 4022000.mmc: ... ` probe without error, then `mmc2: new ... card` and `mmcblk2 ...`.
- In the shell: `cat /proc/partitions` lists `mmcblk2` + its partitions; try
  `mount -t ext4 -o ro /dev/mmcblk2pNN /mnt` (the vendor partitions are GPT; `super`/`userdata` are the big
  ones - mount read-only first).

If the card clock is wrong (CRC/timeout errors, or no enumeration), the `0x838` M/P/mux fields or the
parent rate are off - correct them against the A523 driver / the live `clk_summary` and rebuild. This is
the expected hardware-iteration loop.

## Known follow-ups
- Raise `max-frequency` + add `pll-periph0-800M/600M` parents for HS200/HS400 once basic enumeration works.
- The full `sun50i-h726-hy300ultra.dts` still uses vendor clock numbering elsewhere; it needs a reconcile
  pass to our CCU indices before it can replace the bring-up DTS.
