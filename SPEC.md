# device-test

device-test is a control application for aleph focused on validating USB
device handling. it exercises connect/disconnect and per-class diagnostic
views so libavr32 and aleph USB work can be checked without bees or DSP
module load.

future versions will extend the same shell to exercise USB hub support and
multi-device focus.

---

## goals

- validate USB connect/disconnect for MIDI interfaces, monome grid/arc
  (FTDI and CDC), HID keyboards, and USB mass storage (MSC).
- keep the event loop and screen path responsive under device I/O; a
  header heartbeat proves the loop and draw path have not locked up.
- provide per-class diagnostic views sufficient to debug aleph + libavr32
  USB work (input parse vs LED TX vs connect identify).
- stay a thin control app: no blackfin module load, no bees operator
  network, no scene/preset system.

non-goals (v1):

- USB hub topology UI or simultaneous multi-pane content for several
  classes.
- HID key decode / keymap display (raw hex dump only in v1).
- MSC filesystem browse (header + log only; content TBD).
- MIDI send, or bulk monome LED stress patterns beyond the held-key and
  arc-accumulator feedback described below.
- footer-driven focus switching among connected classes (deferred with
  hub support).

---

## terminology

| term | meaning |
|------|---------|
| **class** | a USB device category the app understands: midi, monome, hid, msc. |
| **focus** | the single class that owns the right-hand header label and the content view. |
| **heartbeat** | a 2×2 white square in the header that toggles every 0.5 s while the frame path runs. |
| **diagnostic log** | one transient mid-grey line at the bottom of the content band (between-style). |
| **accumulator** | per-arc-encoder running sum of signed ring deltas, wrapped onto 64 ring LEDs. |

---

## focus policy (v1)

**last connected wins.** the newest successful connect of a supported class
takes focus (header label + content view). connect and disconnect of any
class still write the diagnostic log, even when that class does not hold
focus.

when the focused class disconnects, focus clears to idle (`none`) even if
other classes remain connected. a later connect of another class takes
focus again. (explicit multi-class focus selection is deferred.)

---

## screen layout

oled is 128×64. chrome matches between’s vertical bands:

```text
┌─ header y=0 h=8 ──────────────────────────────────────┐
│ <class label left>                        [2x2 pulse] │
├─ content y=8 h=48 ────────────────────────────────────┤
│ device view (rows 0–4; five 8px rows)                 │
│ row 5 / y=48: diagnostic log overlay when active      │
├─ footer y=56 h=8 ─────────────────────────────────────┤
│ reserved (blank in v1)                                │
└───────────────────────────────────────────────────────┘
```

### header

- **class label (left):** one of `MIDI`, `MONOME`, `MONOME GRID`,
  `MONOME ARC`, `HID`, `MSC`. empty / blank when idle (no focus).
- **heartbeat (right):** 2×2 white pixels at the far right (even
  coordinates; screen packs two pixels per byte). toggle on/off every
  500 ms from the screen-refresh tick path only — never from USB
  callbacks. if this freezes, the event loop or draw path is stuck.

### content

- idle: show `none` (left-aligned or centered; pick one in render and
  keep it consistent).
