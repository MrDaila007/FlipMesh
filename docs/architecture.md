# Architecture

## Threading model

FlipMesh runs two threads under Furi RTOS:

```
Main thread                     RX thread (fm_rx_thread)
──────────────────────          ──────────────────────────────
UI render (view_port_update)    furi_stream_buffer_receive
Input handling (input_cb)       framing_feed()  ← byte by byte
Keyboard overlay                nanopb decode (FromRadio)
Settings save/load              update roster / history / log
                                fm_notify_message()
```

All shared state lives in `FlipMeshApp`. Access from both threads is protected by `app->lock` (FuriMutex). The render callback must never block — it acquires the lock for the minimum time needed to snapshot data.

## Data flow

```
Meshtastic node
    │ UART bytes
    ▼
rx_cb() — ISR, pushes bytes into rx_stream (FuriStreamBuffer)
    │
    ▼
fm_rx_thread
    ├── framing_feed() — accumulates bytes until a complete frame arrives
    │       header: [0x94][0xC3][len_hi][len_lo]
    │       payload: protobuf bytes (max 512)
    │
    └── decode_fromradio()
            ├── packet → decode_packet()
            │       ├── TEXT_MESSAGE_APP  → fm_history_add() + fm_notify_message()
            │       ├── TELEMETRY_APP     → fm_node_update_metrics() / fm_node_update_env()
            │       ├── NODEINFO_APP      → fm_node_update_user()
            │       ├── POSITION_APP      → fm_node_update_pos()
            │       └── ROUTING_APP       → fm_log()
            ├── my_info      → app->self_id
            ├── node_info    → fm_node_update_info()
            ├── config_complete_id → FM_CONN_LIVE + fm_hb_start()
            └── metadata     → fm_log(firmware version)

TX path:
    input_cb (OK on Messages) → app->kb_active = true
    main loop → opens TextInput overlay → view_dispatcher_run()
    text_input_callback() → fm_proto_send_text() → nanopb encode → send_frame() → UART
```

## Connection state machine

```
FM_CONN_IDLE
    │  fm_proto_sync() — sends want_config_id (random nonce)
    ▼
FM_CONN_SYNC
    │  receives my_info (self node num)
    │  receives node_info × N (bulk roster population)
    │  receives config_complete_id == sent nonce
    ▼
FM_CONN_LIVE
    │  fm_hb_start() — FuriTimer fires every 10/30/60 s
    │  fm_proto_heartbeat() — sends ToRadio.heartbeat{nonce}
    │  normal packet traffic
```

If `config_complete_id` doesn't match the sent nonce, sync is retried at next reconnect.

## Module responsibilities

| Module | File(s) | Responsibility |
|--------|---------|----------------|
| App entry | `fm_app.c` | Lifecycle, main loop, keyboard overlay |
| GUI | `fm_gui.c/.h` | All 7 pages rendering + input dispatch |
| Protocol | `fm_protocol.c/.h` | Framing, nanopb encode/decode, TX/RX |
| UART | `fm_uart.c/.h` | Serial open/close/reopen, heartbeat timer |
| Roster | `fm_roster.c/.h` | Node list CRUD, NodeInfo/Position/Telemetry update |
| History | `fm_history.c/.h` | Circular message buffer, fm_log, fm_status |
| Channel | `fm_channel.c/.h` | Channel metadata, label lookup |
| Position | `fm_position.c/.h` | Coordinate formatting, integer distance calc |
| Settings | `fm_settings.c/.h` | Load/save key=value from SD card |
| Notify | `fm_notify.c/.h` | Vibration, LED, speaker tones |

## Key constraints

| Constraint | Value | Where enforced |
|-----------|-------|---------------|
| Stack per FAP | 10 240 bytes | `application.fam` |
| RX stream buffer | 4 096 bytes | `FM_RX_BUF` in `flipmesh.h` |
| Max frame size | 512 bytes | `FM_MAX_FRAME` |
| Roster capacity | 32 nodes | `FM_ROSTER_MAX` |
| Message history | 16 messages × 200 chars | `FM_MSG_HISTORY` / `FM_MSG_TEXT_MAX` |
| Echo ring | 32 IDs | `FM_ECHO_RING` |
| Log buffer | 20 lines × 64 chars | `FM_LOG_ROWS` / `FM_LOG_COLS` |
| Channels | up to 8 | `FM_MAX_CHANNELS` |

`frame_buf` (512 bytes) is heap-allocated in `fm_app.c` to keep it off the stack.  
`meshtastic_FromRadio` is stack-allocated in the RX thread — with firmware 2.7.22 static arrays it fits comfortably (~500 bytes).
