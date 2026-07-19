# parallel

block-based DSP module: two mono channels with **parallel send/return
delay lines**.

hardware I/O (1-based jack numbers → 0-based ADC/DAC indices):

| jack | role | bus |
|-----:|------|-----|
| in 1 / out 1 | mono channel 0 | `adc0` / `dac0` |
| in 2 / out 2 | mono channel 1 | `adc1` / `dac1` |
| in 3 / out 3 | return 0 / send 0 | `adc2` / `dac2` |
| in 4 / out 4 | return 1 / send 1 | `adc3` / `dac3` |

each mono input has a level control. two independent send buses are fed by
a mix of the faded mono inputs (per-send `send` levels) plus an attenuated
**feedback** of that bus’s delay tap after a **state-variable filter** (lines-
style `filter_svf` + dry/wet). each delay is a lines-style `delayFadeN`
single-tap line with **fixed replace-write**; its raw tap drives the matching
hardware send (`dac2` / `dac3`). hardware inputs 3 and 4 are returns. each
mono output is the post-fade mono dry path plus a mix of the two returns.

amplitude controls use 1-pole slewing (`filter_1p`) unless noted.
delay time / fade / timescale and SVF controls follow
[`modules/lines`](../../modules/lines/).
parameter labels are **1-based**; ADC/DAC hardware indices stay 0-based.

---

## topology

```text
adc0 ──►[in1]──┬── dry1 ──────────────────────────────────┬──► dac0
               │                                          │
               ├──[send1-1]──►┐                           │
               │              ├──► sum ──► delay1 ──┬─────► dac2  (send 1)
               │              │   (delayFadeN,      │
               │              │    replace-write)   │
               │   [fb1]◄──[SVF1]◄──────────────────┘
               │
               ├──[send2-1]──►┐
               │              ├──► sum ──► delay2 ──┬─────► dac3  (send 2)
               │   [fb2]◄──[SVF2]◄──────────────────┘
               │
adc1 ──►[in2]──┼── dry2 ──────────────────────────────────┬──► dac1
               │                                          │
               ├──[send1-2]──► (into send1 sum)           │
               └──[send2-2]──► (into send2 sum)           │
                                                          │
adc2 (ret1) ──►[ret1-1]───────────────────────────────────┤
            └─►[ret1-2]───────────────────────────────────┤
                                                          │
adc3 (ret2) ──►[ret2-1]───────────────────────────────────┤
            └─►[ret2-2]───────────────────────────────────┘
```

`SVF*` = lines-style filter: `filter_svf_next` mixed with the dry tap via
`fdry*` / `fwet*` (same blend as lines’ delay→filter path). **only the
feedback** into the delay write uses this; the hardware send is the **raw**
delay tap.

```text
for each mono M in {1,2}:
  inM              — input level (slew: inMSlew)
  dryM = adc(M-1) * inM
  dac(M-1) = dryM + Σ_R retR-M * adc(1+R)

for each send S in {1,2}:
  sendS-1, sendS-2 — levels from dry1 / dry2 into send bus S
  delayS           — delayFadeN; replace-write; tap → dac(1+S)
  SVFS             — cut/rq/band mixes + fwet/fdry on tap (feedback only)
  fbS              — level of filtered feedback into send bus S
```

signal equation (per sample / frame), after slewed gains. arrays below are
**0-based** hardware indices; param `in(k+1)` controls mono `k`:

```text
dry[k]       = adc[k] * in[k+1]                       // k = 0,1

delay_out[s] = delayFadeN_next(line[s], send_in[s])   // replace-write
svf_out[s]   = filter_svf_next(&svf[s], delay_out[s])
fb_sig[s]    = delay_out[s] * fdry[s+1]
             + svf_out[s]   * fwet[s+1]

send_in[s]   = dry[0] * send[s+1]-1
             + dry[1] * send[s+1]-2
             + fb_sig[s] * fb[s+1]                    // s = 0,1

dac[2+s]     = delay_out[s]                           // raw tap → send
dac[k]       = dry[k]
             + adc[2] * ret1-(k+1)
             + adc[3] * ret2-(k+1)                    // k = 0,1
```

processing order per send (match lines): mix `send_in` → `delayFadeN_next` →
SVF → form `fb_sig` for the **next** sample’s `send_in` (or mix feedback
from the previous tap — implement like lines’ `out_del` → `mix_del_del`
feedback).

overflow: saturating / guarded accumulation (`add_fr1x32`) on all mixes.
**feedback is unrestricted** (no soft clamp on `fb*`).

---

## delay lines (from lines)

