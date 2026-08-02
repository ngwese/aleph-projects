# between

a control app for aleph.

## contents

- [overview](#overview)
  - [the morph plane](#the-morph-plane)
  - [terminology](#terminology)
- [controls and workflows](#controls-and-workflows)
  - [modes and common conventions](#modes-and-common-conventions)
  - [page navigation](#page-navigation)
  - [workflows](#workflows)
- [common header and status indicators](#common-header-and-status-indicators)
  - [modification indicator](#modification-indicator)
  - [xrun warning (](#xrun-warning)`!!!`[)](#xrun-warning)
  - [midi (](#midi-m)`m`[)](#midi-m)
  - [vu grid](#vu-grid)
  - [morph indicator](#morph-indicator)
  - [diagnostic log](#diagnostic-log)
- [page reference](#page-reference)
  - [setups](#setups)
  - [modules](#modules)
  - [slots](#slots)
  - [slots — preset modal](#slots-preset-modal)
  - [slot pages (a–d)](#slot-pages-ad)
  - [play maps](#play-maps)
  - [info](#info)
  - [play (live)](#play-live)
  - [inspect — i/o](#inspect-io)
  - [inspect — cv in](#inspect-cv-in)
- [files on the sd card](#files-on-the-sd-card)
  - [directory layout](#directory-layout)
  - [presets and module name](#presets-and-module-name)
  - [file format overview](#file-format-overview)
- [module pairings](#module-pairings)
  - [mx44](#mx44)



## overview

between is a control application focused on capturing and morphing dsp module
parameters. the inspiration for the app was wondering what would happen if the
vector mixing control on wavestation was combined with the parameter snapshot
feature in bees.

unlike bees, between has no operator network. some basic mapping of panel
controls, midi, and cv inputs is to drive parameters is available but the focus
remains squarely on discovering what happens when moving within the parameter
space set out. one loads a module, places up to four parameter presets in each
corner **slots**, and blends those presets by moving a **morph point** inside a
unit square. a **setup** remembers the whole performance configuration so you
can recall it later.

### the morph plane

four slots sit at the corners of a unit square. origin is at slot **a**
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


| slot | corner       | (x, y) |
| ---- | ------------ | ------ |
| a    | top-left     | (0, 0) |
| b    | top-right    | (1, 0) |
| c    | bottom-left  | (0, 1) |
| d    | bottom-right | (1, 1) |


at any morph point, between bilinearly blends the occupied slots and sends
the result to the running module. empty slots contribute no weight; if only
one slot is occupied, the morph point has no effect.

### terminology


| term                        | meaning                                                                                                                                                    |
| --------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **module**                  | a dsp executable on the blackfin (`.ldr` + `.dsc`), same as bees.                                                                                          |
| **parameter**               | a named module input with type, range, and current value.                                                                                                  |
| **preset**                  | a named snapshot of parameter values for one module, stored as its own text file.                                                                          |
| **slot**                    | one of four live preset banks (a–d) at the corners of the morph plane.                                                                                     |
| **morph** / **morph point** | an (x, y) position in the unit square that selects the blend among the four slots.                                                                         |
| **effective parameters**    | the interpolated parameter set currently sent to the module.                                                                                               |
| **setup**                   | a performance configuration: which module, which presets occupy which slots, the morph point, play bindings, and which parameters are excluded from morph. |




## controls and workflows



### modes and common conventions

at a high level the ui is organized around three modes. if the state file on the
sdcard points to an existing setup then that setup will be loaded automatically
when the device first boots and you will be left in the **play** mode. if some
aspect of the setup fails to load then the **edit** mode will be displayed
allowing a different setup to be selected.


| mode        | purpose                                                                                          |
| ----------- | ------------------------------------------------------------------------------------------------ |
| **play**    | live performance: morph control and mapped encoders / switches dominate the panel. mode LED off. |
| **edit**    | configure setups, modules, slots, parameters, and play maps. mode LED on.                        |
| **inspect** | diagnostics: audio i/o meters and CV input readouts. mode LED on.                                |


the overall ui uses several conventions established by the bees app in the
intention of making easy to navigate.

#### mode button

the hardware **MODE** switch acts on **release**, not press:


| gesture       | from         | to                                  |
| ------------- | ------------ | ----------------------------------- |
| short release | edit         | play                                |
| short release | play         | edit (last edit-ring page)          |
| short release | inspect      | the mode you were in before inspect |
| long release  | edit or play | inspect                             |
| long release  | inspect      | stay on inspect                     |


mode LED **off** means play; **on** means edit or inspect. each mode remembers
which page was previously selected and returns to that page when switching
between modes.

#### encoders

generally speaking the encoders maintain a consistent mapping across the edit
and inspect pages. the play mode differs because it allows panel controls to be
bound to various parameters.


| encoder  | typical role                                                      |
| -------- | ----------------------------------------------------------------- |
| **enc0** | primary selection / scroll on the current page                    |
| **enc1** | page navigation in the edit ring; in inspect, cycle subpages      |
| **enc2** | fine value adjust when the page has a local value                 |
| **enc3** | coarse / accelerated value adjust when the page has a local value |


edit pages never use enc2/enc3 for silent morph control — morph lives in
play mode (and MIDI).

#### softkeys and alt


| switch                      | role                                      |
| --------------------------- | ----------------------------------------- |
| **sw0** / **sw1** / **sw2** | labeled actions in the footer             |
| **sw3**                     | **alt** — hold to show the alt footer set |


holding alt sets alt mode; releasing clears it. changing pages always clears
alt. the footer always shows the four labels for the current (normal or alt)
set.

#### name entry

the name entry modal is presented when renaming setups or presets, while it is
open the encoders and softkeys are mapped differently:


| softkey    | action                                                                                      |
| ---------- | ------------------------------------------------------------------------------------------- |
| **select** | hold to show the character palette; turn enc2 while held to pick a glyph; release to insert |
| **clear**  | erase the name                                                                              |
| **cancel** | leave without changing                                                                      |
| **ok**     | accept the name (in memory only — disk write is still **save**)                             |


the **select** button works in a manner inspired by a certain well known swedish
device maker.

### page navigation

within a given mode there are multiple pages which provide editing controls or
present information

#### edit mode ring

in edit mode, **enc1** walks a ring of pages:

```text
setups → modules → slots → slot a → slot b → slot c → slot d → play → info → (wrap)
```

here **play** means the play-maps editor (bindings for live play), not live
play mode itself.

#### inspect mode ring

inspect has two subpages. **enc1** cycles between them:

```text
i/o ↔ cv in
```

softkeys are unused.

#### play mode

live play is a single surface outside the edit ring. the panel is remapped
by the setup’s play bindings (defaults: enc2/enc3 = morph x/y; sw0–sw3 =
snap to slots a–d).

### workflows



#### first boot

1. between waits for the sd card and creates `/data/between/` directories if
  needed.
2. if `/data/between/state` names a setup and that setup loads successfully,
  you start in **play**.
3. otherwise you start in **edit** on the **setups** page, with no module
  loaded and empty slots.



#### creating a new setup and selecting a module

1. on **setups**, press **new**. between allocates a unique setup name
  (`sNNN`) and jumps to **modules**.
2. on **modules**, highlight one of the modules (from the list of `.ldr` files
  on the sdcard) and press **load**. that clears all slots and resets the morph
   point to (0, 0). a module must be selected when a setup is created because
   that establishes the parameter set
3. after a module is loaded one is immediately presented with the **slots** page
  to assign presets to corners a–d.
4. edit parameters on the slot pages, configure play maps if you like, then
  return to **setups** and press **save**.



#### presets — create, load, rename, save

- **create:** on the slots page open **preset**, then **new** (module
defaults into that slot); or on an empty slot page press **new**.
- **load:** slots → **preset** → highlight a file → **load**.
- **rename:** on an occupied slot page, hold **alt** → **rename**. the new
stem is in memory until you **save** the preset.
- **save / reset:** on the slot page, **save** writes the slot bank to disk;
**reset** reloads the file and discards unsaved edits.
- **delete:** in the preset modal, hold **alt** → **delete** (removes the
file on disk).

clearing a slot (**clear** on the slots page) empties the bank but does not
delete the preset file.

#### saving a setup

on **setups**, press **save**. that writes the current setup `.txt`
(module, slot preset stems, morph point, play maps, manual morph excludes)
and updates the last-setup pointer used on boot.

dirty **presets** are separate files: saving the setup does not write
modified slot banks. save each dirty preset from its slot page when you
want those edits on disk.

similar to **presets**, a **setup** can be renamed by holding **alt**  →
**rename** while on the setup page.

#### capture and focus

dialing in four different presets with many tens of parameters each by hand is
tedious at best. the slot (preset) edit pages offer a creative form of copy and
paste. on an occupied slot page, hold **alt**:


| softkey     | action                                                                                                                                                 |
| ----------- | ------------------------------------------------------------------------------------------------------------------------------------------------------ |
| **capture** | overwrite this slot’s in-memory values with the current **effective** blend (bake the morph into this corner). excluded parameters are still captured. |
| **focus**   | snap the morph point to this slot’s corner so live audio matches the values on this page.                                                              |


the morph point indicator in the header can be used (while in the edit mode) to
see if the **effective** parameters correspond to the one of the slots or some
blend in between.

**copy the current effective state into a slot**

1. morph in play (or otherwise set the blend you want).
2. open the destination slot page.
3. hold **alt** → **capture**.
4. **save** if you want the baked values on disk.

**copy one preset into another slot**

1. open the **source** slot page; hold **alt** → **focus** (morph sits on
  that corner, so effective ≈ source).
2. navigate to the **destination** slot page.
3. hold **alt** → **capture** (destination bank becomes a copy of the
  effective / source values).
4. hold **alt** → **focus** if you want to edit that corner next; **save**
  on the destination when you want the copy on disk.



#### morph exclusion

parameters can be excluded from morph blending (setup-owned). on a slot
page, hold **alt** and turn **enc3**: left enables exclusion, right clears
it. parameters bound to a play map stay excluded until the binding is
removed — see [slot pages (a–d)](#slot-pages-ad).

## common header and status indicators

every page that draws the standard header shares the same status region to
the right of the title / name boxes:

```text
… title / name boxes …  [dirty]  [!!!]  [m]  [vu]  [morph 8×8]
```



### modification indicator

a light-grey **3×3 circle** appears after the title or name box when something
is unsaved:

- on **setups**, **play maps**, and **live play**, the mark means the
**setup** is modified: setup-owned fields changed (module, slot assignments,
morph point, play maps, manual excludes), **or** any occupied slot’s
preset is modified.
- on a **slot** page, the mark after the preset name means that slot’s
preset bank has unsaved edits.

saving a preset clears that slot’s dirty flag; saving the setup saves both the
setup owned state *and any currently dirty presets*.

it is worth noting that the current morph point is saved as part of the setup so
modifying the morph point (even in play mode) will mark the setup as modified.

### xrun warning (`!!!`)

dark-grey `!!!` appears when any DSP xrun counter increases. it clears after
about **5 seconds** with no further increases. the info page shows the raw
counters. it is not uncommon for the xrun indicator to appear right after a
setup is loaded or a new module is loaded and then go away.

### midi (`m`)

lowercase `m` appears when a MIDI device is connected. the glyph flashes briefly
on received traffic.

### vu grid

a **4×2** grid of small boxes:


| row    | channels          |
| ------ | ----------------- |
| top    | audio inputs 0–3  |
| bottom | audio outputs 0–3 |


brightness follows peak level (silence is black; near full scale is light).

### morph indicator

at the far right of the header is a grey square with a small white cursor which
shows the current morph point. this is a smaller version of the morph square
present shown in play mode and allows one to see the plane without leaving the
page. **cv** or **midi** mappings to **morph.x** or **morph.y** remain active
regardless of mode which makes this indicator valuable when editing parameters
as those parameters may not have an audible effect if the morph point is not
focused on the preset being edited.

### diagnostic log

a one-line message appears above the footer for short feedback (`setup saved`, `captured`, `fail`, and so on). it clears after about **2 seconds**.

## page reference

the following section overs each page of the ui in detail

### setups

setups

#### what it does

lists setup files under `/data/between/setups/`. load, save, and create
setups; rename or delete from the alt set. the header shows `setup` plus the
current setup name (or `none`) and the dirty mark when applicable.

#### navigation


| control     | action                    |
| ----------- | ------------------------- |
| enc0        | scroll the setup list     |
| enc1        | next / previous edit page |
| enc2 / enc3 | unused                    |


the directory is scanned the first time you enter the page; afterward use
alt+**scan** to refresh.

#### softkeys


|        | sw0        | sw1        | sw2      | sw3     |
| ------ | ---------- | ---------- | -------- | ------- |
| normal | **load**   | **save**   | **new**  | **alt** |
| alt    | **delete** | **rename** | **scan** | **alt** |


- **load** — load the highlighted setup (module, slot presets, morph, play
maps, excludes). failed preset loads clear those slots and log `fail`
with the slot letters.
- **save** — write the current configuration to the current setup name
(allocates `sNNN` if unnamed).
- **new** — unique `sNNN` name and jump to **modules** for the new-setup
flow.
- **delete** — remove the highlighted setup file and rescan.
- **rename** — name-entry modal; in-memory until **save**.
- **scan** — rescan the setups directory.



### modules

modules

#### what it does

lists DSP modules from `/mod/` (`.ldr` basenames). loads a module into the
dsp. the header shows `module` plus the loaded name (or `none`).

#### navigation


| control     | action                    |
| ----------- | ------------------------- |
| enc0        | scroll the module list    |
| enc1        | next / previous edit page |
| enc2 / enc3 | unused                    |


when first entering this page for the first time the sdcard is scanned for
modules (indicated on the diagnostic line) and cached there after. one can
manually rescan the list of modules by pressing **alt**+**scan**.

#### softkeys


|        | sw0      | sw1   | sw2      | sw3     |
| ------ | -------- | ----- | -------- | ------- |
| normal | **load** | **-** | **-**    | **alt** |
| alt    | **-**    | **-** | **scan** | **alt** |


- **load** — replace the current module. clears all slots and resets morph
to (0, 0) unless this load is part of setup recall. during a new-setup
flow, continues on the **slots** page.



### slots

slots

#### what it does

presents an overview of which preset occupies each corner. one can assign presets, jump
to a slot editor, or clear slots. if no module is loaded, the page shows `empty`
and preset selection sends you to **modules**.

```text
a              b
soft-pad       bright-pad

c              d
noise-bed      soft-pad
```



#### navigation


| control     | action                    |
| ----------- | ------------------------- |
| enc0        | select slot a–d           |
| enc1        | next / previous edit page |
| enc2 / enc3 | unused                    |




#### softkeys

footer labels stay `preset` / `edit` / `clear` / `alt` even while alt is
held (there is no separate alt footer set on this overview).


|     | sw0        | sw1      | sw2       | sw3     |
| --- | ---------- | -------- | --------- | ------- |
|     | **preset** | **edit** | **clear** | **alt** |


- **preset** — open the preset selector modal for the highlighted slot.
- **edit** — jump to that slot’s editor page.
- **clear** — empty the highlighted slot (file stays on disk).
- hold **alt** + **clear** — empty all four slots.



### slots — preset modal

slots-preset-modal

#### what it does

presents a modal list of presets for the **currently loaded module**
(`/data/between/presets/<module>/`) where one can load, cancel, or create new
presets.

#### navigation


| control | action                         |
| ------- | ------------------------------ |
| enc0    | scroll preset files            |
| enc1    | unused while the modal is open |




#### softkeys


|        | sw0        | sw1        | sw2     | sw3     |
| ------ | ---------- | ---------- | ------- | ------- |
| normal | **load**   | **cancel** | **new** | **alt** |
| alt    | **delete** | **-**      | **-**   | **alt** |


- **load** — assign the highlighted preset to the selected slot and close.
- **cancel** — close without changing the slot.
- **new** — unique `pNNN` from **module defaults**, assign to the slot,
close. to bake the current morph instead, open the slot editor and use
alt+**capture**.
- **delete** — delete the highlighted preset file and rescan.



### slot pages (a–d)

slotslot-empty

#### what it does

edit one corner’s preset bank. the list shows **slot-stored** values (not
the effective blend). the selected parameter index is shared across slots
a–d so comparing the same control keeps the cursor in place.

header: capital slot letter plus preset name when occupied. empty slots
show only the letter and the body text `empty`.

a status row above the log shows `nrpn` / `value` as `msb:lsb` for the selected
parameter (MIDI-oriented readout; informational only). MIDI messages sent on
channels 1-4 correspond to parameter slots A-D and can be used to set any
parameter directly with the full 14-bit midi parameter value scaled to cover the
entire domain of the preset parameter. MIDI message sent on channel 16 will set
that parameter on *all* slots. unlike MIDI mappings for play mode these NRPN
messages are active all the time and using the does not cause the parameter from
being excluded from morphing.

#### navigation


| control | action                                                                                    |
| ------- | ----------------------------------------------------------------------------------------- |
| enc0    | select parameter                                                                          |
| enc1    | next / previous edit page                                                                 |
| enc2    | fine adjust selected parameter (no-op if morph-excluded)                                  |
| enc3    | coarse adjust (no-op if morph-excluded); with **alt** held, toggle manual morph exclusion |


**alt+enc3:** turn left to exclude the selected parameter from morph; turn
right to include it again. if the parameter is play-bound, exclusion cannot
be cleared here — the log shows `bound to play`.

excluded parameters draw greyed with value `-`.

*every parameter edit updates the slot bank, recomputes the effective set for
the current morph point, and sends non-excluded params to the module.*

#### softkeys — empty slot


|     | sw0     | sw1   | sw2   | sw3   |
| --- | ------- | ----- | ----- | ----- |
|     | **new** | **-** | **-** | **-** |


- **new** — unique `pNNN` from module defaults into this slot.



#### softkeys — occupied slot


|        | sw0        | sw1         | sw2       | sw3     |
| ------ | ---------- | ----------- | --------- | ------- |
| normal | **save**   | **reset**   | **new**   | **alt** |
| alt    | **rename** | **capture** | **focus** | **alt** |


- **save** — write the slot bank to its preset file.
- **reset** — reload from the assigned file (discard unsaved edits).
- **new** — replace with a new `pNNN` from module defaults.
- **rename** — name-entry modal; in-memory until **save**.
- **capture** — bake current effective parameters into this slot.
- **focus** — snap morph point to this corner.



### play maps

play-maps

#### what it does

edit-mode page for **play bindings** owned by the current setup: how
encoders, switches, footswitches, CV jacks, and MIDI CC 1–12 behave in live
play. bindings are written on setup **save** and restored on setup **load**;
they are not stored in preset files.

header: `play` plus dirty mark when the setup is dirty.

each list line summarizes a control (e.g. `enc2: morph.x`, `sw0: snap.a`).
a status row shows the focused field (`kind` / `slot` / `param` / `value`).

common binding kinds: morph x/y, snap to slot, param (one slot or all
slots), set / momentary param for switches. binding a param target forces
that parameter morph-excluded while the map remains.

#### navigation


| control | action                                                              |
| ------- | ------------------------------------------------------------------- |
| enc0    | select which control’s binding to edit (resets field focus to kind) |
| enc1    | next / previous edit page                                           |
| enc2    | fine adjust of the focused field                                    |
| enc3    | coarse adjust of the focused field                                  |




#### softkeys


|        | sw0       | sw1         | sw2       | sw3     |
| ------ | --------- | ----------- | --------- | ------- |
| normal | **slot**  | **param**   | **value** | **alt** |
| alt    | **reset** | **rst all** | **-**     | **alt** |


- **slot** / **param** / **value** — jump field focus when that field
applies to the current binding kind.
- **reset** — restore the selected control to its default map.
- **rst all** — restore all play maps to defaults.

defaults (also used in live play until you change them): enc2/enc3 =
morph x/y; sw0–sw3 = snap a–d; footswitches and CC 1–12 unmapped.

### info

info

#### what it does

read-only system page at the end of the edit ring: app version, build git
id, and DSP xrun counters (`winRx`, `winTx`, `clashRx`, `clashTx`).

#### navigation


| control            | action                    |
| ------------------ | ------------------------- |
| enc1               | next / previous edit page |
| enc0 / enc2 / enc3 | unused                    |




#### softkeys

none (blank footer).

### play (live)

play

#### what it does

live performance surface. header shows the setup name and dirty mark. left:
morph square with the current point. right: encoder binding labels and
values. footer: switch labels from the play maps (snap letters, or param
names; single-slot param maps show a corner triangle in the footer cell).

#### navigation

panel roles come from the setup’s play maps. with defaults:


| control     | default                   |
| ----------- | ------------------------- |
| enc0 / enc1 | unmapped                  |
| enc2        | morph x                   |
| enc3        | morph y                   |
| sw0–sw3     | snap to slots a–d         |
| fs0 / fs1   | unmapped                  |
| cv0–cv3     | per setup maps (absolute) |


MODE short release returns to edit; long release opens inspect.

#### softkeys

labels follow the maps (not the edit-mode alt convention). there is no
edit-style **alt** set in live play — sw3 is a mapped play switch (default:
snap d).

### inspect — i/o

inspect-io

#### what it does

audio peak bars for inputs and outputs (channels 0–3). entered with a long
MODE release from edit or play.

#### navigation


| control            | action                         |
| ------------------ | ------------------------------ |
| enc1               | switch to **cv in** (and back) |
| enc0 / enc2 / enc3 | unused                         |


short MODE release returns to the mode you came from.

### inspect — cv in

inspect-cv

#### what it does

live CV jack voltages (`cv0`–`cv3`) with a short sparkline history. full
scale is 10 V.

#### navigation


| control            | action                       |
| ------------------ | ---------------------------- |
| enc1               | switch to **i/o** (and back) |
| enc0 / enc2 / enc3 | unused                       |




## files on the sd card

between stores human-editable text files on the sd card. names are the
**filename stem** (without `.txt`); they are not stored inside the files.

### directory layout

```text
/data/between/presets/<module>/<name>.txt
/data/between/setups/<name>.txt
/data/between/state
/mod/*.ldr
/mod/*.dsc
```


| path                              | contents                                                        |
| --------------------------------- | --------------------------------------------------------------- |
| `/data/between/presets/<module>/` | preset files for that module only                               |
| `/data/between/setups/`           | setup files                                                     |
| `/data/between/state`             | last-used setup pointer (`setup:<stem>`) for boot recall        |
| `/mod/`                           | DSP modules (`.ldr`) and descriptors (`.dsc`), shared with bees |


directories under `/data/between/` are created on boot if missing.

### presets and module name

presets are scoped by **module directory**. `soft-pad.txt` under
`presets/waves/` is unrelated to `soft-pad.txt` under `presets/lines/`.

all four slots must belong to the **same** loaded module. loading a preset
whose `module` metadata does not match the running module is rejected.

auto-generated stems:


| kind   | pattern                                                                   |
| ------ | ------------------------------------------------------------------------- |
| preset | `pNNN` (unique among disk files and in-memory slot stems for that module) |
| setup  | `sNNN`                                                                    |


name length is limited to 15 characters (app buffer includes a terminator).

### file format overview

both presets and setups are plain text, utf-8, lf line endings:

- lines starting with `#` are comments.
- blank lines are ignored.
- records are `key:value`.
- `format: 1` is the current file-format version.



#### preset file

one file per preset. metadata first, then one line per parameter. values are
raw native DSP integers (the UI shows scaled strings such as dB).

```text
# between preset
format: 1
module: waves
version: 0.4.5

hz0: 12000
amp0: -12
cut0: 35
res0: 20
```


| key       | meaning                                                                   |
| --------- | ------------------------------------------------------------------------- |
| `format`  | preset file-format version                                                |
| `module`  | module name (must match `.ldr` basename)                                  |
| `version` | module `maj.min.rev` when saved                                           |
| `<label>` | parameter label from the module `.dsc`; value is a raw native DSP integer |


the preset **name** is the filename stem only. only parameters present in
the file are part of the preset; others stay at module default on load.
morph exclusion does not omit parameters from the file — capture and save
still store underlying bank values.

#### setup file

one file per setup. references presets by stem, plus morph position, play
maps, and manual morph excludes.

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

play.enc2: morph.x
play.enc3: morph.y
play.sw0: snap.a
play.sw1: snap.b
play.sw2: snap.c
play.sw3: snap.d

morph.exclude: cut0, res0
```


| key                                                             | meaning                                                                     |
| --------------------------------------------------------------- | --------------------------------------------------------------------------- |
| `format`                                                        | setup file-format version                                                   |
| `module` / `version`                                            | module identity                                                             |
| `slot.a` … `slot.d`                                             | preset stems under that module’s preset directory (`-` or omit = empty)     |
| `x` / `y`                                                       | morph point as integers in `[0, 65535]`                                     |
| `play.enc*` / `play.sw*` / `play.fs*` / `play.cv*` / `play.cc*` | play bindings (omitted keys use defaults)                                   |
| `morph.exclude`                                                 | optional comma-separated list of **manual** morph-excluded parameter labels |


play-bound exclusions are derived from the play maps and are not duplicated
in `morph.exclude`. loading a setup loads the module if needed, fills the
slots from the referenced presets, restores maps and excludes, sets the
morph point, and applies effective parameters.

## module pairings

between will theoretically work with any available dsp modules. in practice the
more fixed control mapping in between; when compared to bees, may make it
difficult to achieve desirable results from certain dsp modules. that said, dsp
modules which were designed with between in mind are outlined below and should
work especially well with between’s morph plane.

### mx44

**mx44** is a 4×4 matrix mixer with a per-output base-width filters. four audio
inputs feed four input buses; each bus is sent to all four output mixes through
independent matrix levels; each mix then passes through a 6 dB/oct bandpass
(1-pole HP then 1-pole LP) and an output level to one DAC. four panel CV outs
(`cv1`…`cv4`) are independent of the audio matrix and can be used to drive
parameters on modular synths or pedals as part of the parameter morphing.

high-level features:

- full 4×4 send matrix (`inX-Y` = input X → output Y mix)
- per-input and per-output levels with slewing
- shared mix-send slew per input (`inXMixSlew`)
- per-output elektron-style **base / width** filter: `base` is the HP corner
in semitones above 1 Hz; `width` is the band in semitones up to the LP
corner. fully open (`base` min, `width` max) is transparent — no dry/wet
- shared slew for each output’s base and width (`outYBWSlew`)
- four CV DAC channels with independent slews
- labels are **1-based** (`in1`…`in4`, `out1`…`out4`, `cv1`…`cv4`)

parameter types as shown in between: **amp** displays as dB, **integrator**
as seconds-to-convergence, **fix** for filter cutoffs as semitones, and
**fix** for CV as 0…10 V.


| name         | type (unit)     | description                                   |
| ------------ | --------------- | --------------------------------------------- |
| `in1`        | amp (dB)        | input 1 level into bus `in1` (from `adc0`)    |
| `in2`        | amp (dB)        | input 2 level into bus `in2` (from `adc1`)    |
| `in3`        | amp (dB)        | input 3 level into bus `in3` (from `adc2`)    |
| `in4`        | amp (dB)        | input 4 level into bus `in4` (from `adc3`)    |
| `in1Slew`    | integrator (s)  | slew time for `in1`                           |
| `in2Slew`    | integrator (s)  | slew time for `in2`                           |
| `in3Slew`    | integrator (s)  | slew time for `in3`                           |
| `in4Slew`    | integrator (s)  | slew time for `in4`                           |
| `in1-1`      | amp (dB)        | matrix send: input 1 → output 1 mix           |
| `in1-2`      | amp (dB)        | matrix send: input 1 → output 2 mix           |
| `in1-3`      | amp (dB)        | matrix send: input 1 → output 3 mix           |
| `in1-4`      | amp (dB)        | matrix send: input 1 → output 4 mix           |
| `in1MixSlew` | integrator (s)  | shared slew for all sends from input 1        |
| `in2-1`      | amp (dB)        | matrix send: input 2 → output 1 mix           |
| `in2-2`      | amp (dB)        | matrix send: input 2 → output 2 mix           |
| `in2-3`      | amp (dB)        | matrix send: input 2 → output 3 mix           |
| `in2-4`      | amp (dB)        | matrix send: input 2 → output 4 mix           |
| `in2MixSlew` | integrator (s)  | shared slew for all sends from input 2        |
| `in3-1`      | amp (dB)        | matrix send: input 3 → output 1 mix           |
| `in3-2`      | amp (dB)        | matrix send: input 3 → output 2 mix           |
| `in3-3`      | amp (dB)        | matrix send: input 3 → output 3 mix           |
| `in3-4`      | amp (dB)        | matrix send: input 3 → output 4 mix           |
| `in3MixSlew` | integrator (s)  | shared slew for all sends from input 3        |
| `in4-1`      | amp (dB)        | matrix send: input 4 → output 1 mix           |
| `in4-2`      | amp (dB)        | matrix send: input 4 → output 2 mix           |
| `in4-3`      | amp (dB)        | matrix send: input 4 → output 3 mix           |
| `in4-4`      | amp (dB)        | matrix send: input 4 → output 4 mix           |
| `in4MixSlew` | integrator (s)  | shared slew for all sends from input 4        |
| `out1`       | amp (dB)        | output 1 level from bandpass to `dac0`        |
| `out1Slew`   | integrator (s)  | slew time for `out1`                          |
| `out1Base`   | fix (semitones) | output 1 HP corner; LP = base + width         |
| `out1Width`  | fix (semitones) | output 1 band width                           |
| `out1BWSlew` | integrator (s)  | shared slew time for `out1Base` / `out1Width` |
| `out2`       | amp (dB)        | output 2 level from bandpass to `dac1`        |
| `out2Slew`   | integrator (s)  | slew time for `out2`                          |
| `out2Base`   | fix (semitones) | output 2 HP corner                            |
| `out2Width`  | fix (semitones) | output 2 band width                           |
| `out2BWSlew` | integrator (s)  | shared slew time for `out2Base` / `out2Width` |
| `out3`       | amp (dB)        | output 3 level from bandpass to `dac2`        |
| `out3Slew`   | integrator (s)  | slew time for `out3`                          |
| `out3Base`   | fix (semitones) | output 3 HP corner                            |
| `out3Width`  | fix (semitones) | output 3 band width                           |
| `out3BWSlew` | integrator (s)  | shared slew time for `out3Base` / `out3Width` |
| `out4`       | amp (dB)        | output 4 level from bandpass to `dac3`        |
| `out4Slew`   | integrator (s)  | slew time for `out4`                          |
| `out4Base`   | fix (semitones) | output 4 HP corner                            |
| `out4Width`  | fix (semitones) | output 4 band width                           |
| `out4BWSlew` | integrator (s)  | shared slew time for `out4Base` / `out4Width` |
| `cv1`        | fix (V)         | panel CV out 1 (0…10 V)                       |
| `cv2`        | fix (V)         | panel CV out 2 (0…10 V)                       |
| `cv3`        | fix (V)         | panel CV out 3 (0…10 V)                       |
| `cv4`        | fix (V)         | panel CV out 4 (0…10 V)                       |
| `cvSlew1`    | integrator (s)  | slew time for `cv1`                           |
| `cvSlew2`    | integrator (s)  | slew time for `cv2`                           |
| `cvSlew3`    | integrator (s)  | slew time for `cv3`                           |
| `cvSlew4`    | integrator (s)  | slew time for `cv4`                           |


