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

editing play bindings is awkward:

- hard to reach every param (encoder selection may be too coarse / wrong
  step when cycling labels)
- slot vs all-slots is tangled with param pick; separate param selection
  from slot binding
- consider a switch that cycles slot a–d vs all, instead of folding scope
  into the kind/field ring

relevant: `apps/between/src/pages/page_play_maps.c`

## setup / preset name entry UX

done: shared save-as name modal (`src/pages/name_edit.c`). Alt+Save on
setups (alt+sw1) and slot (alt+sw0) opens it — hold Select for charset,
ENC2 for cursor/palette, Clear / Cancel / OK.

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

determine if slew parameters can be identified (by name, type, or module
descriptor) and sent first when applying an effective parameter set, so
slew rates are in place before other values change and morph/apply feels
more consistent.

relevant: `apps/between/src/state.c` (apply path), module `ParamDesc` /
scaler types

## mx44 output base-width filter

implement a base width filter and add it on the outputs of the mx44
module (post-mix / pre-DAC path).

relevant: `modules_block/mx44/`
