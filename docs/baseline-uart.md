# UART baseline (pre-split regression reference)

Record hardware, firmware version, and smoke results **before** large layout changes.

## Build

```bash
cd apps/uart && ufbt build
```

## Smoke checklist

- [ ] App starts without crash
- [ ] Status: `Syncing...` → `Connected`
- [ ] Outbound mesh text sends
- [ ] Inbound text appears in history
- [ ] Nodes / Stats / Signal / Logs / Settings usable
- [ ] UART / Baud changes persist and apply

## Notes

_Add device, Meshtastic firmware version, and Flipper OS version here after manual run._
