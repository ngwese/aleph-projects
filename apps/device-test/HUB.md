# USB hub support — implementation notes

report on what ASF / libavr32 already provide, what is missing, and the
steps to support a **fixed number of USB devices** behind a hub (each
device may still be composite with multiple interfaces).

related: [HID.md](HID.md) (per-device multi-interface HID). this document
is about **multiple devices on the bus**, usually via an external hub.

code roots:

- `vendor/libavr32/asf/common/services/usb/uhc/` — UHC (host core)
- `vendor/libavr32/asf/avr32/drivers/usbb/` — USBB host controller
- `vendor/libavr32/src/usb/` — aleph UHIs (HID, MIDI, FTDI, CDC)
- `vendor/libavr32/conf/aleph/conf_usb_host.h` — active host conf

target silicon: **UC3A0512** (aleph AVR32), USBB with **8 pipes** and
**~2 KB DPRAM**.

---

## current state (what ships today)

### hub is compile-blocked

`USB_HOST_HUB_SUPPORT` is documented in `uhc.h` and partially scaffolded,
but:

1. active conf **never** defines it
2. `uhc.c` hard-fails if it is defined:

```c
#ifdef USB_HOST_HUB_SUPPORT
#  error The USB HUB support is not available in this revision.
#endif
```

3. several class drivers also `#error` if hub is enabled (ASF MSC/CDC/
   mouse/vendor/AOA; aleph `uhi_ftdi.c`)
4. there is **no `uhi_hub` driver** in the tree — only call sites for
   `uhi_hub_suspend` / `uhi_hub_send_reset`
5. `usbb_host.c` hub paths contain `#error TODO` (address list, pipe0
   sharing)

**verdict:** hub support is intentionally unfinished scaffolding, not a
flip-a-define feature.

### single-device host model

| limit | today |
|-------|-------|
| devices on bus | **1** (root port only) |
| USB address | fixed **1** after enum (`UHC_DEVICE_ENUM_ADD`) |
| `uhc_device_t` storage | one static `g_uhc_device_root` |
| hub topology fields | compiled out (`prev`/`next`/`hub`/`hub_port`) |
| UHI class slots | **one static device** per class (`SOFTWARE_LIMIT` if busy) |
| data pipes | pipes **1–7** (pipe 0 = control) |
| power budget | `USB_HOST_POWER_MAX` = 500 mA |

enumeration today: root connect → reset → get descriptors → SET_ADDRESS(1)
→ get config → each `USB_HOST_UHI` `.install(dev)` → SET_CONFIGURATION →
all UHIs `.enable(dev)`.

composite **interfaces on that one device** already work (multiple UHIs
can succeed on the same `uhc_device_t`). hubs add **multiple devices**,
each with its own address and its own interface set.

---

## goals for a fixed-N hub design

assume a product constraint like:

- at most **one external hub** on the root port (depth 1), or nested hubs
  later
- at most **N downstream devices** (not counting the hub itself), e.g.
  N = 4
- each device may have **multiple interfaces** (HID×3, MIDI+HID, etc.)
- apps (device-test, bees, …) learn connects/disconnects per device

