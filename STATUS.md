# STATUS

Living status of the project. Updated as reality changes. See [PLAN.md](PLAN.md) for the roadmap.

**Phase:** **Track B is the primary focus; Track A is a deferred fallback.** Phase B1 done (new-driver register maps in `re/`). Phase B2 producing real buildable artifacts (see [kernel/README.md](kernel/README.md)): `kernel/sun50i-h726-hy300ultra.dts` (our board DTS, compiles clean to a 9276-byte DTB), `kernel/configure-h726.sh` + `kernel/h726_defconfig`, and `kernel/build.sh` (self-contained; builds mainline 6.16.7 + our DTS into a ~41 MB `Image` + the DTB, verified). Key finding: sun50iw15 is not H6 (core controllers relocated). Now underway: the **sun50iw15 CCU + pinctrl drivers** (`kernel/ccu`, `kernel/pinctrl`), written as real sunxi-ng drivers from the clock tree read live off the device (`re/ccu-sun50iw15.md`); sun50iw15 is the sun55i (A523) generation, so they are modelled on `ccu-sun55i-a523`. The core subset (PLLs + UART/MMC/TWI/PWM/DMA gates + the board pins) compiles into the kernel Image (`ccu-sun50iw15.o`, `pinctrl-sun50iw15.o`, verified). **But a bigger gate sits before the CCU: the boot handoff** (see [kernel/bringup-strategy.md](kernel/bringup-strategy.md)). The stock kernel is AArch32 (confirmed by inspecting `boot_a`) and entering AArch64 on this SoC is unsolved (`sunxi-fel` sees `0x1911` unknown, `sunxi-fel uboot` -> `Unexpected SCTLR`, the RVBAR AArch64 warm-reset hangs), so the arm64 `Image` cannot be entered by the vendor boot chain. **Decision taken: build the kernel 32-bit (`ARCH=arm`) to match the vendor boot chain** (Branch 1). Validated at the build level: our CCU driver, pinctrl driver and device tree compile unchanged for `ARCH=arm`, and a `multi_v7_defconfig` kernel with them built in produces a `zImage` (~11 MB); `kernel/build.sh` defaults to this 32-bit target. Next: insert a GPIO heartbeat at early 32-bit startup, pack the `zImage` into `boot_a`, flash via PhoenixSuit (not `sunxi-fel`, unsupported on 0x1911), and watch for a reaction to confirm gate #0 is passed (needs the device). A UART console is the real unlock.
**Last updated:** 2026-10-28.

## What is working

