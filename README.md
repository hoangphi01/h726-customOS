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
| B3 | Boot past the vendor **watchdog** (an external ~2 s reset, now diagnosed as the sun55i-A523 key-protected watchdog), then walk the kernel to init with GPIO fan beacons and fix the CCU/pinctrl offsets ([re/bringup-signals.md](re/bringup-signals.md)) | Gate #0 passed; ~2 s watchdog diagnosed; next = disarm it with the `0x16aa` key |
| B4 | Driver bring-up loop (display, keystone, Wi-Fi) from the B1 maps | Not started |
| Track A | LineageOS GSI (deferred fallback) | Not started |

## Hard-won lessons (read before flashing)

Full detail and a troubleshooting table in **[TROUBLESHOOTING.md](TROUBLESHOOTING.md)**.

> [!CAUTION]
> **The cooling fan is the only usable beacon, so pulse it safely - never hold it off.** The light engine is on during the U-Boot splash; a test that turned the fan off while the kernel hung overheated the imager panel in ~15 s (a dark bloom from the centre; it recovered once cool). **The focus motor cannot be used as a signal - focus on this unit is a MANUAL wheel**, so the autofocus motor does not move the image and is inaudible over the fan (the LED is not a SoC GPIO; reading the vendor backlight reboots the device). So the fan is unavoidable: every pulse must turn it off only briefly and **always** back on (resting state fan ON), and you cut power the instant the fan goes fully silent. A UART would remove the need entirely.

> [!WARNING]
> **If a test kernel hangs, `adb reboot efex` cannot reach FEL - use the hardware button.** The FEL entry on this unit is a **recessed FLASH button next to the HDMI port**: hold it with a paperclip while applying power (the PC then shows `1f3a:efe8`). Vol+ on the IR remote does **not** work. Know where this button is before you flash.

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
# 32-bit kernel (default), with the B3 checkpoint beacons + appended DTB:
HEARTBEAT=1 bash kernel/build.sh
# -> arch/arm/boot/zImage-dtb  (zImage + appended bring-up DTB; FLASH THIS)

