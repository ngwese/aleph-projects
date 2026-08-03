# Slot page encoder → NRPN value display

How the slot-edit status row `value msb:lsb` relates to panel encoders.

The readout is **not** “encoder → MSB” and “encoder → LSB.” It is:

```text
encoder → scaler io → bank raw → one 14-bit number (v14) → split for display
```

That bit-split is why MSB can look like the fine digit when you turn the
coarse knob.

## 1. Encoder → `io` (accumulation)

On the slot page, only the **sign** of the encoder event is used:

| Encoder | Role | Step in scaler `io_t` |
|--------:|------|------------------------|
| enc2 | fine | `±0x20` (one table index; see below) |
| enc3 | coarse | `±0x100` (`±256`) |

`scaler_get_in(raw)` snaps to the table-bucket base (amp `inRshift==5`).
A literal fine `±1` stays in the same bucket on increment (raw unchanged) but
crosses into the previous bucket on decrement — so only decrease appeared to
work. Slot fine therefore promotes sub-bucket steps to `±0x20`. Play-mode
mapped encoders use the coarse step (`±0x100`) instead.

For scaled params (amp, integrator, note, …):

1. `scaler_get_in(raw)` → current `io`
2. `scaler_inc(&io, delta)` → new `io`, clamped to `inMin…inMax`
3. store the returned **raw** DSP value in the slot bank

There is no separate MSB/LSB accumulator on the panel path.

## 2. Raw → 14-bit `v14` (status line)

On redraw:

```text
raw → scaler_get_in → io
  → v14 = io mapped linearly across inMin…inMax → 0…16383
```

For amp, `inMax ≈ 1023<<5 = 32736`, so roughly:

```text
v14 ≈ io * 16383 / 32736
```

(Unscaled / discrete types map through `ParamDesc.min…max` instead; see
[SPEC.md](SPEC.md) § midi / absolute range mapping.)

## 3. `v14` → displayed `MSB:LSB`

Standard MIDI 14-bit packing:

```text
MSB = (v14 >> 7) & 0x7F   // top 7 bits  (±128 in v14 per MSB step)
LSB =  v14       & 0x7F   // bottom 7 bits (±1 in v14 per LSB step)
```

So **LSB is the fine half of one integer `v14`**, not a separate encoder
channel. Format on screen is decimal `msb:lsb` with no zero-padding
(e.g. `64:0`).

## Why it can feel backwards

| Encoder | `io` step | ≈ `v14` step | What you see |
|--------|-----------|--------------|--------------|
| enc2 fine | ±0x20 | ~16 | LSB moves; MSB every ~8 clicks |
| enc3 coarse | ±256 | **~128** | **MSB moves every click**; LSB barely moves |

Coarse’s step is almost exactly **one MSB unit** in 14-bit space (`128`).
Turning enc3 therefore looks like “MSB is fine, LSB is stuck” — the
opposite of the usual MIDI mental model (LSB fine, MSB coarse).

That is expected with the current mapping: the knobs step in **scaler
io**, while the labels show a **bit-split of the derived 14-bit absolute
value**, not “MSB encoder / LSB encoder.”

## Related code

- Slot encoders: `src/pages/page_slot.c` (`handle_enc2` / `handle_enc3`,
  `bump_param_scaled`)
- Raw ↔ `v14` (scaler io path): `src/midi_between.c`
  (`between_midi_raw_to_v14` / `between_midi_v14_to_raw`)
- Range map and `msb:lsb` format: `src/lib/midi_nrpn.c`
- Spec: [SPEC.md](SPEC.md) (slot NRPN status row, absolute range mapping)
