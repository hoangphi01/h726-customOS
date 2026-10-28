# RE: the sun50iw15 (H726) clock tree

The clock tree the CCU driver must provide, read live from the running vendor kernel's `clk` debugfs (`/sys/kernel/debug/clk/clk_summary`, 201 clocks) on the rooted device. This is authoritative for the tree shape, names, parents and rates; it does not give register offsets (those follow the generational pattern below, to be confirmed against hardware).

## Architecture: this is the sun55i (A523) generation, not H6

The clock names and structure match the newest mainline sunxi family, `sun55i-a523` (`sun55iw3`), not H6/H616: multi-output peripheral PLLs (`pll-peri0`/`pll-peri1` each fanning out to 480M/800M/400M/300M/200M/160M/150M and a `2x`), `smhc0/1/2` MMC naming, per-peripheral `*-ahb-sw-cfg-bus` config-bus gates, and `twi`/`spi` naming. So the mainline **`ccu-sun55i-a523`** driver is the structural template for `ccu-sun50iw15`, not the H6 one. (The earlier DTS used H6 bindings as a placeholder; this supersedes that.)

## PLLs (rates verified from hardware)

| PLL | Rate | Notes |
|---|---|---|
| `pll-cpu` | 1608 MHz | feeds `cpu` mux -> `cpu-axi-div`, `cpu-apb-div` |
| `pll-ddr0` | 1584 MHz | `dram-mux` -> `hdr-clk` (396M) -> `mbus` (396M) |
| `pll-ddr1` | 2016 MHz | |
| `pll-ddr2` | 1800 MHz | |
| `pll-peri0` | 2400 MHz | -> 480m, 800m -> 400m -> 200m, 160m |
| `pll-peri1` | 2400 MHz | -> 480m/160m, 800m -> 400m/200m, 2x (1200m) -> 600m -> 300m -> 150m |
| `pll-gpu-vco` | 1800 MHz | -> `pll-gpu` 600M -> `gpu-core` 600M |
| `pll-video0` | 1296 MHz | -> 1x (324M), 4x (1296M) |
| `pll-video1` | 2376 MHz | -> 3x (792M), 4x (1188M) |
| `pll-video2` | 2088 MHz | -> 3x (696M), 4x (1044M) -> `ve` 576M |
| `pll-audio-4x` | 614.4 MHz | |

## Oscillators

- `dcxo24M` (24 MHz, the HOSC) - the root of most gates.
- `rc-16m` -> `iosc` (16 MHz) -> `iosc-div32k` -> `osc32k` (internal 32k).
- `ext-32k` (external 32768 Hz LOSC).

## Bus / core clocks

- `cpu` (mux off pll-cpu) with `cpu-axi-div`, `cpu-apb-div`.
- `ahb0` 200 MHz, `apb1` (24M domain), `apb-uart`.
- `mbus` 396 MHz (off pll-ddr0 via dram-mux/hdr).
- `dma-bus` / `dma-mbus`, `pwm-bus`, `spinlock-bus`.

## Peripheral gates (names for the gate table)

- UART: `apb-uart` -> `uart0-bus`, `uart1-bus`, `uart2-bus`, `uart3-bus`. (console is uart0.)
- MMC: `smhc0`/`smhc1`/`smhc2` (modules; smhc2=200M eMMC, smhc1=300M), each with `-bus` and `-ahb-sw-cfg-bus`.
- I2C: `twi0-bus`..`twi4-bus`.
- SPI: `spi0`/`1`/`2` + `-bus`.
- Net: `gmac0-bus`, `gmac-ahb-sw-cfg-bus`, `gmac0-phy` (25M).
- USB: `usb2-ahb-sw-cfg-bus`.
- GPU: `gpu-core` 600M, `gpu-bus`, `gpu-ahb/mbus-sw-cfg-bus`.
- VE: `ve` 576M, `ve-ahb-sw-cfg-bus`.
- Display: `tvfe-axi`/`-axi-bus`, `tvfe/tvcap/tvdisp-ahb-sw-cfg-bus`, `vincap-dma` 400M, `disp-bus`.
- R-block (r_ccu @0x7010000): `r-rtc`, `r-ths`, `r-gpadc`, `r-irrx`, `r-twi0`/`r-twi1`, `r-uart0`, `r-pwm`, `r-wdt1`, `r-timer0..2`, `r-apbs0` (200M), `r-apbs1`, `r-ahb` (200M), `r-ppu`, `r-cpucfg`.

## Register offsets

`clk_summary` does not expose register offsets. The sun55i-a523 CCU is the same generation, so its register map is the working hypothesis for `ccu-sun50iw15`: the driver starts from the A523 offsets and each is confirmed (or corrected) against the hardware when a UART console allows boot testing. The DT register bases are known from the extracted device tree: main CCU `0x2002000`, CPU PLL `0x2001000`, DRAM PLL `0x4830000`, R_CCU `0x7010000`, RTC CCU `0x7090000`.

## For the driver

`kernel/ccu/ccu-sun50iw15.c` provides this tree to the kernel (sunxi-ng, A523-derived). The first pass wires the oscillators, the PLLs above, and the core bootable gates (cpu/ahb/apb, uart0, smhc, twi, pwm, pio via apb); the full fan-out grows from there. Rates and parents here are verified; offsets are the A523 hypothesis until hardware-confirmed.
