# Porting apps/spray to a block-based spray module

Almost all of the work is a new Blackfin module. `apps/spray` only needs a new embedded LDR; it already loads DSP as an opaque SPI boot image and does not care about frame vs block.

## How it works today

`apps/spray` embeds the frame module from `modules/spray` as `.ldr` bytes and boots it once in `app_launch` (`src/app_spray.c`):

- The LDR bytes live in `src/aleph-spray.ldr.inc` and `src/aleph-spray.ldr_size.inc`.
- Launch path: `bfin_load_buf(ldrData, ldrSize)` → `bfin_wait_ready()` → `bfin_enable()`.
- Control uses hardcoded param indices that must match `modules/spray/params.h` (`src/ctl.h`, `src/ctl.c`).

There is no SD module picker and no Makefile dependency on `modules/spray`.

There is currently **no** `modules_block/spray/`. The only in-tree block module is `modules_block/rawsc/`. No app currently launches a block module.

## Frame vs block (DSP only)

| Concern | Frame (`modules/spray` + `bfin_lib`) | Block (`modules_block/*` + `bfin_lib_block`) |
|---------|--------------------------------------|-----------------------------------------------|
| Process hook | `module_process_frame()` in audio RX ISR | `module_process_block(buffer_t*, buffer_t*)` in main loop |
| Audio I/O | Scalar `in[4]` / `out[4]` | Deinterleaved `buffer_t[4][MODULE_BLOCKSIZE]` |
| Block size | 1 (implicit) | `MODULE_BLOCKSIZE` in `module_custom.h` |
| Params | Applied immediately in SPI ISR | Queued, applied from TX ISR via control FIFO |
| CV DAC | `cv.h` / `cv_update()` | **Not present** (SPORT1 is inited; no `cv_*` API) |

The AVR32 load/control protocol is the same either way (`bfin_load_buf`, `MSG_SET_PARAM`, etc.).

## What must change

### 1. New DSP module (main work)

Create something like `modules_block/spray/`, modeled on `modules_block/rawsc/` plus the logic from `modules/spray/`:

| Piece | Change |
|--------|--------|
| Makefile | Point at `bfin_lib_block` (`include ../../bfin_lib_block/bfin_lib_block.mk`), `module_name = spray` |
| `module_custom.h` | Add `#define MODULE_BLOCKSIZE 16` (or another chosen size) |
| Audio callback | Replace `module_process_frame()` / scalar `in[]`/`out[]` with `module_process_block(...)` and loop over frames |
| Init | Set `gModuleData->name` (rawsc does; frame spray does not) |
| Params / descriptors | Keep the same enum **or** deliberately change it and update the app |
| DSP classes | Still pull `dsp/filter_1p` for amp slew |

Rough audio shape (frame → block):

```c
// today (frame): per ISR sample
outBus = sum of mult_fr1x32x32(in[ch], adcVal[ch]);
out[0..3] = outBus;

// block: for each j in 0..MODULE_BLOCKSIZE-1
adcVal[ch] = filter_1p_lo_next(&adcSlew[ch]);  // once per sample if slew timing matters
spray = sum of (*in)[ch][j] * adcVal[ch];
(*out)[0..3][j] = spray;
```

Call `filter_1p_lo_next` **per sample** inside the block if you want the same slew behavior as frame spray. Updating once per block would make slews roughly `MODULE_BLOCKSIZE` times slower.

Closest template: copy structure from `modules_block/rawsc/`, DSP behavior from `modules/spray/spray.c`.

### 2. CV gap in `bfin_lib_block` (blocker for a faithful port)

Frame spray drives CV DACs via `cv.h` / `cv_update()` from `bfin_lib`. **`bfin_lib_block` has no `cv.c` / `cv.h`.** It does initialize SPORT1 for the AD5686 (`serial.c`), but there is no `cv_update` API.

Options:

1. **Port CV into `bfin_lib_block`** — bring over `cv.c` / `cv.h` from `bfin_lib`, then keep CV params.
2. **Drop CV** from the block spray — remove `eParam_cv*` / `eParam_cvSlew*` and the matching `ctl_param_change` calls in `src/ctl.c`.

The spray app only zeros CV at init; the UI never touches CV. Dropping CV is viable for this app if only spray matters.

### 3. Rebuild and re-embed the LDR into `apps/spray`

After building the block module:

1. Build `modules_block/spray` → `spray.ldr`
2. Run `bintool` on that LDR → new `aleph-spray.ldr.inc` + `aleph-spray.ldr_size.inc` (see `utils/bintool/README.md`)
3. Replace the copies under `apps/spray/src/`
4. Rebuild the AVR32 spray app

That is the only launch-path change for the app itself.

### 4. App-side code (only if the param surface changes)

If the block module keeps the same `params.h` enum and meaning:

- **No changes** to `ctl.h`, `ctl.c`, handlers, or render
- SPI load/enable path unchanged

If you drop or reorder params:

- Update the duplicated enum in `src/ctl.h`
- Update `ctl_init()` and any `ctl_param_change` indices in `src/ctl.c`

There is no need to teach the app about “block” vs “frame”.

## What does *not* need to change

- `apps/spray` Makefile / `config.mk` / load path
- AVR32 `bfin.c` SPI boot / param protocol
- UI (encoders, mutes, OLED)
- Parameter values for adc/slew, if indices stay aligned

Param application differs on the DSP (`bfin_lib_block` queues SPI params and applies them from the TX ISR), but that is invisible to the app as long as `module_set_param` still handles the same IDs.

## Practical checklist

1. Add `modules_block/spray/` (Makefile → `bfin_lib_block`, `MODULE_BLOCKSIZE`, block process loop).
2. Decide CV: port `cv_*` into `bfin_lib_block`, or strip CV params and sync `ctl.*`.
3. Preserve (or consciously change) param enum order vs `src/ctl.h`.
4. Build LDR → `bintool` → refresh `src/aleph-spray.ldr*.inc` → rebuild app.
