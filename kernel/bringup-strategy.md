# Bring-up strategy (and the gates that come before the CCU)

How we will debug a from-source kernel on the H726 with no serial console, and the two hard gates that sit **before** the CCU register offsets matter. Written so the development pace is set by reality, not optimism.

**The reframed goal: do not tune CCU offsets until a GPIO heartbeat is seen from the kernel.** Getting a kernel to execute even one instruction through the boot chain is the first problem; the clock tree is the second.

## The loader reality on 0x1911: PhoenixSuit, not `sunxi-fel`

FEL is the right safety net: the eMMC stays intact, and a bad kernel is recovered by re-entering FEL and reflashing. But the usual fast loop (`sunxi-fel uboot ...` to load code straight into DRAM) is **not available on this SoC**:

- `sunxi-fel version` talks to the BootROM but reports `soc=00001911(unknown)`; sunxi-tools has no DRAM controller or SoC thunk for sun50iw15.
- `sunxi-fel uboot` fails with `Unexpected SCTLR`, and the generational `sunxi-fel spl` path has no 0x1911 support.
- `/dev/mem` is absent on the stock kernel, so a live `devmem` register dump is not available either (we read the clock tree from `clk` debugfs instead, see [../re/ccu-sun50iw15.md](../re/ccu-sun50iw15.md)).

The only **proven** way to get code running on this device is **PhoenixSuit over FEL with the stock `fes1.fex`** (the vendor FES initialises DRAM its own way). So the real development loop is:

```
edit -> make Image -> repack into the boot partition -> PhoenixSuit flash over FEL -> power on -> observe
```

Slower than a `sunxi-fel` RAM load, but 100 % safe: the stock IMAGEWTY reflashes over FEL and the reserve/calibration partition is skipped.

## The blind-debug toolbox, assessed for this SoC

| Technique | Verdict here |
|---|---|
| **pstore / ramoops read back** | Weak for our failure mode. An early CCU crash happens before pstore registers (pstore comes up after the memory subsystem), so there is no log yet; and reading DRAM back needs DRAM to survive, which a power-cycle loses. Useful only for later crashes. |
| **`sunxi-fel` SPL / DRAM / hex poke** | Blocked. sunxi-fel does not support 0x1911 (above). |
| **GPIO heartbeat (fan / LED)** | **The keeper.** No FEL read, no pstore, no DRAM, no USB: just `writel` to the PIO data register (`0x2000000`, always-on MMIO, needs no clock) at milestones in early code, and watch the fan or an LED. Best fit for a blind early-boot bug. Caveat: it only reports once the kernel is *executing*, which presupposes the gate below is solved. |
| **USB-gadget dmesg (ACM over USB)** | A later-stage tool. It needs the kernel to reach initramfs with USB UDC/PHY/clocks up, i.e. most of bring-up already working. Not for the early CCU problem. |

## Gate #0: the AArch32 / AArch64 boot handoff (the real time bomb)

This sits **before** `ccu_probe`, before the clock tree, before anything our drivers do.

**Evidence (measured on this device):**

- The stock kernel is **AArch32 (32-bit ARM)**. Inspected directly: `boot_a` is an Android boot image v4; at the kernel offset (4096) the first bytes are 32-bit ARM instructions (`msr cpsr`, `mrc p15`, ...), and there is **no** arm64 `Image` magic (`0x644d5241` at kernel+0x38). The running kernel is `armv7l`, userspace is `armeabi-v7a`. The whole stock stack runs the Cortex-A53 in 32-bit mode.
- Entering **AArch64 on the H726 is unsolved**: `sunxi-fel` reports the SoC unknown and `sunxi-fel uboot` returns `Unexpected SCTLR`; the standard RVBAR warm-reset-to-AArch64 trick (the H713 RVBAR addresses) **hangs** on this SoC.
- (Clarification: the main kernel is in `boot_a`, inspected above. `EXEC_KERNEL_IMAGE.bin` in `/vendor/etc/firmware` is co-processor firmware, not the main kernel.)

**Consequence:** the **arm64** `Image` we build in Phase B2 cannot be entered by the vendor boot chain. A 32-bit U-Boot hands off in AArch32; our AArch64 kernel would take an exception on its very first instruction, and any GPIO heartbeat in `head.S` would never be reached. So the GPIO-heartbeat plan, and the whole CCU effort, are downstream of solving this.

### The fork (both heavy)

1. **Build the kernel 32-bit (`ARCH=arm`)** to match the vendor boot chain (an AArch32 -> AArch32 handoff, which already works on stock) and the 32-bit userspace. Cost: mainline defines the sun50i SoCs only under `arch/arm64`; a 32-bit sunxi base for this SoC (32-bit device tree + platform glue) would be new, pioneering work. The CCU and pinctrl drivers (sunxi-ng) build for both arches, so those port.
2. **Keep arm64 and build an AArch64 boot chain** (boot0/SPL + TF-A + U-Boot that enter AArch64) to `booti` the arm64 kernel. Cost: the H726 BootROM-to-AArch64 entry must be solved first (the RVBAR hang found above), plus a sun50iw15 DRAM init (none in mainline) and a TF-A port. Cleaner long term, but the mode switch is an open blocker.

Neither is clearly easier; each has one hard unknown (pioneer 32-bit sunxi for a new SoC, versus crack the AArch64 entry).

### Two cheap probes to settle the fork (next, not this pass)

- Try an `ARCH=arm` build of the sun50iw15 base and measure how much 32-bit platform/DT scaffolding is really needed.
- Reverse-engineer boot0 / the BootROM to find the correct H726 RVBAR address and the AArch64 entry sequence.

## What unlocks everything: a UART console

A 3.3V USB-UART adapter (about 2-5 USD) on the board's debug pads gives log output from the kernel's assembly head (`earlycon` / `DEBUG_LL`). That shows gate #0 directly, which neither FEL RAM-read nor USB-gadget can. It is worth more than the entire blind-debug toolbox combined, and it turns the 32-vs-64 question from guesswork into a one-boot answer. The H713 pad reference is [YuujiLab's UART_FEL notes](https://github.com/YuujiLab/HY300-H713-Research/blob/main/Hardware/UART_FEL.md).

## Order of attack

1. Decide the fork (32-bit kernel, or crack the AArch64 entry) using the two cheap probes above.
2. Get a near-empty kernel to run one GPIO heartbeat through the (matched) boot chain.
3. Only then tune the CCU/pinctrl offsets (hardware-confirm the A523-hypothesis values in [../re/ccu-sun50iw15.md](../re/ccu-sun50iw15.md)).
4. Bring up drivers in dependency order (see [../re/README.md](../re/README.md)).

Everything in Phase B2 so far (device tree, defconfig, CCU + pinctrl drivers) compiles and is correct as a hardware description; it just cannot boot until gate #0 is cleared.
