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

### Decision: Branch 1 (32-bit `ARCH=arm`), validated at the build level

Branch 1 is chosen. It matches the whole device (the vendor U-Boot, kernel and userspace all live in AArch32), bypasses gate #0 entirely (an AArch32 -> AArch32 handoff, which already works on stock), and avoids cracking the unsolved AArch64 entry with no UART and no BootROM docs.

The cheap probe confirmed the friction is near zero: on `ARCH=arm` (with `arm-linux-gnueabihf-`), the CCU driver, the pinctrl driver and the device tree compile **unchanged** (the sunxi-ng clock and pinctrl frameworks build natively on 32-bit ARM), and a full `multi_v7_defconfig` kernel with our drivers built in produces a **`zImage` (~11 MB)**. So all of Phase B2 ports straight over; only the cross-compiler and the build target change. `kernel/build.sh` now defaults to the 32-bit target (`TARGET=arm`, `zImage`); the 64-bit path stays as `TARGET=arm64`.

What is validated is that the kernel *builds* in the right format. Whether the vendor U-Boot actually enters it, and how far it runs, is the next real test, via the GPIO heartbeat below.

## What unlocks everything: a UART console

A 3.3V USB-UART adapter (about 2-5 USD) on the board's debug pads gives log output from the kernel's assembly head (`earlycon` / `DEBUG_LL`). That shows gate #0 directly, which neither FEL RAM-read nor USB-gadget can. It is worth more than the entire blind-debug toolbox combined, and it turns the 32-vs-64 question from guesswork into a one-boot answer. The H713 pad reference is [YuujiLab's UART_FEL notes](https://github.com/YuujiLab/HY300-H713-Research/blob/main/Hardware/UART_FEL.md).

## Order of attack

1. ~~Decide the fork~~ Done: Branch 1 (32-bit `ARCH=arm`); the `zImage` builds with our drivers in.
2. ~~Confirm gate #0~~ **Done (2026-10-28): gate #0 is PASSED.** A GPIO heartbeat in the decompressor, flashed to `boot_a` by `dd` on the rooted device, ran: the vendor U-Boot entered our 32-bit kernel and the CPU executed it. Branch 1 is proven on hardware.
3. **Now (B3b): walk the kernel to init with checkpoint beacons.** Instead of jumping straight
   to CCU offsets, we bisect *where* the kernel stops using a ladder of GPIO beacons (below) and
   an authoritative appended device tree. Build A is built and compiles clean; it is the next flash.
4. Then confirm/fix the CCU/pinctrl offsets (A523-hypothesis, [../re/ccu-sun50iw15.md](../re/ccu-sun50iw15.md))
   wherever a beacon shows the kernel stalling in clock-dependent code.
5. Bring up drivers in dependency order (see [../re/README.md](../re/README.md)).

## The checkpoint-beacon method (B3b, no UART)

With no serial console, the only output is a GPIO: a direct `writel` to the PIO at `0x02000000`
ticks the focus motor (PH13). Code at successive boot stages ticks a distinct number of times; the
highest count seen is the furthest the boot reached, and a steady repeating tick means the kernel is
fully alive. See [heartbeat.md](heartbeat.md) for the artifacts.

| Ticks | Stage | Proves |
|---|---|---|
| 1 | decompressor entry | gate #0 (already passed) |
| 2 | kernel proper, after `__create_page_tables` (MMU off) | handed into `vmlinux` |
| 3 | `early_initcall` | reached the init thread (setup_arch + mm + sched + timers) |
| steady ~1 Hz | `late_initcall` kthread | kernel fully alive |

Two facts (verified against mainline v6.12/6.16 source) make this robust:

- **The appended DTB wins.** `CONFIG_ARM_APPENDED_DTB` + `cat zImage our.dtb` makes the decompressor
  use our device tree and ignore whatever the vendor U-Boot passes in `r2`, so we control the DT. The
  cmdline is forced (`CMDLINE_FORCE`, `nr_cpus=1`) because there is no console to read anyway, and
  `nr_cpus=1` skips SMP secondary bring-up over PSCI/SMC. (`CONFIG_ARM_ATAG_DTB_COMPAT` is on in case
  U-Boot passes ATAGs rather than a DTB.)
- **A wrong CCU offset does not infinite-hang clk init.** The sunxi-ng PLL lock wait is bounded (70 ms
  timeout + `WARN`, not a spin). So a true freeze at clk init is a *side effect* of a bad register
  write, not a lock spin; we still drop `CLK_IS_CRITICAL` on the bring-up CCU so the framework does not
  touch hardware at a guessed offset during probe, but the beacon runs regardless of CCU outcome (it
  pokes the GPIO directly).

Because the beacon pokes the GPIO directly and does not depend on any driver probing, the only things
that *must* be correct for Build A to reach the steady heartbeat are the CPU/MMU path, the DT memory
node and the arch timer. The CCU matters only when real peripherals come up.

### Safety lesson from the gate-#0 run (important)

The heartbeat that confirmed gate #0 drove the cooling **fan** off (the fan cut out, which is how we knew our code ran). But the vendor U-Boot's splash was still being projected, so the **light engine was on with no cooling**, and after about 15 s the imager panel overheated (a dark bloom spread from the centre of the projected image; power was cut immediately and it is expected to recover once cool). **Rules for any on-device bring-up from now on:**

- **Never drive the fan**, and never leave the device hung with the light engine on and no cooling. Use the **focus motor** (no thermal role) or, better, a UART console (read-only) as the signal. The kernel proper is safe here on its own: the bring-up device tree keeps `pwm-fan` disabled, so our kernel never touches cooling and the fan stays in the state U-Boot left it (running) for the whole run, even if the kernel hangs. The only way to cut the fan is the beacon asm writing PL7 directly, which is exactly what the beacons now refuse to do.
- Keep hang time short: recover promptly (FEL + reflash the rooted image) rather than letting a hung kernel sit.
- The committed heartbeat ([heartbeat/heartbeat-h726.S](heartbeat/heartbeat-h726.S)) is retargeted to the motor and documented accordingly.

Everything in Phase B2 so far (device tree, defconfig, CCU + pinctrl drivers) compiles and is correct as a hardware description; it just cannot boot until gate #0 is cleared.
