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
