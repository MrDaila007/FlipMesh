# Meshtastic Serial Protocol

FlipMesh communicates with Meshtastic nodes using the binary serial protocol over UART.

## Frame format

Every message is wrapped in a 4-byte header followed by a protobuf payload:

```
┌──────────┬──────────┬──────────────────────────┬─────────────────────────┐
│  0x94    │  0xC3    │  Length (2 bytes, big-    │  Protobuf payload       │
│  magic0  │  magic1  │  endian, max 512)         │  (meshtastic_ToRadio /  │
│          │          │                           │   meshtastic_FromRadio) │
└──────────┴──────────┴──────────────────────────┴─────────────────────────┘
```

- **Magic bytes:** `0x94 0xC3` — identify the start of every frame
- **Length:** 2 bytes big-endian, payload length only (not including header)
- **Payload:** nanopb-encoded protobuf message, max 512 bytes

If the framing desyncs (5 consecutive bad magic bytes), FlipMesh logs "Resyncing..." and resets the framing state machine automatically.

## Direction

| Direction | Protobuf message |
|-----------|-----------------|
| Flipper → Node | `meshtastic_ToRadio` |
| Node → Flipper | `meshtastic_FromRadio` |

## Config sync sequence

After connecting, FlipMesh performs a config sync to populate the node roster:

```
Flipper                                  Meshtastic node
───────                                  ───────────────
ToRadio { want_config_id: <nonce> }  →
                                     ←  FromRadio { my_info }
                                     ←  FromRadio { node_info }  × N
                                     ←  FromRadio { config_complete_id: <nonce> }
ToRadio { heartbeat { nonce: 1 } }   →   (every 10/30/60 s)
```

The nonce is a random `uint32_t` generated at startup. If `config_complete_id` does not match, the sync is considered stale and ignored.

## Packet types handled

### Incoming (FromRadio)

| Variant | Action |
|---------|--------|
| `my_info` | Store `my_node_num` as `app->self_id` |
| `node_info` | Update roster: name, short name, GPS, device metrics |
| `config_complete_id` | Transition to LIVE state, start heartbeat timer |
| `metadata` | Log firmware version |
| `packet` | Route by `portnum` — see table below |

### Incoming packets by port number

| Port | Constant | Action |
|------|----------|--------|
| 1 | `TEXT_MESSAGE_APP` | Add to message history, notify |
| 3 | `POSITION_APP` | Update node GPS coordinates |
| 4 | `NODEINFO_APP` | Update node name / short name |
| 5 | `ROUTING_APP` | Log ACK/NACK |
| 67 | `TELEMETRY_APP` | DeviceMetrics (battery, voltage, utilization) or EnvironmentMetrics (temp, humidity, pressure) |

### Outgoing (ToRadio)

| Variant | When |
|---------|------|
| `want_config_id` | Immediately on connect |
| `packet` (TEXT_MESSAGE_APP) | User sends a message |
| `heartbeat` | Every 10/30/60 s (configurable) to prevent the 15-minute serial timeout |

## Echo detection

When FlipMesh sends a text message it stores the packet `id` in a 32-entry ring buffer (`app->echo_ids`). Incoming packets whose `id` appears in this ring are silently dropped to prevent the message appearing twice in the history.

## Protobuf library

FlipMesh uses **nanopb 0.4.9.1** for protobuf encoding/decoding. The `.pb.h`/`.pb.c` files in `lib/meshtastic_api/meshtastic/` are generated from Meshtastic firmware 2.7.22 proto definitions.

Key generated types used:
- `meshtastic_FromRadio` / `meshtastic_ToRadio`
- `meshtastic_MeshPacket` / `meshtastic_Data`
- `meshtastic_NodeInfo` / `meshtastic_User` / `meshtastic_Position`
- `meshtastic_Telemetry` / `meshtastic_DeviceMetrics` / `meshtastic_EnvironmentMetrics`
- `meshtastic_Heartbeat`
