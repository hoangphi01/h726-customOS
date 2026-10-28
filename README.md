<div align="center">

# h726-customOS

### A lean, trustworthy custom OS for the Allwinner H726 (sun50iw15) HY300 Ultra projector

**English** · [Tiếng Việt](#tiếng-việt)

![SoC](https://img.shields.io/badge/SoC-Allwinner%20H726%20(sun50iw15)-blue)
![FEL ID](https://img.shields.io/badge/FEL%20ID-0x1911-informational)
![Android](https://img.shields.io/badge/base-Android%2014-green)
![Status](https://img.shields.io/badge/status-kernel%20bring--up%20(B2)-orange)
![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)
![Affiliation](https://img.shields.io/badge/affiliation-none-lightgrey)

</div>

---

## 1. What this is

An effort to build a clean custom OS for the Magcubic / Gojodoq **HY300 Ultra** projector, powered by the **Allwinner H726 (`sun50iw15p1`, FEL id `0x1911`)**: Android 14, kernel 5.15, 32-bit `armeabi-v7a`, about 1 GB RAM, Mali-G31, AIC8800 Wi-Fi, 720p, DRM/KMS display with no MIPS co-processor.

The stock firmware is a white-label clone carrying the BadBox malware family and a heavy vendor framework. This project replaces its userspace, and if feasible its kernel, with a clean build.

## Progress

Where this is, most recent first. Track B (a from-source kernel) is the primary focus; Track A (a LineageOS GSI) is a deferred fallback. Full detail in [STATUS.md](STATUS.md).

| Phase | What | State |
|---|---|---|
| Phase 0 | Recon: Treble confirmed, device facts, the two-track verdict | Done |
| B1 | Reverse-engineer the display and keystone drivers into register maps ([re/](re/)) | Done, 8 notes |
| B2 | A buildable kernel base: board device tree, `defconfig`, build script; builds mainline 6.16.7 into a kernel `Image` + DTB ([kernel/](kernel/)) | Done, compiles and builds |
| B2 | sun50iw15 CCU + pinctrl drivers | In progress, core subset compiles |
| B3 | Boot bring-up (blind via pstore / USB dmesg; no UART yet) | Not started |
| Track A | LineageOS GSI (deferred fallback) | Not started |

**Builds today:** the device tree, the config, and our sun50iw15 CCU and pinctrl drivers, all into a mainline 6.16.7 kernel `Image` via `kernel/build.sh`. **Not working yet:** booting, which needs the CCU/pinctrl register offsets confirmed on hardware and, ideally, a serial console. The reverse-engineering groundwork (the display/keystone register maps) is what makes the driver work possible.

## 2. Why

- **Smooth on 1 GB.** The stock vendor framework is bloated; a lean userspace is the only way this device runs well on 1 GB RAM.
- **Trustworthy.** The stock userspace carries BadBox (fake Play Store, setuid backdoor, telemetry), and the vendor kernel is an unaudited binary. The goal is a clean, auditable system.
- **Projector-first.** Keep the reverse-engineered keystone and autofocus stack working; drop everything else.

## 3. Goals

- **Lean:** fits and runs well on 1 GB (low-RAM config, tuned zram).
- **Clean userspace:** LineageOS, not MagcubicOS / BigDroid.
- **Trustworthy:** strip BadBox; aim for a from-source kernel.
- **Projector features preserved:** the reverse-engineered keystone / autofocus / projection-mode logic.
- **Sensible defaults:** Aurora Store as the store, SmartTube bundled, PhairPlay integrated as the default cast receiver.

## 4. Approach (two tiers)

- **Tier A, LineageOS GSI over Treble:** a clean Lineage userspace running on the vendor kernel and `/vendor` blobs via Project Treble. No blob extraction or kernel building; the hardware keeps working while the vendor framework is replaced. The projector apps are re-added on top.
- **Tier B, from-source port:** a device tree, a kernel rebuilt from the Allwinner BSP plus the extracted DTB and config, blobs assembled into `/vendor`, and targeted reverse-engineering of the few out-of-tree drivers. A clean, auditable kernel.

The full roadmap is in [PLAN.md](PLAN.md).

## 5. Built on prior work

The root, the reverse-engineering (keystone / display subsystem, the `add_sum` IMAGEWTY method), the debloat and BadBox teardown, and the post-root pipeline live in the sibling repository **[hoangphi01/hy300ultra-h726-allinone](https://github.com/hoangphi01/hy300ultra-h726-allinone)**. This repository builds the OS on that foundation.

## 6. Docs

- **[PLAN.md](PLAN.md)** the roadmap, phases, and open questions.
- **[RULE.md](RULE.md)** how this project is run and written.
- **[STATUS.md](STATUS.md)** where we are, what works, what is untested.

## 7. Disclaimer

Experimental. Flashing an Allwinner device can permanently brick it. For the **H726 (`sun50iw15` / `0x1911`)** only. No proprietary firmware, vendor blobs, or third-party APKs are redistributed here; sources are linked. Not affiliated with Allwinner, Magcubic, Gojodoq or Google.

---

<div align="center">

## Tiếng Việt

</div>

## 1. Đây là gì

Dự án dựng một **custom OS sạch** cho máy chiếu **HY300 Ultra** (Magcubic / Gojodoq) chạy **Allwinner H726 (`sun50iw15p1`, FEL `0x1911`)**: Android 14, kernel 5.15, 32-bit `armeabi-v7a`, khoảng 1 GB RAM, Mali-G31, Wi-Fi AIC8800, 720p, hiển thị DRM/KMS không có co-processor MIPS.

Firmware gốc là bản clone white-label dính họ malware BadBox và framework hãng nặng nề. Dự án này thay userspace, và nếu khả thi là cả kernel, bằng một bản build sạch.

## Tiến độ

Đang ở đâu, mới nhất lên đầu. Track B (kernel từ source) là trọng tâm; Track A (LineageOS GSI) là fallback hoãn lại. Chi tiết ở [STATUS.md](STATUS.md).

| Giai đoạn | Việc | Trạng thái |
|---|---|---|
| Phase 0 | Recon: xác nhận Treble, thông số máy, chốt hai track | Xong |
| B1 | Dịch ngược driver display + keystone thành register map ([re/](re/)) | Xong, 8 note |
| B2 | Nền kernel build-được: device tree, `defconfig`, build script; build mainline 6.16.7 ra kernel `Image` + DTB ([kernel/](kernel/)) | Xong, compile + build |
| B2 | Driver CCU + pinctrl sun50iw15 | Đang làm, core subset compile |
| B3 | Bring-up boot (mù qua pstore / USB dmesg; chưa có UART) | Chưa bắt đầu |
| Track A | LineageOS GSI (fallback hoãn) | Chưa bắt đầu |

**Build được hiện tại:** device tree, config, và driver CCU + pinctrl sun50iw15 của ta, tất cả biên dịch vào kernel `Image` mainline 6.16.7 qua `kernel/build.sh`. **Chưa chạy:** boot, cần xác nhận offset register CCU/pinctrl trên phần cứng và lý tưởng là có serial console. Phần RE nền (register map display/keystone) chính là cái làm việc viết driver trở nên khả thi.

## 2. Vì sao

- **Mượt trên 1 GB.** Framework hãng quá nặng; chỉ userspace tinh gọn mới chạy tốt trên 1 GB RAM.
- **Đáng tin.** Userspace gốc dính BadBox (Play Store giả, backdoor setuid, telemetry), kernel hãng là nhị phân chưa kiểm toán. Mục tiêu là hệ sạch, kiểm toán được.
- **Ưu tiên máy chiếu.** Giữ nguyên stack keystone và autofocus đã dịch ngược; bỏ hết phần còn lại.

## 3. Mục tiêu

- **Gọn:** vừa và chạy tốt trên 1 GB (cấu hình low-RAM, zram).
- **Userspace sạch:** LineageOS, không phải MagcubicOS / BigDroid.
- **Đáng tin:** vứt BadBox; hướng tới kernel từ source.
- **Giữ tính năng máy chiếu:** logic keystone / autofocus / chế độ chiếu đã reverse.
- **Default hợp lý:** Aurora Store làm store, bundle SmartTube, PhairPlay làm receiver cast mặc định.

## 4. Hướng đi (hai nấc)

- **Nấc A, LineageOS GSI qua Treble:** userspace Lineage sạch chạy trên kernel + blob `/vendor` của hãng qua Project Treble. Không phải đào blob hay build kernel; phần cứng vẫn chạy trong khi framework hãng bị thay. App máy chiếu gắn lại lên trên.
- **Nấc B, port từ source:** dựng device tree, rebuild kernel từ BSP Allwinner cộng DTB và config trích xuất, gom blob vào `/vendor`, và dịch ngược có mục tiêu vài driver out-of-tree. Kernel sạch, kiểm toán được.

Lộ trình đầy đủ ở [PLAN.md](PLAN.md).

## 5. Dựa trên việc đã làm

Root, phần reverse-engineering (subsystem keystone / display, phương pháp `add_sum` cho IMAGEWTY), debloat và dọn BadBox, cùng pipeline hậu-root nằm ở repo chị em **[hoangphi01/hy300ultra-h726-allinone](https://github.com/hoangphi01/hy300ultra-h726-allinone)**. Repo này dựng OS trên nền đó.

## 6. Tài liệu

- **[PLAN.md](PLAN.md)** lộ trình, các giai đoạn, câu hỏi mở.
- **[RULE.md](RULE.md)** cách vận hành và viết.
- **[STATUS.md](STATUS.md)** đang ở đâu, cái gì chạy, cái gì chưa test.

## 7. Miễn trừ

Thử nghiệm. Flash thiết bị Allwinner có thể brick vĩnh viễn. Chỉ cho **H726 (`sun50iw15` / `0x1911`)**. Không tái phân phối firmware, blob hãng hay APK bên thứ ba; chỉ link nguồn. Không liên kết với Allwinner, Magcubic, Gojodoq hay Google.
