# UI Reference

FlipMesh presents a 128×64 pixel canvas UI organized into 7 pages. Left/Right navigate between pages in a circular carousel.

---

## Status Bar

Every page shares a 10 px black bar at the top:

```
[OK]   Alpha  ch1  ●●○
 ^       ^     ^    ^
 |       |     |    RSSI (3 dots, filled = stronger)
 |       |     active channel index
 |       own short_name (or !XXXX if not yet known)
 connection state: -- / SYNC / OK
```

RSSI thresholds for dot fill: ≥ −70 dBm → 3, ≥ −90 → 2, ≥ −110 → 1, below → 0.

---

## Page 0 — Messages

Broadcast chat: all incoming/outgoing messages whose `to` field is `0xFFFFFFFF` (broadcast) or whose `outgoing` flag is set.

```
< Messages           1/7 >
                  Me  HH:MM
          ┌──────────────┐
          │ hello world  │  ← outgoing (right-aligned, filled)
          └──────────────┘
Alpha  HH:MM
┌──────────────┐
│ hey there    │            ← incoming (left-aligned, outline)
└──────────────┘
```

- **Up/Down** — scroll older/newer messages
- **OK** — open keyboard to compose a broadcast message
- **OK (long)** — cycle active channel
- **Back** — exit the app

Long messages obey the `Long msg` setting:
- `Scroll` — text scrolls horizontally inside the bubble
- `Wrap` — bubble expands vertically, text wraps word-by-word

---

## Page 1 — Nodes

Three sub-views cycle via OK/Back:

### List view (default)

```
< Nodes              2/7 >
! Alpha  ●●● dir  5s
  Beta   ●●○  2h  12m
  !3A4F  ●○○  1h   1h
```

Columns: `!` = unread DM badge, name (short_name or `!XXXX`), RSSI dots, hops (`dir` = 0 hops / direct), last-heard age.

- **Up/Down** — move selection
- **OK** — open DM chat with selected node
- **OK (long)** — open node info
- **Left/Right** — navigate to previous/next page

### DM Chat view

Shows message history between own node and selected node. Same bubble rendering as Messages page.

- **Up/Down** — scroll
- **OK** — compose DM
- **Left/Back** — return to list

### Info view

```
< Alpha              2/7 >
Alpha Station
Seen: 23s ago
SNR:5.2 RSSI:-78
Hops: 2
Bat: 87% 4.05V
ChUtil:3.4% Air:1.2%
```

Shows: long name, last-heard age, signal quality, hops, battery/voltage (if telemetry received), channel utilization (if available).

- **Left/Back** — return to list

---

## Page 2 — Position

Lists all roster nodes that have GPS coordinates, one per entry with distance from the first node in the list.

```
< Position           3/7 >
Alph  55.7520°N 37.6175°E
Beta  48.8566°N  2.3522°E
       -> 2458km
```

- **Up/Down** — scroll through nodes with position data
- **Left/Right** — navigate pages

If no node has position data, shows "No position data yet".

---

## Page 3 — Stats

Counters and link statistics:

```
< Stats              4/7 >
USART @ 115200
RX: 48392 bytes / 121 frm
Err: mag=2 len=0 dec=0
TX: 5 frm  nodes: 8
HB: 47 sent
```

Fields: UART type and baud, total bytes received, frames decoded OK, error counts (bad magic / length / decode), TX frames, roster size, heartbeats sent.

---

## Page 4 — Signal

Last-received packet signal info:

```
< Signal             5/7 >
My ID: !A1B2C3D4
From: !1234
RSSI: -85 dBm
SNR:  4.25 dB
●●○
```

Shows own full 8-hex node ID, last sender (short 4-hex), RSSI in dBm, SNR in dB, and RSSI dot indicator.

---

## Page 5 — Logs

Rolling system log — up to 20 entries, 5 visible at a time:

```
< Logs               6/7 >
[SYNC] want_config sent
[INFO] my_info: !A1B2C3D4
[INFO] roster: 8 nodes
[INFO] config_complete → LIVE
[HB]   heartbeat sent #47
```

- **OK** — toggle pause/scroll freeze (header shows `[PAUSE]`)
- **Up/Down** — scroll while paused
- **Left/Right** — navigate pages

---

## Page 6 — Settings

11 settings, edited in-place with left/right arrows:

```
< Settings           7/7 >
UART          USART
Baud          115200
Vibro         On
LED           On
Ringtone      Chime
Scroll spd    5
Framerate     4 fps
Long msg      Wrap
Heartbeat     30s
Channels      3
Timestamps    On
```

- **Up/Down** — move cursor between items
- **OK** — enter edit mode for selected item (arrows shown: `< value >`)
- **Left/Right in edit** — cycle value
- **Back in edit** — exit edit mode; settings auto-saved to SD card

### Settings reference

| Setting | Values | Default | Effect |
|---------|--------|---------|--------|
| UART | USART / LPUART | USART | Which serial port (GPIO 13/14 vs 15/16) |
| Baud | 9600 / 19200 / 38400 / 57600 / **115200** / 230400 / 460800 / 921600 | 115200 | Must match Meshtastic node config |
| Vibro | On / Off | On | Vibrate on incoming message |
| LED | On / Off | On | Blink LED on incoming message |
| Ringtone | Off / 18 tones | Ping | Notification sound |
| Scroll spd | 1–10 | 5 | Horizontal scroll speed (10 = fastest) |
| Framerate | 1–10 fps | 4 | UI refresh rate |
| Long msg | Scroll / Wrap | Scroll | How text longer than the bubble is handled |
| Heartbeat | 10s / 30s / 60s | 30s | How often a heartbeat is sent to keep the connection alive |
| Channels | 1–8 | 3 | Number of Meshtastic channels available for cycling |
| Timestamps | On / Off | Off | Show `HH:MM` (uptime-based) in message bubbles |

Settings are persisted to `/ext/flipmesh/settings.cfg` on the SD card immediately after each change.
