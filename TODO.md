# between TODOs

## amp scaler max display shows -0.0587 instead of 0.0

bees amp UI only prints `0.0` dB when the table index is exactly
`1023` (`tabSize - 1`). index `1022` is the last real `scaler_amp_rep.dat`
entry and is about **-0.0587 dB**.

between displays via `scaler_get_in(raw)` → `scaler_get_str`. for unity /
`0x7fffffff`, `scaler_amp_in`'s binary search is off by one (bees notes this
with a FIXME): after the search loop it returns `jm` instead of `jl`, so max
amp often maps to index 1022 and shows **-0.0587** instead of the special-cased
`0.0`.

relevant code:

- `apps/between/src/scalers/scaler_amp.c` — `scaler_amp_str`, `scaler_amp_in`
- `utils/param_scaling/scaler_amp_rep.dat` — `tabRep[1022]` ≈ -0.0587

fix later: correct the binary-search return (use `jl`), and/or treat max DSP
value / `inMax` as unity in the string path.

## play morph encoders feel too slow

turning encoders mapped to morph x/y in live play moves the point too slowly
for performance. bump the step (or add fine/coarse / alt acceleration) so a
short turn can cross a useful fraction of the morph plane.

relevant: `apps/between/src/pages/page_play.c` — `nudge_axis`

## play maps parameter selection UX

done: softkeys select slot / param / value; enc2/enc3 fine/coarse adjust the
focused field; reset / rst all under alt; set/mom value shown on its own row;
`edit: <field>` status; space after `encN:` / `swN:`.

relevant: `apps/between/src/pages/page_play_maps.c`

## setup / preset name entry UX

done: shared name modal (`src/pages/name_edit.c`). Alt+**rename** on
setups (alt+sw1) and slot (alt+sw0) opens it — hold Select for charset
(enc0 cursor, enc2 palette), Clear / Cancel / OK (rename only; then
**save**).

still open: wire the same modal into setups **new** and slots-grid **new**
so create flows can choose a name instead of only auto stems.

relevant: `apps/between/src/pages/name_edit.c`, `page_setups.c`,
`page_slot.c`, `page_slots.c`

## header morph cursor size

the header morph-position indicator uses a 3×3 white cursor. consider
making it a single pixel or 2×2 so it reads more proportional to the
large morph view on the play screen.

relevant: `apps/between/src/render.c` — `head_draw_morph_indicator`

## send slew params before other params on apply

done: `slots_apply` sends via a type-priority schedule built at module
load (`k_slots_apply_type_order` — integrators first). UI lists and
`slots_capture_effective` remain descriptor-index order.

relevant: `apps/between/src/lib/slots.c` — `slots_rebuild_apply_order`,
`slots_apply`

## mx44 output base-width filter

done (mx44 0.9.0): elektron-style base-width bandpass on each output
(post-mix / pre-DAC). `outYBase` / `outYWidth` are fix16 semitones above a
1 Hz root, so a fixed width holds a constant octave span as base sweeps.
fully open is transparent, so the dry/wet control was removed.

between shows these as plain semitone numbers, not Hz — the fix scaler is
linear, which is correct on a log axis, but there is no unit suffix. a
display variant that resolves semitones to Hz would be a nice follow-up.

relevant: `modules_block/mx44/`, `dsp_block/filter_bp_alpha_tab.*`

## queued morph apply (sample-and-hold)

allow morph changes from the panel or MIDI to be **queued** and applied
only when a separate event fires. that gives sample-and-hold behavior:
drive the morph point continuously (e.g. footswitch / continuous
controller) while a sequencer or other trigger decides *when* the
pending position is committed to the live blend.

relevant: play morph path (`page_play.c`, MIDI morph handlers),
`slots_apply` / morph2d update timing
