# PLAN

The roadmap for building a clean custom OS on the Allwinner H726 (`sun50iw15`). Read [RULE.md](RULE.md) for how we work and [STATUS.md](STATUS.md) for where we are.

## 1. Context and decision

The stock OS is a bloated, untrusted BadBox clone that runs poorly on 1 GB RAM. We want a lean, clean Lineage-based system that keeps the projector working. There are two tiers, and which one we can reach is decided by two cheap questions answered in Phase 0:

1. Is the device Treble compliant, so a Generic System Image (GSI) can replace the vendor userspace?
2. Is there a buildable kernel for `sun50iw15` (Allwinner BSP source, or a path to rebuild from the extracted DTB and config)?

Tier A does not need kernel source. Tier B does. We pursue A first for a result that boots, and B as the path to a clean, auditable kernel.

## 2. The honest scope of "reverse the kernel"

We do not decompile the kernel binary into buildable source; that is not realistic. What is realistic and sufficient for a port:

- **Extract the DTB** (device tree blob) from the boot image. This is unpacking, not reverse-engineering, and it gives the hardware description a port needs.
- **Extract the kernel config** (`/proc/config.gz` if present, or from the image) to learn exactly how the vendor built it.
- **Rebuild from the BSP.** The base is kernel `5.15.167`; match it to Allwinner's open BSP (longan / tina) for `sun50iw15` and rebuild with the extracted config and DTB. This reconstructs the build, it does not reverse the binary.
- **Targeted driver RE.** Reverse only the specific out-of-tree drivers the BSP lacks (for example parts of the display and keystone path; the AIC8800 `rwnx` driver is relatively open). This is where tools such as [morluto/rea](https://github.com/morluto/rea) with Ghidra are used, on one `.ko` or `.so` at a time, never the whole kernel.

## 3. Phases

### Phase 0, recon (cheap, decides everything)

- Confirm Treble and GSI capability: `ro.treble.enabled`, VNDK version, dynamic partitions (`super`), the system-as-GSI slot layout.
- Find a 32-bit `armeabi-v7a` LineageOS or AOSP GSI candidate (the device is 32-bit userspace, so the common arm64 GSIs will not boot).
- Extract the DTB and kernel config from the stock `boot` / `init_boot` image (we have the full stock image from the sibling repo).
- Determine whether the Allwinner BSP (longan / tina) has a public `sun50iw15` branch on kernel 5.15.
- Inventory the proprietary bits to keep or extract: keystone and autofocus (`com.hysd.vafocus`, `KeystoneUtils_726`, `set_angle_offset` to `/oem/gsensor_data`, `ProjectActivity` / `InitAngleActivity`), the Mali-G31 GPU blob, the AIC8800 Wi-Fi driver, the DRM/KMS display path, audio.

### Phase A, GSI route (no kernel source needed)

- Flash a 32-bit Lineage GSI onto the system slot, keeping the vendor kernel and `/vendor` partition.
- Verify boot and hardware: display, Wi-Fi, Bluetooth, audio, HDMI input, USB, gravity sensor.
- Re-add the projector apps on top (the keystone / autofocus stack), reusing the reverse-engineering from the sibling repo.
- Integrate the defaults (see Section 4).
- Deliverable: a clean-userspace build that boots and projects.

### Phase B, from-source route (if a buildable kernel exists)

- Build a device tree for `sun50iw15` from the extracted DTB and config.
- Rebuild the kernel from the BSP plus config and DTB.
- Extract the vendor blobs and assemble `/vendor`; write HAL shims only where a blob does not match the clean userspace.
- Reverse-engineer the out-of-tree drivers that block boot or hardware, with REA / Ghidra.
- Boot, then debug the inevitable hardware quirks (panel timing, gsensor, Mali) from `dmesg`.
- Deliverable: a from-source, clean-kernel build.

### Integration (both tiers)

- Aurora Store as the default store (no Google account required).
- SmartTube bundled for ad-free YouTube (the device is 32-bit, pick AVC presets).
- PhairPlay integrated as the default cast receiver: autostart, the aspect-ratio fix and the Wi-Fi low-latency lock baked in (both from the sibling repo).
- Dark Material UI and the Hub app.
- Keep only the reverse-engineered keystone / autofocus stack from the vendor.
- A slim Magisk module remains for runtime-only bits (the hotacksu mask, the Wi-Fi lock daemon).

### Packaging and flashing

- Repack into the Allwinner IMAGEWTY with recomputed `add_sum` verify-files (the method proven by the root build in the sibling repo).
- Flash via PhoenixSuit over FEL (`adb reboot efex`); unbrick with the stock image; the reserve partition (per-unit colour and keystone calibration) is skipped by stock flashing and must be preserved.

## 4. Open questions and risks

- Treble compliance is unconfirmed.
- 32-bit ARM GSIs are rarer and less tested than arm64.
- Kernel source availability for `sun50iw15` is the crux for Tier B.
- A clone vendor often patches on top of the BSP without publishing the patches, so a rebuilt kernel can hit hardware quirks that need debugging.
- The keystone and autofocus stack may depend on vendor HALs that are not trivial to carry onto a GSI.
- Blob and firmware licensing: we extract for local use and do not redistribute.

## 5. Tools and references

- Sibling repo: [hoangphi01/hy300ultra-h726-allinone](https://github.com/hoangphi01/hy300ultra-h726-allinone) (root, RE, `add_sum` build, debloat, casting fixes).
- RE tooling: [morluto/rea](https://github.com/morluto/rea), Ghidra.
- GSI and Treble: LineageOS, [phhusson/treble_experimentations](https://github.com/phhusson/treble_experimentations).
- Allwinner BSP: longan / tina (to be located for `sun50iw15`).
- H713 research base (closest prior art): [YuujiLab/HY300-H713-Research](https://github.com/YuujiLab/HY300-H713-Research), [probonopd/hy310-deshittify](https://github.com/probonopd/hy310-deshittify).
