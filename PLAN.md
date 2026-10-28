# PLAN

The roadmap for building a clean custom OS on the Allwinner H726 (`sun50iw15`). Read [RULE.md](RULE.md) for how we work and [STATUS.md](STATUS.md) for where we are.

## 1. Context and the central idea

The stock OS is a bloated, untrusted BadBox clone that runs poorly on 1 GB RAM. We want a lean, clean Lineage-based system that keeps the projector working, and ideally a trustworthy from-source kernel.

There is no public Allwinner BSP for `sun50iw15`, and that is normal: the linux-sunxi and LineageOS communities never get the factory BSP. The central method of this project is therefore:

> **The stock firmware is the gold mine.** We have the full Magcubic IMAGEWTY image. Everything a port needs is inside it, and we extract rather than beg for a secret SDK: the device tree, the kernel config, the proprietary blobs, and the boot chain to reverse-engineer. We diff the open pieces against mainline Linux and write a new, transparent BSP; we reuse the closed pieces as binaries.

Phase 0 recon is complete (see STATUS). Verdict: two tracks, both fed from the gold mine.

## 2. The gold mine: what we extract from the stock firmware

The stock Magcubic IMAGEWTY (`HY300Ultra_H726_Android14_STOCK_Magcubic.img`, 2.2 GB) unpacks into the partitions; the live rooted device gives the same artifacts more easily. Already extracted (kept local, not committed, see RULE):

| Artifact | From | Use |
|---|---|---|
| Device tree blob (`system.dtb`, valid FDT) | `/sys/firmware/fdt` | `dtc` to `.dts`: exact memory map, clocks, pinctrl, interrupts, display nodes. The hardware spec. |
| Kernel config (`config.gz`) | `/proc/config.gz` | Exactly which features and drivers the vendor built. The defconfig base. |
| Kernel modules (`*.ko`) | `/vendor/lib/modules` | `aic8800_*` (Wi-Fi), `snd_soc_sunxi_*` (audio), `sunxi-*`, `mali_kbase`, `vs-display`/`vs-osd` (the custom display), sensors (`sc7a20h`, `st_lsm6dsx`, `stk`, `stmvl53l1`), `motor-limiter` (focus). Reuse where possible; RE the sunxi-specific ones. |
| Userspace blobs (`*.so`) | `/vendor/lib*` | `libGLES_mali.so` (GPU), the CedarX VE stack (`libVE`, `libcedarse`, `libvdecoder`, `libvenc_*`, `libawcodec2_vndk`), `libdisplayconfig.so`, display HAL. Copy as-is. |
| Firmware blobs (`*.bin`) | `/vendor/etc/firmware` | `EXEC_KERNEL_IMAGE.bin`, `OS_ROM.bin`, `LogoRegData.bin`, cedarSE. Reuse as-is. |
| Boot chain (`boot_a`, `vendor_boot`, `bootloader_a`) | partitions | The kernel binary + DTB + ramdisk, and U-Boot / SPL for low-level init RE (Ghidra). |

## 3. Two tracks, both fed from the mine

### Track A, clean userspace now (the sure win)
Build a 32-bit LineageOS 21 (arm) GSI from source and flash it to `system_a`, keeping the vendor kernel, `/vendor` and `/product`. From the mine we take the keystone and autofocus apps (and the reverse-engineering from the sibling repo) to re-add the projector features on the clean Lineage userspace. Result: clean userspace, lean on 1 GB, vendor kernel.

### Track B, independent BSP (the real, trustworthy kernel)
Build a transparent kernel the linux-sunxi way, entirely from the mine plus open sources:
1. `dtc` the extracted DTB to `.dts`; this is the authoritative hardware description.
2. Use the extracted `config.gz` as the defconfig base.
3. **Anchor on the H713 (`sun50iw12`) sibling** ([YuujiLab/HY300-H713-Research](https://github.com/YuujiLab/HY300-H713-Research), probonopd) and mainline linux-sunxi: H726 is one SoC step away, so their bring-up (CCU clocks, pinctrl, DRAM, device tree) is the base to extend to sun50iw15.
4. Reuse the closed blobs as-is (Mali userspace, CedarX VE, display HAL, VE firmware).
5. Write or port the genuinely new drivers: the SoC-specific display path (`vs-display` / `vs-osd` / `tvtop` / the DE) and the VE kernel glue. RE the vendor `.ko` with REA / Ghidra only where mainline has nothing to diff against.
6. Boot, then debug hardware quirks from `dmesg`.

Track A gives a usable machine while Track B matures.

## 4. Phases

- **Phase 0 (done):** recon. Treble confirmed, device facts, the two-track verdict, the gold-mine inventory. See STATUS.
- **Phase 1, Track A:** set up the Lineage 21 build host (external drive, WSL), sync `lineage-21`, build the 32-bit arm GSI, flash-test (keep the stock image for unbrick), verify hardware, re-add keystone and the defaults (Aurora, SmartTube, PhairPlay).
- **Phase 1, Track B (parallel):** `dtc` the DTB to `.dts`; stage the blobs and `.ko`; obtain the H713 kernel tree; map the extracted config and DTS onto it; identify the drivers to write (display, VE) versus reuse.
- **Phase 2, Track B:** build the kernel from the new BSP, boot, bring up hardware driver by driver.
- **Integration (both):** Aurora as the store, SmartTube bundled, PhairPlay as the default receiver (the aspect and Wi-Fi-lock fixes baked in), dark Material UI and the Hub, only the keystone stack kept from the vendor, a slim Magisk module for runtime bits.
- **Packaging:** repack into the Allwinner IMAGEWTY with recomputed `add_sum` verify-files (proven by the root build in the sibling repo); flash via PhoenixSuit over FEL; the reserve partition (calibration) is skipped by stock flashing and must be preserved.

## 5. Open questions and scope

- 32-bit arm Lineage 21 GSI is self-built (no A14 arm prebuilt) and less tested than arm64.
- Track B is a real kernel bring-up: standard and reusable parts are easy, but the display and VE drivers are new work, accelerated by the H713 sibling and the extracted DTS.
- Vendor HIDL@6.0 HALs on a newer GSI may need shims.
- Reuse blobs for local build only; do not redistribute (see RULE).

## 6. Tools and references

- Sibling repo: [hoangphi01/hy300ultra-h726-allinone](https://github.com/hoangphi01/hy300ultra-h726-allinone) (root, RE, `add_sum` build, debloat, casting fixes, the stock image).
- Mining and RE: `dtc`, `extract-ikconfig`, `unpackbootimg`, the sibling repo's IMAGEWTY tools, [morluto/rea](https://github.com/morluto/rea) + Ghidra, [smaeul/sunxi-blobs](https://github.com/smaeul/sunxi-blobs).
- Kernel base: mainline linux-sunxi, the H713 work at [YuujiLab/HY300-H713-Research](https://github.com/YuujiLab/HY300-H713-Research) and [probonopd/hy310-deshittify](https://github.com/probonopd/hy310-deshittify).
- GSI and Treble: LineageOS, [phhusson/treble_experimentations](https://github.com/phhusson/treble_experimentations).
