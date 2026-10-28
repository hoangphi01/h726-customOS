# RE: `sunxi_tvtop` (TV subsystem top: clocks, resets, ADC)

Map of the shared TV-subsystem "top" block, reverse-engineered from `sunxi_tvtop.ko` (not redistributed; see [RULE.md](../RULE.md)). This is the base that the capture, front-end and display blocks register into, so it comes early in the bring-up order.

## Source and method

- Module: `sunxi_tvtop.ko`, `Allwinner`, `description=Sunxi tv subsystem top module driver`, `license=GPL v2`, no `depends`. `compatible = allwinner,sunxi-tvtop` (also `-pm`).
- DT anchor: `tvtop@6000000`, `reg = <0x6000000 0x100  0x6e00000 0x100>` (two windows), interrupt `0x52`, ten resets (`reset_bus_demod`, `..._tvfe_axi`, `..._mcsi`, `..._tvfe_ahb`, `..._tvfe_mbu`, `..._cgu_fa`, `..._audio_ahb/axi/core`, `..._tvcap`) and fifteen clocks (`clk_bus_tvcap`, `adc`, `vincap-dma`, `tvfe1296m`, `mcsi`, ...). So tvtop owns the whole TV-subsystem clock/reset tree.
- Method: `arm-linux-gnueabihf-objdump -d -r`, read the exported and enable functions. The driver keeps a global `tvtopdev`; the device struct holds the register base pointer and the clock/reset handles.

## Exported API (what the rest of the pipeline hooks into)

`EXPORT_SYMBOL`: `sunxi_tvtop_client_register`, `sunxi_tvtop_capclk_sel`, `sunxi_tvtop_clk_get`, `sunxi_tvtop_clk_put`, `sunxi_tvtop_set_adc_ctrl`. `vs-display` and the capture path call these; tvtop is the arbiter of the shared clocks, resets and ADC.

## Device struct layout (offsets into the `tvtopdev` object)

Derived from the enable paths:

| Offset | Field |
|---|---|
| `+0x64` | sub-struct carrying a mutex at `+0x190` (serialises tvcap enable) |
| `+0xE8` | clock bundle passed to `sunxi_tvtop_clock_enable` |
| `+0xEC` | **pointer to the MMIO register base** (the tvtop/tvcap window, DT `reg[0]` = `0x6000000`) |
| `+0xF0` | `reset_control` for tvcap |
| `+0xF4` / `+0xF8` | the `cap` clock and a "cap clk enabled" bool |
| `+0x1B8` | "tvcap enabled" bool (guards re-entry) |
| `+0x220` | the `struct device *` (used for `_dev_err`) |
| ADC array | per-ADC sub-struct, **stride `0xD8` (216 bytes)**, each with a reg base pointer at `+0x14` |

## Register findings (tvtop window, base `0x6000000`)

The reg base pointer at `dev+0xEC` is dereferenced and written at small offsets. The ioremap span reaches at least `0x25C`, wider than the `0x100` the DT declares for `reg[0]`.

| Offset | Meaning |
|---|---|
| `0x00` | sub-block clock/reset gate. `tvcap_enable` clears it, then writes `0x111111` (nibble-per-subblock enable pattern). |
| `0x04` | second gate, written `0x1111115`. |
| `0x08` | third gate, written `0x404`; **bit 8 (`0x100`) = capture clock source select** (set/cleared by `sunxi_tvtop_capclk_sel` and `sunxi_tvtop_cap_clk_sel`). |
| `0x258` | read-modify-write, OR **bit 16 (`0x10000`)** = a capture-path enable, last step of `tvcap_enable`. |

The `0x111111` / `0x1111115` / `0x404` values are nibble-encoded enables for the demod / tvfe / mcsi / audio sub-blocks that the DT reset-names enumerate.

## ADC control (`sunxi_tvtop_set_adc_ctrl(id, offset, val)`)

Indexes the per-ADC struct by `id` (0..1, stride 216), takes the reg base at `+0x14`, and writes `val` at `base + offset`. The `offset` is validated: it must be `20` (`0x14`), or in the window `0x500..0x508` masked by `0x111`, or `4`; anything else is rejected with `-EINVAL`. So the ADC register file has control at `0x04`, `0x14`, and a `0x500..0x508` block.

## tvcap enable sequence (the bring-up recipe)

`mutex_lock(dev+0x190)` -> if not already enabled: `sunxi_tvtop_clock_enable(dev+0xE8)` -> `usleep 10-15ms` -> prepare/enable the `cap` clock (`dev+0xF4`) -> `reset_control_deassert(dev+0xF0)` -> `usleep` -> clear reg `0x00/0x04/0x08` -> `usleep` -> write the gate pattern `0x00=0x111111`, `0x04=0x1111115`, `0x08=0x404` -> OR `0x258` bit16 -> mark enabled (`dev+0x1B8=1`) -> `mutex_unlock`.

## For the clean build

tvtop is mostly standard clk/reset-framework code plus a handful of MMIO gate writes, so it ports cleanly: wire the fifteen clocks and ten resets from the DT, implement the `0x00/0x04/0x08/0x258` gate writes and the `capclk_sel` bit, and expose the same `sunxi_tvtop_*` symbols the display and capture drivers expect. No proprietary magic beyond the gate bit patterns captured above.

## Next

- `vs-display` (the orchestrator that calls `sunxi_tvtop_clk_get` and drives panel + AFBD + SVP).
