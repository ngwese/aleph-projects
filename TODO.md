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

setups and presets still lean on fixed or auto stems (`setup0`, `pNNN`,
save-as unique) instead of a usable rename/create name editor. SPEC calls
for enc2/enc3 name edit when creating/renaming; that path is incomplete or
missing.

want a shared name-entry flow for setup and preset stems (cursor, charset,
confirm/cancel) so save / save-as / new can take a chosen name instead of
only generated ones.

relevant: `apps/between/src/pages/page_setups.c`, `page_slots.c`,
`page_slot.c`; SPEC setups / slots / slot page name-edit notes
