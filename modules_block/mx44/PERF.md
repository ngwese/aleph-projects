# mx44 `module_process_block` performance analysis

## Remediation (0.9.0)

The base-width rework removed the last divides from the module. Cutoffs are
now semitones on a log axis, and coefficients come from two precomputed
`fract32` tables in [`dsp_block/filter_bp_alpha_tab.h`](../../dsp_block/filter_bp_alpha_tab.h)
(1408 bytes of L1 `.rodata`) read with a linear interpolation.

| Change | Effect |
|--------|--------|
| `filter_bp_blk_prepare` → `filter_bp_blk_set_alpha` + table lookup | **0** integer divides / block, down from 8 |
| Dropped `outYWet` / `outYWetSlew` | 4 fewer slew `prepare`s and 4×16 fewer crossfade mul/add/sub per block |
| Sample loop | mix → HP → LP → out gain; no blend stage |

Per-block slew `prepare`s drop 36 → 32. The remaining cost is the 4×4
matrix and the eight 1-pole poles, which is ordinary mixer arithmetic.

## Remediation (0.7.0)

Implemented block-rate path via [`dsp_block/`](../../dsp_block/):

| Change | Effect |
|--------|--------|
| All param slews → `filter_1p_lo_blk` | **36** `prepare`s / block instead of **576** sample steps |
| BPF → `filter_bp_blk_prepare` + `next` | **8** integer divides / block instead of **128** |
| Gains / wet / Hz held constant across the 16-frame loop | Sample loop is mix + static-coeff IIR + dry/wet |

Classic `dsp/filter_1p` and `dsp/ricks_tricks` BPF remain for frame modules.
Further ideas: [`dsp_block/TODO.md`](../../dsp_block/TODO.md).

(Superseded in 0.9.0 — see above. The divide counts below describe the
0.7.0 path.)

### What the hot path does now

```text
prepare (once / block):
  36 × filter_1p_lo_blk_prepare   # in, out, mix[4][4], base, width, wet
  4  × filter_bp_blk_prepare      # 2× hzToDimensionless + 2× freq_calc (/)

for frame in 0..15:
  bus[x] = adc[x] * inVal[x]                    # 4 mults
  for y in 0..3:
    mix = Σ bus[x] * mixVal[x][y]               # 4 mul-adds
    filt = filter_bp_blk_next(outFilt[y], mix)  # HP+LP, stored alphas
    dac[y] = (mix*dry + filt*wet) * outVal[y]
```

### Cost model (same assumptions as below)

- `filter_1p_lo_blk_prepare` ≈ same math as classic `filter_1p_lo_next`
  (~3 fract ops; ~8–20 cy with call overhead).
- `filter_bp_blk_next` ≈ HP body + LP body (~8 fract ops; **no** `/`).
- `filter_bp_blk_prepare` ≈ 2× `hzToDimensionless` + `hpf_freq_calc` +
  `lpf_freq_calc` → **2 integer divides** per output.
- Integer `/` still ~40–100+ cy each on Blackfin.

### New implementation — ops / block

| Stage | Count / block | Fract-ish ops | Integer `/` | Notes |
|-------|--------------:|--------------:|------------:|-------|
| **P1. Slew prepare** | 36 × `prepare` (+ cheap `next` reads) | ~108 | 0 | Was 576 sample slews |
| **P2. BPF prepare** | 4 × `filter_bp_blk_prepare` | ~40 (incl. Hz scale) | **8** | Was 128 divides |
| **S1. Input gain** | 16 × 4 mult | 64 | 0 | Unchanged rate |
| **S2. Matrix mix** | 16 × 16 mul-adds | ~512 | 0 | Unchanged rate |
| **S3. BPF IIR** | 16 × 4 × `filter_bp_blk_next` | ~512 | **0** | Alphas cached |
| **S4. Dry/wet + out** | 16 × 4 × (~5 ops) | ~320 | 0 | Wet constant in block |
| **Total (order)** | | **~1.5k fract ops** | **8** | |

### Rough cycle sketch — new vs old

Same heuristics as the pre-fix section (~12 cy/slew call, ~60 cy/divide,
~few cy/fract op). Illustrative only.

| Work | Old / block | New / block | Δ |
|------|------------:|------------:|--:|
| Integer `/` (BPF alphas) | 128 × ~60 ≈ **7 700** | 8 × ~60 ≈ **480** | **~16×** less |
| 1-pole slew steps | 576 × ~12 ≈ **6 900** | 36 × ~12 ≈ **430** | **~16×** less |
| BPF IIR bodies | ~2–4 k (plus calc) | ~2–4 k (IIR only) | similar |
| Matrix + gains + dry/wet | ~2–4 k | ~2–4 k | similar |
| Loop / call glue | high (slews+div every sample) | lower (tight sample loop) | improves |
| **Dominated total (ballpark)** | often **≫ 20 k**, could approach **~170 k** | typically **~8–20 k** | large headroom |

