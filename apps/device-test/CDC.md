# CDC library (device-test)

local USB CDC connect / detect / dispatch for device-test, under
`src/cdc/`. mirrors the HID library layout: thin device façade + kind
classify + optional probe, with protocol handlers remaining elsewhere
(monome in libavr32; crow stubbed for later).

## kinds

| kind | meaning |
|------|---------|
| `CDC_KIND_MONOME` | mext grid/arc (iii or legacy) |
| `CDC_KIND_CROW` | monome crow ACM |
| `CDC_KIND_UNKNOWN` | CDC data device, not claimed |

## VID/PID table

| VID:PID | kind |
|---------|------|
| `0483:5740` | crow (STM VCP in crow firmware) |
| `CAFE:1110` | iii grid/arc |

classification is `cdc_classify_ids()` in `cdc_kind.h`. add new rows there
as more devices get stable IDs.

## probe fallback

when IDs do not match, `cdc_probe_mext_identity()` sends mext query `0x00`
with hard timeouts (no infinite `tx_busy` / `rx_busy` spin). acceptance:
response `0x00, type, count` with type `1` (grid) or `5` (arc).

success → treat as `CDC_KIND_MONOME`. failure → remain unknown; **do
not** call `monome_setup_mext()`.

## event flow

```text
uhi_cdc enable
  → cdc_change(dev) posts kEventSerialConnect with e.data = uhc_device_t*
  → handle_SerialConnect
       → cdc_dev_on_connect
            classify IDs → optional probe → if MONOME: monome_setup_mext()
       → log crow up / cdc monome VID:PID / cdc unknown VID:PID
  → (monome only) kEventMonomeConnect → existing monome UI + timers

disconnect
  → kEventSerialDisconnect (e.data = uhc_device_t*)
  → handle_SerialDisconnect
       → cdc_dev_on_disconnect
            if was MONOME: post kEventMonomeDisconnect
       → log crow down / cdc down
```

FTDI monomes still use framework `kEventFtdiConnect` → `ftdi_setup` and
are unchanged.

## vendor note

`vendor/libavr32/src/usb/cdc/cdc.c` posts `e.data = (s32)dev` on
SerialConnect/Disconnect (same pattern as HID). without that, VID/PID
cannot be read from the connect event.

## files

| path | role |
|------|------|
| `src/cdc/cdc_kind.h` | kinds + ID constants + `cdc_classify_ids` |
| `src/cdc/cdc_dev.c` / `.h` | connect snapshot, dispatch, disconnect |
| `src/cdc/cdc_probe.c` / `.h` | bounded mext query |
| `src/cdc/cdc_crow.h` | phase-1 stub (identify/log only) |

phase 1 crow: connect log only; no poll timer or command UI.