# plain build, or the 64-bit reference Image:
bash kernel/build.sh
TARGET=arm64 bash kernel/build.sh
```

See [kernel/README.md](kernel/README.md) for the details and the SoC-vs-H6 facts.

## 7. Build artifacts

- `zImage` (32-bit ARM, ~11 MB) with our CCU + pinctrl drivers and device tree built in; under `HEARTBEAT=1` also `zImage-dtb` (zImage + appended bring-up DTB + checkpoint beacons), the image to flash for B3.
- `sun50i-h726-hy300ultra.dtb` (~9 KB) and the minimal `-bringup.dtb` (~2 KB).
- a 64-bit `Image` (~41 MB) via `TARGET=arm64`, kept for reference (not bootable by the vendor chain, see gate #0).

Everything compiles and builds; nothing boots yet (see below).

## 8. Known challenges

- **Gate #0, the boot handoff.** The stock kernel is AArch32 and entering AArch64 on this SoC is unsolved, so we build 32-bit. **Passed on hardware (2026-10-28):** U-Boot enters our 32-bit kernel.
- **CCU / pinctrl register offsets.** Modelled on the A523 generation and the live clock tree; the offsets are a hypothesis until a boot confirms them. The B3 bring-up de-risks this (the beacon does not depend on the CCU, which is registered without `CLK_IS_CRITICAL`).
- **No UART yet.** Bring-up is blind: a ladder of GPIO **checkpoint beacons** (and pstore for later stages) stands in for serial. `sunxi-fel` does not support `0x1911`, so the loader is PhoenixSuit over FEL. A 3.3V USB-UART adapter is the single biggest accelerator.
- **The display stack.** `vs-display`, `tvtop`, the keystone and the panel are the real remaining driver work; they are reverse-engineered in [re/](re/) but not yet written as mainline drivers.

## 9. Documentation

- **[STATUS.md](STATUS.md)** the living current state (what works, what is next).
- **[PLAN.md](PLAN.md)** the roadmap, tracks and phases.
- **[RULE.md](RULE.md)** how the project is run and written.
- **[DOCS_GUIDE.md](DOCS_GUIDE.md)** the documentation index and update discipline.
- **[TROUBLESHOOTING.md](TROUBLESHOOTING.md)** on-device failures and how to recover (thermal, FEL button, verify, `/data`, flashing).
- **[LESSONS.md](LESSONS.md)** development-process and toolchain mistakes, so they are not repeated.
- **[re/](re/)** the reverse-engineering notes (one per driver, plus the clock tree).
- **[kernel/](kernel/)** the buildable artifacts, [the build + SoC facts](kernel/README.md), [the bring-up strategy](kernel/bringup-strategy.md), and [the checkpoint beacons](kernel/heartbeat.md).

## 10. Built on prior work, and gratitude

The root, the reverse-engineering (keystone / display subsystem, the `add_sum` IMAGEWTY method), the debloat and BadBox teardown, and the post-root pipeline live in the sibling repository **[hoangphi01/hy300ultra-h726-allinone](https://github.com/hoangphi01/hy300ultra-h726-allinone)**. This repository builds the OS on that foundation.

**The H726 (sun50iw15) is one SoC step from the H713 (sun50iw12), so this port stands on the shoulders of the H713 HY300 community.** With real gratitude:

- **[well0nez/allwinner-h713-linux](https://github.com/well0nez/allwinner-h713-linux)** (on the base of **[cstenger/allwinner-h713-mainline](https://github.com/cstenger/allwinner-h713-mainline)**) - the one that *actually boots mainline Linux daily* on the H713, with a full open boot chain (SPL + TF-A + U-Boot) and a working driver stack (Panfrost, Cedrus, AIC8800, keystone). Their careful reverse-engineering - above all of the **sun55i-A523 key-protected watchdog** - is precisely what let us diagnose our own hardest boot blocker. This project is the lodestar and the inspiration; much of our road ahead is to adapt their H713 work to the H726.
- **[srgneisner/hy300-linux-porting](https://github.com/srgneisner/hy300-linux-porting)** - a hardware-first effort whose live vendor device-tree dump is ground truth for our UART and watchdog nodes.
- **[shift/sun50iw12p1-research](https://github.com/shift/sun50iw12p1-research)** - the original H713 research and the `sunxi-fel` SoC-table pattern we will follow for `0x1911`.

Standing on this prior art is the whole linux-sunxi way: nobody gets the factory BSP, so each new sibling SoC reaches mainline on the generosity of the one before it. Thank you.

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
| B3 | Vượt **watchdog** vendor (cú reset ngoài ~2s, nay đã chẩn đoán = watchdog layout A523 có key), rồi đưa kernel tới init bằng beacon quạt + chỉnh offset CCU/pinctrl ([re/bringup-signals.md](re/bringup-signals.md)) | Gate #0 qua; watchdog ~2s đã chẩn đoán; kế = tắt nó bằng key `0x16aa` |
| B4 | Vòng bring-up driver (display, keystone, Wi-Fi) từ map B1 | Chưa bắt đầu |
| Track A | LineageOS GSI (fallback hoãn) | Chưa bắt đầu |

## Bài học xương máu (đọc trước khi flash)

Chi tiết + bảng troubleshoot ở **[TROUBLESHOOTING.md](TROUBLESHOOTING.md)**.

> [!CAUTION]
> **Quạt là tín hiệu beacon DUY NHẤT dùng được, nên phải xung AN TOÀN - không bao giờ giữ tắt lâu.** Đèn chiếu BẬT lúc splash U-Boot; một lần test tắt quạt trong khi kernel treo đã làm **panel quá nhiệt trong ~15 giây** (vệt đen loang từ tâm; nguội thì hồi). **Motor KHÔNG dùng làm tín hiệu được - máy này lấy nét bằng VÒNG XOAY TAY**, nên motor autofocus không làm đổi ảnh và bị quạt át tiếng (đèn LED không phải GPIO của SoC; đọc backlight vendor thì reboot máy). Vậy buộc phải dùng quạt: mỗi xung chỉ tắt NGẮN rồi **luôn** bật lại (trạng thái nghỉ = quạt BẬT), và NGẮT ĐIỆN ngay khi quạt im hẳn. Có UART thì khỏi cần đụng quạt.

> [!WARNING]
> **Kernel test treo thì `adb reboot efex` KHÔNG vào được FEL - phải dùng nút cứng.** FEL của máy này là **nút FLASH ẩn cạnh cổng HDMI**: lấy kẹp giấy ấn giữ **trong lúc cấp nguồn** (PC hiện `1f3a:efe8`). **Vol+ trên remote IR KHÔNG ăn.** Nhớ vị trí nút này trước khi flash.

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
# kernel 32-bit (mặc định), kèm checkpoint beacon B3 + DTB ghép đuôi:
HEARTBEAT=1 bash kernel/build.sh
# -> arch/arm/boot/zImage-dtb  (zImage + bring-up DTB; FLASH FILE NÀY)

# build thường, hoặc Image 64-bit tham chiếu:
bash kernel/build.sh
TARGET=arm64 bash kernel/build.sh
```

Chi tiết + các fact SoC-vs-H6 ở [kernel/README.md](kernel/README.md).

## 7. Artifact build ra