Against the **~170 700** cycle / block ceiling, the new path is expected to
sit in the low tens of thousands of cycles in steady state — ample margin
for SPI/`control_process` — whereas the old path was regularly late
(~800 xruns / 500 ms poll).

### Side-by-side comparison

| Metric | Pre-0.7.0 (per sample × 16) | 0.7.0 (block prepare + sample) |
|--------|----------------------------:|-------------------------------:|
| `filter_1p_*` steps / block | **576** | **36** |
| Integer `/` / block | **128** | **8** |
| `hzToDimensionless` / block | **128** | **8** |
| BPF IIR evaluations / block | 64 (4 outs × 16) | 64 (same; cheaper each) |
| Matrix mul-adds / block | 256 | 256 |
| Amp/Hz/wet values in sample loop | updated every frame | **held for block** |
| Primary bottleneck | per-sample BPF `/` + slew volume | remaining: matrix + IIR + glue |

**Net:** the two 16× reductions (slews and BPF divides) remove the work that
drove xruns. Remaining cost is ordinary mix/filter arithmetic that other
block modules already sustain.

---

**Context (pre-fix):** between reported roughly **800+** DSP xrun counts (sum of
`winRx` + `winTx` + `clashRx` + `clashTx`) per **~500 ms** poll while mx44
was loaded. That was consistent with `module_process_block` frequently
missing the one-block deadline on `bfin_lib_block`.

The remainder of this note is the **original** cost breakdown that motivated
the work (order-of-magnitude estimates unless instrumented with the cycle
counter). Section titles below that say “today” refer to the **pre-0.7.0**
implementation.

---

## Platform budget

| Constant | Value | Source |
|----------|------:|--------|
| Sample rate | 48 000 Hz | `bfin_lib_block` `AUDIO_SAMPLERATE` / `SR` |
| Channels | 4 | `AUDIO_CHANNELS` |
| `MODULE_BLOCKSIZE` | **16** | `modules_block/mx44/module_custom.h` |
| Block period | **≈ 333 µs** | 16 / 48000 |
| Core clock | **512 MHz** | `PROCESSOR_CLOCK_HZ` |
| Cycles / block (hard ceiling) | **≈ 170 700** | 512e6 × 16 / 48000 |
| Blocks / 500 ms poll | **≈ 1500** | |

Xrun semantics (`MODULE_AUDIO_XRUN_DETECT`):

| Counter | Meaning |
|---------|---------|
| `xrunWindowRx` / `Tx` | ISR saw done flag still set → previous process overran the window |
| `xrunClashRx` / `Tx` | DMA about to reclaim the half main still holds |

~800 summed counts / 500 ms implies on the order of **hundreds of late
blocks per half-second** (often both RX and TX window flags fire for the
same overdue process). The module is not “a little hot”; it is regularly
over budget.

DSP runs in **main** after both DMA halves complete; interrupts stay
enabled. Control SPI and ISR work steal from the same 333 µs.

---

## What the hot loop did (pre-0.7.0)

From `mx44_module.c` (`module_process_block`), **per sample** inside
`for (frame = 0; frame < 16; ++frame)`:

```text
1. Input + matrix + output amp slews
   for x in 0..3:
     inVal[x]  = filter_1p_lo_next(inSlew[x])
     outVal[x] = filter_1p_lo_next(outSlew[x])
     for y in 0..3:
       mixVal[x][y] = filter_1p_lo_next(mixSlew[x][y])
     bus[x] = adc[x] * inVal[x]

2. Per output y in 0..3:
     mix = Σ_x bus[x] * mixVal[x][y]
     baseFix / widthFix = filter_1p_lo_next(baseHzSlew / widthHzSlew)
     hpHz, lpHz = …; clamp
     filt = bpf_next_dynamic_precise(outFilt[y], mix,
              hzToDimensionless(hpHz), hzToDimensionless(lpHz))
     wet = filter_1p_lo_next(wetSlew[y]); dry = 1 - wet
     blend = mix*dry + filt*wet
     dac[y] = blend * outVal[y]
```

Spec requirement: BPF **always runs** so state stays warm while wet
moves (`SPEC.md`, dry default).

---

## Stage breakdown — pre-0.7.0 (ops / sample and / block)

