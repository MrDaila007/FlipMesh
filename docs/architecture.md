# Architecture

## Project layout

```
flipmesh/
├── core/                         — shared Meshtastic client (transport-agnostic)
│   ├── flipmesh.h                — types, constants, FlipMeshApp
│   ├── fm_transport.h            — TX / ready hooks (implemented per app)
│   ├── fm_channel.c/.h
│   ├── fm_gui.c/.h
│   ├── fm_history.c/.h
│   ├── fm_notify.c/.h
│   ├── fm_position.c/.h
│   ├── fm_protocol.c/.h          — UART framing + RX thread; nanopb; heartbeat timer
│   ├── fm_roster.c/.h
│   ├── fm_settings.c/.h
│   └── lib/                      — nanopb + meshtastic_api (same as before)
├── apps/uart/                    — FlipMesh UART (`flipmesh_uart` FAP)
│   ├── application.fam
│   ├── flipmesh_uart_app.c       — entry, main loop, RX thread + UART open
│   ├── fm_uart.c/.h              — serial ISR → rx_stream
│   ├── fm_transport_uart.c       — framing + furi_hal_serial TX
│   ├── lib/                      — symlinks into core (private_libs layout for uFBT)
│   └── icons/
├── apps/bt/                      — FlipMesh BLE (`flipmesh_bt` FAP)
│   ├── application.fam
│   ├── flipmesh_bt_app.c         — entry, main loop (no RX thread until BLE exists)
│   ├── fm_bt_transport.c/.h      — BLE transport stub + fm_transport_* implementation
│   ├── lib/
│   └── icons/
└── docs/
```

Build each FAP from its app directory (`cd apps/uart && ufbt build`, etc.).

---

## Threading model

**UART app** runs two threads under Furi RTOS:

```
Main thread                          RX thread  (fm_rx_thread)
────────────────────────             ────────────────────────────────
UI render  (render_cb)               furi_stream_buffer_receive
Input      (input_cb)                framing_feed() — byte by byte
Keyboard overlay                     fm_proto_deliver_fromradio()
Settings save / load                 update roster / history / log
fm_proto_sync()                      fm_notify_message()
```

**BLE app** uses the main thread only today (no `fm_rx_thread`); when a BLE stack is integrated, a second thread or BLE callbacks should feed `fm_proto_deliver_fromradio()` with raw `FromRadio` payloads (no UART framing).

Shared state lives entirely in `FlipMeshApp`. Every access from either thread
is wrapped in `furi_mutex_acquire(app->lock, FuriWaitForever)` /
`furi_mutex_release(app->lock)`. The render callback snapshots what it needs
and releases the lock immediately — it never blocks.

---

## Data flow

```
Meshtastic node
      │ UART bytes
      ▼
rx_cb()                   ← UART ISR — pushes each byte into rx_stream
      │
      ▼
fm_rx_thread
      ├── framing_feed()  ← byte-by-byte header/payload accumulator
      │         [0x94][0xC3][len_hi][len_lo] → payload[0..len-1]
      │         5 bad-magic bytes in a row → "Resyncing..." + reset
      │
      └── fm_proto_deliver_fromradio()
                ├── packet → decode_packet()
                │       ├── port 1  TEXT_MESSAGE_APP  → fm_history_add + notify
                │       ├── port 3  POSITION_APP      → fm_node_update_pos
                │       ├── port 4  NODEINFO_APP      → fm_node_update_user
                │       ├── port 5  ROUTING_APP       → fm_log
                │       └── port 67 TELEMETRY_APP     → fm_node_update_metrics
                │                                        fm_node_update_env
                ├── my_info            → app->self_id
                ├── node_info × N      → fm_node_update_info (bulk roster fill)
                ├── config_complete_id → FM_CONN_LIVE + fm_hb_start
                └── metadata           → fm_log firmware version

TX path:
  input_cb (OK on Messages or DM chat)
    → app->kb_active = true
    → main loop opens TextInput overlay (view_dispatcher_run)
    → text_input_callback()
    → fm_proto_send_text()
    → nanopb encode → fm_transport_tx() (UART: framing + serial; BT: GATT when implemented)
```

---

## Connection state machine

```
FM_CONN_IDLE
    │  App start → fm_proto_sync()
    │  ToRadio { want_config_id: random_nonce }
    ▼
FM_CONN_SYNC                           status bar shows "SYNC"
    │  FromRadio.my_info    → self_id
    │  FromRadio.node_info  × N → roster (bulk)
    │  FromRadio.config_complete_id == nonce
    ▼
FM_CONN_LIVE                           status bar shows "OK"
    │  fm_hb_start() → FuriTimer periodic
    │  ToRadio { heartbeat { nonce++ } } every 10 / 30 / 60 s
    │  Normal packet traffic
```

Nonce mismatch (stale config_complete_id) is silently ignored — SYNC
continues waiting. If the Meshtastic node resets, the serial ISR will push
bad frames → 5 consecutive bad-magic events → auto-resync log entry.

---

## Module responsibilities

