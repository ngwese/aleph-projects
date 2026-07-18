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
| **play mode** | live performance: morph control and modulation dominate the front panel. |
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
6. values are human-readable forms appropriate to the parameter type where
   practical (e.g. db for amp, hz or note-ish for note, `0`/`1` for bool).
   an implementation may also accept raw native integers; the written form
   should prefer the display form used in the ui.
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

- `midi.x`, `midi.y` — cc assignments
- `cv.x`, `cv.y` — cv input mapping
- `lfo.*` — lfo type / rate / depth
- `foot.*` — footswitch targets

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
setups → modules → slots → slot a → slot b → slot c → slot d → setups
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
- sw1 **save**: write the current configuration to the selected / named setup.
- sw2 **new**: begin a new setup and jump to the modules page for module
  selection.
- alt+sw0 **delete** with confirm.
- enc2/enc3: name edit when creating/renaming.

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
- sw2 **new**: create a new uniquely named preset from the current
  effective parameters, assign it to the selected slot, and return to the
  slots page.
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
- sw2 **new**: capture this slot’s current values into a new uniquely
  named preset file and assign it to this slot.
- sw3 alt
- alt+sw0 **save as**: write to a new unique filename, leave original
  untouched, assign new file to this slot.
- alt+sw2 **capture eff**: overwrite this slot’s in-memory values with the
  current effective blend (useful for “bake” a morph position into a
  corner).

generated preset names use the form `pNNN` and skip any stem already
present on disk for the module or assigned to a slot in memory.
**live update:** every encoder change to a slot parameter updates that
slot’s in-memory values, recomputes the effective set for the current
morph point, and sends parameters to the module immediately.

unsaved edits: mark the slot dirty (2x2 square in the header indicator).
leaving the page keeps in-memory dirty state until save or reset; setup
save should warn if dirty.

---

## play mode ux

play remaps the front panel away from menu navigation.

### morph control (v1)

primary interaction is positioning the morph point.

proposed default mapping:

| control | function |
|---------|----------|
| enc0 | morph x |
| enc1 | morph y |
| enc2 | (reserved follow-on: mod depth x) |
| enc3 | (reserved follow-on: mod depth y) |
| sw0 | snap to slot a |
| sw1 | snap to slot b |
| sw2 | snap to slot c |
| sw3 | snap to slot d |
| mode | return to edit |

snap is immediate (morph point jumps to that corner). optional later:
momentary snap-while-held vs toggle (see footswitch follow-on).

### display

play screen shows:

- setup name (if any) and module name
- morph point as a simple 2d indicator (crosshair / block in a square)
- slot names at corners (abbreviated)
- optional: last changed parameter / value line

keep it sparse; play is for performing, not editing.

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

## follow-on features

these are out of scope for the first usable version but should inform
naming and play control reservations.

### midi control of (x, y)

- assign midi ccs to x and y (14-bit optional later).
- learn mode from play or a small midi page in edit.
- store assignments in setup metadata.

### footswitch

- go-to-position: press jumps morph point to a stored (x, y) or corner.
- toggle: press swaps between a stored position and the pre-press position.
- two footswitches → two independent targets, or a/b bank.

### cv control of (x, y)

- map aleph cv inputs to x and y with attenuversion / bias.
- combine with panel encoders (sum, or cv replaces encoder when connected).

### front-panel snap in play

- already sketched: sw0–sw3 snap to a–d.
- extend with alt-hold for intermediate positions (edges / center) if
  needed.

### lfos on (x, y)

- independent or linked lfos for x and y.
- types: triangle, sine, square, random/s&h, wander.
- rate, depth, phase; depth scalable in play via enc2/enc3.
- lfo output sums with manual / midi / cv position, then clamps to
  `[0, 65535]`.

### play-mode modulation scaling

- enc2 / enc3 set modulation depth for x / y (lfo and/or cv).
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

1. **value syntax in preset files:** display units only, raw native only, or
   both accepted? (recommendation: write display form; accept display form
   primarily.)
2. **bool/label morph:** highest-weight slot vs distance-to-corner snap?
   (recommendation: highest-weight slot.)
3. **setup extension format:** stay flat `key:value` or allow nested
   sections later?

---

## acceptance criteria (v1)

- [ ] load a module from the modules page.
- [ ] create a preset file from current parameters; reload it into a slot.
- [ ] select a slot on the slots page and load its preset through the modal
      preset selector.
- [ ] assign four presets to a–d; morph with enc0/enc1 in play.
- [ ] editing a parameter on a slot page changes sound immediately at the
      current morph point.
- [ ] an empty slot page displays only `empty`.
- [ ] save / reset / new behave as specified on the slot page.
- [ ] snap switches in play jump to corner presets.
- [ ] save and load a setup restoring module, four slots, and morph point;
      loading it replaces a different currently loaded module.
- [ ] empty slots renormalize weights without crashing or silencing audio
      unexpectedly.
- [ ] preset files are readable/editable as text on the sd card.
