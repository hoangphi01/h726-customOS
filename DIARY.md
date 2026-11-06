# DIARY

A running journal of the Track B journey: building a from-source mainline Linux kernel for the
Magcubic/Gojodoq **HY300 Ultra** projector (Allwinner **H726 / sun50iw15p1**, board `HY200_QZ726_V0`).
Newest entries are added at the bottom. For the current state see [STATUS.md](STATUS.md); for durable
lessons see [LESSONS.md](LESSONS.md) and [TROUBLESHOOTING.md](TROUBLESHOOTING.md). This file is the story;
those files are the reference.

Why a from-source kernel at all: the device is a counterfeit running an untrusted vendor firmware on a
SoC (`sun50iw15`, FEL id `0x1911`) that is one generation newer than anything the linux-sunxi community
has mainlined. No public BSP, no `sunxi-fel` support, no sibling that boots. The only sibling research is
the H713 (`sun50iw12`), one SoC back. So the whole kernel had to be mined from the stock ROM and written
against mainline, treating every H713-derived register as a hypothesis until the hardware confirms it.

---

## 2026-10-27 - Root first

Before any kernel work, the device had to be rooted and reflashable. The win was understanding the
Allwinner IMAGEWTY per-partition **verify files** (`V<name>.fex`, an `add_sum` checksum): patch a payload
without recomputing its verify file and PhoenixSuit rejects the whole image. With that, a minimal-diff
rooted image flashed over FEL, and `su` came up. This is likely the first documented root of an
H726/sun50iw15 projector. It also gave the two things Track B needs: a way to read partitions, and a
safe-ish way to flash.

## 2026-10-28 - The decision, and gate #0

Decided to go past debloat to a real custom OS. Two tracks: **A** = a LineageOS GSI over Treble on the
vendor kernel (deferred fallback); **B** = a from-source mainline kernel. Chose B as the primary goal,
because only a lean from-source build is trustworthy on a counterfeit's vendor kernel, and smooth on 1 GB.

Then the first real milestone: **gate #0**. The stock kernel is AArch32 (the A53 runs in 32-bit), and
entering AArch64 on this SoC is unsolved (`sunxi-fel` reports `0x1911` unknown, the RVBAR warm-reset trick
hangs). So the kernel was built **32-bit (`ARCH=arm`)** to match the vendor boot chain. A mainline 6.16
kernel with a GPIO heartbeat in the decompressor, `dd`'d to `boot_a`, **ran on the device** - U-Boot
enters our kernel. The handoff was solved by matching the vendor's bitness instead of fighting it.

### The hardware-safety lesson that day

The first heartbeat toggled the **cooling fan** (PL7) to prove the code ran. It did prove it - but it
turned the fan *off* while the U-Boot splash was still projecting (light engine on), and in ~15 seconds
the imager panel overheated (a dark bloom spreading from the centre of the image). Power was cut; the
panel recovered. **Rule written in blood: never drive the fan in a bring-up test.** Use the focus motor
or, better, a UART. The panel recovered with no permanent damage, but it was a genuine scare.

## 2026-10-28/29 - Mining the drivers, and the watchdog wall

B1/B2: reverse-engineered the vendor display stack from the extracted `.ko` modules (tvtop, tvdisp, tcon,
lvds, ksc keystone, vs-display/vs-osd, cedar VE) and wrote the sun50iw15 CCU + pinctrl drivers from the
live clock tree, modelled on the A523 generation. Built a bring-up kernel with GPIO "checkpoint beacons"
(ticking the focus motor, never the fan) and an appended device tree.

On hardware, gate #0 kept passing, but an **external ~2 s cutoff** reset the SoC every time - even a
kernel that just spins. Diagnosed from the H713 sibling as the **watchdog** (the sun55i-A523 key-protected
layout: writes are silent no-ops without the `0x16aa` key; three watchdogs, one secure/ARISC-owned). But
blind debugging had hit its ceiling: with no console, every theory was a guess. The conclusion was stark -
**we need a UART.**

## 2026-11-02 - The UART saga begins

Bought a cheap PL2303 clone. Confirmed the decisive ground truth: the vendor firmware **does** print -
its `bootargs` carry `earlyprintk=sunxi-uart,0x02600000` and `console=ttyAS0,115200`, so UART0 (PH0/PH1)
is live at 115200 from power-on. So a 0-byte read meant *wrong pad or bad contact*, not a silent console.

