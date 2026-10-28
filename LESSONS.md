# Lessons (development process and toolchain)

Mistakes made while building this repo, written down so they are not repeated. These are the
**development-environment and process** lessons (Windows + WSL + the build loop). For the
**on-device hardware** failures and recoveries (thermal, FEL, PhoenixSuit verify, `/data` wipe,
sunxi-fel), see [TROUBLESHOOTING.md](TROUBLESHOOTING.md); each fact lives in one place.

## 1. WSL `bash -lc '...'` silently empties inline shell variables

`wsl.exe -- bash -lc 'K=~/path; ls $K'` intermittently expands `$K` to **empty**, so the command
runs against `/` or `''`. It has bitten this project many times (a build "ran" against an empty
path; a packer got `SOURCE=''`).

- **Fix:** do not use shell variables inside the `-lc` string. Pass **literal absolute paths**, or
  write a small wrapper **script file** in WSL and run that (an `export`/assignment inside a real
  script file is fine; only the inline `-lc` form is broken).

## 2. The Bash tool is Git-bash, and it path-mangles `/mnt/...` and `/data/...`

Git-bash (MSYS) rewrites arguments that look like Unix paths before they reach the subprocess:
`wsl.exe -- bash /mnt/c/.../build.sh` became `bash C:/Program Files/Git/mnt/c/.../build.sh`
(→ "No such file or directory", exit 127). The identical bug hit `adb push /data/local/tmp/...`
(→ `C:/Program Files/Git/data/...`, 0 bytes written).

- **Fix:** prefix the command with `MSYS_NO_PATHCONV=1`, or run adb/`dd` from **PowerShell**
  (no rewriting). For WSL, combine with lesson 1 (literal paths, or a wrapper script).

## 3. An env-var prefix passed through `wsl.exe --` may not reach the target

`wsl.exe -- env HEARTBEAT=1 bash build.sh` (and `HEARTBEAT=1 bash ...` across the interop boundary)
ran a **non-heartbeat** build while still exiting 0 - the flag was dropped, so none of the gated
work happened and the wrong artifacts were produced.

- **Fix:** put the `export` inside a WSL **wrapper script** and run the wrapper. Then (lesson 4)
  verify it actually took effect.

## 4. Exit 0 is not success - verify the real artifact

Twice a command "succeeded" (exit 0) while doing nothing useful: a build that never ran (lesson 2),
and a non-heartbeat build (lesson 3). Trusting the exit code alone wasted a round-trip each time.

- **Fix:** after a build, **grep the actual output**: the injected lines are present
  (`grep H726_BLINK head.S`), the object was built (`ls h726-checkpoint.o`), `.config` has the flags,
  the expected image size/line appears. Only then report it as done. Do not claim "it works" or
  "compiles clean" before checking the artifact, even under time pressure.

## 5. The build reuses the cached kernel tree - injections must be idempotent

`build.sh` reuses `~/h726kernel/linux-$KVER`, so patches into `head.S` / `Makefile` accumulate across
runs. A stale injection from an earlier build (the old `heartbeat-h726.S` include) lingered because
the new remove-sed did not match it.

- **Fix:** every source injection is **remove-then-add** (`sed -i '/marker/d'` first, including any
  legacy markers), and every Makefile append is `grep -q ... ||` guarded. Re-running `build.sh` must
  be safe and converge to the same tree.

## 6. Validate a signal moves something observable BEFORE building a beacon on it

For the no-UART bring-up we built a kernel beacon that steps the **focus motor** to make the
projected image "breathe", then flashed it - and saw nothing. Only afterwards did we learn (from the
printed manual and live probing) that **focus on this model is manual** (a physical focus wheel), so
the autofocus motor does not visibly change the image, and the motor is inaudible over the fan. The
beacon could never have been seen; the flash + recovery cycle (and a `/data` wipe) was wasted.

- **Fix / rule:** before committing a kernel beacon to a signal, confirm from the running device that
  driving that hardware produces a change a human can actually perceive, under real conditions (fan
  on, room lighting). Full write-up and the signal table are in [re/bringup-signals.md](re/bringup-signals.md).

## 7. Reading a vendor driver's sysfs attributes can reboot the device

Probing `/sys/class/prj/*` (the vendor `hidtvreg`/projector class: `bl_duty`, temps, `lcdadc`, ...)
**rebooted the device**, reproduced twice. The read handlers do a fragile hardware/I2C/ADC access
that faults the kernel. A `cat` is not free on a vendor BSP.

- **Fix / rule:** treat vendor sysfs as potentially destructive on read. Pull hardware facts from the
  **static device tree** (`/sys/firmware/fdt`, `/proc/device-tree`) - which is just reading an FDT
  blob and is safe - not from the driver's attribute handlers. When a probe coincides with a reboot,
  stop and change method rather than retrying.

## 8. Pointers to the on-device lessons (one home each, in TROUBLESHOOTING.md)

Not repeated here, but part of the same hard-won set:

- PhoenixSuit rejects an edited image unless the per-partition `add_sum` verify file is recomputed;
  only a **minimal-diff** boot-only edit is accepted (never touch `dlinfo`).
- A **full** PhoenixSuit recovery flash **wipes `/data`** (AVB/encryption mismatch force-formats it);
  a **boot-only** minimal-diff or `dd` flash leaves `/data` intact.
- **Never drive the cooling fan** in a bring-up test (the panel overheats in ~15 s); use the focus
  motor. The kernel proper is safe because the bring-up DT keeps `pwm-fan` disabled.
- Hardware FEL entry is the recessed **FLASH button next to HDMI** (paperclip + power), not Vol+ IR;
  `adb reboot efex` needs a booted Android, which a hung test kernel never reaches.
