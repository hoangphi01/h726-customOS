# STATUS

Living status of the project. Updated as reality changes. See [PLAN.md](PLAN.md) for the roadmap.

**Phase:** Phase 0 (recon) complete. Starting Track A (build a clean GSI) and Track B (hunt the kernel BSP) in parallel.
**Last updated:** 2026-10-28.

## What is working

Nothing is built yet. Phase 0 recon is done and the path is decided (below). The groundwork from the sibling repo still holds.

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

## Verdict

- **Track A (clean userspace) is reachable:** build a 32-bit LineageOS 21 (arm) GSI from source, flash it to `system_a`, keep the vendor kernel, `/vendor` and `/product`. Clean userspace, vendor kernel.
- **Track B (clean kernel) is blocked for now** by the missing `sun50iw15` BSP source. It becomes a parallel research track (find the Tina SDK, or assess porting sun50iw15 onto a generic AIoT 5.15 tree using the extracted config and DTB).

Both run in parallel: A delivers a usable clean-userspace build; B chases a trustworthy kernel.

## Device facts (confirmed)

- SoC: Allwinner H726 = `sun50iw15p1`, FEL id `0x1911`, FEL USB `1f3a:efe8`, board codename `hermes`.
- CPU: quad Cortex-A53 (ARMv8-A, run in 32-bit AArch32 mode). ABI: `armeabi-v7a` only.
- RAM: about 1 GB (low-RAM). Panel: 720p (`1280x720`), MIPI DSI.
- GPU: Mali-G31 (blob `libGLES_mali.so`, 32-bit).
- Wi-Fi/BT: AIC8800 (`aic8800_*` modules, `rwnx`); `ps_on=Y`.
- Kernel: `5.15.167`, Allwinner AIoT BSP (AOSP flavor), 32-bit.
- Display: DRM/KMS direct, no MIPS co-processor; sunxi `tvtop` / `ve` / `vs-display`.
- Partitions: A/B, dynamic `super`; `boot`, `init_boot`, `vendor_boot`, `dtbo`, `vbmeta` (+ system / vendor), reserve partition for calibration.
- Rooted-build state: `verifiedbootstate=orange`, verity disabled, SELinux Permissive.

## Next (parallel)

**Track A, build the clean userspace:**
- [ ] Set up a Lineage 21 build host (external drive, WSL Ubuntu, `repo` and deps).
- [ ] Sync `lineage-21`; build the 32-bit arm GSI target; produce `system.img`.
- [ ] Flash test (keep the stock image for unbrick), verify hardware, write "What works / Bugs".
- [ ] Re-add the keystone / autofocus stack and the defaults (Aurora, SmartTube, PhairPlay).

**Track B, hunt the kernel BSP:**
- [ ] Find the Allwinner Tina 5.0_AIOT / longan SDK for `sun50iw15`.
- [ ] If not found, assess porting sun50iw15 onto a generic AIoT 5.15 tree using the extracted config and DTB.
- [ ] Reverse-engineer the out-of-tree drivers (`tvtop`, `vs-display`, panel) as needed.

## Bugs

None recorded yet (no build exists). This section will track "What is working / Bugs" once a build boots.
