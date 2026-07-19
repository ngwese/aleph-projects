# mx44

block-based DSP module: a simple **4×4 matrix mixer**.

four ADC inputs feed four internal input buses; each input bus is sent to
four output mix buses through independent matrix levels; each output mix
bus drives one DAC through an output level. every amplitude control has
1-pole slewing (`filter_1p`). matrix send slews share one time constant per
**input** (all four sends from that input use the same slew).

indices are **0-based** (`in0`…`in3`, `out0`…`out3`) to match ADC/DAC
channels and other aleph modules.

---

## topology

```text
adc0 ──►[in0]──┬──[in0-0]──►┬──[out0]──► dac0
               ├──[in0-1]──►┤
               ├──[in0-2]──►┤
               └──[in0-3]──►┤
                            │
adc1 ──►[in1]──┬──[in1-0]──►┤
               ├──[in1-1]──►┤
               ├──[in1-2]──►┤
               └──[in1-3]──►┤
                            │
adc2 ──►[in2]──┬──[in2-0]──►┤
               ├──[in2-1]──►┤
               ├──[in2-2]──►┤
               └──[in2-3]──►┤
                            │
adc3 ──►[in3]──┬──[in3-0]──►┤
               ├──[in3-1]──►┤
               ├──[in3-2]──►┤
               └──[in3-3]──►┘
                            │
                         (sum per
                          out bus)
```

each `[…]` box is an amplitude with slew. dashed grouping on the sends:

```text
for each input X:
  inX          — level into the input bus (slew: inXSlew)
  inX-0..inX-3 — sends into out0..out3 mixes (shared slew: inXMixSlew)
for each output Y:
  outY         — level from mix bus Y to dacY (slew: outYSlew)
```

signal equation (per sample / frame inside the block), after slewed gains
have been updated:

```text
bus_in[X]  = adc[X] * in[X]
mix[Y]     = Σ_X  bus_in[X] * in[X]-Y
dac[Y]     = mix[Y] * out[Y]
```

overflow: use saturating / guarded accumulation consistent with other
`modules_block` mixers (e.g. spray / mix-style `add_fr1x32`).

---

## parameter summary

| group | count | labels | type (bees) |
|-------|------:|--------|-------------|
| input levels | 4 | `in0`…`in3` | amp |
| input level slews | 4 | `in0Slew`…`in3Slew` | integrator |
| matrix sends | 16 | `inX-Y` | amp |
| matrix send slews | 4 | `in0MixSlew`…`in3MixSlew` | integrator |
| output levels | 4 | `out0`…`out3` | amp |
| output level slews | 4 | `out0Slew`…`out3Slew` | integrator |
| **total** | **36** | | |

amp params: `0`…`PARAM_AMP_MAX` (`0x7fffffff`), displayed as dB in bees.
integrator (slew) params: `0`…`PARAM_SLEW_MAX`, displayed as seconds-to-
convergence; default around `PARAM_SLEW_DEFAULT` (`0x7ffecccc`) matching
mix/spray.

suggested startup: identity matrix (`inX-X` = unity, other sends = 0),
`in*` / `out*` = unity, `in*Slew` / `out*Slew` = min (`0`), matrix send
slews = `PARAM_SLEW_DEFAULT`.

---

## parameter table

`src` / `dst` name the audio endpoints of the controlled gain (or `—` for
pure control such as slew time). buses `inX` / `outY` are internal.

| name | src | dst | description |
|------|-----|-----|-------------|
| `in0` | `adc0` | `in0` | input 0 level into bus `in0` |
| `in1` | `adc1` | `in1` | input 1 level into bus `in1` |
| `in2` | `adc2` | `in2` | input 2 level into bus `in2` |
| `in3` | `adc3` | `in3` | input 3 level into bus `in3` |
| `in0Slew` | — | `in0` | slew time for `in0` |
| `in1Slew` | — | `in1` | slew time for `in1` |
| `in2Slew` | — | `in2` | slew time for `in2` |
| `in3Slew` | — | `in3` | slew time for `in3` |
| `in0-0` | `in0` | `out0` | matrix send: input 0 → output 0 mix |
| `in0-1` | `in0` | `out1` | matrix send: input 0 → output 1 mix |
| `in0-2` | `in0` | `out2` | matrix send: input 0 → output 2 mix |
| `in0-3` | `in0` | `out3` | matrix send: input 0 → output 3 mix |
| `in0MixSlew` | — | `in0-*` | shared slew for all sends from input 0 |
| `in1-0` | `in1` | `out0` | matrix send: input 1 → output 0 mix |
| `in1-1` | `in1` | `out1` | matrix send: input 1 → output 1 mix |
| `in1-2` | `in1` | `out2` | matrix send: input 1 → output 2 mix |
| `in1-3` | `in1` | `out3` | matrix send: input 1 → output 3 mix |
| `in1MixSlew` | — | `in1-*` | shared slew for all sends from input 1 |
| `in2-0` | `in2` | `out0` | matrix send: input 2 → output 0 mix |
| `in2-1` | `in2` | `out1` | matrix send: input 2 → output 1 mix |
| `in2-2` | `in2` | `out2` | matrix send: input 2 → output 2 mix |
| `in2-3` | `in2` | `out3` | matrix send: input 2 → output 3 mix |
| `in2MixSlew` | — | `in2-*` | shared slew for all sends from input 2 |
| `in3-0` | `in3` | `out0` | matrix send: input 3 → output 0 mix |
| `in3-1` | `in3` | `out1` | matrix send: input 3 → output 1 mix |
| `in3-2` | `in3` | `out2` | matrix send: input 3 → output 2 mix |
| `in3-3` | `in3` | `out3` | matrix send: input 3 → output 3 mix |
| `in3MixSlew` | — | `in3-*` | shared slew for all sends from input 3 |
| `out0` | `out0` | `dac0` | output 0 level from mix bus to DAC |
| `out1` | `out1` | `dac1` | output 1 level from mix bus to DAC |
| `out2` | `out2` | `dac2` | output 2 level from mix bus to DAC |
| `out3` | `out3` | `dac3` | output 3 level from mix bus to DAC |
| `out0Slew` | — | `out0` | slew time for `out0` |
| `out1Slew` | — | `out1` | slew time for `out1` |
| `out2Slew` | — | `out2` | slew time for `out2` |
| `out3Slew` | — | `out3` | slew time for `out3` |

---

## implementation notes (block DSP)

- live under `modules_block/mx44/` with `bfin_lib_block` and
  `module_process_block`.
- slewing: one `filter_1p_lo` (or equivalent) per amplitude target; for
  matrix sends, four filters per input share the same `inXMixSlew`
  coefficient.
- param labels must fit `PARAM_LABEL_LEN` (16); names above are ≤10 chars.
- no CV DAC surface (block lib has no `cv_*` API); audio matrix only.
- keep enum order stable once published; bees / between / ctl apps key off
  indices and labels.

### suggested enum grouping

```text
in0..in3
in0Slew..in3Slew
in0-0..in0-3, in0MixSlew
in1-0..in1-3, in1MixSlew
in2-0..in2-3, in2MixSlew
in3-0..in3-3, in3MixSlew
out0..out3
out0Slew..out3Slew
```

(exact C enum names can use underscores, e.g. `eParam_in0_1` for label
`in0-1`.)

---

## open questions

- default matrix: identity vs all muted vs all unity (risk of hot summing).
- whether matrix levels are post-`inX` only (as specified) or optionally
  tap pre-input-gain (not needed for v1).