hardware will often be the tighter bound than N (see [pipe and DPRAM
budget](#pipe-and-dpram-budget)).

---

## implementation steps

ordered roughly by dependency. several steps are large; estimates are
engineering effort, not calendar promises.

### 1. unlock and finish UHC multi-device core

**files:** `uhc.c`, `uhc.h`, optionally `uhc_doc.h`

- remove / replace the top-level `#error` for `USB_HOST_HUB_SUPPORT`
- allocate a **fixed pool** of `uhc_device_t` (size `1 + N` or
  `1 + HUBS + N`), not a single `g_uhc_device_root`
- enable hub fields on `uhc_device_t` (`hub`, `hub_port`, `prev`/`next`,
  `power`)
- implement address allocation: addresses **1..M** (USB allows 1–127;
  keep M small and fixed)
- generalize `UHC_DEVICE_ENUM_ADD` / `uhc_dev_enum` to the device currently
  being enumerated
- connection tree: root plug still uses root HC IRQs; downstream plug/
  unplug comes from the hub driver (step 3)
- power accounting: walk parent hubs vs `USB_HOST_POWER_MAX` (scaffolding
  already sketched under `#ifdef`)
- suspend/reset of a downstream device must call hub port APIs, not only
  root-port HC reset

**exit criteria:** can hold multiple `uhc_device_t` in memory and enum more
than address 1 on the wire (still needs steps 2–3 to attach anything
behind a hub).

### 2. finish USBB host multi-address support

**files:** `usbb_host.c`, `usbb_host.h`, `uhd.h`

today’s TODOs when hub is defined:

- maintain a **list of configured USB addresses** (or fixed bitmap)
- control pipe 0 shared at **64 B** in hub mode; select device address
  per setup
- update / check address list on alloc/free / disconnect

also required in practice:

- `uhd_ep_alloc` / `uhd_ep_free` keyed by `(address, ep)` for many devices
- abort and free **all pipes for one address** on disconnect (partially
  present)
- SOF / bandwidth awareness if many interrupt endpoints (HID) share the
  frame

**exit criteria:** control and data transfers succeed to address ≠ 1
without corrupting root-device traffic.

### 3. implement USB hub class driver (`uhi_hub`)

**new files** under e.g. `asf/.../usb/class/hub/host/` or
`src/usb/hub/`, registered in `USB_HOST_UHI`.

hub class responsibilities (USB 2.0 ch. 11):

1. **install:** match `bDeviceClass` / interface class hub; read hub
   descriptor (`bNbrPorts`, power characteristics); alloc status-change
   interrupt IN endpoint
2. **enable:** power ports (`SET_FEATURE PORT_POWER`); arm status-change
   IN
3. **status-change loop:** on interrupt, `GET_PORT_STATUS` for changed
   ports; `CLEAR_FEATURE` change bits
4. **connect:** `SET_FEATURE PORT_RESET` → wait for reset complete →
   notify UHC to enumerate the new device (speed from port status)
5. **disconnect:** notify UHC to uninstall that downstream `uhc_device_t`
   and free its pipes
6. implement `uhi_hub_suspend` / `uhi_hub_send_reset` expected by `uhc.c`

keep **one static hub slot** (or fixed `USB_HOST_MAX_HUBS`) for the
fixed-N design.

**exit criteria:** plug a keyboard into a hub on the root port; UHC
enumerates it at a non-1 address.

### 4. make every UHI multi-instance (fixed slots)

today each aleph UHI is:

```c
static uhi_*_dev_t uhi_*_dev;  /* single */
if (uhi_*_dev.dev != NULL) return UHC_ENUM_SOFTWARE_LIMIT;
```

for hubs with multiple devices of the **same class** (two HID keyboards,
two MIDI interfaces, …) each UHI needs a **fixed array**:

```c
#define UHI_HID_MAX_DEV  N   /* or shared pool */
static uhi_hid_dev_t uhi_hid_dev[UHI_HID_MAX_DEV];
```

`install` finds a free slot; `enable`/`uninstall` match on `dev` pointer
or address.

classes to update (aleph path):

| UHI | file | notes |
|-----|------|-------|
| HID | `src/usb/hid/uhi_hid.c`, `hid.c` | frame buffer(s); see multi-iface [HID.md](HID.md) |
| MIDI | `src/usb/midi/uhi_midi.c` | rx/tx rings per instance |
| FTDI | `src/usb/ftdi/uhi_ftdi.c` | remove hub `#error`; monome grids |
| CDC | `src/usb/cdc/uhi_cdc.c` | monome CDC |
| MSC | ASF `uhi_msc.c` if enabled | already has incomplete hub stubs |

app callbacks (`UHI_*_CHANGE`, events) must carry **which device** (pointer,
address, or slot index), not a global singleton.

**exit criteria:** two HID devices behind a hub both install without
`SOFTWARE_LIMIT`.

### 5. app / event layer

**files:** `events.h`, app handlers, bees/device-test

- connect/disconnect/packet events need a device id (or slot)
- device-test focus policy: last connected device, or explicit select
  among devices **and** interfaces (see HID.md)
- bees operators: bind to a device index or “first HID”

without this, hub support in the stack is invisible to products.

### 6. conf knobs

in `conf_usb_host.h` (suggested):

```c
#define USB_HOST_HUB_SUPPORT
#define USB_HOST_MAX_DEVICES     4   /* downstream functions budget */
#define USB_HOST_MAX_HUBS        1
#define UHI_HID_MAX_DEV          4
#define UHI_MIDI_MAX_DEV         2
/* ... */
```

document that **sum of open data endpoints** must fit in 7 pipes and DPRAM
(next section).

### 7. testing plan (device-test)

- root device alone (regression)
- hub alone (enum hub, ports powered)
- hub + 1 HID keyboard
- hub + HID keyboard + HID mouse (two devices)
- hub + composite wireless dongle (one device, many interfaces)
- hotplug / unplug mid-stream
- pipe exhaustion: attach until `HARDWARE_LIMIT`, confirm clean failure

---

## pipe and DPRAM budget

UC3A0512 USBB:

| resource | limit |
|----------|-------|
| pipes | 8 total |
| pipe 0 | control (shared among addresses in hub mode) |
| data pipes | **7** |
| DPRAM | **2048 B** (FIFO) |

### typical data-pipe use per device

| device type | pipes | DPRAM (rough, 64 B maxpkt) |
|-------------|-------|------------------------------|
| HID (1 IN) | 1 | ~64 B (1 bank interrupt) |
| HID composite (3 IN) | 3 | ~192 B |
| MIDI (IN+OUT bulk) | 2 | ~256 B (2 banks × 64 × 2) |
| FTDI / CDC (IN+OUT) | 2 | ~256 B |
| hub status IN | 1 | ~8–64 B |

examples for N devices (illustrative):

- 4× simple HID keyboards: 4 pipes — OK
- 2× wireless dongles × 3 HID IN each: 6 pipes — OK until anything else
- 2× MIDI + 2× HID: 2×2 + 2 = 6 pipes — tight
- hub status IN + 3× MIDI: 1 + 6 = 7 — full

**fixed N must be chosen together with expected class mix**, not only head
count.

---

## per-device memory cost (estimate)

AVR32 ILP32. figures are approximate; measure with `sizeof` when
implementing.

### always paid per `uhc_device_t` (with hub fields)

| item | bytes | storage |
|------|------:|---------|
| `uhc_device_t` body + hub links | ~48 | static pool × (1+N) or malloc |
| `conf_desc` (`wTotalLength`) | ~32–256+ | **heap** (retained while connected) |
| control setup nodes | ~32 each | transient heap during enum |

use **~64–300 B RAM per device** for UHC bookkeeping + config descriptor
(composite configs at the high end).

### plus class instance (one “primary” UHI claim)

| class | static / slot .bss | heap on install | notes |
|-------|-------------------:|----------------:|-------|
| HID | ~12 (UHI) + 64 frame + 8 dirty ≈ **84** if shared frame; or ×slots | report buffer **8–64** | multi-iface may need more IN buffers |
| MIDI | ~8 + 128 (rx/tx) ≈ **136** per slot | 0 | |
| FTDI | ~8 + 64 rx + 192 strings ≈ **264** per slot | 0 | |
| CDC | ~8 + 64 rx ≈ **72** per slot | 0 | |
| MSC | ~12 + ~100 SCSI temps + **16×LUN** | LUN table | |

### plus DPRAM (not in .bss, but scarce)

budget **~64–256 B FIFO** per open data endpoint (see table above).

### worked example: N = 4, prefer HID

assume pool of 4 downstream devices, mostly HID, shared app frame policy
like today (one focused HID frame in device-test):

| layer | cost |
|-------|------|
| 4 × `uhc_device_t` + hub | ~200 B static |
| 4 × conf_desc avg 128 B | ~512 B heap |
| 4 × HID UHI slot + report 64 | ~4×(12+64) ≈ 300 B |
| app HID frame/dirty (shared) | ~72 B |
| DPRAM 4× HID IN | ~256 B FIFO |

**order of ~1 KB SRAM + ~0.25 KB DPRAM** for four simple HID devices,
before MIDI/FTDI/hub status pipe.

worst case four MIDI devices: ~4×136 B class + ~1 KB DPRAM + confs — still
small in SRAM, but **pipe/DPRAM limited**.

### hub device itself

add one `uhc_device_t` + conf + hub UHI state + status-change buffer
(~8–64 B) + 1 interrupt pipe. typically **&lt; 200 B** SRAM + one data
pipe.

---

## multi-interface vs multi-device (do not confuse)

| concept | unit | polling |
|---------|------|---------|
| multi-**interface** | endpoints on **one** address | each IN independently ([HID.md](HID.md)) |
| multi-**device** (hub) | multiple addresses | each device enum’d; each has its own interfaces |

hub work does not remove the need for per-interface IN management on a
composite device. it **multiplies** that problem by N devices.

also: vendored `uhi_hid` still claims only the **first** HID interface of
a given device unless extended (device-test already opens extras for the
single root device; with hubs, that pattern must be per-device slot).

---

## recommended approach for aleph

1. **fixed pools** everywhere (`USB_HOST_MAX_DEVICES`, per-UHI max) — no
   unbounded malloc of device nodes beyond conf_desc / report buffers
2. **depth-1 hub first** (root → one hub → N devices); defer nested hubs
3. **HID-first** product path (device-test): multi HID slots + hub; MIDI/
   FTDI/CDC next
4. treat **7 data pipes / 2 KB DPRAM** as the real capacity meter; document
   supported topologies (e.g. “≤4 HID” or “≤2 MIDI + ≤2 HID”)
5. extend events with device identity before enabling hubs in apps
6. keep device-test as the bring-up app (hub plug matrix in SPEC)

---

## effort sketch (coarse)

| step | relative effort | risk |
|------|-----------------|------|
| 1 UHC multi-device | large | high — core enum rewrite |
| 2 USBB multi-address | large | high — HC TODOs, pipe0 |
| 3 `uhi_hub` | medium–large | medium — well-spec’d class |
| 4 UHI arrays | medium per class | medium — app API breakage |
| 5 app events | medium | low–medium |
| 6 conf + docs | small | low |
| 7 device-test matrix | ongoing | — |

steps 1–3 are the blocking dependency chain; 4–5 can proceed in parallel
once multi-`uhc_device_t` exists.

---

## references in-tree

- `vendor/libavr32/asf/common/services/usb/uhc/uhc.c` — hub `#error`, enum
  steps, hub `#ifdef` scaffolding
- `vendor/libavr32/asf/common/services/usb/uhc/uhc.h` — `uhc_device_t` hub
  fields
- `vendor/libavr32/asf/avr32/drivers/usbb/usbb_host.c` — address-list TODOs
- `vendor/libavr32/asf/avr32/drivers/usbb/usbb_host.h` —
  `AVR32_USBB_EPT_NUM` (8)
- `vendor/libavr32/src/usb/hid/uhi_hid.c` — single-slot HID claim
- `vendor/libavr32/conf/aleph/conf_usb_host.h` — `USB_HOST_UHI`, power max