- `zImage` (ARM 32-bit, ~11 MB) đã nhúng driver CCU + pinctrl + device tree; với `HEARTBEAT=1` còn có `zImage-dtb` (zImage + DTB bring-up ghép đuôi + checkpoint beacon), là file để flash cho B3.
- `sun50i-h726-hy300ultra.dtb` (~9 KB) và bản tối giản `-bringup.dtb` (~2 KB).
- `Image` 64-bit (~41 MB) qua `TARGET=arm64`, giữ để tham chiếu (chuỗi vendor chưa boot được, xem gate #0).

Tất cả compile + build được; chưa boot (xem dưới).

## 8. Thách thức đã biết

- **Gate #0, handoff boot.** Kernel gốc AArch32, vào AArch64 trên SoC này chưa giải, nên build 32-bit. **Đã qua trên phần cứng (2026-10-28):** U-Boot vào kernel 32-bit của ta.
- **Offset register CCU / pinctrl.** Dựa thế hệ A523 + cây clock đọc trực tiếp; offset là giả thuyết tới khi boot xác nhận. Bring-up B3 giảm rủi ro chỗ này (beacon không phụ thuộc CCU, và CCU đăng ký không `CLK_IS_CRITICAL`).
- **Chưa có UART.** Bring-up mù: thang **checkpoint beacon** GPIO (+ pstore cho giai đoạn sau) thay serial. `sunxi-fel` không hỗ trợ `0x1911` nên loader là PhoenixSuit over FEL. Cáp USB-UART 3.3V là đòn bẩy lớn nhất.
- **Stack display.** `vs-display`, `tvtop`, keystone và panel là việc driver lớn còn lại; đã dịch ngược ở [re/](re/) nhưng chưa viết thành driver mainline.

## 9. Tài liệu

- **[STATUS.md](STATUS.md)** trạng thái sống (cái gì chạy, cái gì kế).
- **[PLAN.md](PLAN.md)** lộ trình, track và phase.
- **[RULE.md](RULE.md)** cách vận hành và viết.
- **[DOCS_GUIDE.md](DOCS_GUIDE.md)** chỉ mục tài liệu và kỷ luật cập nhật.
- **[TROUBLESHOOTING.md](TROUBLESHOOTING.md)** sự cố trên máy + cách khôi phục (nhiệt, nút FEL, verify, `/data`, flash).
- **[LESSONS.md](LESSONS.md)** lỗi môi trường dev + toolchain, để không lặp lại.
- **[re/](re/)** các note dịch ngược (mỗi driver một note, kèm cây clock).
- **[kernel/](kernel/)** artifact build-được, [build + fact SoC](kernel/README.md), [chiến lược bring-up](kernel/bringup-strategy.md), [checkpoint beacon](kernel/heartbeat.md).

## 10. Dựa trên việc đã làm, và lời tri ân

Root, phần reverse-engineering (keystone / display, phương pháp `add_sum` cho IMAGEWTY), debloat + dọn BadBox, cùng pipeline hậu-root nằm ở repo chị em **[hoangphi01/hy300ultra-h726-allinone](https://github.com/hoangphi01/hy300ultra-h726-allinone)**. Repo này dựng OS trên nền đó.

**H726 (sun50iw15) chỉ cách H713 (sun50iw12) một bước SoC, nên bản port này đứng trên vai cộng đồng H713 HY300.** Xin tri ân:

- **[well0nez/allwinner-h713-linux](https://github.com/well0nez/allwinner-h713-linux)** (trên nền **[cstenger/allwinner-h713-mainline](https://github.com/cstenger/allwinner-h713-mainline)**) — người *thực sự boot được mainline Linux hàng ngày* trên H713, với chuỗi boot mở đầy đủ (SPL + TF-A + U-Boot) và stack driver chạy (Panfrost, Cedrus, AIC8800, keystone). Phần dịch ngược tỉ mỉ của họ — nhất là **watchdog layout A523 có key** — chính là thứ giúp mình chẩn đoán đúng nút thắt boot khó nhất. Đây là ngọn hải đăng và nguồn cảm hứng; phần lớn chặng đường tới là đem việc H713 của họ sang H726.
- **[srgneisner/hy300-linux-porting](https://github.com/srgneisner/hy300-linux-porting)** — hướng hardware-first, bản dump device-tree vendor của họ là ground truth cho node UART + watchdog của mình.
- **[shift/sun50iw12p1-research](https://github.com/shift/sun50iw12p1-research)** — nghiên cứu H713 gốc + mẫu bảng SoC `sunxi-fel` mà mình sẽ theo cho `0x1911`.

Đứng trên nền tiền nhân chính là tinh thần linux-sunxi: không ai có BSP gốc của hãng, nên mỗi SoC anh em tới được mainline đều nhờ lòng hào phóng của người đi trước. Cảm ơn.

## 11. Miễn trừ

Thử nghiệm. Flash thiết bị Allwinner có thể brick vĩnh viễn (dù FEL + ảnh stock giúp khôi phục đáng tin). Chỉ cho **H726 (`sun50iw15` / `0x1911`)**. Không tái phân phối firmware, blob hãng hay APK bên thứ ba; chỉ link nguồn. Không liên kết với Allwinner, Magcubic, Gojodoq hay Google.