| Module | Public API highlights |
|--------|-----------------------|
| `apps/uart/flipmesh_uart_app.c` | `flipmesh_uart_app_entry()` — lifecycle, UART + RX thread |
| `apps/bt/flipmesh_bt_app.c` | `flipmesh_bt_app_entry()` — lifecycle, BLE glue (stub) |
| `fm_gui.c/.h` | `render_cb`, `input_cb`, `text_input_callback`, `kb_back_callback` |
| `fm_protocol.c/.h` | `fm_proto_sync`, `fm_proto_heartbeat`, `fm_proto_send_text`, `fm_proto_deliver_fromradio`, `fm_hb_*`, `fm_rx_thread`, `fm_proto_transport_set_ready` |
| `fm_transport.h` | `fm_transport_tx`, `fm_transport_ready` — implemented per app |
| `apps/uart/fm_uart.c/.h` | `fm_uart_open`, `fm_uart_close`, `fm_uart_reopen` |
| `apps/bt/fm_bt_transport.c/.h` | BLE transport stub + `fm_transport_*` |
| `fm_roster.c/.h` | `fm_node_get`, `fm_node_update_info/user/signal/metrics/env/pos`, `fm_node_mark_dm`, `fm_node_display` |
| `fm_history.c/.h` | `fm_history_add`, `fm_log`, `fm_status` |
| `fm_channel.c/.h` | `fm_ch_init`, `fm_ch_next`, `fm_ch_set`, `fm_ch_set_meta`, `fm_ch_label` |
| `fm_position.c/.h` | `position_format_coords`, `position_calc_distance_m`, `position_format_distance` |
| `fm_settings.c/.h` | `settings_save`, `settings_load` |
| `fm_notify.c/.h` | `fm_play_tone`, `fm_notify_message`, `fm_tone_name` |

---

## Key algorithms

### Message ring buffer (`fm_history.c`)

`FMHistory` holds up to `FM_MSG_HISTORY` (16) messages in a fixed array.

```
 head = index of the OLDEST message
 count = number of valid messages

 Write slot = (head + count) % FM_MSG_HISTORY   ← when count < FM_MSG_HISTORY
           or  head                               ← when full (evicts oldest,
                                                              then head++)
 Read i-th (0=oldest) = buf[(head + i) % FM_MSG_HISTORY]
```

This is different from a write-pointer ring: `head` tracks the oldest entry,
not the next write slot, so iteration is simply `head + 0, head + 1, ...`.

### Settings parser (`fm_settings.c`)

A single-pass character FSM over the raw file buffer — no `strchr`, no line
extraction. Two accumulators (`key[]`, `val[]`) fill based on current phase:

```
phase = PS_KEY  ──'='──► PS_VAL  ──'\n'──► dispatch(key, val) → PS_KEY
                           └──'\0'──► dispatch(key, val) → PS_DONE
```

`dispatch()` maps key strings to `FlipMeshApp` fields via a chain of
`strcmp` comparisons with range validation on each value.

### Word-token text wrapping (`fm_gui.c`)

Used in message bubbles when `long_msg == FM_LMH_WRAP`.

1. `next_word()` extracts the next whitespace-delimited token from the input.
2. A candidate line `probe = line + " " + word` is constructed.
3. If `canvas_string_width(probe) <= max_w`: accept, update `line`.
4. Otherwise: flush `line` to screen (or count it), reset `line = word`.

This greedy word-fit approach is structurally different from char-by-char
accumulation with backtracking.

### Integer distance calculation (`fm_position.c`)

Equirectangular approximation — no floating-point trig:

1. `dlat_m`, `dlon_m` computed from 1e-7 degree units via integer scaling
   (`× 11132 / 1000000` ≈ metres per 1e-7 degree).
2. Longitude correction: `cos(avg_lat)` from a 19-entry lookup table at 5°
   steps (values scaled × 1000).
3. `sqrt(dlat_m² + dlon_m²)` via Newton's method in integer arithmetic.

---

## Memory budget

| Item | Size | Location |
|------|------|----------|
| `FlipMeshApp` struct | ~9 KB | heap (`malloc` in `fm_app.c`) |
| `frame_buf` | 512 B | heap (separate `malloc`, field `app->frame_buf`) |
| `meshtastic_FromRadio` decode buffer | ~500 B | RX thread stack |
| FAP stack | 10 240 B | `application.fam` |
| RX stream buffer | 4 096 B | heap (`furi_stream_buffer_alloc`) |

`FlipMeshApp` is heap-allocated so its size does not consume FAP stack.
`frame_buf` is a separate heap allocation to make it independently
freeable and keep `FlipMeshApp` fields logically separated from the
raw I/O scratch buffer.

---

## Key compile-time constants (`flipmesh.h`)

| Constant | Value | Meaning |
|----------|-------|---------|
| `FM_MAGIC0 / FM_MAGIC1` | `0x94 / 0xC3` | Meshtastic frame magic bytes |
| `FM_MAX_FRAME` | 512 | Max payload bytes per frame |
| `FM_RX_BUF` | 4096 | UART stream buffer size |
| `FM_LOG_ROWS / FM_LOG_COLS` | 20 / 64 | Log ring dimensions |
| `FM_PAGE_COUNT` | 7 | Number of UI pages |
| `FM_MSG_HISTORY` | 16 | Message ring capacity |
| `FM_MSG_TEXT_MAX` | 200 | Max chars per message |
| `FM_ROSTER_MAX` | 32 | Max tracked nodes |
| `FM_ECHO_RING` | 32 | Echo-suppression ring size |
| `FM_MAX_CHANNELS` | 8 | Max Meshtastic channels |
| `FM_TONE_COUNT` | 19 | Number of notification tones |
