<div align="center">

# h726-customOS

### A lean, trustworthy custom OS for the Allwinner H726 (sun50iw15) HY300 Ultra projector

**English** · [Tiếng Việt](#tiếng-việt)

![SoC](https://img.shields.io/badge/SoC-Allwinner%20H726%20(sun50iw15)-blue)
![FEL ID](https://img.shields.io/badge/FEL%20ID-0x1911-informational)
![Kernel](https://img.shields.io/badge/kernel-mainline%206.16%20(arm)-green)
![Status](https://img.shields.io/badge/status-kernel%20bring--up%20(B3)-orange)
![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)
![Affiliation](https://img.shields.io/badge/affiliation-none-lightgrey)

</div>

---

## 1. What this is

An effort to build a clean, from-source operating system for the Magcubic / Gojodoq **HY300 Ultra** projector, powered by the **Allwinner H726 (`sun50iw15p1`)**. The stock firmware is a white-label clone carrying the BadBox malware family on top of a heavy vendor Android framework; this project replaces it with a mainline Linux kernel built from scratch for the SoC, reverse-engineered from the device itself.

The current focus is **Track B, a trustworthy from-source kernel**. Everything needed to port it was mined out of the running device (device tree, clock tree, the driver register maps), because there is no public Allwinner BSP for this SoC, which is normal: the linux-sunxi community always mines rather than waiting for a factory SDK.

## 2. Hardware overview

| | |
|---|---|
| **SoC** | Allwinner H726 = `sun50iw15p1`, quad Cortex-A53, FEL id `0x1911`, board codename `hermes` |
| **Clock/pin generation** | sun55i (A523) class: `pll-peri0/1` multi-output, `smhc` MMC, per-peripheral `sw-cfg-bus` gates (not H6) |
| **Execution state** | AArch32: the vendor runs the 64-bit-capable A53 in 32-bit mode, so the clean kernel is built 32-bit to match |
| **RAM** | ~1 GB (low-RAM) |
| **Display** | 720p (1280x720) 4-lane **LVDS** imager (`z20hd720m`), no MIPS co-processor, **hardware keystone** (`ksc100`) |
| **GPU** | Mali (Midgard-class), driven by Panfrost upstream |
| **Video** | dual Cedar VE (CedarX blobs) |
| **Wi-Fi / BT** | AIC8800 (`rwnx`) |
| **Storage** | eMMC, A/B dynamic `super` |
| **Recovery** | FEL (BootROM), flashed via PhoenixSuit; stock IMAGEWTY reflashes safely, reserve/calibration partition is skipped |
| **Stock** | Android 14, Allwinner 5.15 BSP kernel (32-bit), BadBox clone |

## 3. Progress

Track B (a from-source kernel) is the primary focus; Track A (a LineageOS GSI) is a deferred fallback. Full detail in [STATUS.md](STATUS.md).

| Phase | What | State |
|---|---|---|
| Phase 0 | Recon: Treble confirmed, device facts, the two-track verdict | Done |
| B1 | Reverse-engineer the display + keystone drivers into register maps ([re/](re/)) | Done, 9 notes |
| B2 | Buildable kernel base: board device tree, `defconfig`, build scripts, and real sun50iw15 **CCU + pinctrl** drivers ([kernel/](kernel/)) | Done, compiles into the kernel |
| Gate #0 | Boot handoff: the stock chain is AArch32, so the kernel is built **32-bit** (`ARCH=arm`) to match it; confirmed by a GPIO heartbeat in the `zImage` running on the device ([kernel/bringup-strategy.md](kernel/bringup-strategy.md)) | **PASSED** - U-Boot enters our 32-bit kernel |
| B3 | Boot further: hardware-correct the CCU/pinctrl register offsets so the kernel runs past the heartbeat (a UART console is now the key accelerator) | Next |
| B4 | Driver bring-up loop (display, keystone, Wi-Fi) from the B1 maps | Not started |
| Track A | LineageOS GSI (deferred fallback) | Not started |

## 4. Why

- **Smooth on 1 GB.** The stock vendor framework is bloated; a lean kernel and userspace are the only way this device runs well on 1 GB.
- **Trustworthy.** The stock userspace carries BadBox (fake Play Store, setuid backdoor, telemetry) and the vendor kernel is an unaudited binary. The goal is a clean, auditable, from-source system.
- **Projector-first.** Keep the reverse-engineered keystone and autofocus working; drop everything else.

## 5. Approach (two tracks)

- **Track B, from-source kernel (primary):** mainline Linux 6.16 for the SoC, with device tree, CCU, pinctrl and the display/keystone drivers written from the reverse-engineering in [re/](re/); the closed userspace pieces (Mali, CedarX, AIC8800 firmware) are reused as blobs. This is on the critical path because even a GSI needs an open display driver to be stable.
- **Track A, LineageOS GSI (deferred fallback):** a clean Lineage userspace on the vendor kernel via Treble. Needs no UART and gives a usable device sooner, but inherits the vendor HWC quirk until Track B's display driver lands.

The roadmap is in [PLAN.md](PLAN.md); the boot/debug strategy in [kernel/bringup-strategy.md](kernel/bringup-strategy.md).

## 6. Build

No Nix required; the script installs an ordinary cross toolchain on Ubuntu / WSL.

```sh
# 32-bit kernel (default), with the gate-#0 GPIO heartbeat built in:
HEARTBEAT=1 bash kernel/build.sh
# -> arch/arm/boot/zImage  +  sun50i-h726-hy300ultra.dtb

# plain build, or the 64-bit reference Image:
bash kernel/build.sh
TARGET=arm64 bash kernel/build.sh
```

See [kernel/README.md](kernel/README.md) for the details and the SoC-vs-H6 facts.

## 7. Build artifacts

- `zImage` (32-bit ARM, ~11 MB) with our CCU + pinctrl drivers and device tree built in; optionally the heartbeat.
- `sun50i-h726-hy300ultra.dtb` (~9 KB).
- a 64-bit `Image` (~41 MB) via `TARGET=arm64`, kept for reference (not bootable by the vendor chain, see gate #0).

Everything compiles and builds; nothing boots yet (see below).

## 8. Known challenges

- **Gate #0, the boot handoff.** The stock kernel is AArch32 and entering AArch64 on this SoC is unsolved, so we build 32-bit. Confirmed at the build level; unconfirmed on hardware until the heartbeat is flashed.
- **CCU / pinctrl register offsets.** Modelled on the A523 generation and the live clock tree; the offsets are a hypothesis until a boot (ideally a UART console) confirms them.
- **No UART yet.** Bring-up is blind: a GPIO heartbeat and pstore stand in for serial. `sunxi-fel` does not support `0x1911`, so the loader is PhoenixSuit over FEL. A 3.3V USB-UART adapter is the single biggest accelerator.
- **The display stack.** `vs-display`, `tvtop`, the keystone and the panel are the real remaining driver work; they are reverse-engineered in [re/](re/) but not yet written as mainline drivers.

## 9. Documentation

- **[STATUS.md](STATUS.md)** the living current state (what works, what is next).
- **[PLAN.md](PLAN.md)** the roadmap, tracks and phases.
- **[RULE.md](RULE.md)** how the project is run and written.
- **[DOCS_GUIDE.md](DOCS_GUIDE.md)** the documentation index and update discipline.
- **[re/](re/)** the reverse-engineering notes (one per driver, plus the clock tree).
- **[kernel/](kernel/)** the buildable artifacts, [the build + SoC facts](kernel/README.md), [the bring-up strategy](kernel/bringup-strategy.md), and [the heartbeat](kernel/heartbeat.md).

## 10. Built on prior work

The root, the reverse-engineering (keystone / display subsystem, the `add_sum` IMAGEWTY method), the debloat and BadBox teardown, and the post-root pipeline live in the sibling repository **[hoangphi01/hy300ultra-h726-allinone](https://github.com/hoangphi01/hy300ultra-h726-allinone)**. The mainline porting method is anchored on **[shift/sun50iw12p1-research](https://github.com/shift/sun50iw12p1-research)** (the H713 sibling). This repository builds the OS on those foundations.

## 11. Disclaimer

Experimental. Flashing an Allwinner device can permanently brick it (though FEL + the stock image make recovery reliable here). For the **H726 (`sun50iw15` / `0x1911`)** only. No proprietary firmware, vendor blobs, or third-party APKs are redistributed here; sources are linked. Not affiliated with Allwinner, Magcubic, Gojodoq or Google.

---

<div align="center">

## Tiếng Việt

</div>

## 1. Đây là gì

Dự án dựng một **hệ điều hành sạch, build từ source** cho máy chiếu **HY300 Ultra** (Magcubic / Gojodoq) chạy **Allwinner H726 (`sun50iw15p1`)**. Firmware gốc là bản clone white-label dính họ malware BadBox trên nền framework Android hãng nặng nề; dự án thay nó bằng một kernel Linux mainline dựng từ đầu cho SoC, dịch ngược từ chính máy.

Trọng tâm hiện tại là **Track B, kernel sạch từ source**. Mọi thứ cần để port đều được đào ra từ máy đang chạy (device tree, cây clock, register map driver), vì không có BSP Allwinner công khai cho SoC này, điều bình thường: cộng đồng linux-sunxi luôn đào mỏ thay vì chờ SDK hãng.

## 2. Tổng quan phần cứng

| | |
|---|---|
| **SoC** | Allwinner H726 = `sun50iw15p1`, 4x Cortex-A53, FEL `0x1911`, codename `hermes` |
| **Thế hệ clock/pin** | lớp sun55i (A523): `pll-peri0/1` nhiều nhánh, `smhc`, gate `sw-cfg-bus` theo từng peripheral (không phải H6) |
| **Chế độ thực thi** | AArch32: hãng chạy A53 (vốn 64-bit) ở 32-bit, nên kernel sạch build 32-bit cho khớp |
| **RAM** | ~1 GB (low-RAM) |
| **Hiển thị** | 720p (1280x720) imager **LVDS** 4-lane (`z20hd720m`), không có co-processor MIPS, **keystone phần cứng** (`ksc100`) |
| **GPU** | Mali (lớp Midgard), chạy Panfrost |
| **Video** | dual Cedar VE (blob CedarX) |
| **Wi-Fi / BT** | AIC8800 (`rwnx`) |
| **Lưu trữ** | eMMC, A/B dynamic `super` |
| **Khôi phục** | FEL (BootROM), flash qua PhoenixSuit; ảnh stock IMAGEWTY reflash an toàn, partition reserve/calib được skip |
| **Stock** | Android 14, kernel BSP Allwinner 5.15 (32-bit), clone BadBox |

## 3. Tiến độ

Track B (kernel từ source) là trọng tâm; Track A (LineageOS GSI) là fallback hoãn lại. Chi tiết ở [STATUS.md](STATUS.md).

| Giai đoạn | Việc | Trạng thái |
|---|---|---|
| Phase 0 | Recon: xác nhận Treble, thông số máy, chốt hai track | Xong |
| B1 | Dịch ngược driver display + keystone thành register map ([re/](re/)) | Xong, 9 note |
| B2 | Nền kernel build-được: device tree, `defconfig`, build script, và driver **CCU + pinctrl** sun50iw15 thật ([kernel/](kernel/)) | Xong, compile vào kernel |
| Gate #0 | Handoff boot: chuỗi stock là AArch32, nên build kernel **32-bit** (`ARCH=arm`) cho khớp; đã xác nhận bằng heartbeat GPIO trong `zImage` chạy trên máy ([kernel/bringup-strategy.md](kernel/bringup-strategy.md)) | **ĐÃ QUA** - U-Boot vào kernel 32-bit của ta |
| B3 | Boot xa hơn: chỉnh offset register CCU/pinctrl để kernel chạy qua heartbeat (cáp UART giờ là đòn bẩy chính) | Kế tiếp |
| B4 | Vòng bring-up driver (display, keystone, Wi-Fi) từ map B1 | Chưa bắt đầu |
| Track A | LineageOS GSI (fallback hoãn) | Chưa bắt đầu |

## 4. Vì sao

- **Mượt trên 1 GB.** Framework hãng quá nặng; chỉ kernel + userspace tinh gọn mới chạy tốt trên 1 GB.
- **Đáng tin.** Userspace gốc dính BadBox (Play Store giả, backdoor setuid, telemetry), kernel hãng là nhị phân chưa kiểm toán. Mục tiêu là hệ sạch, kiểm toán được, từ source.
- **Ưu tiên máy chiếu.** Giữ keystone + autofocus đã reverse; bỏ hết phần còn lại.

## 5. Hướng đi (hai track)

- **Track B, kernel từ source (chính):** mainline Linux 6.16 cho SoC, với device tree, CCU, pinctrl và driver display/keystone viết từ phần dịch ngược ở [re/](re/); các mảnh closed userspace (Mali, CedarX, firmware AIC8800) reuse dạng blob. Nằm trên đường găng vì ngay cả GSI cũng cần driver display mở mới ổn định.
- **Track A, LineageOS GSI (fallback hoãn):** userspace Lineage sạch trên kernel hãng qua Treble. Không cần UART, cho máy dùng sớm, nhưng thừa hưởng lỗi HWC hãng tới khi driver display của Track B xong.

Lộ trình ở [PLAN.md](PLAN.md); chiến lược boot/debug ở [kernel/bringup-strategy.md](kernel/bringup-strategy.md).

## 6. Build

Không cần Nix; script tự cài cross toolchain trên Ubuntu / WSL.

```sh
# kernel 32-bit (mặc định), nhúng sẵn heartbeat gate-#0:
HEARTBEAT=1 bash kernel/build.sh
# -> arch/arm/boot/zImage  +  sun50i-h726-hy300ultra.dtb

# build thường, hoặc Image 64-bit tham chiếu:
bash kernel/build.sh
TARGET=arm64 bash kernel/build.sh
```

Chi tiết + các fact SoC-vs-H6 ở [kernel/README.md](kernel/README.md).

## 7. Artifact build ra

- `zImage` (ARM 32-bit, ~11 MB) đã nhúng driver CCU + pinctrl + device tree; tùy chọn heartbeat.
- `sun50i-h726-hy300ultra.dtb` (~9 KB).
- `Image` 64-bit (~41 MB) qua `TARGET=arm64`, giữ để tham chiếu (chuỗi vendor chưa boot được, xem gate #0).

Tất cả compile + build được; chưa boot (xem dưới).

## 8. Thách thức đã biết

- **Gate #0, handoff boot.** Kernel gốc AArch32, vào AArch64 trên SoC này chưa giải, nên build 32-bit. Đã xác nhận ở mức build; chưa xác nhận trên phần cứng tới khi flash heartbeat.
- **Offset register CCU / pinctrl.** Dựa thế hệ A523 + cây clock đọc trực tiếp; offset là giả thuyết tới khi boot (lý tưởng là UART) xác nhận.
- **Chưa có UART.** Bring-up mù: heartbeat GPIO + pstore thay serial. `sunxi-fel` không hỗ trợ `0x1911` nên loader là PhoenixSuit over FEL. Cáp USB-UART 3.3V là đòn bẩy lớn nhất.
- **Stack display.** `vs-display`, `tvtop`, keystone và panel là việc driver lớn còn lại; đã dịch ngược ở [re/](re/) nhưng chưa viết thành driver mainline.

## 9. Tài liệu

- **[STATUS.md](STATUS.md)** trạng thái sống (cái gì chạy, cái gì kế).
- **[PLAN.md](PLAN.md)** lộ trình, track và phase.
- **[RULE.md](RULE.md)** cách vận hành và viết.
- **[DOCS_GUIDE.md](DOCS_GUIDE.md)** chỉ mục tài liệu và kỷ luật cập nhật.
- **[re/](re/)** các note dịch ngược (mỗi driver một note, kèm cây clock).
- **[kernel/](kernel/)** artifact build-được, [build + fact SoC](kernel/README.md), [chiến lược bring-up](kernel/bringup-strategy.md), [heartbeat](kernel/heartbeat.md).

## 10. Dựa trên việc đã làm

Root, phần reverse-engineering (keystone / display, phương pháp `add_sum` cho IMAGEWTY), debloat + dọn BadBox, cùng pipeline hậu-root nằm ở repo chị em **[hoangphi01/hy300ultra-h726-allinone](https://github.com/hoangphi01/hy300ultra-h726-allinone)**. Phương pháp port mainline neo vào **[shift/sun50iw12p1-research](https://github.com/shift/sun50iw12p1-research)** (sibling H713). Repo này dựng OS trên nền đó.

## 11. Miễn trừ

Thử nghiệm. Flash thiết bị Allwinner có thể brick vĩnh viễn (dù FEL + ảnh stock giúp khôi phục đáng tin). Chỉ cho **H726 (`sun50iw15` / `0x1911`)**. Không tái phân phối firmware, blob hãng hay APK bên thứ ba; chỉ link nguồn. Không liên kết với Allwinner, Magcubic, Gojodoq hay Google.