Assumptions used below:

- `filter_1p_lo_next` ≈ **1 sub + 1 mult + 1 add** (`dsp/filter_1p.c`) plus
  call/prolog if not inlined → treat as **~3 fract ops**, **~8–20 cycles**
  with call overhead in current C.
- `mult_fr1x32x32` / `add_fr1x32` / `sub_fr1x32` are **toolchain fract32
  intrinsics** (saturating). Cheap relative to integer divide; typically
  a few cycles each when kept in registers.
- `hzToDimensionless(hz)` = `hz * (FR32_MAX/SR)` → **1 integer multiply**.
- `bpf_next_dynamic_precise` = HP then LP; each `*_freq_calc` does a
  **32-bit integer `/`** (`hpf_freq_calc` / `lpf_freq_calc` in
  `dsp/ricks_tricks.h`). Blackfin has **no single-cycle 32-bit divide**,
  only `DIVS`/`DIVQ` primitives; C `/` expands to a multi-iteration
  sequence or libcall (**tens of cycles**, often **~40–100+** depending
  on compiler and values).
- Nested `for` loops, pointer arrays `inCh[x][frame]`, and out-of-line
  calls add non-trivial overhead on top of “pure math.”

### Per-sample call counts

| Stage | Calls / sample | Fract-ish ops (math only) | Integer `/` | Notes |
|-------|---------------:|--------------------------:|------------:|-------|
| **A. Amp slews** (in + out + 4×4 mix) | 4+4+16 = **24** × `filter_1p_lo_next` | ~72 | 0 | Pure param slews |
| **B. Input gain** | 4 × `mult` | 4 | 0 | `bus[x] = adc * inVal` |
| **C. Matrix mix** | 16 × (`mult`+`add`) | ~32 | 0 | Σ into 4 outs |
| **D. Filt Hz slews** | 8 × `filter_1p_lo_next` | ~24 | 0 | base + width × 4 |
| **E. Hz → dimensionless** | 8 × `hzToDimensionless` | ~8 muls | 0 | before every BPF |
| **F. BPF precise** | **4** × `bpf_next_dynamic_precise` | ~16 mult/add + calc | **8** | **Dominant** |
| **G. Wet slew + dry/wet + out gain** | 4 slews + ~4×(2 mult + add + sub) + 4 out mult | ~40 | 0 | |

### Per-block totals (×16)

| Stage | `filter_1p_lo_next` | Fract muls (order) | Integer divides | Share of pain (qualitative) |
|-------|--------------------:|-------------------:|----------------:|-----------------------------|
| A Amp slews | **384** | ~1.1k | 0 | Medium (volume of calls) |
| B Input gain | 0 | 64 | 0 | Low |
| C Matrix | 0 | ~256 | 0 | Low–medium |
| D Filt Hz slews | **128** | ~0.4k | 0 | Low–medium |
| E Hz scale | 0 | 128 | 0 | Low |
| **F BPF** | 0 | ~0.3k | **128** | **Very high** |
| G Wet + blend + out | 64 | ~0.3k | 0 | Low–medium |
| **Total** | **576** | **~2.5k+** | **128** | |

### Rough cycle budget sketch (illustrative)

| Work | Heuristic | Cycles / block (ballpark) |
|------|-----------|---------------------------:|
| 128 × integer `/` @ ~60 cy | BPF coeff rebuild | **~8 000** |
| BPF IIR bodies (8 poles × 16) | ~10–20 cy each | **~2 000–4 000** |
| 576 × `filter_1p_lo_next` @ ~12 cy | slews + calls | **~7 000** |
| Matrix + gains + dry/wet | ~500–800 fract ops | **~2 000–4 000** |
| Loop / pointer / call glue | compiler-dependent | **~5 000–30 000+** |
| **Sum** | | **often >> 20 k; can approach or exceed ~170 k** with poor codegen |

Even if divides alone are only ~5–10 % of the ceiling, **recomputing BPF
alphas every sample** plus **hundreds of out-of-line 1p calls** and a
scalar nested-loop matrix is enough to blow the budget once you add
24↔32 conversion, SPI, and control_process on the same core.

**Primary finding:** cost is not “4×4 mixing.” It is **per-sample dynamic
BPF with two divides per output**, stacked on **per-sample slewing of 36
coefficients**.

---

## Stage-by-stage notes

### A — Input / matrix / output amp slews

- 24 one-poles per sample match other aleph mixers (spray/mix also slew
  amps per sample).
