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
indices are **0-based** in parameter names.

---

## topology

```text
adc0 ──►[in0]──┬── dry0 ──────────────────────────────────┬──► dac0
               │                                          │
               ├──[send0-0]──►┐                           │
               │              ├──► sum ──► delay0 ──┬─────► dac2  (send 0)
               │              │   (delayFadeN,      │
               │              │    replace-write)   │
               │   [fb0]◄──[SVF0]◄──────────────────┘
               │
               ├──[send1-0]──►┐
               │              ├──► sum ──► delay1 ──┬─────► dac3  (send 1)
               │   [fb1]◄──[SVF1]◄──────────────────┘
               │
adc1 ──►[in1]──┼── dry1 ──────────────────────────────────┬──► dac1
               │                                          │
               ├──[send0-1]──► (into send0 sum)           │
               └──[send1-1]──► (into send1 sum)           │
                                                          │
adc2 (ret0) ──►[ret0-0]───────────────────────────────────┤
            └─►[ret0-1]───────────────────────────────────┤
                                                          │
adc3 (ret1) ──►[ret1-0]───────────────────────────────────┤
            └─►[ret1-1]───────────────────────────────────┘
```

`SVF*` = lines-style filter: `filter_svf_next` mixed with the dry tap via
`fdry*` / `fwet*` (same blend as lines’ delay→filter path). **only the
feedback** into the delay write uses this; the hardware send is the **raw**
delay tap.

```text
for each mono M in {0,1}:
  inM              — input level (slew: inMSlew)
  dryM = adcM * inM
  dacM = dryM + Σ_R retR-M * adc(2+R)

for each send S in {0,1}:
  sendS-0, sendS-1 — levels from dry0 / dry1 into send bus S
  delayS           — delayFadeN; replace-write; tap → dac(2+S)
  SVFS             — cut/rq/band mixes + fwet/fdry on tap (feedback only)
  fbS              — level of filtered feedback into send bus S
```

signal equation (per sample / frame), after slewed gains:

