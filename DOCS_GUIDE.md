# DOCS_GUIDE

The map of this repo's documentation: what each file is for, and when to update it. The point is to keep the docs from drifting out of sync with reality (which is easy to let happen, e.g. a "Next" list still showing a phase that is already done). Read this before editing docs; follow [RULE.md](RULE.md) for how to write them.

## The documents

| File | Purpose | Update when |
|---|---|---|
| **[README.md](README.md)** | The front door. What/why/goals, the two tracks, and a **Progress table** (EN + VI). | Every milestone: keep the Progress table and the status badge current. Keep EN and VI in sync. |
| **[STATUS.md](STATUS.md)** | The **living current state**. The `Phase:` line, "What is working", and "Next" are the truth of where we are. | **Every working session that changes reality.** This is the one that drifts first; see the rule below. |
| **[PLAN.md](PLAN.md)** | The **roadmap**: tracks, phases, approach, open questions. The intended path, not the current position. | When the direction changes (a track reprioritised, a phase reshaped, a fork decided). Not for day-to-day progress. |
| **[RULE.md](RULE.md)** | How this project is run and written (commit rules, no redistribution, tone, redaction). | Rarely, when a working rule changes. |
| **[DOCS_GUIDE.md](DOCS_GUIDE.md)** | This file: the doc index and update discipline. | When a document is added, removed, or repurposed. |
| **[TROUBLESHOOTING.md](TROUBLESHOOTING.md)** | On-device hard failures and recoveries (thermal, FEL button, PhoenixSuit verify, `/data`, flashing). | When a new on-device failure mode is hit and solved. |
| **[LESSONS.md](LESSONS.md)** | Development-process and toolchain mistakes (WSL var bug, Git-bash path mangling, verify-the-artifact, idempotent injections). Points to TROUBLESHOOTING for the device ones. | Whenever a dev-environment or process mistake is made and corrected, so it is not repeated. |
| **[re/](re/)** | Reverse-engineering notes, one per driver, plus [re/README.md](re/README.md) (the index) and the clock tree. Append-mostly reference. | When a driver is reversed or a finding corrected. Update [re/README.md](re/README.md)'s table too. |
| **[kernel/](kernel/)** | The buildable artifacts (DTS, defconfig, drivers, scripts) and their docs: [kernel/README.md](kernel/README.md) (the build + the SoC-vs-H6 facts), [kernel/bringup-strategy.md](kernel/bringup-strategy.md) (the boot gates and debug plan), [kernel/heartbeat.md](kernel/heartbeat.md). | When an artifact is added or a kernel fact/decision changes. |

## The anti-drift rule

Reality lives in three places that must agree. After any session that builds, decides, or discovers something, update **all three** before committing:

1. **STATUS.md** - the `Phase:` line, the "What is working" list, and the "Next" checklist (tick what is done, add what is now current).
2. **README.md** - the Progress table (EN and VI) and the badge.
3. The relevant **sub-doc** (a `re/` note, `kernel/README.md`, `bringup-strategy.md`) if the change is technical.

A quick self-check when editing STATUS: does "What is working" match what actually compiles/boots today? Is the first unchecked item in "Next" really the next thing to do, not something already finished? If a `Phase:` line and a "Next" list disagree, the doc has drifted.

**Record mistakes as they happen.** When a development-environment or process mistake is made and corrected (a toolchain gotcha, a wrong assumption, a wasted round-trip), add it to [LESSONS.md](LESSONS.md); when it is an on-device failure, add it to [TROUBLESHOOTING.md](TROUBLESHOOTING.md). Verify a build's real artifacts (grep the injection, check `.config`, check sizes) before reporting it done - exit 0 is not success.

## The H713 is a SIBLING, not the H726 - verify everything borrowed

This project leans heavily on the H713 (sun50iw12) HY300 work - above all **well0nez/allwinner-h713-linux**, plus cstenger, srgneisner, shift - because there is no mainline for our H726 (sun50iw15) and the two SoCs are one step apart. That borrowing is the right strategy, but it is also the single biggest source of wrong facts in this repo, so it gets its own rule:

**Every hardware fact taken from the H713 (or any sibling - H6, H616, A523, D1) is a HYPOTHESIS for the H726 until confirmed against our own H726 ground truth. Label it as such in the docs, and never state a sibling-derived address, offset, key, bit, or clock as an H726 fact without that confirmation.**

- **Write it as a hypothesis.** Use wording like "H713-derived, unverified on H726" or "A523-generation hypothesis" next to any value borrowed from a sibling. Cite where it came from. When it is later confirmed (or corrected) on H726, update the wording to say so and how it was confirmed.
- **What counts as H726 ground truth** (authoritative, in rough order): our extracted vendor device tree (the decompiled `system.dts` / `/proc/device-tree` / `/sys/firmware/fdt` of this device), the live `clk`/`gpio` debugfs and `/proc/config.gz` read off the rooted device, and the vendor's own `.ko` drivers. Prefer these over any sibling doc. The CCU clock tree in [re/ccu-sun50iw15.md](re/ccu-sun50iw15.md), for example, is read live off *this* device; its register offsets, however, are still an A523 hypothesis and are marked so.
- **The burned example (keep it visible):** the watchdog. We took well0nez's H713 register map (MODE `+0x14`, key `0x16aa`, bases `0x02051000`/`0x07020400`) and applied it to the H726. On hardware the main-WDT disarm had **no effect** and the R_WDOG write **faulted**, and because our vendor watchdog compatible is `allwinner,wdt-v103` (the H713's is `allwinner,sun50i-wdt`), we cannot even be sure the H726 layout matches - so "no effect" might mean the disarm is a no-op on H726, not that the WDT is innocent. A sibling map that is almost right is worse than none, because it fails silently. See [re/bringup-signals.md](re/bringup-signals.md).
- **How to confirm a borrowed value on H726:** find the same node/register in our extracted vendor DTS; read it live on the rooted device where possible (clocks, gpio); disassemble the vendor `.ko` that drives it; or, once a UART is attached, watch what the stage actually does. Only then promote it from hypothesis to fact.

## What goes where (so facts have one home)

- **Current position** (what works today, what is next): STATUS.md only. Do not restate it in PLAN.md.
- **The plan / intended path**: PLAN.md only.
- **How a piece of hardware works** (registers, clocks, pins): the matching `re/` note; STATUS/kernel link to it rather than copy it.
- **How to build or flash**: `kernel/` (README, build.sh, heartbeat.md, bringup-strategy.md).
- **Historical recon** (the Phase-0 gold-mine dump in STATUS) stays as background and is marked as such; it is not edited to look current.

## Conventions (summary; full rules in RULE.md)

- Commits: follow RULE.md (no AI co-author line, no em dashes, cite sources, limit emoji).
- No redistribution of vendor firmware, blobs, or third-party APKs; link sources, keep extracted artifacts local (they are in `.gitignore`).
- Redact the secrets listed in RULE.md from anything public.
- Bilingual where the audience is the user (README); technical sub-docs are English to match the code.
- **Never present a sibling-SoC (H713/H6/H616/A523/D1) value as an H726 fact.** Label it a hypothesis and cite the source until confirmed on our own H726 ground truth (see the H713-is-a-sibling rule above).
