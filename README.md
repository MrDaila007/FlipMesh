# FlipMesh

A Meshtastic mesh network client for Flipper Zero with a shared **core** and two **applications**:

| App | Directory | Transport | Notes |
|-----|-----------|-----------|--------|
| **FlipMesh UART** | [`apps/uart/`](apps/uart/) | Serial (GPIO USART/LPUART) | Full Meshtastic serial framing; same settings path as legacy (`/ext/flipmesh/settings.cfg`). |
| **FlipMesh BLE** | [`apps/bt/`](apps/bt/) | Bluetooth LE (planned) | Settings under `/ext/flipmesh-bt/`. On stock firmware, BLE **central** / GATT client is not available — see [`docs/ble-feasibility-report.md`](docs/ble-feasibility-report.md). |

Shared logic (protocol decode, UI, roster, history, …) lives in [`core/`](core/).

## Features

- **Config sync** — automatically discovers all nodes on the mesh at startup (NodeInfo, names, GPS)
- **Messages** — send and receive text messages on any channel; direct messages to specific nodes
- **Nodes page** — roster of up to 32 nodes with short names, RSSI signal dots, hop count, last-seen age
- **Position page** — GPS coordinates for all nodes that have them, with inter-node distances
- **Stats page** — battery, voltage, channel utilization, air utilization, uptime per node
- **Signal page** — SNR and RSSI of the last received packet
- **Logs page** — scrollable debug log with freeze support
- **Settings** — UART port, baud rate, vibration, LED, 19 notification tones, scroll speed, framerate, heartbeat interval, channel count, timestamps
- **Heartbeat** — periodic keep-alive to prevent 15-minute serial timeout on the Meshtastic node
- **8 channels** — switch channels with the left button on the Messages page

## Hardware Setup

| Flipper Zero GPIO | Meshtastic node |
|:-----------------:|:---------------:|
| Pin 13 (TX)       | RX              |
| Pin 14 (RX)       | TX              |
| GND               | GND             |

On the Meshtastic node: enable **Serial Module** in **PROTO** mode at **115200 baud**.

> **Note:** Do not use the Flipper's 5V pin if the node has its own power source.

## Build

Requires [uFBT](https://github.com/flipperdevices/ufbt).

```bash
cd apps/uart && ufbt build    # UART FAP → .ufbt/build/flipmesh_uart.fap
cd apps/bt  && ufbt build     # BLE FAP  → .ufbt/build/flipmesh_bt.fap
ufbt launch                   # run from the app directory you want to deploy
```

CI runs on every push and PR: both FAPs are built and uploaded as separate artifacts.

## Documentation

| Document | Description |
|----------|-------------|
| [docs/hardware-setup.md](docs/hardware-setup.md) | Wiring diagram, tested hardware, Meshtastic config, troubleshooting |
| [docs/protocol.md](docs/protocol.md) | Meshtastic serial framing, protobuf message types, config sync sequence |
| [docs/architecture.md](docs/architecture.md) | Threading model, data flow, module responsibilities, memory constraints |

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for how to set up the dev environment, code style, commit format, and PR process.

## Navigation

| Button       | Action                                     |
|:------------:|:------------------------------------------:|
| Left / Right | Switch between pages                       |
| Up / Down    | Scroll list / log                          |
| OK           | Confirm / open chat / send message         |
| Back         | Go back / unfreeze log                     |

On **Messages** page, hold **Left** to cycle channels.

## License

FlipMesh is distributed under the **GNU General Public License v3.0**.
See [LICENSE](LICENSE) for the full text.

### Third-party components

| Component | License | Source |
|-----------|---------|--------|
| nanopb 0.4.9.1 | zlib (permissive) | https://github.com/nanopb/nanopb |
| Meshtastic protobuf definitions | GPL v3 | https://github.com/meshtastic/firmware |
| Flipper Zero SDK | GPL v3 | https://github.com/flipperdevices/flipperzero-firmware |

See [THIRD_PARTY_NOTICES](THIRD_PARTY_NOTICES) for full copyright notices.