- focused class owns the full content band above the log overlay (see
  [per-class views](#per-class-views)).

### diagnostic log

same semantics as between:

- one line at `y=48`, mid-grey text, ~21 characters.
- drawn **immediately** (bypass frame coalesce) so progress remains
  visible if something later blocks.
- auto-clear after 2000 ms via the screen-refresh tick.
- when inactive, leave that row to the page content.

example phrases: `midi up`, `midi down`, `monome grid 16x8`, `monome arc 4`,
`hid up`, `msc up`, `monome down`.

### footer

reserved in v1 (blank white/black cells or empty). later versions may use
SW0–SW3 to select focus among connected classes.

---

## frame and redraw

copy between’s async, rate-limited path:

- USB and input handlers call `render_mark_dirty()` (or status-specific
  marks). they must **not** draw the device view synchronously.
- soft timer every 50 ms posts `kEventScreenRefresh` (ISR posts only; SPI
  stays on the main loop).
- `handle_ScreenRefresh` ages heartbeat and log, then calls
  `render_frame_service()`.
- `app_idle_handler = render_frame_service` drains pending frames when the
  queue is empty, still capped by `RENDER_MIN_FRAME_MS` (50 ms).
- heartbeat period: 500 ms on / 500 ms off (toggle on refresh ticks).

constants (suggested names):

```text
RENDER_TICK_MS       50
RENDER_MIN_FRAME_MS  50
RENDER_LOG_CLEAR_MS  2000
HEARTBEAT_HALF_MS    500
```

---

## per-class views

### midi

triggered by focus after `kEventMidiConnect`.

- header: `MIDI`
- content: scrolling multi-line hex log of each `kEventMidiPacket`.
  - decode the packed `s32` into USB-MIDI CIN + data bytes.
  - **newest at bottom**; keep up to five lines visible above the log
    overlay.
  - example line: `9 90 3C 40` (CIN and payload bytes in hex).
- connect / disconnect log: `midi up` / `midi down`.
- start MIDI poll timer on connect; stop on disconnect (between pattern).

### monome

triggered by focus after `kEventMonomeConnect`.

- header: `MONOME` until type is known; then `MONOME GRID` or
  `MONOME ARC` from `monome_device()`.
- on connect, log size or encoder count, e.g. `monome grid 16x8` or
  `monome arc 4`.
- start monome poll + refresh timers on connect; stop on disconnect
  (bees-style local timers — not bees `net_monome`).
- drive `monomeLedBuffer`, `monomeFrameDirty`, and `monome_refresh` so
  device LEDs update without blocking handlers (mark dirty / set buffer
  only in the event handler).

framework note: `avr32` owns `kEventSerialConnect` → `monome_setup_mext()`
and FTDI setup → `kEventMonomeConnect`. device-test consumes monome
connect/key/enc/disconnect events and must not override framework setup
unless diagnosing setup itself.

#### grid (screen + device LEDs)

- content: 16×16 pixel key map (one pixel per key). map device coordinates
  with `monome_size_x()` / `monome_size_y()`. smaller grids leave unused
  pixels off.
- on `kEventMonomeGridKey` **press**: set the screen pixel on **and** set
  the matching device LED on (held = lit).
- on **lift**: clear the screen pixel **and** clear that device LED.
- LED path: write the app LED buffer, set quadrant dirty flags; the
  refresh timer pushes maps. no other LED patterns in v1.

#### arc (screen + device LEDs)

- content: four text lines:

  ```text
  e0: <accum>
  e1: <accum>
  e2: <accum>
  e3: <accum>
  ```

  unused encoders (`i >= monome_encs()`) show `—`.
- per connected encoder: keep a simple accumulator (signed wrap is fine;
  display the numeric accum). on `kEventMonomeRingEnc`, add the signed
  delta into that encoder’s accumulator.
- map to ring LED: `led_index = accum & 63` (64 LEDs per ring).
- device feedback: clear the previous lit LED (or clear the ring), light
  **one** LED at `led_index` via `monome_arc_led_set` / ring map and the
  dirty bit for that encoder.
- optionally show `led_index` on the same line so USB TX issues are
  separable from input parse (e.g. `e0: 12 @5`).
- on monome connect: zero accumulators and clear all ring LEDs.
- on disconnect: stop refresh timers.

### hid

- header: `HID`
- content: scrolling multi-line hex log of each HID report (same shape as
  MIDI: newest at bottom, up to five lines). each report is dumped as hex
  bytes; long frames wrap across successive lines (7 bytes per line).
- the HID stack updates a dirty frame without posting events; the app poll
  timer posts `kEventHidPacket` when dirty so the main loop can log.
- log: `hid up` / `hid down`
- start/stop HID poll timer (bees pattern).

### msc

- header: `MSC`
- content: placeholder `TBD`
- handle `kEventMscConnect` / `kEventMscDisconnect` (first intentional app
  consumer; bees/between do not use these today).
- log: `msc up` / `msc down`
- distinguish from onboard SD: `fat_init()` at boot is not MSC hotplug.
  device-test does not treat SD mount as an MSC focus event.

---

## event surface (v1)

assign app handlers for:

| class | events |
|-------|--------|
| midi | `kEventMidiConnect`, `kEventMidiDisconnect`, `kEventMidiPacket` |
| monome | `kEventMonomeConnect`, `kEventMonomeDisconnect`, `kEventMonomeGridKey`, `kEventMonomeRingEnc` |
| hid | `kEventHidConnect`, `kEventHidDisconnect`, `kEventHidPacket` |
| msc | `kEventMscConnect`, `kEventMscDisconnect` |
| ui | `kEventScreenRefresh`; encoders/switches no-op or ignored |

do **not** override framework `kEventSerialConnect` / `kEventFtdiConnect`
setup handlers for normal operation. log monome identify outcome on
`kEventMonomeConnect` (`monome_device()`, sizes, `monome_encs()`).

canonical event definitions live in `libavr32/src/events.h`. connect
replay after `app_launch` is handled in `avr32/src/main.c` (same as other
apps).

---

## implementation sketch (future)

not part of the SPEC-only deliverable. when implementing:

```text
apps/device-test/
  SPEC.md                 (this file)
  Makefile
  config.mk               APP = device-test
  version.mk
  aleph-device-test.lds
  src/app_device_test.c   app_init / app_launch
  src/handler.c           event assignment + focus
  src/render.c / .h       regions, heartbeat, log, views
  src/app_timers.c / .h   screen, midi, monome, hid polls
```

scaffold from mix/between. build:

```sh
cd apps/device-test && aleph-builder make
```

---

## acceptance criteria (v1)

- heartbeat pulses (~1 Hz duty: 0.5 s on / 0.5 s off) while idle and while
  devices stream.
- with no focused device, content shows `none` and the class label is
  clear.
- unplug of the focused class restores `none` and clears the class label;
  heartbeat keeps pulsing.
- MIDI: packets appear as hex lines (newest at bottom) without freezing
  the heartbeat.
- CDC or FTDI grid: held keys light matching device LEDs and screen
  pixels; release clears both; heartbeat keeps pulsing.
- CDC or FTDI arc: turning a ring moves a single lit LED around that ring
  from the accumulator; screen lines track accum; heartbeat keeps pulsing.
- monome mis-detect (wrong grid vs arc / bad size) is visible in the
  header and/or diagnostic log.
- HID reports appear as hex lines without freezing the heartbeat; MSC
  connect updates header + log (content may remain `TBD`).
- diagnostic log shows connect/disconnect phrases and auto-clears after
  ~2 s.
