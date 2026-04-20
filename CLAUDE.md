# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build

Requires [uFBT](https://github.com/flipperdevices/ufbt):

```bash
cd apps/uart && ufbt build    # flipmesh_uart.fap
cd apps/bt  && ufbt build     # flipmesh_bt.fap
```

No automated test suite — verification is manual on hardware with a live Meshtastic node (UART app).

## Architecture

### Threading

Two threads share all state via `app->lock` (FuriMutex):

- **Main thread** — UI render (`render_cb`), input (`input_cb`), keyboard overlay, settings save/load, `fm_proto_sync()`
- **RX thread** (`fm_rx_thread` in `core/fm_protocol.c`, **UART app only**) — reads `rx_stream`, byte-feeds `framing_feed()`, calls `fm_proto_deliver_fromradio()`, updates roster/history, calls `fm_notify_message()`

`render_cb` acquires the lock, snapshots what it needs, and releases immediately — it never blocks.

### Connection State Machine

```
FM_CONN_IDLE → (fm_proto_sync sends want_config_id) → FM_CONN_SYNC
FM_CONN_SYNC → (config_complete_id matches nonce)   → FM_CONN_LIVE
FM_CONN_LIVE → heartbeat every 10/30/60 s via FuriTimer
```

Five consecutive bad-magic bytes trigger auto-resync ("Resyncing..." log entry).

### Key Data Structures

- `FlipMeshApp` — single heap-allocated context (~9 KB); every module takes a pointer to it
- `FMHistory` — ring buffer where `head` = index of **oldest** message; iteration: `buf[(head + i) % FM_MSG_HISTORY]`
- `FMRoster` — up to 32 `FMNode` entries; updated from both bulk `node_info` (sync) and live packets

### Wire protocol (UART)

```
[0x94][0xC3][len_hi][len_lo][nanopb payload]
```

Max frame: 512 bytes. UART ISR pushes bytes into `rx_stream` (4096 B); RX thread reads and feeds `framing_feed()` byte by byte.

TX uses `fm_transport_tx()` (`apps/uart/fm_transport_uart.c` adds framing and writes serial).

### Private libraries

Under each `apps/*/lib/` uFBT expects `lib/<name>/` — this repo uses symlinks into `core/lib/` for **nanopb** and **meshtastic_api**, and per-file symlinks into `core/` for **flipmesh_core** sources.

## Coding Conventions

- Every source file starts with `// SPDX-License-Identifier: GPL-3.0-or-later`
- All public functions use the module prefix: `fm_history_*`, `fm_roster_*`, `fm_proto_*`, etc.
- Shared state access: always `furi_mutex_acquire(app->lock, FuriWaitForever)` / `furi_mutex_release(app->lock)`
- No heap allocations in hot paths; static arrays sized at compile time via constants in `flipmesh.h`
- Protobuf anonymous unions (Meshtastic 2.7.22): access members directly — `to.heartbeat`, `from.packet`, etc. (no `.payload_variant` wrapper)

## Important Constants (`flipmesh.h`)

| Constant | Value | Meaning |
|----------|-------|---------|
| `FM_MSG_HISTORY` | 16 | Message ring capacity |
| `FM_ROSTER_MAX` | 32 | Max tracked nodes |
| `FM_LOG_ROWS` | 20 | Log ring rows |
| `FM_MAX_CHANNELS` | 8 | Max Meshtastic channels |
| `FM_TONE_COUNT` | 19 | Notification tones (0 = Off) |
| `FM_PAGE_COUNT` | 7 | UI pages |

## Docs

- `docs/architecture.md` — threading, data flow, algorithms, memory budget
- `docs/protocol.md` — Meshtastic frame format, config sync sequence, packet types
- `docs/hardware-setup.md` — GPIO wiring, Meshtastic node configuration
- `docs/ui.md` — all 7 UI pages, navigation, settings reference