```text
dry[M]       = adc[M] * in[M]                         // M = 0,1

delay_out[S] = delayFadeN_next(line[S], send_in[S])   // replace-write
svf_out[S]   = filter_svf_next(&svf[S], delay_out[S])
fb_sig[S]    = delay_out[S] * fdry[S]
             + svf_out[S]   * fwet[S]

send_in[S]   = dry[0] * send[S]-0
             + dry[1] * send[S]-1
             + fb_sig[S] * fb[S]                      // S = 0,1
             // note: send_in uses previous sample's delay_out/fb_sig
             // (or compute delay_out from prior write; same as lines order)

dac[2+S]     = delay_out[S]                           // raw tap → send
dac[M]       = dry[M]
             + adc[2] * ret0-M
             + adc[3] * ret1-M                        // M = 0,1
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

**included delay params:** `timescale`, `delay0`/`delay1`, `fade0`/`fade1`.

**not included:** `loop*`, `pos_*`, `run_*`, `rMul*`/`rDiv*`, `pre*`,
`write*`, CV outs, lines’ full mix matrices.

---

## feedback SVF (from lines)

one `filter_svf` per send, controls mirrored from lines’ per-line filter
surface (labels may use `0`/`1` for send index):

| name | type (lines) | role |
|------|--------------|------|
| `cut0`/`cut1` | `eParamTypeSvfFreq` | cutoff coefficient (`filter_svf_set_coeff`) |
| `rq0`/`rq1` | fix | reciprocal Q (`filter_svf_set_rq`) |
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
| mono input levels | 2 | `in0`, `in1` | amp |
| mono input slews | 2 | `in0Slew`, `in1Slew` | integrator |
| send levels | 4 | `send0-0`…`send1-1` | amp |
| send level slews | 2 | `send0Slew`, `send1Slew` | integrator |
| feedback levels | 2 | `fb0`, `fb1` | amp |
| feedback slews | 2 | `fb0Slew`, `fb1Slew` | integrator |
| delay times | 2 | `delay0`, `delay1` | fix (≤ 20 s) |
| delay fades | 2 | `fade0`, `fade1` | fix (lines) |
| SVF cut / rq | 4 | `cut0`, `rq0`, `cut1`, `rq1` | svfFreq / fix |
| SVF cut / rq slews | 4 | `cut0Slew`, `rq0Slew`, … | integrator |
| SVF band mixes | 8 | `low*`, `high*`, `band*`, `notch*` | amp |
| SVF dry/wet | 4 | `fdry0`, `fwet0`, `fdry1`, `fwet1` | amp |
| SVF dry/wet slews | 4 | `fdry*Slew`, `fwet*Slew` | integrator |
| return levels | 4 | `ret0-0`…`ret1-1` | amp |
| return level slews | 2 | `ret0Slew`, `ret1Slew` | integrator |
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
| `in0` | `adc0` | `dry0` | mono 0 input level |
| `in1` | `adc1` | `dry1` | mono 1 input level |
| `in0Slew` | — | `in0` | slew for `in0` |
| `in1Slew` | — | `in1` | slew for `in1` |
| `send0-0` | `dry0` | `send0` | send 0 from mono 0 |
| `send0-1` | `dry1` | `send0` | send 0 from mono 1 |
| `send0Slew` | — | `send0-*` | shared slew for send 0 levels |
| `send1-0` | `dry0` | `send1` | send 1 from mono 0 |
| `send1-1` | `dry1` | `send1` | send 1 from mono 1 |
| `send1Slew` | — | `send1-*` | shared slew for send 1 levels |
| `fb0` | `fb0` | `send0` | feedback level (filtered path) into send 0 |
| `fb0Slew` | — | `fb0` | slew for `fb0` |
| `fb1` | `fb1` | `send1` | feedback level into send 1 |
| `fb1Slew` | — | `fb1` | slew for `fb1` |
| `delay0` | — | `del0` | delay 0 time (max 20 s) |
| `delay1` | — | `del1` | delay 1 time (max 20 s) |
| `fade0` | — | `del0` | delay 0 read-tap crossfade rate |
| `fade1` | — | `del1` | delay 1 read-tap crossfade rate |
| `cut0` | — | `svf0` | SVF 0 cutoff |
| `rq0` | — | `svf0` | SVF 0 reciprocal Q |
| `low0` | `svf0` | `fb0` | SVF 0 lowpass mix |
| `high0` | `svf0` | `fb0` | SVF 0 highpass mix |
| `band0` | `svf0` | `fb0` | SVF 0 bandpass mix |
| `notch0` | `svf0` | `fb0` | SVF 0 notch mix |
| `fdry0` | `del0` | `fb0` | dry tap into feedback blend |
| `fwet0` | `svf0` | `fb0` | filtered into feedback blend |
| `cut0Slew` | — | `cut0` | slew for `cut0` |
| `rq0Slew` | — | `rq0` | slew for `rq0` |
| `fdry0Slew` | — | `fdry0` | slew for `fdry0` |
| `fwet0Slew` | — | `fwet0` | slew for `fwet0` |
| `cut1` … `fwet1Slew` | (same as `*0` for send 1) | | |
| `ret0-0` | `adc2` | `dac0` | return 0 → mono out 0 |
| `ret0-1` | `adc2` | `dac1` | return 0 → mono out 1 |
| `ret0Slew` | — | `ret0-*` | shared slew for return 0 |
| `ret1-0` | `adc3` | `dac0` | return 1 → mono out 0 |
| `ret1-1` | `adc3` | `dac1` | return 1 → mono out 1 |
| `ret1Slew` | — | `ret1-*` | shared slew for return 1 |

(send-1 SVF rows omitted in the table for brevity; mirror `*0` → `*1`.)

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
in0, in1, in0Slew, in1Slew
send0-0, send0-1, send0Slew
send1-0, send1-1, send1Slew
fb0, fb0Slew, fb1, fb1Slew
delay0, delay1, fade0, fade1
cut0, rq0, low0, high0, band0, notch0, fdry0, fwet0
cut0Slew, rq0Slew, fdry0Slew, fwet0Slew
cut1, rq1, low1, high1, band1, notch1, fdry1, fwet1
cut1Slew, rq1Slew, fdry1Slew, fwet1Slew
ret0-0, ret0-1, ret0Slew
ret1-0, ret1-1, ret1Slew
```

---

## decisions (locked)

- max delay **20 s**; buffers sized exactly for that at 48 kHz.
- **fixed replace-write**; no `write*` / `pre*` params.
- **feedback unrestricted** (no soft clamp).
- simple delay surface: `timescale` + `delay*` + `fade*` only (no looper
  heads).
- **SVF in the feedback path** (lines filter controls); send outs stay raw.
