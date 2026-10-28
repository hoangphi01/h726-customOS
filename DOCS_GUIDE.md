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
