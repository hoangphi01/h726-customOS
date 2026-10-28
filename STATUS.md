# STATUS

Living status of the project. Updated as reality changes. See [PLAN.md](PLAN.md) for the roadmap.

**Phase:** Planning, about to start Phase 0 (recon).
**Last updated:** 2026-10-28.

## What is working

Nothing is built yet. This repository currently holds the plan and the rules. The groundwork below is done and lives in the sibling repo.

## Groundwork done (sibling repo)

In [hoangphi01/hy300ultra-h726-allinone](https://github.com/hoangphi01/hy300ultra-h726-allinone):

- **Root** on H726 / sun50iw15, via the per-partition `add_sum` verify-file fix for the Allwinner IMAGEWTY, flashed with PhoenixSuit over FEL.
- **Reverse-engineering** of the keystone / auto-keystone / autofocus subsystem and the display path (DRM/KMS, no MIPS), including the gravity-sensor orientation rule and the baseline store.
- **Debloat and BadBox teardown**: fake Play Store (Tubesky), droppers, OTA reinstaller, the setuid-root `hotacksu` backdoor; neutralized systemlessly.
- **Casting**: PhairPlay patched (cast aspect-ratio fix traced to the Allwinner decoder crop, plus a Wi-Fi low-latency lock for self-disconnects), autostarted from a Magisk module.
- **Dark-theme** of the vendor settings UI, a Material Hub app, hidden OTA rows.

## Device facts (confirmed)

- SoC: Allwinner H726 = `sun50iw15p1`, FEL id `0x1911`, FEL USB `1f3a:efe8`.
- CPU: quad Cortex-A53 (ARMv8-A, run in 32-bit AArch32 mode).
- Userspace ABI: `armeabi-v7a` only (no `arm64-v8a`).
- RAM: about 1 GB (low-RAM device). Panel: 720p (`1280x720`).
- GPU: Mali-G31 (OpenGL ES 3.2, driver r20p0).
- Wi-Fi/BT: AIC8800 family (`rwnx` driver); `ps_on=Y` by default.
- Kernel: `5.15.167`, 32-bit (`armv7l`).
- Display: DRM/KMS direct, no MIPS co-processor.
- Partitions: A/B, dynamic `super` (system on a read-only device-mapper partition), `init_boot`, reserve partition for calibration.
- Security state of the rooted build: `verifiedbootstate=orange`, verity disabled, SELinux Permissive.

## Next (Phase 0)

- [ ] Check Treble / GSI capability (`ro.treble.enabled`, VNDK, partition layout).
- [ ] Find a 32-bit `armeabi-v7a` LineageOS / AOSP GSI candidate.
- [ ] Extract the DTB and kernel config from the stock boot image.
- [ ] Determine whether the Allwinner BSP (longan / tina) has a public `sun50iw15` branch.
- [ ] Inventory the proprietary bits to keep or extract (keystone, display, Mali, AIC8800, audio).

## Open questions

- Is the device Treble compliant?
- Do usable 32-bit ARM GSIs exist and boot here?
- Is there buildable kernel source for `sun50iw15`? (Decides Tier B.)
- Can the keystone / autofocus stack be carried onto a GSI, or does it need vendor HALs?

## Bugs

None recorded yet (no build exists). This section will track "What is working / Bugs" once a build boots.