- Individually cheap; **aggregate call count** is high (384/block).
- Candidates: force-inline `filter_1p_lo_next`; batch advance; or
  **block-rate slew** for mix sends (audible difference only on very
  fast slews).

### B — Input gain

- Four `mult_fr1x32x32`. Ideal for dual-issue with loads if unrolled.

### C — Matrix slew already counted in A; mix math

- Dense 4×4 · 4 MAC-accumulates. Perfect dual-MAC / `LSETUP` target;
  today it is a double `for` with little ILP visible to the compiler.

### D/E — Filter Hz slews + `hzToDimensionless`

- Hz slews are fine; converting Hz→dimensionless **every sample per
  cutoff** is redundant when the slewed fix16 only moves slowly.

### F — `bpf_next_dynamic_precise` (main suspect)

```c
// ricks_tricks: each precise pole rebuilds alpha via `/`
hpf_freq_calc → FR32_MAX / x_16_16
lpf_freq_calc → (temp << 12) / ((1 << 16) + temp)
bpf = lpf(hpf(in))
```

- **2 divides × 4 outs × 16 samples = 128 divides / block.**
- IIR itself is only a handful of mult/adds once `alpha` is known.
- Spec forces warm BPF even at wet=0 → cannot skip forever without a
  design change, but **alphas need not be rebuilt every sample**.

### G — Wet crossfade + output gain

- Modest; keep per-sample for zipper-free wet moves, or slew wet once
  per block if acceptable.

---

## Blackfin architecture hooks (what the silicon wants)

Aleph modules today are almost entirely **C + fract32 builtins**. There
is essentially **no** use of `LSETUP`, dual-MAC multi-issue packets, or
hand asm in `dsp/` / `modules_block/`. Delay lines use
`__builtin_bfin_circptr`; that pattern does not help mx44.

Relevant BF53x capabilities:

| Feature | Use for mx44 |
|---------|----------------|
| **Dual MAC** (2× 16×16→40 per cycle) | 4×4 mix and dry/wet blends if data are kept as `fract16`/`fract2x16`, or use 32-bit MAC pairs carefully |
| **Multi-issue** (32-bit compute \|\| two 16-bit loads/stores) | Keep pointers in `P` regs, samples in `R`/`D` regs; software-pipeline the frame loop |
| **`LSETUP` / zero-overhead loops** | Outer loop over `MODULE_BLOCKSIZE`; inner unrolled 4×4 (avoid nested HW loops ending on same insn without LC0/LC1 rules) |
| **40-bit `A0`/`A1` accumulators** | Saturating mix sums without repeated `add_fr1x32` round-trips |
| **`DIVS`/`DIVQ`** | Only for rare coeff updates — never inside the sample loop |
| **L1 SRAM** | Hot state (`mixVal`, filter memory, channel pointers) already benefits if linker keeps them in L1; avoid SDRAM bounce |

Toolchain fract builtins (`mult_fr1x32x32`, etc.) already map toward
multiply hardware; the C `/` in `*_freq_calc` does **not** stay in that
fast path.

---

## Proposed alternative implementations

Ordered by **expected gain / risk**. Prefer measuring with
`START_CYCLE_COUNT` / `STOP_CYCLE_COUNT` (or re-enable macros in
`cycle_count_aleph.h`) around stages after each change.

### 1. Cache BPF alphas (highest leverage, low risk)

**Idea:** When `base`/`width` slews have not moved (or once per block),
compute `hp_alpha` / `lp_alpha` once and run a **static-coeff** HP→LP
for the 16 samples.

```text
per block (or when !filter_1p_sync):
  hpHz, lpHz ← slew once or take current y
  hpα = hpf_freq_calc(hzToDimensionless(hpHz))
  lpα = lpf_freq_calc(hzToDimensionless(lpHz))
per sample:
  filt = lpf_static(hpf_static(mix, hpα), lpα)   // no divides
```

- Divides / block: **8** (or fewer) instead of **128** → **~16×** less
  divide work.
- Keeps warm state and SPEC wet behavior.
- Add `hpf_next` / `lpf_next` variants that take precomputed `alpha`
  (mirror `filter_1p_lo` style).

### 2. Block-prologue coeff update; sample loop is pure IIR + mix

Structure:

```text
module_process_block:
  advance_or_snapshot all slews (see §3)
  for y: compute alphas / wet / outVal
  for frame:   // LSETUP candidate
    bus[] = adc[] * inVal[]
    for y: mix, bpf_static, blend, * outVal
```

Separates **control-rate** work from **audio-rate** work so the compiler
(and later asm) can schedule a tight inner loop.

### 3. Reduce slew rate for non-audio-critical params