Built and verified (compile/build level; nothing boots yet, see gate #0 in the Phase line):

- **Reverse-engineering (B1):** 9 register-map notes in [re/](re/) covering the whole display + keystone stack (`ksc`, `tvtop`, `vs-display`, `vs-osd`, `tvpanel` = TCON/LVDS/DSI/DE, panels, `sunxi-ve`) plus the live-read clock tree ([re/ccu-sun50iw15.md](re/ccu-sun50iw15.md)).
- **Kernel base (B2):** [kernel/](kernel/) has our board device tree, `h726_defconfig`, a self-contained `build.sh`, and real sun50iw15 **CCU + pinctrl drivers** (sunxi-ng, A523-generation). Verified: they compile into both a 32-bit `zImage` (~11 MB) and a 64-bit `Image` (~41 MB), and the DTB builds (9276 bytes).
- **Boot path (gate #0 / B3):** the kernel is built 32-bit (`ARCH=arm`) to match the vendor AArch32 chain, and a gate-#0 **GPIO heartbeat** (red LED x5) is `#include`d into the decompressor and compiles into the `zImage` ([kernel/heartbeat.md](kernel/heartbeat.md)). Ready to flash and observe on the device.

Not booting yet: the CCU register offsets are an A523 hypothesis, and gate #0 (does the vendor U-Boot enter our kernel) is unconfirmed until the heartbeat is flashed. The Phase-0 recon below is historical background.

## Phase 0 recon: results

Run read-only on the rooted device plus web research.

- **Treble: yes.** `ro.treble.enabled=true`, VNDK 34, first API level 34, dynamic partitions, A/B, virtual A/B. `super` (2 GB) holds `system` / `vendor` / `product` / `vendor_dlkm` / `system_dlkm` (a and b). `vendor` and `product` are separate read-only ext4 partitions. HIDL HALs are present (audio@6.0, and `gralloc` / `hwcomposer` / `audio` `.hermes` variants, so the board codename is **hermes**). The Mali userspace blob is `/vendor/lib/egl/libGLES_mali.so` (32-bit). Kernel modules live in `/vendor/lib/modules` (aic8800, ehci-sunxi, and the rest). The Treble plumbing a GSI needs is all present.
- **Kernel:** `5.15.167` (git `ge72cb82a460c`), 32-bit `armv7l`. The config (`/proc/config.gz`) has `CONFIG_AW_BSP=y`, `CONFIG_AW_KERNEL_AOSP=y`, `CONFIG_AW_SOC_NAME="H726"`, so it is the Allwinner AIoT / longan BSP kernel in its AOSP flavor (the Tina 5.0_AIOT line).
- **Device tree:** model `sun50iw15`, compatible `allwinner,h726 arm,sun50iw15p1`. Key hardware nodes: `dsi0` (MIPI DSI), `tvdisp`, `tvpanel`, `tvtop` (the Allwinner display top), `ve@` (video engine), `vs-display`.
- **The 32-bit blocker (Track A):** the device is 32-bit (`armeabi-v7a`) on a 32-bit kernel, so it needs a 32-bit **arm** GSI. No Android 14 GSI is published for 32-bit arm (the A14 GSIs are arm64 / a64 only; the arm GSIs stop at Android 10). A GSI is the right vehicle, but it must be self-built.
- **Kernel source (Track B): no secret SDK, so build an independent BSP the linux-sunxi way.** There is no public Allwinner BSP for `sun50iw15` (the vendor services are `vendor.aw.homlet.*`, so this is the Allwinner **homlet** Android set-top BSP, not the Tina AIoT line; the public Tina 5.0_AIOT SDK covers A527 / T527 / A733, and `radxa/kernel@allwinner-aiot-linux-5.15` is the A527 Cubie, neither is sun50iw15). That is not a dead end: the linux-sunxi and LineageOS communities never get the factory BSP. They mine the stock ROM (device tree, config, blobs), diff against mainline Linux, and write a new, transparent, independent BSP. We hold the whole mine (the stock Magcubic image plus the extracted artifacts below).
  - Anchor: the **H713 (sun50iw12)** sibling, reverse-engineered by [YuujiLab/HY300-H713-Research](https://github.com/YuujiLab/HY300-H713-Research) and probonopd. H726 is one SoC step away, so the H713 work (mainline bring-up, device tree) is the base to extend to sun50iw15 using our extracted DTB and config.
  - Honest scope: most of the kernel side is standard or reusable (USB, MMC, audio ASoC, the AIC8800 `rwnx` Wi-Fi which is relatively open, sensors, the Mali blob). The genuinely new drivers to write or port are the SoC-specific display path (`vs-display` / `vs-osd` / `tvtop` / the DE) and the Video Engine glue (CedarX); the closed userspace blobs for those are reused as-is. Big but legitimate, and this is exactly how new sunxi SoCs reach mainline.
- **Security note (keeping the vendor kernel under Track A):** a GSI replaces only `/system`, not the kernel, the ramdisk (`boot` / `init_boot` / `vendor_boot`) or the vendor SELinux policy. So SELinux stays **Permissive** (baked into the cmdline, `androidboot.selinux=permissive`) and the unaudited vendor kernel and `.ko` drivers remain. Evidence today is reassuring but not a guarantee: the loaded modules are all plausible Allwinner hardware drivers, there are no unknown listeners, no C2 or MQTT, and outbound traffic is only Google, YouTube and the weather API. Userspace bloatware and malware are fully removable (the GSI deletes `/system`, which kills the `hotacksu` backdoor; the BadBox apps in `/product` are handled by disabling or cleaning product). Kernel-level trust needs Track B.

### Ground-truth artifacts extracted (kept local, not in the repo)

The effective device tree (`/proc/device-tree`) and the `dtbo_a` partition were pulled, and the kernel config (`/proc/config.gz`) was saved, to seed Track B. These are vendor binaries and are not committed here (see [RULE.md](RULE.md)).

### Gold mine, first read of the device tree

The extracted `system.dtb` (from `/sys/firmware/fdt`, valid FDT) was decompiled with `dtc` to a 3196-line `.dts`. This is the authoritative hardware map for Track B. First findings:

- **DRAM:** base `0x40000000`; a `linux,cma` default pool is carved in `reserved-memory`.
- **Panel is LVDS, not MIPI DSI (correction).** The active display is `lvds0@5800000` driving `panel0@0`, compatible `allwinner,z20hd720m`: a 4-lane LVDS 1280x720 imager (`hactive=0x500`, `vactive=0x2d0`, pixel clock about 61.8 MHz). A `dsi0@5506000` node exists but is not the panel in use. Earlier "720p MIPI DSI" was wrong; it is 720p LVDS.
- **Display pipeline (the new work).** `vs-display` (compatible `vs,display`) is the large custom block: regions `soc`/`afbd`/`cap`/`svp`/`panel`, 9 interrupts (`cap`, `deint`, `meter`, `vnr`, `panel0/1`, `proc`, `scaler`, `afbd_dec`), 20 clocks. It is fed by `tvtop@6000000` (`allwinner,sunxi-tvtop`, the TV front-end / capture: demod, `tvfe`, `tvcap`, audio), `tvdisp@5500000` (`allwinner,tvdisp_top`), `tcon0@5501000`, `lvds0`, plus `vs-osd` (`vs,osd`) and a `vs,trid-audio-bridge`. These `vs,*` and `allwinner,sunxi-tvtop`/`tvdisp_top` nodes are the drivers with no mainline equivalent, so they are the real port/RE target.
- **Hardware keystone.** `ksc@5900000`, compatible `allwinner,ksc100`, is a dedicated keystone-correction engine (reg `0x5900000`, its own clock and CMA range). The projector's keystone is hardware, not just app math; re-adding keystone on a clean build means driving this block.
- **Video engine:** dual Cedar VE, `ve@1c0e000` (`allwinner,sunxi-cedar-ve` and `...-ve1`), under power domain `pd_ve`. Reuse the CedarX blobs over this.
- **GPU:** `gpu@0x01800000` binds as `arm,mali-midgard` (under `pd_gpu`), not Bifrost. The userspace blob `libGLES_mali.so` remains authoritative for the GL version; the DT compatible just tells us which `kbase` variant the kernel used.
- **IOMMU:** `allwinner,iommu-v14-sun50iw15` at `0x2010000`, with master ids wired to `vs-display`, `tvtop` and the VE.
- **Clocks/pinctrl (reusable base):** `sun50iw15-ccu`, `-r-ccu`, `-rtc-ccu`, `-pll-dram`, `-cpupll`, `-pinctrl`, `-r-pinctrl`. These are standard sunxi CCU/pinctrl drivers to extend from the H713 base.
- **Standard, reusable or near-mainline:** MMC (`sunxi-mmc-v4p6x`/`v5p3x`), USB (`ehci/ohci/udc/otg-manager`), GMAC + MDIO ethernet, TWI/`twi-v105`, SPI `v1.5`, UART `v100/v101`, PWM `v201` (0..13), THS thermal, GPADC, SID efuse, watchdog, RTC `v200`, DMA `v105`, CE crypto, hwspinlock, msgbox.
- **Audio:** `sunxi-snd-codec` + `sunxi-snd-mach` with i2s / owa (SPDIF) / aaudio platforms; plus the `trid-audio-bridge` on the display side.
- **PMIC:** `x-powers,axp1530` (pek + regulators `dcdc1-3`, `aldo1`, `dldo1`).
- **Wi-Fi/BT:** `sunxi-wlan` + `sunxi-rfkill` + `sunxi-bt`/`btlpm` (AIC8800, the `rwnx` stack, relatively open).
- **Projector sensors/actuators:** accelerometers `kxtj3`, `lsm6dsr`, `lsm6dsv`, `stk83xx` (auto-keystone tilt); `motor-control` (autofocus motor); `htc,pwm-fan` (cooling); `irrx` (IR remote); `gpio-keys`.
- **Boot/firmware nodes:** `allwinner,box_start_os`, `allwinner,fastlogo` (the splash), `android,firmware` + `android,vbmeta` (AVB), `linaro,optee-tz` (OP-TEE secure world), `arm,psci-1.0`, `arm,gic-400`.

#### Driver surface, classified for Track B

| Class | Nodes | Plan |
|---|---|---|
| Reuse mainline / H713 almost as-is | CCU, pinctrl, MMC, USB, GMAC, TWI, SPI, UART, PWM, THS, GPADC, SID, watchdog, RTC, DMA, CE, GIC, PSCI, AXP1530 | Extend the sun50iw12 (H713) bring-up to sun50iw15 ids. |
| Reuse with vendor blob | Mali (midgard kbase + `libGLES_mali.so`), Cedar VE (`sunxi-cedar-ve` + CedarX `.so`/firmware), AIC8800 Wi-Fi/BT (`rwnx` + firmware) | Port the open kernel glue, keep the closed userspace. |
| New: write or RE from the vendor `.ko` | `vs,display`, `vs,osd`, `vs,trid-audio-bridge`, `allwinner,sunxi-tvtop`, `allwinner,tvdisp_top`, `tcon`, `lvds0`, `allwinner,z20hd720m` panel, `allwinner,ksc100` keystone | The genuine port target. RE with REA/Ghidra against the DTS reg/clock/irq map above. |

### Gold mine, the modules themselves (extracted)

The running kernel loads its drivers as modules, split across two stores. `/vendor` and `/vendor_dlkm` hold 65 `.ko` (the second-stage set: `aic8800_*`, `mali_kbase.ko`, `vs-display.ko`, `vs-osd.ko`, the ASoC `snd_soc_sunxi_*`, sensors, `motor-limiter.ko`, `stmvl53l1.ko` which is the ToF autofocus sensor, and so on). The first-stage display stack is not on any mounted filesystem; it lives in the **`vendor_boot` ramdisk** (`vendor_boot_a` = `mmcblk0p7`, a v4 `VNDRBOOT` image, LZ4 cpio), because it has to paint the boot logo before `/vendor` mounts. Extracted from that ramdisk, with the exact RE targets:

| Module | Size | Role |
|---|---|---|
| `tvpanel.ko` | 167 KB | panel/timing management (largest, panel tables) |
| `sunxi-ve.ko` | 81 KB | Cedar video-engine kernel glue (CedarX `.so` do the rest) |
| `sunxi_ksc.ko` | 68 KB | hardware keystone-correction engine |
| `sunxi_tvtop.ko` + `_adc.ko` | 30 + 13 KB | TV front-end / capture top |
| `panel_lvds_gen.ko` / `panel_dsi_gen.ko` | 19 / 24 KB | generic LVDS / DSI panel glue |
| `z20hd720m.ko` | 19 KB | the specific 720p LVDS imager |
| `backlight.ko`, `hidtvreg_dev.ko`, `sunxi_addr.ko` | 26 / 9 / 20 KB | backlight, TV reg HID, address manager |

`vs-display.ko` (115 KB) and `vs-osd.ko` (150 KB) come from `/vendor/lib/modules`. All are small enough to reverse in Ghidra against the DTS reg/clock/irq map.

**Exact build string to match (Track B):** `vermagic = 5.15.167-ge72cb82a460c-dirty SMP preempt mod_unload modversions ARMv7 p2v8`, built with **Android clang 17.0.2 (r487747c) + LTO**. Two consequences: the tree is `-dirty` (vendor-modified, no clean public source, which is why we mine), and `modversions` means symbol CRCs are enforced, so individual replacement modules would need matching `Module.symvers`; Track B builds the whole kernel from source, which sidesteps that.

### First RE pass: the keystone engine (`sunxi_ksc.ko`)

A static pass (ELF symbols + `.modinfo` + strings; no full decompilation yet) already maps how projector keystone works, which is what re-adding keystone on a clean build depends on. The driver is `AllWinner`'s `KSC driver`, a platform driver, `compatible = allwinner,ksc100` (also `ksc110`), `depends=tvpanel`, `import_ns=DMA_BUF`, `license=GPL`.

- **It is a LUT-based geometric warp engine, not a matrix.** It allocates three LUT banks (`Memory0/1/2 ... ksc lut buffer`) plus a double buffer, and swaps them live (`ksc_reg_switch_lut_buf`, `is_reg_update_finish`), with an `KSC_IRQ_LUT_OVER_SPAN` guard. Userspace computes the warp table (corner geometry) and uploads it; the hardware resamples the image through it.
- **It also does picture quality on the warped image:** `antialias` (param 0..128) and `sharpen` (level 0..4), via `ksc_reg_pq_setting` / `__ksc_set_pq_para`.
- **Interface to userspace:** a char device (`alloc_chrdev_region` + `__class_create` + `device_create`), `ksc_ioctl`, `ksc_mmap` / `ksc_mem_mmap` (maps the LUT and frame buffers), a sysfs group, and DMA-BUF import (`ksc_dma_map`, `dma_mmap_attrs`) so the LUT and frames are shared with the display pipeline rather than copied.
- **Interface to the display pipeline:** it exports `ksc_online_enable` (`EXPORT_SYMBOL_GPL`) and has `tvdisp_ksc_enable` / `sunxi_tvdisp_top_ksc_enable`, so `tvdisp_top` / `vs-display` switch keystone on inline. Front-end and back-end error/finish interrupts (`KSC_IRQ_FE_ERR`, `KSC_IRQ_BE_FINISH`, `SVP2KSC_DMT_ERR`) gate the frame handoff from the SVP path.

Implication for the clean build: keystone is a kernel LUT engine plus a userspace table generator. Re-adding it means keeping (or porting) `sunxi_ksc.ko` and its `tvpanel` dependency, keeping the keystone app that computes the LUT, and wiring `ksc_online_enable` into the clean kernel's display path. The same first-pass method now applies to `sunxi_tvtop.ko`, `sunxi-ve.ko` and the `vs-*` modules.

### Display stack: dependency graph and bring-up order

Reading each module's `.modinfo depends` plus its exported/imported symbols gives the whole display subsystem as a dependency graph. The bring-up (and module load) order for Track B falls straight out of it:

```
backlight        sunxi_tvtop        sunxi_ve            hidtvreg_dev
   |                |  \               (standalone:          (standalone)
 tvpanel          tvtop_adc            "User mode CEDAR
   |  \                                 device interface")
   |   +----------------+
panel_lvds_gen         sunxi_ksc  <--- ksc_online_enable
z20hd720m                |   \
                       vs_osd   vs_display
                               (orchestrator: panel + tvtop
                                + afbd + svp, dp_buf via dma-buf,
                                behind the sun50iw15 IOMMU)
```

- `backlight` (no deps) -> `tvpanel` (deps `backlight`) is the panel base; `panel_lvds_gen`, `z20hd720m` and `sunxi_ksc` all depend on `tvpanel`.
- `sunxi_tvtop` (no deps) is the TV-subsystem top: it exports a client-register API (`sunxi_tvtop_client_register`, `sunxi_tvtop_capclk_sel`, `sunxi_tvtop_clk_get/put`, `sunxi_tvtop_set_adc_ctrl`) that `sunxi_tvtop_adc`, `vs_display` and the capture path register into. It owns the shared clocks / resets / ADC.
- `vs_display` (author "Display Team", deps `tvpanel, sunxi_tvtop`) is the orchestrator: it drives panel prepare/enable, AFBD (`sunxi_tvdisp_top_afbd_enable`), SVP (`...svp_enable`), allocates display buffers (`dp_buf_*`) as DMA-BUF fds, and sits behind the IOMMU (`sunxi_enable_device_iommu`).
- **`vs_osd` depends on `sunxi_ksc`:** the OSD / UI layer is routed through the keystone warp too, so the whole composited frame (video and UI) is keystone-corrected, not just video.
- **`sunxi_ve` is standalone** ("User mode CEDAR device interface", no deps): it is just the `/dev` node the userspace CedarX stack talks to, decoupled from the display path. It can be brought up and reused on its own, which de-risks video decode from the harder display port.

### The anchor repos: what they actually provide (correction)

The H713/H310 anchors are **research and tooling repos, not kernel source trees**, so "anchor on H713" means reuse their findings and scripts over mainline linux-sunxi, not fork a ready kernel.

- [YuujiLab/HY300-H713-Research](https://github.com/YuujiLab/HY300-H713-Research): same HY300 family, one SoC back. Has `Firmware/PARTITION_LAYOUT.md` + `SYSTEM_ANALYSIS.md`, `GSI/GSI_DISCOVERIES.md` (directly useful for Track A), `Recovery/build_super.py` (a `super.img` builder for repacking), `Boot/` + `FEL/` + `Hardware/UART_FEL.md` (the UART/FEL recovery path).
- [probonopd/hy310-deshittify](https://github.com/probonopd/hy310-deshittify): `docs/drm.md` and `docs/sleep-and-screen-off.md` (display/DRM notes for this family), `hdmi-input/`, `projection/`, `privacy/ioc.txt` (indicators of compromise), debloat and launcher-switch scripts.

So Track B's kernel base is mainline linux-sunxi plus whatever sun50iw12 (H713) bring-up exists upstream, guided by our extracted DTS/config and these research repos; there is no H713 kernel tree to clone.

## Verdict

- **Track B (clean kernel) is the primary track: an independent BSP, not a blocked SDK hunt.** There is no public `sun50iw15` factory BSP and there does not need to be. The gold mine holds the full hardware spec: the DTS above is the authoritative map, `config.gz` is the defconfig base, and the vendor `.ko`/`.so` are the closed pieces to reuse or reverse. The work is to extend the H713 (`sun50iw12`) mainline bring-up to sun50iw15 for the standard blocks, and to write or RE the short list of genuinely new drivers (the `vs,*` display pipeline, `tvtop`/`tvdisp_top`, `tcon`/`lvds0`, the `z20hd720m` panel, the `ksc100` keystone). Real kernel bring-up, but bounded and seeded, exactly the linux-sunxi method. It is first because it is on the critical path (even Track A needs its DRM/KMS to be stable) and because the mine is already mined.
- **Track A (clean userspace) is a deferred fallback:** build a 32-bit LineageOS 21 (arm) GSI from source, flash it to `system_a`, keep the vendor kernel, `/vendor` and `/product`. Clean userspace, vendor kernel. Reachable and needs no UART, but it inherits the vendor HWC frame-commit quirk (below) until Track B lands. Started only if Track B stalls on hardware access, or a usable daily-driver is wanted sooner.

**No UART yet**, so Track B debugs blind in software (pstore / ramoops, a ramdisk USB `dmesg` dump, a GPIO LED heartbeat), upgrading to live serial if a 3.3V USB-UART adapter is obtained and the pads are reachable. See PLAN section 3.

## Track A: blockers inherited (and not) from the H713 GSI attempt

The H713 sibling already flashed a prebuilt PHH Treble ARM32 GSI and documented the failures ([GSI_DISCOVERIES.md](https://github.com/YuujiLab/HY300-H713-Research/blob/main/GSI/GSI_DISCOVERIES.md)). Checked which transfer to H726:

- **`UDISK` data-label blocker: does NOT apply to H726.** On H713 the data partition is labelled `UDISK`, so GSI init crashed mounting `/data`. H726 labels it the standard `userdata` (`mmcblk0p29`, `metadata` at `p21`). One blocker gone for free.
- **HWC frame-commit blocker: applies in principle.** H726's composer is the vendor `hwcomposer.hermes.so` behind `android.hardware.graphics.composer@2.2-service` (**HIDL composer 2.2**, old), with `gralloc.hermes.so`, `ro.hardware.egl=mali`, `ro.board.platform=hermes`, and a model config at `/vendor/etc/dispconfigs/default.json`. This is the same stack whose present/commit handshake timed out (`waitForCommitFinish: timeout`) against a generic SurfaceFlinger on H713, blanking the optical engine after `/data` mounts. A self-built Lineage 21 GSI keeps this vendor HWC, so the risk is concrete: a newer SF driving the quirky Allwinner HIDL@2.2 HWC. Mitigations to try first (cheap): native props already match (`egl=mali`, `gralloc=hermes`), force `wm size 1280x720`, set `device_provisioned=1` / `user_setup_complete=1`, and a VNDK shim if a vendor lib wants an older symbol. The clean fix is Track B's open DRM/KMS path (`vs-display`), which is exactly what the H713 write-up concludes is required for a stable GSI.
- **Widevine: L3 only.** H726's `/vendor/lib/mediadrm` ships only `libdrmclearkeyplugin.so` (no `libwvdrmengine` in the 32-bit dir), and the family is documented L3 (software, HDCP unprotected). Premium HD/UHD streaming (Netflix, Disney+) will not work on any build here regardless of root or ROM; use an external stick on HDMI-in. Aurora, SmartTube and YouTube are unaffected. A product note, not a blocker.

## Device facts (confirmed)

- SoC: Allwinner H726 = `sun50iw15p1`, FEL id `0x1911`, FEL USB `1f3a:efe8`, board codename `hermes`.
- CPU: quad Cortex-A53 (ARMv8-A, run in 32-bit AArch32 mode). ABI: `armeabi-v7a` only.
- RAM: about 1 GB (low-RAM). Panel: 720p (`1280x720`) LVDS imager `z20hd720m`, 4-lane (a `dsi0` node exists but is not the panel in use).
- GPU: Mali (userspace blob `libGLES_mali.so`, 32-bit; the kernel DT binds it as `arm,mali-midgard`).
- Wi-Fi/BT: AIC8800 (`aic8800_*` modules, `rwnx`); `ps_on=Y`.
- Kernel: `5.15.167`, Allwinner AIoT BSP (AOSP flavor), 32-bit.
- Display: no MIPS co-processor; pipeline is `tvtop` (TV front-end/capture) + `tvdisp_top` + `tcon`/`lvds0` + `vs-display`/`vs-osd` + dual Cedar VE, through the sun50iw15 IOMMU. Hardware keystone via `ksc100`.
- Partitions: A/B, dynamic `super`; `boot`, `init_boot`, `vendor_boot`, `dtbo`, `vbmeta` (+ system / vendor), reserve partition for calibration.
- Rooted-build state: `verifiedbootstate=orange`, verity disabled, SELinux Permissive.

## Next

**Track B (primary), build the independent BSP from the mine:**
- [x] **Phase B1: deep-RE the new drivers** into `re/` register maps (see [re/README.md](re/README.md)) and read the clock tree live. The kernel is not stripped and every vendor display driver is a module, so no `vmlinux` RE was needed.
- [x] **Phase B2: a buildable kernel base.** Board DTS, `h726_defconfig`, `build.sh`, and real sun50iw15 CCU + pinctrl drivers (sunxi-ng, A523-generation, from the live clock tree in [re/ccu-sun50iw15.md](re/ccu-sun50iw15.md)). All compile into the kernel.
- [x] **Gate #0 decided: build 32-bit (`ARCH=arm`).** The stock kernel is AArch32 and entering AArch64 on this SoC is unsolved, so an arm64 Image cannot be entered by the vendor chain. The 32-bit `zImage` builds with our drivers in; see [kernel/bringup-strategy.md](kernel/bringup-strategy.md).
- [ ] **Phase B3 (current): confirm gate #0.** A GPIO heartbeat (red LED x5) is built into the `zImage` ([kernel/heartbeat.md](kernel/heartbeat.md)); pack it into `boot_a`, flash via PhoenixSuit over FEL (keep stock to unbrick), and watch the LED. **Needs the physical device.**
- [ ] Phase B3b: once gate #0 is confirmed, hardware-correct the CCU/pinctrl register offsets (A523-hypothesis today), using the blind-debug toolbox or, ideally, a UART console.
- [ ] Phase B4: driver bring-up loop by dependency order (see [re/README.md](re/README.md)), reusing the Mali / CedarX / AIC8800 blobs.

**Track A (deferred fallback), build the clean userspace:**
- [ ] Set up a Lineage 21 build host (external drive, WSL Ubuntu, `repo` and deps).
- [ ] Sync `lineage-21`; build the 32-bit arm GSI target; produce `system.img`.
- [ ] Flash test (keep the stock image for unbrick), verify hardware over `adb logcat`, write "What works / Bugs".
- [ ] Re-add the keystone / autofocus stack and the defaults (Aurora, SmartTube, PhairPlay).

## Bugs

None recorded yet (no build exists). This section will track "What is working / Bugs" once a build boots.
