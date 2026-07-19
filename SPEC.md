# between

between is a control application for aleph focused on capturing and morphing
dsp module parameters.

unlike bees, between has no operator network. its job is simpler: load a
module, hold four parameter presets at the corners of a 2d plane, and blend
those presets by position within the unit square.

---

## goals

- capture module parameters into simple, human-editable preset text files.
- load up to four presets for the same module into corner slots.
- morph continuously between those presets from an (x, y) control point.
- save and recall complete performance setups.
- provide bees-like play/edit modes with immediate auditory feedback while
  editing.

non-goals (v1):

- operator networks, routing, or scene graphs.
- morphing connections or non-parameter state.

---

## terminology

| term | meaning |
|------|---------|
| **module** | a dsp executable on the blackfin (`.ldr` + `.dsc`), same as bees. |
| **parameter** | a named module input with type, range, and current value. |
| **preset** | a named snapshot of parameter values for one module, stored as its own text file. |
| **slot** | one of four live preset positions (a–d) at the corners of the morph plane. |
| **morph point** | an (x, y) position in the unit square, stored as `u16` values in `[0, 65535]`, that selects the blend among the four slots. |
| **effective parameters** | the interpolated parameter set currently sent to the module. |
| **setup** | a performance configuration: module identity, which presets occupy which slots, and the initial morph point. |
| **play mode** | live performance: morph control and mappable encoders/switches dominate the front panel. |
| **edit mode** | configuration and tuning: module, presets, slots, and parameters are browsed and changed. |

---

## core model

### slots and the morph plane

four slots sit at the corners of a unit square. origin is at slot a
(upper-left); x increases right, y increases downward:

```text
  x=0,y=0           x=1
   a ●─────────● b
     │         │
     │    ●    │   ← morph point (x, y)
     │         │
   c ●─────────● d
        y = 1
```

| slot | corner | (x, y) |
|------|--------|--------|
| a | top-left | (0, 0) |
| b | top-right | (1, 0) |
| c | bottom-left | (0, 1) |
| d | bottom-right | (1, 1) |

at any morph point `(x, y)`, each slot contributes with bilinear weights:

```text
wa = (1 - x) * (1 - y)
wb = x * (1 - y)
wc = (1 - x) * y
wd = x * y
```

