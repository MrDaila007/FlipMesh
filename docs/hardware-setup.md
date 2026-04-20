# Hardware Setup

## Wiring

```
Flipper Zero GPIO          Meshtastic node
─────────────────          ───────────────
Pin 13  (TX)  ──────────►  RX
Pin 14  (RX)  ◄──────────  TX
Pin 8   (GND) ──────────── GND
```

> **Do not connect the Flipper 5V pin** if the Meshtastic node has its own power source.
> If you need to power a small node from the Flipper, use pin 1 (3.3V) only if the node supports it — check your node's datasheet first.

## Tested hardware

| Meshtastic device | Notes |
|-------------------|-------|
| LILYGO T-Beam v1.1 | Works at 115200, use the JST serial header |
| LILYGO T-Echo | Works — onboard USB-serial, wire to the exposed pads |
| Heltec LoRa 32 v3 | Works at 115200 |
| RAK WisBlock 4631 | Works — wire to the IO1/IO2 serial pins |
| Seeed XIAO S3 | Works at 115200 |

## Meshtastic node configuration

In the Meshtastic mobile app or web interface:

1. Open **Config → Module Config → Serial**
2. Set **Serial enabled:** On
3. Set **Mode:** PROTO
4. Set **Baud rate:** 115200 (default, configurable in FlipMesh settings)
5. Set **RX pin / TX pin** to match your board's UART pins
6. Apply and reboot the node

> The node must be in **PROTO** mode (not TEXT or NMEA) — FlipMesh speaks the binary protobuf framing protocol.

## UART selection in FlipMesh

FlipMesh supports both Flipper Zero UART interfaces:

| Setting value | GPIO pins | Use when |
|:---:|:---:|---|
| USART | 13 (TX) / 14 (RX) | Default — standard GPIO header |
| LPUART | 15 (TX) / 16 (RX) | Alternative if USART is occupied |

Change the UART interface in **Settings → UART**.

## Troubleshooting

**No nodes appear after connecting:**
- Verify the Meshtastic node is in PROTO mode
- Check TX/RX are not swapped (TX→RX, RX←TX)
- Check GND is connected
- Try pressing Back on the Logs page to watch for decoding errors

**"Resyncing..." in logs:**
- The framing desynchronised — this is automatic and harmless
- If it repeats every few seconds, check baud rate matches on both ends

**Heartbeat not sent:**
- Connection must reach LIVE state (green "OK" in status bar) first
- Ensure config sync completes — watch for "Sync complete" in Logs