| piece | lines | parallel |
|-------|-------|----------|
| object | `delayFadeN` | same |
| buffer | `LINES_BUF_FRAMES` (`0x5FFF40`) | **`PARALLEL_BUF_FRAMES = 20 * 48000`** (`960000`) per line |
| write | `pre` / `write` params | **fixed replace-write** (`write` on, `pre` unused) |
| process | `delayFadeN_next` | same, per sample in the block loop |
| delay time | `delay*` + `calc_ms` + read-tap fade | same |
| fade | `fade*` / `filter_ramp` | same |
| timescale | `timescale` | same |

buffer memory: \(960000 \times 4\) bytes ≈ **3.66 MB** per line; two lines ≈
**7.3 MB** SDRAM — well within 64 MB.

cap `delay*` descriptor max at **20 s** (still `eParamTypeFix`; use a
parallel-specific max rather than lines’ full `PARAM_SECONDS_MAX` if that
implies a longer range than the buffer).

**included delay params:** `timescale`, `delay1`/`delay2`, `fade1`/`fade2`.

**not included:** `loop*`, `pos_*`, `run_*`, `rMul*`/`rDiv*`, `pre*`,
`write*`, CV outs, lines’ full mix matrices.

---

## feedback SVF (from lines)

one `filter_svf` per send, controls mirrored from lines’ per-line filter
surface (labels use `1`/`2` for send index):

| name | type (lines) | role |
|------|--------------|------|
| `cut1`/`cut2` | `eParamTypeSvfFreq` | cutoff coefficient (`filter_svf_set_coeff`) |
| `rq1`/`rq2` | fix | reciprocal Q (`filter_svf_set_rq`) |
| `low*` / `high*` / `band*` / `notch*` | amp | SVF output mixes |
| `fwet*` / `fdry*` | amp | blend filtered vs dry tap into `fb_sig` |
| `cut*Slew` / `rq*Slew` | integrator | slews for cut / rq |
| `fwet*Slew` / `fdry*Slew` | integrator | slews for wet / dry (as lines `Wet*`/`Dry*` slews) |

defaults: follow lines (`PARAM_CUT_DEFAULT`, `PARAM_RQ_DEFAULT`, low ≈ −6 dB,
high/band/notch = 0, fwet/fdry ≈ −6 dB, slews = lines defaults).

---

## parameter summary

| group | count | labels | type (bees) |
|-------|------:|--------|-------------|
| timescale | 1 | `timescale` | fix |
| mono input levels | 2 | `in1`, `in2` | amp |
| mono input slews | 2 | `in1Slew`, `in2Slew` | integrator |
| send levels | 4 | `send1-1`…`send2-2` | amp |
| send level slews | 2 | `send1Slew`, `send2Slew` | integrator |
| feedback levels | 2 | `fb1`, `fb2` | amp |
| feedback slews | 2 | `fb1Slew`, `fb2Slew` | integrator |
| delay times | 2 | `delay1`, `delay2` | fix (≤ 20 s) |
| delay fades | 2 | `fade1`, `fade2` | fix (lines) |
| SVF cut / rq | 4 | `cut1`, `rq1`, `cut2`, `rq2` | svfFreq / fix |
| SVF cut / rq slews | 4 | `cut1Slew`, `rq1Slew`, … | integrator |
| SVF band mixes | 8 | `low*`, `high*`, `band*`, `notch*` | amp |
| SVF dry/wet | 4 | `fdry1`, `fwet1`, `fdry2`, `fwet2` | amp |
| SVF dry/wet slews | 4 | `fdry*Slew`, `fwet*Slew` | integrator |
| return levels | 4 | `ret1-1`…`ret2-2` | amp |
| return level slews | 2 | `ret1Slew`, `ret2Slew` | integrator |
| **total** | **49** | | |

`sendS-*` share `sendSSlew`; `retR-*` share `retRSlew`. `fb*` unrestricted.

suggested startup: `in*` = unity; `send*` / `fb*` / `ret*` = 0; delays /
fades / timescale / SVF as lines defaults; amp slews = default.

---

## parameter table

`src` / `dst` name audio endpoints (or `—` for pure control). `dryM` =
post-`inM`; `sendS` = pre-delay mix; `delS` = raw delay tap; `fbS` path =
filtered feedback.

