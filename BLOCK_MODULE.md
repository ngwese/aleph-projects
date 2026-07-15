# Porting apps/spray to a block-based spray module

Almost all of the work is a new Blackfin module. `apps/spray` loads DSP as an
opaque SPI boot image from the SD card and does not care about frame vs block.

## How it works today

`apps/spray` boots the companion module once in `app_launch` (`src/app_spray.c`):

- Wait for SD (`sd_mmc_spi_mem_check`), then `files_load_dsp("spray.ldr")`
  (`src/files.c`) opens `/mod/spray.ldr`, reads it into RAM, and calls
  `bfin_load_buf`.
- Then: `bfin_wait_ready()` → `bfin_enable()`.
- Control uses hardcoded param indices that must match
  `modules_block/spray/params.h` (`src/ctl.h`, `src/ctl.c`).

There is no SD module picker and no Makefile dependency on the DSP module.
Place `spray.ldr` on the card under `/mod/` (build from `modules_block/spray`).

## Frame vs block (DSP only)

| Concern | Frame (`modules/mix` + `bfin_lib`) | Block (`modules_block/*` + `bfin_lib_block`) |
|---------|--------------------------------------|-----------------------------------------------|
| Process hook | `module_process_frame()` in audio RX ISR | `module_process_block(buffer_t*, buffer_t*)` in main loop |
| Audio I/O | Scalar `in[4]` / `out[4]` | Deinterleaved `buffer_t[4][MODULE_BLOCKSIZE]` |
| Block size | 1 (implicit) | `MODULE_BLOCKSIZE` in `module_custom.h` |
| Params | Applied immediately in SPI ISR | Queued, applied from TX ISR via control FIFO |
| CV DAC | `cv.h` / `cv_update()` | **Not present** (SPORT1 is inited; no `cv_*` API) |

The AVR32 load/control protocol is the same either way (`bfin_load_buf`, `MSG_SET_PARAM`, etc.).

## DSP module notes

`modules_block/spray/` is modeled on `modules_block/rawsc/` plus mixer logic from
`modules/mix/`:

| Piece | Change |
|--------|--------|
| Makefile | Point at `bfin_lib_block`, `module_name = spray` |
| `module_custom.h` | `#define MODULE_BLOCKSIZE ...` |
| Audio callback | `module_process_block(...)` loop over frames |
| Init | Set `gModuleData->name` |
| Params / descriptors | Keep enum in sync with app `ctl.h` |
| DSP classes | `dsp/filter_1p` for amp slew |

### CV gap in `bfin_lib_block`

Frame mix drives CV DACs via `cv.h` / `cv_update()`. `bfin_lib_block` has no
`cv_*` API. Options: port CV into `bfin_lib_block`, or drop CV params from the
block module and from `src/ctl.c`. The spray app only zeros CV at init.

## Deploy updated DSP

1. Build `modules_block/spray` → `spray.ldr`
2. Copy to SD card as `/mod/spray.ldr`
3. Rebuild/flash the AVR32 spray app only if app-side code changed

No bintool re-embed step; the LDR is no longer compiled into flash.

## App-side code (only if the param surface changes)

If the block module keeps the same `params.h` enum and meaning:

- **No changes** to `ctl.h`, `ctl.c`, handlers, or render
- SPI load/enable path unchanged

If you drop or reorder params:

- Update the duplicated enum in `src/ctl.h`
- Update `ctl_init()` and any `ctl_param_change` indices in `src/ctl.c`

There is no need to teach the app about “block” vs “frame”.

## What does *not* need to change

- AVR32 `bfin.c` SPI boot / param protocol
- UI (encoders, mutes, OLED)
- Parameter values for adc/slew, if indices stay aligned

Param application differs on the DSP (`bfin_lib_block` queues SPI params and
applies them from the TX ISR), but that is invisible to the app as long as
`module_set_param` still handles the same IDs.
