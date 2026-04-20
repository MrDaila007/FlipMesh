# Contributing to FlipMesh

## Before you start

- This project targets **Flipper Zero** hardware and the **Meshtastic** protocol — test on real hardware when possible
- All code must compile cleanly with `ufbt build` before opening a PR
- Keep the embedded constraints in mind: 10 KB stack, 128×64 display, no dynamic allocation in hot paths

## Setting up the development environment

1. Install [uFBT](https://github.com/flipperdevices/ufbt):
   ```bash
   pip install ufbt
   ufbt update
   ```

2. Clone the repo:
   ```bash
   git clone https://github.com/DanilaE/FlipMesh
   cd FlipMesh
   ```

3. Build:
   ```bash
   ufbt build        # build only
   ufbt launch       # build + deploy to connected Flipper Zero
   ```

## Hardware for testing

Connect a Meshtastic node to Flipper Zero GPIO 13/14 (TX/RX) with a common GND.
On the Meshtastic node, enable **Serial Module** in **PROTO** mode at **115200 baud**.
See [docs/hardware-setup.md](docs/hardware-setup.md) for the full wiring diagram.

## Code style

- **Language:** C99, matching the Flipper Zero SDK style
- **Naming:** `fm_` prefix for all public functions, `FM_` for constants, `FMXxx` for types
- **Files:** one module per `.c`/`.h` pair; keep files under 800 lines
- **No heap in hot paths:** avoid `malloc`/`free` inside the RX thread or render callback
- **Mutex:** always acquire `app->lock` before touching shared state (`roster`, `history`, `log`)
- **No comments on obvious code** — only comment non-obvious invariants or workarounds

Every new `.c`/`.h` file must start with:
```c
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2025 DanilaE
```

## Commit messages

Follow [Conventional Commits](https://www.conventionalcommits.org/):

```
<type>: <short description>

<optional body>
```

Types: `feat`, `fix`, `refactor`, `docs`, `chore`, `perf`

Examples:
```
feat: add battery percentage to nodes page
fix: prevent crash when roster is full and DM arrives
docs: add protocol framing diagram
```

## Opening a pull request

1. Fork the repo and create a branch from `main`:
   ```bash
   git checkout -b feat/your-feature
   ```
2. Make your changes and verify `ufbt build` passes
3. Run the SPDX header check locally:
   ```bash
   for f in *.c *.h; do grep -q "SPDX" "$f" || echo "Missing: $f"; done
   ```
4. Open a PR against `main` with a clear description of what changed and why
5. CI must pass before merging (build + lint checks)

## Reporting bugs

Open a GitHub issue with:
- Flipper Zero firmware version (`ufbt --version`)
- Meshtastic node model and firmware version
- Steps to reproduce
- What you expected vs. what happened
- Log output from the Logs page if relevant

## Adding a new tone

Add an entry to the `FMTone` enum in `flipmesh.h`, implement the pattern in the `fm_play_tone` switch in `fm_notify.c`, and add a label to `tone_labels[]`. Keep tones short (< 1 s total) to avoid blocking the UI.

## Architecture overview

See [docs/architecture.md](docs/architecture.md) for the threading model, data flow, and module responsibilities.