| name | src | dst | description |
|------|-----|-----|-------------|
| `timescale` | — | `del*` | global delay time scaler (`calc_ms`) |
| `in1` | `adc0` | `dry1` | mono 0 input level |
| `in2` | `adc1` | `dry2` | mono 1 input level |
| `in1Slew` | — | `in1` | slew for `in1` |
| `in2Slew` | — | `in2` | slew for `in2` |
| `send1-1` | `dry1` | `send1` | send 1 from mono 1 |
| `send1-2` | `dry2` | `send1` | send 1 from mono 2 |
| `send1Slew` | — | `send1-*` | shared slew for send 1 levels |
| `send2-1` | `dry1` | `send2` | send 2 from mono 1 |
| `send2-2` | `dry2` | `send2` | send 2 from mono 2 |
| `send2Slew` | — | `send2-*` | shared slew for send 2 levels |
| `fb1` | `fb1` | `send1` | feedback level (filtered path) into send 1 |
| `fb1Slew` | — | `fb1` | slew for `fb1` |
| `fb2` | `fb2` | `send2` | feedback level into send 2 |
| `fb2Slew` | — | `fb2` | slew for `fb2` |
| `delay1` | — | `del1` | delay 1 time (max 20 s) |
| `delay2` | — | `del2` | delay 2 time (max 20 s) |
| `fade1` | — | `del1` | delay 1 read-tap crossfade rate |
| `fade2` | — | `del2` | delay 2 read-tap crossfade rate |
| `cut1` | — | `svf1` | SVF 1 cutoff |
| `rq1` | — | `svf1` | SVF 1 reciprocal Q |
| `low1` | `svf1` | `fb1` | SVF 1 lowpass mix |
| `high1` | `svf1` | `fb1` | SVF 1 highpass mix |
| `band1` | `svf1` | `fb1` | SVF 1 bandpass mix |
| `notch1` | `svf1` | `fb1` | SVF 1 notch mix |
| `fdry1` | `del1` | `fb1` | dry tap into feedback blend |
| `fwet1` | `svf1` | `fb1` | filtered into feedback blend |
| `cut1Slew` | — | `cut1` | slew for `cut1` |
| `rq1Slew` | — | `rq1` | slew for `rq1` |
| `fdry1Slew` | — | `fdry1` | slew for `fdry1` |
| `fwet1Slew` | — | `fwet1` | slew for `fwet1` |
| `cut2` … `fwet2Slew` | (same as `*1` for send 2) | | |
| `ret1-1` | `adc2` | `dac0` | return 1 → mono out 1 |
| `ret1-2` | `adc2` | `dac1` | return 1 → mono out 2 |
| `ret1Slew` | — | `ret1-*` | shared slew for return 1 |
| `ret2-1` | `adc3` | `dac0` | return 2 → mono out 1 |
| `ret2-2` | `adc3` | `dac1` | return 2 → mono out 2 |
| `ret2Slew` | — | `ret2-*` | shared slew for return 2 |

(send-2 SVF rows omitted in the table for brevity; mirror `*1` → `*2`.)

---

## implementation notes (block DSP)

- live under `modules_block/parallel/` with `bfin_lib_block` /
  `module_process_block`.
- SDRAM: `fract32 audioBuffer[2][PARALLEL_BUF_FRAMES]` with
  `PARALLEL_BUF_FRAMES = (20 * 48000)`.
- share/port `delayFadeN`, `buffer`, `filter_ramp`, `filter_svf` from lines /
  `dsp/`.
- fixed replace-write: init write enabled, never use `pre` mix.
- on `delay*` change: `start_fade_rd` + `delayFadeN_set_delay_ms` (lines
  `param_set.c`).
- hardware sends = raw taps; SVF only on feedback blend.
- no CV DAC surface; keep enum order stable once published.

### suggested enum grouping

```text
timescale
in1, in2, in1Slew, in2Slew
send1-1, send1-2, send1Slew
send2-1, send2-2, send2Slew
fb1, fb1Slew, fb2, fb2Slew
delay1, delay2, fade1, fade2
cut1, rq1, low1, high1, band1, notch1, fdry1, fwet1
cut1Slew, rq1Slew, fdry1Slew, fwet1Slew
cut2, rq2, low2, high2, band2, notch2, fdry2, fwet2
cut2Slew, rq2Slew, fdry2Slew, fwet2Slew
ret1-1, ret1-2, ret1Slew
ret2-1, ret2-2, ret2Slew
```

---

## decisions (locked)

- max delay **20 s**; buffers sized exactly for that at 48 kHz.
- **fixed replace-write**; no `write*` / `pre*` params.
- **feedback unrestricted** (no soft clamp).
- simple delay surface: `timescale` + `delay*` + `fade*` only (no looper
  heads).
- **SVF in the feedback path** (lines filter controls); send outs stay raw.
