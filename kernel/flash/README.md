# Flashing the heartbeat test (step by step)

How to put the 32-bit `zImage` (with the gate-#0 red-LED heartbeat) onto the device and read the result. The goal of this run is one yes/no answer: **does the vendor U-Boot enter our kernel?** See [../bringup-strategy.md](../bringup-strategy.md) and [../heartbeat.md](../heartbeat.md) for the why.

This touches only `boot_a`. `/data`, the rooted system and the calibration partition are left untouched, and FEL + the stock image recover anything, so it cannot brick the device.

## What you need

- WSL Ubuntu with the build set up (the same one `kernel/build.sh` uses).
- The stock IMAGEWTY (for packing and for recovery): `HY300Ultra_H726_Android14_STOCK_Magcubic.img`.
- PhoenixSuit V1.19 (the one used to root the device).
- The USB-C to USB-A cable (for FEL / adb).
- A dark room: the heartbeat is a red LED blink, easier to see with the lamp dim.

## Step 1 - build the zImage with the heartbeat

```sh
cd <h726-customOS>
HEARTBEAT=1 bash kernel/build.sh
# produces  ~/h726kernel/linux-6.16.7/arch/arm/boot/zImage  (32-bit, heartbeat in the decompressor)
```

## Step 2 - pack it into a flashable image (boot_a only)

```sh
python3 kernel/flash/pack_heartbeat_boot.py \
    /path/to/HY300Ultra_H726_Android14_STOCK_Magcubic.img \
    ~/h726kernel/linux-6.16.7/arch/arm/boot/zImage \
    boot_hb.img
```

The script prints its checks. It must say **`add_sum(stock boot) ... MATCH`** before it writes anything; if it says MISMATCH it aborts and nothing is flashed. The output `boot_hb.img` is a full IMAGEWTY whose dlinfo lists **only `boot_a`**, so PhoenixSuit writes only that partition.

## Step 3 - flash with PhoenixSuit over FEL

The same flow that flashed the rooted image:

1. Put the device in FEL: `adb reboot efex` (or hold Vol+ on the IR remote at power-on).
2. Open PhoenixSuit V1.19, choose `boot_hb.img` as the firmware.
3. Click Upgrade, confirm "Do Mandatory Upgrade?", then replug USB / power to trigger. PhoenixSuit writes `boot_a` only (seconds, not the full 2 GB).

## Step 4 - watch

Power the device on and watch the red LED in the first second or two.

| What you see | What it means | Next |
|---|---|---|
| Red LED blinks ~5 times at power-on | **Gate #0 passed.** U-Boot entered our 32-bit kernel; the CPU is running it. Branch 1 is correct. | Move on to the CCU/pinctrl offsets. |
| No blink, device does not reach Android | Either the pin guess is wrong, or U-Boot did not enter the kernel. | Recover (below), then retarget the pin and retry. |
| Device boots Android as normal | Our kernel probably was not entered (U-Boot fell through). | Recover, re-check the pack step. |

The device will **not** boot into Android with our kernel (the CCU is not corrected yet, so the kernel hangs after the heartbeat). That is expected. The heartbeat is the whole signal for this run.

## Recovery (always available)

Re-flash the stock `boot_a`, or the whole stock image, over FEL:

- Quick: re-run the pack step but point it at an **unmodified** stock `zImage`... simpler, just flash the full stock IMAGEWTY in PhoenixSuit (the `02_STOCK_firmware_unbrick` image). That restores `boot_a` (and everything), and the device boots the rooted Android again (data and calibration are preserved by the stock flash, as when rooting).

## If it does not blink: retarget the pin

The red LED is `PL0` as a best guess from the H713 sibling; this unit may differ. Edit [../heartbeat/heartbeat-h726.S](../heartbeat/heartbeat-h726.S) (the register math and alternative pins are in [../heartbeat.md](../heartbeat.md)), rebuild Step 1, and retry. Each attempt is safe.

The real accelerator remains a 3.3V USB-UART on the debug pads: with `earlycon` the kernel prints from its assembly head, so you see exactly where it is instead of guessing from a blink.