| Param class | Keep per-sample? | Alternative |
|-------------|------------------|-------------|
| `in*` / `out*` amps | nice-to-have | per-sample or linear interp across block |
| Matrix `inX-Y` | usually slow UI | **once per block** or every N samples |
| `base` / `width` | slow | **once per block** + alpha cache |
| `wet` | medium | once per block or per-sample only if moving |

Block-rate slew changes the *feel* of very fast integrator settings but
is usually inaudible for matrix/Hz.

### 4. Optional BPF bypass when wet ≈ 0 **and** synced

SPEC wants warm filters. Compromise:

- If `wet` below threshold **and** `filter_1p_sync(wet)` **and** Hz
  slews synced: skip BPF for that block (or run 1 sample / block to
  refresh state).
- Else full warm path.

Large savings in the common “mixer only” default (`wet = 0`).

### 5. Unroll + dual-issue-friendly C for the 4×4

```c
// conceptual — force registers, no &mixSlew[x][y] chasing
mix0 = bus0*m00; mix0 = add(mix0, bus1*m10); ...
```

- Fully unroll `x` and `y` (N=4 is tiny).
- Prefer `mult_fr1x32x32NS` where saturation is guaranteed by headroom.
- Place `bus[]` and `mixVal` in locals for the frame.

Expected: better dual-issue packing from gcc/bfin without hand asm.

### 6. Hand-tuned Blackfin loop (if C still misses budget)

Sketch:

```asm
LSETUP (top, bot) LC0 = 16;
top:
  // dual-issue: load next adc || MAC accumulate
  // A0 += R1.H * R2.H, A1 += ...  (if using 16-bit path)
bot:
```

Guidance:

- One HW loop over frames; **unroll** channels (don’t nest two
  `LSETUP`s ending on the same instruction without LC0/LC1 discipline).
- Use `A0`/`A1` for mix sums; sat-store once per output.
- Keep BPF as separate short functions with precomputed alphas to avoid
  killing the software pipeline with divides.

Only invest here after §1–§3.

### 7. `fract16` / SIMD path (higher risk)

Dual MAC shines on **16-bit** packed data. A `fract16` mix+filter path
could roughly double MAC throughput but needs:

- Careful noise / headroom analysis
- Different filter coeff format
- Conversion cost at block edges if SPORT stays 24/32

Consider only if 32-bit static-coeff path is still over budget.

### 8. Library hygiene

- Move `filter_1p_lo_next` to a header `static inline`.
- Provide `bpf_next_alphas(bpf*, in, hpα, lpα)` next to
  `bpf_next_dynamic_precise`.
- Avoid `clamp_lp_hz` branches in the sample loop; clamp when params
  change.

---

## Recommended roadmap

1. **Instrument** `module_process_block` (and optionally each stage) with
   cycle counts; confirm divides dominate on device.
2. **Implement alpha caching / static-coeff BPF** (proposal §1–§2). Expect
   the largest single drop in xruns while preserving sound and warm
   state.
3. **Inline 1p slews**; optionally advance matrix/Hz slews **once per
   block**.
4. **Unroll** matrix + gains in C; remeasure.
5. If still hot with wet=0: **conditional BPF** (§4).
6. Only then: **`LSETUP` / dual-MAC asm** for the frame loop (§6).

Success criterion: summed xruns ≈ 0 at idle and under normal UI slew
activity on between’s info page over multi-second polls; cycle count
comfortably under ~100 k / block leaving margin for SPI/control.

---

## Comparison: spray vs mx44 (why spray survives)

Spray-style mix: a few amp slews + mult/adds per sample, **no** dynamic
precise BPF, **no** per-sample integer divides. mx44 adds **4 warm
bandpasses with coeff rebuild every sample** on top of a full 4×4 slewed
matrix — that is the architectural difference behind the xrun storm.

---

## References (in-tree)

- `modules_block/mx44/mx44_module.c` — `module_process_block`
- `modules_block/mx44/SPEC.md` — topology; BPF always on
- `dsp/filter_1p.c` — `filter_1p_lo_next`
- `dsp/ricks_tricks.h` / `.c` — `hpf_freq_calc`, `lpf_freq_calc`,
  `bpf_next_dynamic_precise`, `hzToDimensionless`
- `bfin_lib_block/AUDIO-CONTROL-FLOW.md` — deadline and xrun classes
- `bfin_lib_block/src/cycle_count_aleph.h` — clock and CPU-use macro
- ADI Blackfin Programming Reference — dual-issue, `LSETUP`, `DIVS`/`DIVQ`
