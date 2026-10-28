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

Track B is now the **primary** track and Track A is a **deferred fallback**. The reason is the critical path: Phase 0 showed the vendor display HAL (the Allwinner HWC) fails the frame-commit handshake with a generic SurfaceFlinger (the H713 "mount-to-blank"), so even Track A's GSI needs an open DRM/KMS driver to be stable, which is exactly what Track B produces. Track B also has the momentum: the mine is already decompiled and the new-driver list is short and bounded (see STATUS). So we lead with Track B and keep Track A in reserve.

### Track B (primary), independent BSP (the real, trustworthy kernel)
Build a transparent kernel the linux-sunxi way, entirely from the mine plus open sources:
1. `dtc` the extracted DTB to `.dts`; this is the authoritative hardware description (done).
2. Use the extracted `config.gz` as the defconfig base.
3. **Anchor on the H713 (`sun50iw12`) sibling** ([YuujiLab/HY300-H713-Research](https://github.com/YuujiLab/HY300-H713-Research), [probonopd/hy310-deshittify](https://github.com/probonopd/hy310-deshittify)) and mainline linux-sunxi. Note these anchors are research and tooling repos (partition layout, GSI discoveries, `build_super.py`, DRM and FEL notes), not kernel trees, so the kernel base is mainline linux-sunxi plus any upstream sun50iw12 bring-up, guided by our extracted DTS/config and their findings.
4. Reuse the closed blobs as-is (Mali userspace, CedarX VE, display HAL, VE firmware).
5. Deep-RE and write or port the genuinely new drivers: the SoC-specific display path (`vs-display` / `vs-osd` / `tvtop` / `tcon` / `lvds0` / the panel / the `ksc100` keystone) and the VE kernel glue. RE the vendor `.ko` with Ghidra / REA / ARM objdump only where mainline has nothing to diff against.
6. Boot and debug. **We have no UART serial yet** (see the no-serial strategy below), so early boot is debugged blind through pstore, a USB dmesg dump, and a GPIO heartbeat, upgrading to live serial if a UART adapter is obtained.

### Track A (deferred fallback), clean userspace on the vendor kernel
Build a 32-bit LineageOS 21 (arm) GSI from source and flash it to `system_a`, keeping the vendor kernel, `/vendor` and `/product`. From the mine we take the keystone and autofocus apps (and the reverse-engineering from the sibling repo) to re-add the projector features. Result: clean userspace, lean on 1 GB, vendor kernel. Deferred: we start this only if Track B stalls on hardware access (UART, opening the case), or when a usable daily-driver is wanted sooner. It needs no UART (a GSI boots past the hardware stage and gives `adb logcat` / `dmesg` over plain USB), but it inherits the vendor kernel's HWC quirk until Track B's DRM/KMS lands.

### No-serial debug strategy (until a UART adapter is in hand)
Standard sunxi bring-up assumes a 3.3V UART console. We do not have one yet (the case may not even be openable), so Track B debugs blind, in software:
- **pstore / ramoops:** reserve a small RAM region (the DTS already maps `reserved-memory`) with `CONFIG_PSTORE` + `CONFIG_PSTORE_RAM`, so a panic in the new kernel is written to RAM; reboot into the stock ROM and read `/sys/fs/pstore/` to recover the dying kernel's last log.
- **Auto-dump dmesg:** patch the ramdisk `init` so that as soon as USB or storage enumerates it runs `dmesg > /dev/<usb-or-userdata>/boot_log.txt`; plug into the PC and read it.
- **GPIO LED heartbeat:** a few lines in the new kernel toggling a GPIO LED at milestones (DRAM init done, CCU up, display driver loaded), so the boot stage where it dies is visible by eye.
- **Upgrade path:** if a USB-UART adapter (FTDI / CP2102 / CH340, 3.3V, 115200 8N1) is obtained and the pads are reachable, switch to live serial bring-up, which is far faster. The H713 pad map ([Hardware/UART_FEL.md](https://github.com/YuujiLab/HY300-H713-Research/blob/main/Hardware/UART_FEL.md)) is the starting reference for locating H726's pads.

## 4. Phases (Track B leads)

- **Phase 0 (done):** recon + first mining. Treble confirmed, device facts, DTS decompiled and the driver surface classified, the new-driver `.ko` extracted with the build string pinned, the keystone first-pass, the display dependency graph, the anchors understood, the H713 GSI blockers mapped onto H726. See STATUS.
- **Phase B1 (in progress): deep-RE the new drivers.** Produce real register / IO maps (offsets, bitfields, probe order) for the drivers mainline cannot supply, in bring-up order: `sunxi_ksc` -> `sunxi_tvtop` -> `vs-display` -> `tcon` / `lvds0` + the `z20hd720m` panel -> `vs-osd`; `sunxi-ve` last (reuse the CedarX blobs, it is standalone). Tools: ARM objdump for the MMIO accesses, Ghidra headless for the drivers whose logic needs decompiling (ksc warp, vs-display orchestration), anchored on the DTS reg bases. Output: one RE note per driver in `re/`, no blobs committed.
- **Phase B2: a buildable kernel base.** Clone mainline linux-sunxi (check the 5.15 LTS branch against `vermagic 5.15.167`); check sun50iw12 upstream status; generate a defconfig from the extracted `config.gz`; convert the decompiled DTS to clean source; add sun50iw15 CCU / pinctrl / DRAM extending the sun50iw12 bindings.
- **Phase B3: boot.img + the no-serial debug harness.** Build the kernel + DTB + a minimal ramdisk into a boot image; enable pstore / ramoops on the reserved-memory region, the ramdisk USB dmesg dump, and the optional GPIO heartbeat; flash to `boot_a` over FEL / PhoenixSuit (keep the stock IMAGEWTY to unbrick, the reserve partition is skipped and safe); boot, and on a crash reboot into stock to read `/sys/fs/pstore`.
- **Phase B4: driver bring-up loop.** Load drivers in dependency order using the B1 register maps plus the reused blobs (Mali midgard, CedarX VE, AIC8800 + firmware); iterate boot -> read pstore / dmesg -> fix. Record "What works / Bugs" in STATUS.
- **Phase A (deferred fallback):** set up the Lineage 21 build host (external drive, WSL), sync `lineage-21`, build the 32-bit arm GSI, flash-test (keep the stock image for unbrick), verify hardware over `adb logcat`, re-add keystone and the defaults (Aurora, SmartTube, PhairPlay). Started only if Track B stalls on hardware access or a usable device is wanted sooner.
- **Integration (both):** Aurora as the store, SmartTube bundled, PhairPlay as the default receiver (the aspect and Wi-Fi-lock fixes baked in), dark Material UI and the Hub, only the keystone stack kept from the vendor, a slim Magisk module for runtime bits.
- **Packaging:** repack into the Allwinner IMAGEWTY with recomputed `add_sum` verify-files (proven by the root build in the sibling repo); flash via PhoenixSuit over FEL; the reserve partition (calibration) is skipped by stock flashing and must be preserved.

## 5. Open questions and scope

- Track B is a real kernel bring-up: standard and reusable parts are easy, but the display and VE drivers are new work, accelerated by the H713 sibling and the extracted DTS.
- No UART serial yet: early-boot debugging is blind (pstore + USB dmesg dump + GPIO heartbeat), so the fix-boot-fix cycle is slower. Buying a USB-UART adapter and reaching the pads is the single biggest accelerator for Track B; the case may not be openable, which is an open hardware risk.
- sun50iw15 may have nothing upstream, so CCU / pinctrl / DRAM may be written from scratch extending sun50iw12 plus the DTS.
- Track A (deferred): 32-bit arm Lineage 21 GSI is self-built (no A14 arm prebuilt), less tested than arm64, and vendor HIDL HALs on a newer GSI may need shims (the HWC frame-commit quirk).
- Reuse blobs for local build only; do not redistribute (see RULE).

## 6. Tools and references

- Sibling repo: [hoangphi01/hy300ultra-h726-allinone](https://github.com/hoangphi01/hy300ultra-h726-allinone) (root, RE, `add_sum` build, debloat, casting fixes, the stock image).
- Mining and RE: `dtc`, `extract-ikconfig`, `unpackbootimg`, the sibling repo's IMAGEWTY tools, [morluto/rea](https://github.com/morluto/rea) + Ghidra, [smaeul/sunxi-blobs](https://github.com/smaeul/sunxi-blobs).
- Kernel base: mainline linux-sunxi, the H713 work at [YuujiLab/HY300-H713-Research](https://github.com/YuujiLab/HY300-H713-Research) and [probonopd/hy310-deshittify](https://github.com/probonopd/hy310-deshittify).
- GSI and Treble: LineageOS, [phhusson/treble_experimentations](https://github.com/phhusson/treble_experimentations).
