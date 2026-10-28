# The gate-#0 GPIO heartbeat

A tiny GPIO tick inserted at the very start of the 32-bit `zImage` to answer one question with no serial console: **did the vendor U-Boot actually enter our mainline kernel and start executing it?** (See [bringup-strategy.md](bringup-strategy.md) for why this is gate #0.)

## Result: gate #0 is PASSED (2026-10-28)

This worked. Flashed to `boot_a` and powered on, the heartbeat ran on the real device, so the vendor U-Boot enters our mainline 32-bit kernel and the CPU executes it. **Branch 1 (32-bit) is validated on hardware.** The heartbeat has done its job; it is kept here for reference and for re-use if the early boot ever needs re-checking.

## !!! Safety: never drive the cooling fan !!!

The first version blinked `fan-power-gpio` (PL7). It proved gate #0 (the fan cut out, so our code had run), but it turned the fan **off** while the vendor U-Boot splash was still being projected (light engine on), and after ~15 s the imager panel overheated (a dark bloom spread from the centre of the image). Never touch the fan, and never leave the device hung with the light engine on and no cooling. The signal is now the **focus motor**, which has no thermal role.

## What it does

[heartbeat/heartbeat-h726.S](heartbeat/heartbeat-h726.S) is `#include`d into `arch/arm/boot/compressed/head.S`, right where the decompressor begins, **before any clock or CCU init**. The MMU is off there, so it pokes the GPIO registers directly, needing no clock:

- **Ticks the focus motor (phase 0 = PH13) eight times.** Main PIO `0x02000000`, PH bank base `0x020000FC`, `CFG1` at `+0x04` (set PH13 to output), `DAT` at `+0x10` bit 13. An audible click / small lens move each tick.

It uses only `r3`-`r6` and preserves `r0`-`r2` (the U-Boot machine id / DTB pointer), so it does not disturb the boot. The pin numbers are confirmed from the running device's GPIO map (`fan-power-gpio` on PL7, the four `motor-gpio-phaseN` on PH).

## Build it

```sh
HEARTBEAT=1 bash kernel/build.sh
# -> arch/arm/boot/zImage (with the heartbeat built into the decompressor)
```

(Verified to compile into the `zImage`.)

## Flash and observe

Pack the `zImage` into the `boot_a` partition and flash it the same proven way the device was rooted: byte-copy the stock IMAGEWTY, replace only the `boot_a` payload with the new `zImage` (keep the ramdisk), recompute the per-partition `add_sum` verify-file, and flash via PhoenixSuit over FEL (`adb reboot efex`). Keep the stock image to reflash. Then power on and watch the red LED.

## Reading the result

| Observation | Meaning |
|---|---|
| Red LED blinks ~5 times at power-on | **Gate #0 passed.** U-Boot entered the 32-bit kernel and the CPU is running it. Branch 1 (32-bit) is the right path, and the next job is the CCU offsets. |
| No blink (and the device does not boot stock either) | Either the guess is wrong (see below) or U-Boot did not enter the kernel. Reflash stock, then retarget and retry. |
| Device boots as before / LED behaves normally | The kernel was probably not entered (U-Boot fell through to its normal path). Check the boot partition packing. |

Nothing here can brick the device: FEL stays available and the stock IMAGEWTY reflashes over PhoenixSuit; the reserve/calibration partition is skipped.

## If it does not blink: retarget

The PL0 red LED is a best guess from the HY300 sibling (the H713), so the pin may differ on this unit. In `heartbeat-h726.S`, change the target:

- **Different LED / pin on the R_PIO (PL) bank:** keep the base `0x07022000`, change the `CFG0` nibble and the `DAT` bit to the pin number (pin N uses `CFG(N/8)` nibble `(N%8)*4` and `DAT` bit N).
- **A pin on the main PIO** (banks PA-PH, e.g. a motor phase on PH): base `0x02000000`, bank pitch `0x24` (PH = bank 7 -> `0x020000FC`), same `CFG`/`DAT` layout.
- The fan is loud and drowns out the focus motor, so an LED is the clearest indicator on this unit.

The loop delay (`0x00300000`) can be made longer or shorter to change the blink speed.

## The real unlock

A 3.3V USB-UART on the debug pads makes this whole file unnecessary: with `earlycon`, the kernel prints from its assembly head, so gate #0 and every later stage are visible as text, not guessed from a blink.