weights always sum to 1. when a slot is empty (no preset loaded), its weight
is redistributed among the remaining occupied slots, or the empty corner is
treated as “hold last / ignore” — see [empty slots](#empty-slots).

### effective value

for each continuous parameter `p`:

```text
p_eff = wa*pa + wb*pb + wc*pc + wd*pd
```

interpolation is performed in the parameter’s **native dsp domain** (32-bit
module values), then written to the running module. discrete types do not
blend; see [interpolation rules](#interpolation-rules).

moving the morph point updates effective parameters continuously. editing a
slot’s parameters updates that slot’s stored values and immediately
recomputes the effective set, so tuning by ear works while morphing.

### empty slots

- a slot may be empty (no preset assigned).
- morphing only uses occupied slots. weights of empty slots are zeroed and
  the remaining weights are renormalized.
- if only one slot is occupied, the morph point has no effect; effective
  parameters equal that slot.
- if no slots are occupied, the module keeps its last written / default
  parameter state.

### module identity

all four slots must belong to the **same module** (name match). loading a
preset whose module name differs from the currently loaded module is
rejected (or offered as “load module + clear other slots” — see ux).

preset **version** is advisory:

- same major.minor: load normally.
- mismatched minor or major: warn and load best-effort by parameter name.
- unknown parameters in the file are ignored; missing parameters keep the
  module default (or current value — decide at implementation; prefer
  module default on fresh load).

---

## file formats

### paths

| kind | path |
|------|------|
| presets | `/data/between/presets/<module>/<name>.txt` |
| setups | `/data/between/setups/<name>.txt` |

preset files are grouped by module directory so browsing stays scoped.

### preset file (`.txt`)

plain text, utf-8, lf line endings. one file per preset.

```text
# between preset
format: 1
module: waves
version: 0.4.5

hz0: 12000
amp0: -12.0
cut0: 0.35
res0: 0.2
```

the preset name is the file name without the `.txt` extension; it is not
stored inside the file.

rules:

1. lines starting with `#` are comments and ignored.
2. blank lines are ignored.
3. metadata keys appear first; required keys:
   - `format` — preset file-format version (`u8` integer, starts at `1`).
   - `module` — module name string (matches `.ldr` basename / module name).
   - `version` — module `maj.min.rev` as reported by the module.
4. after metadata, each line is `paramlabel:value`.
5. labels must match the module’s `.dsc` parameter labels.
6. values are raw native `ParamValue` integers (`s32`) as used by the DSP.
   the slot editor UI presents and edits them through Bees-compatible
   ParamType scalers (e.g. dB for amp, note-ish for note); presets and morph
   banks always store the raw integer form.
7. only parameters present in the file are part of the preset. on load,
   unspecified parameters remain at module default (or are filled from the
   current effective state when “capture all” is used — see save).
8. parameter order in the file need not match `.dsc` order; the ui may
   rewrite files in `.dsc` order on save.

### setup file (`.txt`)

plain text, same comment/blank-line rules.

```text
# between setup
format: 1
module: waves
version: 0.4.5

slot.a: soft-pad
slot.b: bright-pad
slot.c: noise-bed
slot.d: soft-pad

x: 22937
y: 39321
```

the setup name is the file name without the `.txt` extension; it is not
stored inside the file.

rules:

1. required keys:
   - `format` — setup file-format version (`u8` integer, starts at `1`).
   - `module` — module name string.
   - `version` — module `maj.min.rev`.
2. `slot.a` … `slot.d` — preset **filename stem** (without `.txt`) under
   that module’s preset directory. omit or use `-` / empty for an empty
   slot.
3. `x`, `y` — initial morph point as `u16` integers in `[0, 65535]`
   (full scale = unit square corners).
4. loading a setup: if needed, replace the current module with the setup’s
   module, load each referenced preset into its slot, set the morph point,
   and apply the effective parameters.

future setup keys (follow-on; reserved names):

- `cv.x`, `cv.y` — cv input mapping
- `lfo.*` — lfo type / rate / depth
- `foot.*` — footswitch targets
- `play.enc0` … `play.enc3` — encoder map (target, optional slot, optional
  param label)
- `play.sw0` … `play.sw3` — switch map (snap corner, or param
  set/momentary with slot scope, label, and value)

morph MIDI (channel 16, CC14 / CC15) is fixed in [midi](#midi); it is not
stored as setup keys unless learn/reassign returns later.

---

## interpolation rules

| param type | morph behavior |
|------------|----------------|
| fix, amp, integrator, note, svffreq, fract, short, integratorshort | linear blend in native dsp value space. |
| bool | nearest occupied corner by morph distance, or threshold on dominant weight (pick one; prefer **snap to highest-weight slot**). |
| label | same as bool: discrete, take value from highest-weight occupied slot. |

notes:

- no slew is applied by between itself beyond writing the blended values;
  modules may still have their own integrator/slew parameters, which are
  themselves morphable like any other fix/integrator param.
- when **sending** the effective set to the module (`slots_apply`), between
  uses a **type-priority schedule** built at module load: integrator /
  integrator-short params are written first, then all other types in
  descriptor index order. this affects SPI send order only — UI parameter
  lists, capture into slot banks, and preset storage stay in descriptor
  index order.
- extremely nonlinear perceptual mappings (amp, note) will not feel
  perceptually linear in the middle of the square; that is accepted for v1.
  a later option could morph in “display/control” space instead.

---

## modes

hardware **mode** switch (same as bees / `switch4`):

- toggle between play and edit.
- mode led on = play.
- leaving play restores the last edit page.
- boot default: play if a last-used setup exists; otherwise edit on the
  setups page.

---

## edit mode ux

edit pages form a ring, navigated like bees (enc1 = page, enc0 = selection
unless a page overrides). setups is the first page:

```text
setups → modules → slots → slot a → slot b → slot c → slot d → play → setups
```

common conventions (align with bees):

- enc0: primary selection / scroll
- enc1: page navigation
- enc2 / enc3: value edit (fine / coarse) where applicable
- sw3: alt
- footer labels the four switches for the current page

### setups page

- list of `.txt` files under `/data/between/setups/`.
- first content line: currently loaded setup name (2px mid-grey bar, 3px
  spacer, name aligned with the header title), or `none`.
- the setup list fills the remaining content rows above the log.
- directory listing is scanned automatically the first time the page is
  entered; afterward only via hold alt, then sw2 **scan**.
- sw0 **load**: load the selected setup.
  - if its module differs from the currently loaded module, load the setup’s
    module, replacing the current module.
  - load the referenced presets into the four slots, set the saved morph
    point, and apply the effective parameters.
- sw1 **save**: write the current configuration to the current setup name
  (`g_setup_name` / status line), creating that file if needed (module,
  slots, morph point, and play bindings). if no name is set yet, allocate a
  unique `sNNN` stem.
- alt+sw1 **save as**: open the name-entry modal (header `setup name`)
  prefilled with the current stem (or a unique `sNNN` if unset); OK writes
  under the chosen name.
- sw2 **new**: begin a new setup with a unique `sNNN` name and jump to the
  modules page for module selection.
- alt+sw0 **delete** with confirm.

### modules page

- scrolling list of modules from `/mod/` (`.ldr` basename).
- first content line: currently loaded module (2px mid-grey bar, 3px spacer,
  name aligned with the header title), or `none`.
- the module list fills the remaining content rows above the log.
- directory listing is scanned automatically the first time the page is
  entered; afterward only via hold alt, then sw2 **scan**.
- sw0 **load**: load the selected module, replacing the current module.
- loading a module clears all slots and resets morph point to `(0, 0)`
  unless loading as part of a setup recall.
- when reached through **new** on the setups page, loading a module continues
  the new-setup flow on the slots page.

### slots page

the slots page combines slot overview and preset selection. it shows the
four slots and their assigned presets in a 2x2 grid. column b/d starts at
the horizontal midpoint; each preset name is left-aligned with its slot
letter:

```text
a              b
soft-pad       bright-pad

c              d
noise-bed      soft-pad
```

- enc0: select slot a–d (highlight).
- enc2 / enc3: adjust morph x / y while editing (optional but useful for
  auditioning blends).
- sw0 **preset**: open the preset selector for the selected slot.
- sw1 **edit**: jump to the selected slot’s editor page.
- sw2 **clear**: empty the selected slot (does not delete the preset file).
- sw3 alt; alt+sw2 **clear all**.

the preset selector is a modal list scoped to the currently loaded module:

- enc0: scroll through preset files.
- sw0 **load**: load the highlighted preset into the selected slot and
  return to the slots page.
- sw1 **cancel**: return without changing the slot.
- sw2 **new**: create a new uniquely named preset from **module defaults**,
  assign it to the selected slot, and return to the slots page. to bake the
  current effective morph into that slot afterward, open the slot editor and
  use alt+**capture**.
- alt+sw0 **delete**: delete the highlighted preset with confirmation.
- alt+sw1 **refresh**: rescan the preset directory.

if no module is loaded, the slots page displays `empty`; preset selection
redirects to the modules page.

### slot pages

when a preset is loaded, the header shows a capital slot letter box and a
separate preset-name box. if the slot is empty, only the letter box is shown.

body: scrolling parameter list from the module `.dsc`, showing each
parameter’s **slot-stored value** (not the effective blend). beside or
under the list, a compact readout of the effective value can help, but is
optional for v1.

if no preset is loaded in the slot, the page displays only:

```text
empty
```

no parameter list is shown. controls on an empty slot:

- sw0 **new**: create a uniquely named preset from module defaults, assign
  it to this slot, and enter the normal parameter editor.

when a preset is loaded, controls are:

- enc0: select parameter
- enc2: fine adjust selected parameter (slot value)
- enc3: coarse / accelerated adjust
- sw0 **save**: write current slot values to the assigned preset file
  (create file if the slot was filled from new/unsaved capture). if the
  slot has no file yet, behave like new then save.
- sw1 **reset**: reload slot values from the assigned preset file
  (discard unsaved edits). disabled / no-op if empty or never saved.
- sw2 **new**: create a new uniquely named preset from **module defaults**
  and assign it to this slot (replaces in-memory slot values). to store the
  current effective morph instead, use alt+**capture** after creating or
  loading a preset.
- sw3 alt
- alt+sw0 **save as**: open the name-entry modal (header `preset name`)
  prefilled with the current stem (or a unique `pNNN` if unset); OK writes
  under the chosen name and assigns it to this slot (original file left
  untouched if the name differs).
- alt+sw1 **capture eff**: overwrite this slot’s in-memory values with the
  current effective blend (useful for “bake” a morph position into a
  corner).
- alt+sw2 **focus**: snap the morph point to this slot’s corner so live
  edits match the values on this page exactly.

generated preset names use the form `pNNN` and skip any stem already
present on disk for the module or assigned to a slot in memory.
**live update:** every encoder change to a slot parameter updates that
slot’s in-memory values, recomputes the effective set for the current
morph point, and sends parameters to the module immediately.

unsaved edits: show a light-grey `*` after the preset-name box in the
header (1px black spacer). the upper-right header chrome always shows the
current morph position (mid-grey outline, white 3×3 cursor); when MIDI is
connected, a dark-grey `m` sits immediately left of that indicator and
flashes light grey on received traffic (see [midi](#midi)). leaving the
page keeps in-memory dirty state until save or reset; setup save should
warn if dirty.

### play page

edit-mode page for configuring **play bindings** (encoder and switch maps).
bindings are properties of the current setup: edited here in memory, written
on setup **save**, restored on setup **load**. they are not stored in preset
files.

header: `play` (plus dirty indicator if maps differ from the last saved
setup, if that distinction is tracked).

body: a selectable list of the eight controls (`enc0`–`enc3`, `sw0`–`sw3`).
each line shows a short summary of the current binding (e.g. `enc2: morph x`,
`sw0: snap a`, `enc0: slot.b / amp`).

controls:

- enc0: select which control’s binding to edit
- enc1: page navigation
- enc2 / enc3: adjust the selected binding’s fields (target kind, slot,
  param, set/momentary mode, value — field focus can be a sub-selection or
  stepped with alt; exact field ux may be refined)
- sw0 **reset**: restore the selected control to its [default map](#default-encoder-map)
  / [default switch map](#default-switch-map)
- sw1 **reset all**: restore all eight controls to defaults (confirm if
  needed)
- sw2: reserved (or **copy defaults** / clear — open)
- sw3: alt

if no module is loaded, param-target bindings cannot be chosen (morph and
snap targets remain available). changing module invalidates bindings that
refer to missing param labels (clear or leave unbound until fixed).

---

## play mode ux

play remaps the front panel away from menu navigation. encoders and
switches are **mappable** to internal play targets. all play bindings are
owned by the **setup**: configure them on the edit-mode [play page](#play-page),
persist them with setup save/load (see reserved `play.enc*` / `play.sw*`
keys).

encoder and switch indices match bees / hardware (`enc0`–`enc3`,
`sw0`–`sw3`).

### encoder targets

each encoder may be mapped to one of:

| target | behavior |
|--------|----------|
| morph x | drive the morph point’s x coordinate |
| morph y | drive the morph point’s y coordinate |
| slot param (absolute) | adjust **one** named parameter on **one** specified slot (a–d); updates that slot’s bank and reapplies the effective set |
| all-slots param (absolute) | adjust the same named parameter on **all occupied** slots to the same absolute value (each bank gets the same raw/`io_t` result) |
| all-slots param (relative) | **stretch goal:** vca-group style — nudge the same named parameter on all occupied slots by a shared relative delta (preserve per-slot offsets / ratios as far as the param type allows) |

notes:

- param targets use the module’s `.dsc` label (and bees-style scaler
  stepping when editing, same as the slot page).
- empty slots are skipped for all-slots targets.
- discrete params (bool / label) on encoder maps: step through legal
  values; relative all-slots for discrete is undefined until the stretch
  goal is designed.
- unmapped encoders do nothing.

### default encoder map

by default the **bottom** two encoders drive morph position:

| control | default |
|---------|---------|
| enc0 | unmapped (reserved for user maps / follow-on) |
| enc1 | unmapped (reserved for user maps / follow-on) |
| enc2 | morph x |
| enc3 | morph y |

### switch targets

each switch may be mapped to one of:

| target | behavior |
|--------|----------|
| snap slot a–d | jump morph point to that corner (immediate) |
| param set (one slot) | write a configured value into one named param on one specified slot |
| param momentary (one slot) | while held, force that param on that slot to a configured value; on release, restore the pre-press slot value |
| param set (all slots) | write a configured value into the same named param on all occupied slots |
| param momentary (all slots) | while held, force that param on all occupied slots to a configured value; on release, restore each slot’s pre-press value |

notes:

- **set** is latching (press applies; no automatic restore).
- **momentary** stores the previous bank value(s) on press and restores on
  release; if the mapping is changed while held, restore still uses the
  press-time snapshot.
- snap and param maps are mutually exclusive per switch (one target each).
- unmapped switches do nothing.

### default switch map

| control | default |
|---------|---------|
| sw0 | snap to slot a |
| sw1 | snap to slot b |
| sw2 | snap to slot c |
| sw3 | snap to slot d |
| mode | return to edit |

### display

play layout (128×64, bees-style footer cells for `sw0`–`sw3`):

```text
┌──────────────────────────────────────┐
│ ▌ setup name              [m] [morph] │  ← header (edit-style)
├────────────┬─────────────────────────┤
│ morph      │ enc0 label   enc1 label │
│  (square)  │ enc0 value   enc1 value │
│            │ enc2 label   enc3 label │
│            │ enc2 value   enc3 value │
├──────┬─────┴──┬──────────┬───────────┤
│ sw0  │  sw1   │   sw2    │   sw3     │
└──────┴────────┴──────────┴───────────┘
```

**header:** same chrome as edit mode — 2px mid-grey bar, gap, then the
current setup name in a white text box (or `none` if unset). upper-right
morph-position indicator and optional MIDI `m` as on other pages.

**morph position (left):** a square region above the switch labels. a light
gray square frame marks the unit morph plane; the current morph point is a
**3×3 white** block inside that frame (position scaled from `(x, y)` in
`[0, 65535]`).

**encoder readouts (right):** four compact bindings in a 2×2 grid (same
column split idea as the slots page: left column enc0/enc2, right column
enc1/enc3). each cell is a **label** line for what the encoder is mapped to,
and the line below for its **current value** (scaled string when applicable;
morph axes may show normalized or raw position). unmapped encoders show a
blank or `-` label/value.

**switch footer labels:**

- snap-to-slot maps: label is just the slot letter (`A`, `B`, `C`, `D`).
- param maps: label is the param name (truncated to fit the 32px-wide
  footer cell). if the target is a **single slot**, paint a **dark gray
  3-pixel right triangle** in the footer cell background in the corner that
  matches that slot on the morph plane (a = top-left, b = top-right,
  c = bottom-left, d = bottom-right). all-slots param maps omit the
  triangle.
- unmapped switches: blank or `-`.

keep chrome minimal; play is for performing, not editing.

### persistence

encoder and switch maps (target kind, slot if any, param label if any,
set/momentary value if any) are stored **only** in the setup file. loading a
setup replaces the in-memory play bindings; saving a setup writes the
current bindings from the play page. see reserved setup keys below.

---

## parameter capture policy

when creating or saving a preset:

- **default (new / save from slot):** store **all** module parameters at
  their current slot (or effective, depending on action) values.
- file always contains a complete parameter list for robust reload.
- there is no bees-style include/exclude bit in v1; presets are complete
  module captures.

this keeps the text format and mental model small. selective morphing
(ignore some params) can be a follow-on via per-param lock or a second
file section.

---

## startup and persistence

on boot:

1. init ui; scan modules.
2. if `/data/between/state` (or last setup path) exists, load it.
3. else enter edit → modules.

autosave of last morph point + slot assignments is desirable so play
survives power cycles without requiring an explicit setup write. explicit
setups remain the portable unit.

---

## midi

MIDI is a live control path alongside the panel. channel numbering below is
**1-based** (MIDI channel 1 = status nibble `0`).

### channel map

| MIDI channel | role |
|-------------:|------|
| 1 | slot A |
| 2 | slot B |
| 3 | slot C |
| 4 | slot D |
| 16 | setup (morph and other setup-level controls) |

messages on channels 1–4 affect the corresponding slot (A–D). the exact
per-slot message set (note / CC / program change, which CCs map to which
parameters, etc.) is still to be specified; this section locks the
**channel → slot** assignment.

channel 16 is reserved for **setup-level** parameters (not a single slot’s
preset values). morph position is the first of those.

### setup channel: morph position

on channel 16:

| CC | control | mapping |
|---:|---------|---------|
| 14 | morph x | value `0`…`127` → morph x `0`…`65535` (full plane) |
| 15 | morph y | value `0`…`127` → morph y `0`…`65535` (full plane) |

map linearly: `morph = (cc * 65535) / 127`. a CC of `0` is the low edge of
the axis; `127` is the high edge (`MORPH2D_ONE`). after updating x and/or y,
recompute effective parameters and send them to the module (same as panel
morph moves), and refresh the current page UI (play-mode morph square and
encoder readouts, header morph cursor, etc.).

panel encoders / play maps that drive morph continue to work; MIDI and panel
both write the same morph point (last writer wins unless a later rule defines
summing).

### header midi indicator

all pages that draw the edit-mode header (and play mode when a header is
shown) reserve space **immediately left of** the upper-right morph-position
indicator for a MIDI presence glyph:

```text
… title / name boxes …   [m]  [morph 8×8]
```

- when a MIDI device is **connected** to the aleph, draw a lowercase `m` in
  **dark grey**.
- when no MIDI device is connected, omit the `m` (leave that space black /
  empty; do not shift the morph indicator).
- when MIDI **traffic is received**, briefly flash the `m` in **light grey**,
  then return to dark grey while the device remains connected.

the flash should be short enough to read as activity (on the order of the
diagnostic log clear time or shorter), and may retrigger on further messages
without requiring the glyph to go dark between bursts.

layout note: keep a 2px black gap between the `m` and the morph indicator box
so the two reads stay distinct. the `m` is not drawn inside a white text box
(unlike page titles); it is a lone glyph on the black header background.

### still open (midi)

- which messages on channels 1–4 edit or recall slot state.
- 14-bit CC pairs for finer morph resolution.
- MIDI learn / editable CC numbers in setup files (earlier reserved
  `midi.x` / `midi.y` keys are superseded by the fixed CC14 / CC15 mapping
  above unless learn mode returns).
- how “MIDI device connected” is detected on this hardware (USB host enum,
  UART activity, etc.).

---

## follow-on features

these are out of scope for the first usable version but should inform
naming and play control reservations.

### midi (further)

- see [midi](#midi) for the locked channel map and morph CC14 / CC15.
- learn mode from play or a small midi page in edit (if CCs become
  reassignable again).
- additional setup-channel CCs beyond morph.

### footswitch

- go-to-position: press jumps morph point to a stored (x, y) or corner.
- toggle: press swaps between a stored position and the pre-press position.
- two footswitches → two independent targets, or a/b bank.

### cv control of (x, y)

- map aleph cv inputs to x and y with attenuversion / bias.
- combine with panel encoders (sum, or cv replaces encoder when connected).

### front-panel play maps

- defaults and target kinds are specified under [play mode ux](#play-mode-ux).
- binding configuration ui is the edit-mode [play page](#play-page).
- relative all-slots encoder control (vca-group style) is a stretch goal.

### lfos on (x, y)

- independent or linked lfos for x and y.
- types: triangle, sine, square, random/s&h, wander.
- rate, depth, phase; depth scalable from play via **mapped** encoders
  (not hard-wired to enc2/enc3).
- lfo output sums with manual / midi / cv position, then clamps to
  `[0, 65535]`.

### play-mode modulation scaling

- map encoders to lfo/cv depth for x / y when those modulators exist.
- display depth as percentage or bipolar attenuverter.

### selective parameter morph

- per-parameter lock to a single slot or to “fixed effective”.
- allows static params while others morph.

### morph curves

- optional perceptual interpolation for amp / note.
- optional easing curves on axes.

---

## implementation notes (non-normative)

- reuse aleph module load path (`.ldr` / `.dsc` / `.lab`) as bees does;
  between does not need the operator network or scene blob.
- prefer text i/o over binary for presets and setups; keep parsers
  tolerant of extra keys for forward compatibility.
- recompute and send all parameters when the morph point moves, or dirty
  only changed params if profiling requires it.
- dirty-slot and dirty-setup flags should be visible in the ui.
- mirror bees mode led and encoder/switch indexing for user familiarity.

---

## open questions

1. **bool/label morph:** highest-weight slot vs distance-to-corner snap?
   (recommendation: highest-weight slot.)
2. **setup extension format:** stay flat `key:value` or allow nested
   sections later?
3. **play map value syntax:** store encoder/switch map values as raw
   `ParamValue` (like presets) or as display-oriented strings?
4. **relative all-slots (vca group):** preserve linear offsets in raw
   domain, or attempt gain-style ratios for amp params only?

---

## acceptance criteria (v1)

- [ ] load a module from the modules page.
- [ ] create a preset file from current parameters; reload it into a slot.
- [ ] select a slot on the slots page and load its preset through the modal
      preset selector.
- [ ] assign four presets to a–d; morph with enc2/enc3 in play (default map).
- [ ] editing a parameter on a slot page changes sound immediately at the
      current morph point.
- [ ] an empty slot page displays only `empty`.
- [ ] save / reset / new behave as specified on the slot page.
- [ ] default snap switches in play jump to corner presets a–d.
- [ ] play page in edit mode can change encoder/switch bindings; setup
      save/load restores those bindings.
- [ ] save and load a setup restoring module, four slots, and morph point;
      loading it replaces a different currently loaded module.
- [ ] empty slots renormalize weights without crashing or silencing audio
      unexpectedly.
- [ ] preset files are readable/editable as text on the sd card.
