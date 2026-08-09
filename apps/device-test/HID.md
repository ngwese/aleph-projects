# HID handling in device-test

notes on USB HID composite devices, how device-test polls interfaces today,
and ways to cut overhead when only one interface matters.

related code: `src/hid/` (parsers + `hid_dev`), vendored host path
`vendor/libavr32/src/usb/hid/` (`uhi_hid`, `hid_parse_frame`).

---

## USB HID in general

### device vs interface vs endpoint

a single USB **device** (one address, one VID/PID) may expose several **HID
interfaces**. each interface is a separate HID function (keyboard, mouse,
consumer control, vendor pad, etc.).

each interface has its own **interrupt IN endpoint** (and sometimes OUT).
one endpoint does **not** multiplex multiple interfaces. a wireless keyboard
dongle is typically:

| piece | role |
|-------|------|
| device | shared VID/PID, bus address |
| interface 0 | e.g. boot keyboard → IN ep `0x81` |
| interface 1 | e.g. boot mouse → IN ep `0x82` |
| interface 2 | e.g. consumer keys → IN ep `0x83` |

so “multiple HID interfaces” means **multiple independent report streams**,
each on its own pipe.

### boot vs report protocol

HID boot-subclass keyboards/mice support a fixed report layout used by BIOS
and simple hosts. after configuration, even boot devices often sit in
**report protocol** until the host sends `SET_PROTOCOL(BOOT)`.

device-test issues `SET_PROTOCOL(BOOT)` for keyboard and mouse interfaces on
select so the fixed boot parsers apply when the device accepts the request.

report protocol without a report-descriptor parser uses fixed “common layout”
heuristics (optional Report ID, etc.). gamepads use a de facto 12–16 byte
layout. there is no full descriptor parse in this app.

### independent polling

each interface’s IN endpoint needs its own receive loop: arm → complete →
re-arm. they do not share one poll covering every interface. you may:

- arm **all** interesting endpoints and run them in parallel, or
- arm **only** the interface you care about and leave others idle

whichever interfaces should deliver live data must be polled independently.

---

## how device-test handles HID today

### stack split

```text
USB device (composite HID)
  → ASF UHC + UHI_HID (vendor/libavr32)
      claims first HID interface only
      continuous IN on that endpoint → hid_parse_frame()
  → device-test hid_dev
      walks all HID interfaces into slots[0..N)
      focus/selection, SET_PROTOCOL, extra EPs for slots 1+
      parsers + OLED views
```

**iface slot 0** is special: its IN pipe is owned by vendored `uhi_hid` for
the whole connection. device-test reads it via `hid_get_frame_*`.

**iface slots 1+** are opened by device-test (`uhd_ep_alloc` + `uhd_ep_run`)
only when that slot is selected (e.g. SW0 “next”).

### focus and the app poll timer

the soft timer (~20 ms) posts `kEventHidPacket` when
`hid_dev_frame_dirty()` is set for the **currently focused**
`iface_index` only. other slots are not consumed by the UI until focused.

SW0 (while HID focused) cycles `iface_index`, may `SET_PROTOCOL(BOOT)`,
starts RX on slots 1+ if needed, and resets the decode view.

### what is polled when

| slot | on connect | after SW0 has visited it | when not focused |
|------|------------|--------------------------|------------------|
| 0 | `uhi_hid` arms IN and keeps re-arming | same | **still polled** by `uhi_hid` |
| 1+ | no EP alloc, idle | RX started; callback keeps re-arming | **still polled** once started (today) |
| never visited 1+ | idle | — | idle |

so today:

- only slot 0 is live immediately after connect
- extra interfaces stay idle until selected once
- once an extra interface has been started, its IN loop keeps running even
  after focus moves away
- the UI only **displays** the focused interface

connect always sets `iface_index = 0` (descriptor order). there is no
app preference for “prefer keyboard over mouse” yet.

### parsers

under `src/hid/`:

- `hid_kbd` / `hid_mouse` — boot + common report layouts
- `hid_gamepad` — 12–16 byte TinyUSB-style packed report
- `hid_report.h` — `hid_kind_t`, `hid_classify`
- `hid_dev` — identity, multi-iface slots, frame accessors

---

## reducing overhead: poll only what you need

if the app only cares about one interface (e.g. keyboard), unnecessary IN
traffic and ISR work can be avoided.

### stop unfocused pipes (device-test-owned slots)

for slots 1+:

1. on blur (leaving that `iface_index`), clear `running`, call
   `uhd_ep_abort` on that endpoint (optionally leave the EP allocated)
2. on focus again, `start_slot_rx` as today

never-started slots already cost nothing. the missing piece is **pausing**
after a visit.

### the hard case: interface 0 / `uhi_hid`

vendored `uhi_hid` always re-arms its IN endpoint in
`uhi_hid_report_reception`. device-test cannot stop that pipe without
changing libavr32 (or replacing the host HID claim path).

so even if the app “focuses” a mouse on slot 1 and only wants mouse data,
**slot 0’s endpoint keeps being polled** for the life of the device.

options if that overhead matters:

- extend `uhi_hid` with pause/resume or “selected endpoint” support
- stop using `UHI_HID` for multi-iface devices and own all HID IN pipes in
  the app (larger change)
- accept continuous polling of the first HID interface as a host-stack limit

### summary

| goal | feasible in device-test alone? |
|------|--------------------------------|
| don’t start unused ifaces 1+ | yes (already) |
| stop 1+ when unfocused | yes (abort + don’t re-arm) |
| stop iface 0 when unfocused | no — needs libavr32 / host changes |
| prefer keyboard as default focus | yes (selection only; see below) |

---

## proposed `hid_dev_prefer_kind(...)`

### intent

on connect, choose the initial `iface_index` from an app-provided preference
instead of always using descriptor order (slot 0).

example priority: keyboard → mouse → gamepad → unknown / first slot.

### sketch

```c
/* app registers before or at connect; stored statically in hid_dev */
void hid_dev_prefer_kind(hid_kind_t kind); /* or a small priority list */

/* inside hid_dev_on_connect, after walk_hid_ifaces: */
/* pick first slot whose protocol/classify matches preference order */
/* iface_index = chosen; maybe_set_boot_protocol(); start_slot_rx if idx>0 */
```

classification can use `bInterfaceProtocol` (`KEYBOARD` / `MOUSE` /
`GENERIC`) plus existing `hid_classify` / size hints.

### what it would do

- change which interface the **app reads and displays** first
- for a preferred slot `> 0`: allocate that EP, optional `SET_PROTOCOL(BOOT)`,
  start RX
- SW0 can still cycle through all interfaces

### caveat: libavr32 still polls interface 0

preferring keyboard on slot 1 does **not** stop `uhi_hid` from claiming and
continuously polling the **first** HID interface in the config descriptor.

so:

- UI / parsers can follow `hid_dev_prefer_kind`
- bus activity on iface 0 continues regardless of preference
- “focus” in device-test is a consumption/selection concept, not full
  ownership of which HID IN pipes the host keeps live — unless the vendored
  host path is changed

apps that need both preferred default **and** no idle iface-0 traffic must
treat the `uhi_hid` always-on pipe as a separate limitation from the
prefer-kind API.
