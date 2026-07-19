# mx44

block-based DSP module: a simple **4×4 matrix mixer**.

four ADC inputs feed four internal input buses; each input bus is sent to
four output mix buses through independent matrix levels; each output mix
bus drives one DAC through an output level. every amplitude control has
1-pole slewing (`filter_1p`). matrix send slews share one time constant per
**input** (all four sends from that input use the same slew).

parameter labels are **1-based** (`in1`…`in4`, `out1`…`out4`). ADC/DAC
hardware channels remain 0-based (`adc0`…`adc3`, `dac0`…`dac3`);
`in1` / `out1` map to channel 0, `in4` / `out4` to channel 3.

---

## topology

```text
adc0 ──►[in1]──┬──[in1-1]──►┬──[out1]──► dac0
               ├──[in1-2]──►┤
               ├──[in1-3]──►┤
               └──[in1-4]──►┤
                            │
adc1 ──►[in2]──┬──[in2-1]──►┤
               ├──[in2-2]──►┤
               ├──[in2-3]──►┤
               └──[in2-4]──►┤
                            │
adc2 ──►[in3]──┬──[in3-1]──►┤
               ├──[in3-2]──►┤
               ├──[in3-3]──►┤
               └──[in3-4]──►┤
                            │
adc3 ──►[in4]──┬──[in4-1]──►┤
               ├──[in4-2]──►┤
               ├──[in4-3]──►┤
               └──[in4-4]──►┘
                            │
                         (sum per
                          out bus)
```

each `[…]` box is an amplitude with slew. dashed grouping on the sends:

```text
for each input X in 1..4:
  inX          — level into the input bus (slew: inXSlew)
  inX-1..inX-4 — sends into out1..out4 mixes (shared slew: inXMixSlew)
for each output Y in 1..4:
  outY         — level from mix bus Y to dac(Y-1) (slew: outYSlew)
```

signal equation (per sample / frame inside the block), after slewed gains
have been updated. arrays below are **0-based** hardware indices; param
`in(k+1)` / `out(k+1)` control index `k`:

```text
bus_in[k]  = adc[k] * in[k+1]
mix[m]     = Σ_k  bus_in[k] * in[k+1]-(m+1)
dac[m]     = mix[m] * out[m+1]
```

overflow: use saturating / guarded accumulation consistent with other
`modules_block` mixers (e.g. spray / mix-style `add_fr1x32`).

---

## parameter summary

| group | count | labels | type (bees) |
|-------|------:|--------|-------------|
| input levels | 4 | `in1`…`in4` | amp |
| input level slews | 4 | `in1Slew`…`in4Slew` | integrator |
| matrix sends | 16 | `inX-Y` (X,Y ∈ 1…4) | amp |
| matrix send slews | 4 | `in1MixSlew`…`in4MixSlew` | integrator |
| output levels | 4 | `out1`…`out4` | amp |
| output level slews | 4 | `out1Slew`…`out4Slew` | integrator |
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
pure control such as slew time). buses `inX` / `outY` are internal
(1-based labels).

| name | src | dst | description |
|------|-----|-----|-------------|
| `in1` | `adc0` | `in1` | input 1 level into bus `in1` |
| `in2` | `adc1` | `in2` | input 2 level into bus `in2` |
| `in3` | `adc2` | `in3` | input 3 level into bus `in3` |
| `in4` | `adc3` | `in4` | input 4 level into bus `in4` |
| `in1Slew` | — | `in1` | slew time for `in1` |
| `in2Slew` | — | `in2` | slew time for `in2` |
| `in3Slew` | — | `in3` | slew time for `in3` |
| `in4Slew` | — | `in4` | slew time for `in4` |
| `in1-1` | `in1` | `out1` | matrix send: input 1 → output 1 mix |
| `in1-2` | `in1` | `out2` | matrix send: input 1 → output 2 mix |
| `in1-3` | `in1` | `out3` | matrix send: input 1 → output 3 mix |
| `in1-4` | `in1` | `out4` | matrix send: input 1 → output 4 mix |
| `in1MixSlew` | — | `in1-*` | shared slew for all sends from input 1 |
| `in2-1` | `in2` | `out1` | matrix send: input 2 → output 1 mix |
| `in2-2` | `in2` | `out2` | matrix send: input 2 → output 2 mix |
| `in2-3` | `in2` | `out3` | matrix send: input 2 → output 3 mix |
| `in2-4` | `in2` | `out4` | matrix send: input 2 → output 4 mix |
| `in2MixSlew` | — | `in2-*` | shared slew for all sends from input 2 |
| `in3-1` | `in3` | `out1` | matrix send: input 3 → output 1 mix |
| `in3-2` | `in3` | `out2` | matrix send: input 3 → output 2 mix |
| `in3-3` | `in3` | `out3` | matrix send: input 3 → output 3 mix |
| `in3-4` | `in3` | `out4` | matrix send: input 3 → output 4 mix |
| `in3MixSlew` | — | `in3-*` | shared slew for all sends from input 3 |
| `in4-1` | `in4` | `out1` | matrix send: input 4 → output 1 mix |
| `in4-2` | `in4` | `out2` | matrix send: input 4 → output 2 mix |
| `in4-3` | `in4` | `out3` | matrix send: input 4 → output 3 mix |
| `in4-4` | `in4` | `out4` | matrix send: input 4 → output 4 mix |
| `in4MixSlew` | — | `in4-*` | shared slew for all sends from input 4 |
| `out1` | `out1` | `dac0` | output 1 level from mix bus to DAC |
| `out2` | `out2` | `dac1` | output 2 level from mix bus to DAC |
| `out3` | `out3` | `dac2` | output 3 level from mix bus to DAC |
| `out4` | `out4` | `dac3` | output 4 level from mix bus to DAC |
| `out1Slew` | — | `out1` | slew time for `out1` |
| `out2Slew` | — | `out2` | slew time for `out2` |
| `out3Slew` | — | `out3` | slew time for `out3` |
| `out4Slew` | — | `out4` | slew time for `out4` |

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
in1..in4
in1Slew..in4Slew
in1-1..in1-4, in1MixSlew
in2-1..in2-4, in2MixSlew
in3-1..in3-4, in3MixSlew
in4-1..in4-4, in4MixSlew
out1..out4
out1Slew..out4Slew
```

(exact C enum names can use underscores, e.g. `eParam_in1_2` for label
`in1-2`.)

---

## open questions

- default matrix: identity vs all muted vs all unity (risk of hot summing).
- whether matrix levels are post-`inX` only (as specified) or optionally
  tap pre-input-gain (not needed for v1).