Then a long, frustrating pad hunt on a board with tiny unlabelled pads and no soldering iron. Garbage from
a USB-data pad at every baud; 0 bytes from a 3-pad cluster; a multimeter reading (2.64 V) that I second-
guessed and wrongly abandoned. Near the end the clone adapter **died** from a probe short. Session ended
on a dead adapter and no clean read.

## 2026-11-06 - UART solved, and the floodgates open

A new **CP2102** (genuine SiLabs, via usbipd -> WSL) and solid contact on the right pad - the **3-pad
cluster by the `LED` silkscreen / power connector, TX = the middle pad** (the 2.64 V reading was right all
along) - and the stock boot log poured out clean: 8963 bytes, 100% printable, `BOOT0` to the Android
shell. The multi-session blocker was over. Pinout, photo and log are in [re/uart.md](re/uart.md).

### Our own kernel, at last visible

Flashed our earlycon kernel and read it live. It **runs**: `Linux version 6.16.7`, our DTS machine model,
and `earlycon: uart8250 at MMIO32 0x02600000` - the UART base confirmed from our *own* kernel, not
borrowed. It reached PSCI and the arch timer, then went dark at `legacy bootconsole [uart8250] disabled`.
Not a crash - the earlycon->console handover, where the kernel drops the earlycon and its only real
console is the `tty0` dummy. Fix: `keep_bootcon ignore_loglevel` on the cmdline.

### A detour into U-Boot, and a costly recovery

To avoid a FEL recovery on every kernel hang, tried to read via `boot_b`. Discovered "boot_b never comes
up" is really **U-Boot dropping to its `=>` prompt** (slot b's `env_b` fails CRC -> default env ->
`distro_bootcmd` undefined). Added a **TX wire** (adapter TXD -> board RX, the top pad; CP2102 measured a
safe 3.56 V) and got an **interactive U-Boot console**. But this Allwinner `bootm` is Android-only: it
ignores a raw zImage or a uImage and always demands `vendor_boot`, faulting on eMMC in the slot-b state.
Both `bootm` attempts crashed U-Boot. (A boot_a flash earlier in the day did hang and cost a FEL +
PhoenixSuit recovery, which wiped `/data` - the price the loop below was designed to stop paying.)

### The /data-safe loop: clone slot a to slot b

The fix that stuck: make **slot b an exact clone of slot a** (`env`, `vendor_boot`, `init_boot`, `vbmeta`,
`dtbo`) with **our kernel in `boot_b`**. Slot b then boots our kernel through the identical, proven android
flow, reading needs only RX, and a power-cycle always falls back to stock `boot_a` - no FEL, no `/data`
wipe, ever. This is the sustainable bring-up loop (see [re/uboot-console.md](re/uboot-console.md)).

### The bug the UART was built to find

With `keep_bootcon` and the slot-clone loop, the kernel finally printed past the handover - straight into
a stack trace: a **NULL-pointer oops in `sunxi_ccu_probe`** during clock registration, killing init. The
cause, found by reading our own code against the log: `CLK_NUMBER` (the `clk_hw_onecell_data.num`) was
**64**, but our `hws[]` array only has **62** entries (highest index `CLK_VE` = 61). `sunxi_ccu_probe`
loops `for (i = 0; i < num; i++)` and read `hws[62]` and `hws[63]` past the array end -> a garbage pointer
-> `clk_hw_register(garbage)` -> oops. A one-line fix: `CLK_NUMBER = 62`.

### Kernel bring-up: success

Reflashed and read again. The kernel now runs **all the way**: every subsystem (8250 serial, net, i2c,
sdhci/dw-mmc, usbhid, IPv6, CAN, X.509), then `clk: Disabling unused clocks` and `PM: genpd: Disabling
unused power domains` (the core fully up), into `kernel_init` -> `prepare_namespace`, panicking only at
**`VFS: Unable to mount root fs on unknown-block(0,0)`**. That is the *healthy* end of a kernel with no
rootfs - no watchdog reset, no driver crash, just nothing to run as init yet. From "invisible hang at
0.02 s" to "full boot at 2.2 s" in one day, because the UART finally let us read the truth. Log:
[re/h726-kernel-fullboot.txt](re/h726-kernel-fullboot.txt).

One loose end still open: `2000000.pinctrl` probe fails `-2`, but it is not fatal (earlycon needs no
pinctrl). Next chapters: fix pinctrl, bring up the eMMC (dw-mmc) or add an initramfs so there is a real
root device, and get the real `ttyS0` console for an interactive shell.
