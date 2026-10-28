# RULE

How this project is run and written. These apply to every commit, doc, and change.

## Commits and attribution

- The commit author is the repository owner.
- Do **not** add a `Co-Authored-By` line for any AI assistant, and do not add AI generation trailers. AI help is acknowledged in prose where relevant, never as a co-author.
- Commit messages are clear and specific: what changed and why.

## Writing

- **No em dashes** anywhere. Use commas, colons, or parentheses.
- **Limit emoji.** Prose carries the meaning.
- The README is **bilingual (English and Vietnamese)**. Deeper docs are in **English**.
- **Always cite related research** with a link. Credit prior art.

## Redistribution and licensing

- Do **not** redistribute proprietary firmware, vendor blobs (`.so`, `.ko`, DTB), or third-party APKs in this repository. Link to their source instead.
- Extracted vendor material is for local build and analysis only.
- Original work in this repository is MIT. Upstream projects keep their own licenses.

## Privacy

Redact from anything public:

- the real projector IP, Wi-Fi SSID and password,
- the host username, email, and the device serial.

## Safety

- This is experimental and can brick the device. Document the brick and unbrick path for every flash step.
- Scope is the **H726 (`sun50iw15` / FEL `0x1911`)** only. Do not apply to H713 or other SoCs.
- Keep a known-good stock image for recovery at all times.
- The reserve partition (per-unit colour and keystone calibration) must survive every flash.

## Working style

- **Reversible first.** During development, prefer systemless and reversible changes (overlays, Magisk modules, user-level disables). Bake a change into the image only once it is proven.
- **Honesty in STATUS.** Record what works, what is broken, and what is untested, in the plain "What is working / Bugs" style. No claiming a thing boots until it is seen to boot.
- One source of truth: [PLAN.md](PLAN.md) is the roadmap, [STATUS.md](STATUS.md) is the current state. Update STATUS as reality changes.
