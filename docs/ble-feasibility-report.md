# BLE feasibility (Stage 0) — FlipMesh

**Date:** 2026-04-20  
**SDK checked:** uFBT `sdk_headers` (Flipper release channel, f7)

## Verdict: **NO-GO** for Meshtastic BLE central on stock external FAP (today)

### Evidence

1. **Public HAL (`furi_hal_bt.h`)**  
   Documented entry points are oriented around starting a **Flipper BLE app profile** (peripheral / device stack), radio stack control, and extra beacon APIs. There is no documented, stable API sequence for an external FAP to act as a **BLE Central** (scanner + GATT client) comparable to a phone.

2. **Public GAP/GATT headers**  
   `gap.h` describes advertising / GAP background behaviour oriented at **peripheral** flows (`gap_start_advertising`, connection events as the **slave** side).  
   `gatt.h` exposes **GATT server** primitives (`ble_gatt_service_add`, characteristic instances for services **hosted** on Flipper).

3. **Low-level controller symbols**  
   Headers under `stm32wb_copro` mention scan-related HCI structures, but these are **not** exposed as a supported, linkable FAP API surface in the same way as `furi_hal_serial` / `gui` / `storage`.

### Implications for FlipMesh

- The **FlipMesh BT** target is delivered as a **second FAP** with the **same Meshtastic core** and a **BLE transport module boundary** (`fm_transport_*`) ready for a future implementation (custom firmware, new official APIs, or vendor extensions).
- Runtime behaviour today: BT app shows transport status **“BLE central not available”** (or equivalent) and does not claim a working Meshtastic BLE link on stock firmware.

### Go conditions (re-evaluate when any become true)

- Official Flipper SDK documents and ships a **supported** FAP API for: scan → connect → GATT discover → write/notify/read on arbitrary peripherals **as central**, **or**
- Project adopts a **known-good** third-party firmware/SDK that exposes the above with a stable ABI, **or**
- Meshtastic exposes a **non-BLE** path acceptable for Flipper (e.g. USB serial gadget) — out of scope for this BLE plan.

### Meshtastic BLE recap (for future implementation)

- Service UUID: `6ba1b218-15a8-461f-9fa8-5dcae273eafd`
- `ToRadio` (write): `f75c76d2-129e-4dad-a1dd-7866124401e7`
- `FromRadio` (read): `2c55e69e-4993-11ed-b878-0242ac120002`
- `FromNum` (notify): `ed9da18c-a800-4f66-a670-aa7547e34453`
- Payload: raw protobuf (no `0x94 0xC3` framing). Target ATT MTU 512 when the stack allows.
