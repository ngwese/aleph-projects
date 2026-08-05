/*
  mx44_module.c
  aleph-bfin (block)

  mx44 — 4×4 matrix mixer for bfin_lib_block.

  adc[i] * in[i+1] → bus; bus * in[i+1]-[j+1] summed into out mix j;
  base-width bpf (HP at base, LP at base+width), then * out → dac.
  cv1..cv4 drive panel CV DACs (slewed, once-per-block commit).
  parameter labels are 1-based; arrays stay 0-based.
  each amplitude has a 1-pole slew (block-rate filter_1p_lo_blk);
  matrix sends from input X share inXMixSlew.
  filter cutoffs are semitones; each output's base and width share a
  slewed time constant (outYBWSlew). fully open (base min, width max) is
  transparent, so there is no dry/wet control.
*/

#include <string.h>

#include "audio_channels.h"
#include "cv.h"
#include "filter_1p_blk.h"
#include "filter_bp_alpha_tab.h"
#include "filter_bp_blk.h"
#include "fract_math.h"
#include "module.h"
#include "module_custom.h"
#include "mx44_params.h"

/* mx44_params.h cannot include the table header (it also builds on the
   host for the descriptor), so check the ranges line up here. */
typedef char mx44_base_st_max_matches_table
    [(PARAM_BASE_ST_MAX == FILTER_BP_ALPHA_HP_ST_MAX) ? 1 : -1];
typedef char mx44_width_st_max_matches_table
    [(PARAM_WIDTH_ST_MAX == FILTER_BP_ALPHA_LP_ST_MAX) ? 1 : -1];

ModuleData *gModuleData;

static ModuleData super;
static ParamData mParamData[eParamNumParams];

static filter_1p_lo_blk inSlew[4];
static filter_1p_lo_blk outSlew[4];
static filter_1p_lo_blk mixSlew[4][4]; /* [in][out], 0-based */

static filter_1p_lo_blk baseStSlew[4];
static filter_1p_lo_blk widthStSlew[4];
static filter_1p_lo_blk cvSlew[4];
static filter_bp_blk outFilt[4];

static fract32 inVal[4];
static fract32 outVal[4];
static fract32 mixVal[4][4];
static fract32 baseStVal[4];
static fract32 widthStVal[4];
static fract32 cvVal[4];

static inline void param_setup(u32 id, ParamValue v) {
  gModuleData->paramData[id].value = v;
  module_set_param(id, v);
}

static void set_mix_slew(u8 xin, fract32 slew) {
  u8 y;
  for (y = 0; y < 4; ++y) {
    filter_1p_lo_blk_set_slew(&(mixSlew[xin][y]), slew);
  }
}

/* shared time constant for the independent base and width slews */
static void set_bw_slew(u8 y, fract32 slew) {
  filter_1p_lo_blk_set_slew(&(baseStSlew[y]), slew);
  filter_1p_lo_blk_set_slew(&(widthStSlew[y]), slew);
}

/* LP corner sits `width` semitones above the HP corner; the lookup clamps
   to the open end of the table. both are well under s32 range. */
static inline fract32 lp_st(fract32 baseSt, fract32 widthSt) {
  return baseSt + widthSt;
}

void module_init(void) {
  u8 x;
  u8 y;

  gModuleData = &super;
  strcpy(gModuleData->name, "mx44");
  gModuleData->paramData = mParamData;
  gModuleData->numParams = eParamNumParams;

  for (x = 0; x < 4; ++x) {
    filter_1p_lo_blk_init(&(inSlew[x]), 0, MODULE_BLOCKSIZE);
    filter_1p_lo_blk_init(&(outSlew[x]), 0, MODULE_BLOCKSIZE);
    filter_1p_lo_blk_init(&(cvSlew[x]), 0, MODULE_BLOCKSIZE);
    filter_1p_lo_blk_init(&(baseStSlew[x]), PARAM_BASE_DEFAULT,
                          MODULE_BLOCKSIZE);
    filter_1p_lo_blk_init(&(widthStSlew[x]), PARAM_WIDTH_DEFAULT,
                          MODULE_BLOCKSIZE);
    filter_bp_blk_init(&(outFilt[x]));
    for (y = 0; y < 4; ++y) {
      filter_1p_lo_blk_init(&(mixSlew[x][y]), 0, MODULE_BLOCKSIZE);
    }
  }

  /* identity matrix at unity; input/output levels unity; default slews */
  param_setup(eParam_in1, PARAM_AMP_MAX);
  param_setup(eParam_in2, PARAM_AMP_MAX);
  param_setup(eParam_in3, PARAM_AMP_MAX);
  param_setup(eParam_in4, PARAM_AMP_MAX);

  param_setup(eParam_in1Slew, PARAM_SLEW_MIN);
  param_setup(eParam_in2Slew, PARAM_SLEW_MIN);
  param_setup(eParam_in3Slew, PARAM_SLEW_MIN);
  param_setup(eParam_in4Slew, PARAM_SLEW_MIN);

  param_setup(eParam_in1_1, PARAM_AMP_MAX);
  param_setup(eParam_in1_2, 0);
  param_setup(eParam_in1_3, 0);
  param_setup(eParam_in1_4, 0);
  param_setup(eParam_in1MixSlew, PARAM_SLEW_DEFAULT);

  param_setup(eParam_in2_1, 0);
  param_setup(eParam_in2_2, PARAM_AMP_MAX);
  param_setup(eParam_in2_3, 0);
  param_setup(eParam_in2_4, 0);
  param_setup(eParam_in2MixSlew, PARAM_SLEW_DEFAULT);

  param_setup(eParam_in3_1, 0);
  param_setup(eParam_in3_2, 0);
  param_setup(eParam_in3_3, PARAM_AMP_MAX);
  param_setup(eParam_in3_4, 0);
  param_setup(eParam_in3MixSlew, PARAM_SLEW_DEFAULT);

  param_setup(eParam_in4_1, 0);
  param_setup(eParam_in4_2, 0);
  param_setup(eParam_in4_3, 0);
  param_setup(eParam_in4_4, PARAM_AMP_MAX);
  param_setup(eParam_in4MixSlew, PARAM_SLEW_DEFAULT);

  param_setup(eParam_out1, PARAM_AMP_MAX);
  param_setup(eParam_out1Slew, PARAM_SLEW_MIN);
  param_setup(eParam_out1Base, PARAM_BASE_DEFAULT);
  param_setup(eParam_out1Width, PARAM_WIDTH_DEFAULT);
  param_setup(eParam_out1BWSlew, PARAM_SLEW_DEFAULT);

  param_setup(eParam_out2, PARAM_AMP_MAX);
  param_setup(eParam_out2Slew, PARAM_SLEW_MIN);
  param_setup(eParam_out2Base, PARAM_BASE_DEFAULT);
  param_setup(eParam_out2Width, PARAM_WIDTH_DEFAULT);
  param_setup(eParam_out2BWSlew, PARAM_SLEW_DEFAULT);

  param_setup(eParam_out3, PARAM_AMP_MAX);
  param_setup(eParam_out3Slew, PARAM_SLEW_MIN);
  param_setup(eParam_out3Base, PARAM_BASE_DEFAULT);
  param_setup(eParam_out3Width, PARAM_WIDTH_DEFAULT);
  param_setup(eParam_out3BWSlew, PARAM_SLEW_DEFAULT);

  param_setup(eParam_out4, PARAM_AMP_MAX);
  param_setup(eParam_out4Slew, PARAM_SLEW_MIN);
  param_setup(eParam_out4Base, PARAM_BASE_DEFAULT);
  param_setup(eParam_out4Width, PARAM_WIDTH_DEFAULT);
  param_setup(eParam_out4BWSlew, PARAM_SLEW_DEFAULT);

  param_setup(eParam_cv1, 0);
  param_setup(eParam_cv2, 0);
  param_setup(eParam_cv3, 0);
  param_setup(eParam_cv4, 0);

  param_setup(eParam_cvSlew1, PARAM_SLEW_DEFAULT);
  param_setup(eParam_cvSlew2, PARAM_SLEW_DEFAULT);
  param_setup(eParam_cvSlew3, PARAM_SLEW_DEFAULT);
  param_setup(eParam_cvSlew4, PARAM_SLEW_DEFAULT);
}

void module_process_block(buffer_t *inChannels, buffer_t *outChannels) {
  u16 frame;
  u8 x;
  u8 y;
  fract32 bus[4];
  fract32 mix;

  fract32 *inCh[4];
  fract32 *outCh[4];

  inCh[0] = audio_in_channel(inChannels, 0);
  inCh[1] = audio_in_channel(inChannels, 1);
  inCh[2] = audio_in_channel(inChannels, 2);
  inCh[3] = audio_in_channel(inChannels, 3);

  outCh[0] = audio_out_channel(outChannels, 0);
  outCh[1] = audio_out_channel(outChannels, 1);
  outCh[2] = audio_out_channel(outChannels, 2);
  outCh[3] = audio_out_channel(outChannels, 3);

  /* block-rate slews: one step per amp / cutoff */
  for (x = 0; x < 4; ++x) {
    filter_1p_lo_blk_prepare(&(inSlew[x]));
    inVal[x] = filter_1p_lo_blk_next(&(inSlew[x]));
    filter_1p_lo_blk_prepare(&(outSlew[x]));
    outVal[x] = filter_1p_lo_blk_next(&(outSlew[x]));
    filter_1p_lo_blk_prepare(&(baseStSlew[x]));
    baseStVal[x] = filter_1p_lo_blk_next(&(baseStSlew[x]));
    filter_1p_lo_blk_prepare(&(widthStSlew[x]));
    widthStVal[x] = filter_1p_lo_blk_next(&(widthStSlew[x]));
    for (y = 0; y < 4; ++y) {
      filter_1p_lo_blk_prepare(&(mixSlew[x][y]));
      mixVal[x][y] = filter_1p_lo_blk_next(&(mixSlew[x][y]));
    }
  }

  /* BPF alphas once per block (table lookup + lerp, no divides) */
  for (y = 0; y < 4; ++y) {
    filter_bp_blk_set_alpha(
        &(outFilt[y]), filter_bp_alpha_hp(baseStVal[y]),
        filter_bp_alpha_lp(lp_st(baseStVal[y], widthStVal[y])));
  }

  for (frame = 0; frame < MODULE_BLOCKSIZE; frame++) {
    for (x = 0; x < 4; ++x) {
      bus[x] = mult_fr1x32x32(inCh[x][frame], inVal[x]);
    }

    for (y = 0; y < 4; ++y) {
      mix = 0;
      for (x = 0; x < 4; ++x) {
        mix = add_fr1x32(mix, mult_fr1x32x32(bus[x], mixVal[x][y]));
      }

      outCh[y][frame] =
          mult_fr1x32x32(filter_bp_blk_next(&(outFilt[y]), mix), outVal[y]);
    }
  }

  /* CV: slew once per block, then one hardware flush (cv1→ch0 … cv4→ch3) */
  for (x = 0; x < 4; ++x) {
    filter_1p_lo_blk_prepare(&(cvSlew[x]));
    cvVal[x] = filter_1p_lo_blk_next(&(cvSlew[x]));
    cv_set(x, cvVal[x]);
  }
  cv_commit();
}

void module_set_param(u32 idx, ParamValue v) {
  switch (idx) {
  case eParam_in1:
    filter_1p_lo_blk_in(&(inSlew[0]), v);
    break;
  case eParam_in2:
    filter_1p_lo_blk_in(&(inSlew[1]), v);
    break;
  case eParam_in3:
    filter_1p_lo_blk_in(&(inSlew[2]), v);
    break;
  case eParam_in4:
    filter_1p_lo_blk_in(&(inSlew[3]), v);
    break;

  case eParam_in1Slew:
    filter_1p_lo_blk_set_slew(&(inSlew[0]), v);
    break;
  case eParam_in2Slew:
    filter_1p_lo_blk_set_slew(&(inSlew[1]), v);
    break;
  case eParam_in3Slew:
    filter_1p_lo_blk_set_slew(&(inSlew[2]), v);
    break;
  case eParam_in4Slew:
    filter_1p_lo_blk_set_slew(&(inSlew[3]), v);
    break;

  case eParam_in1_1:
    filter_1p_lo_blk_in(&(mixSlew[0][0]), v);
    break;
  case eParam_in1_2:
    filter_1p_lo_blk_in(&(mixSlew[0][1]), v);
    break;
  case eParam_in1_3:
    filter_1p_lo_blk_in(&(mixSlew[0][2]), v);
    break;
  case eParam_in1_4:
    filter_1p_lo_blk_in(&(mixSlew[0][3]), v);
    break;
  case eParam_in1MixSlew:
    set_mix_slew(0, v);
    break;

  case eParam_in2_1:
    filter_1p_lo_blk_in(&(mixSlew[1][0]), v);
    break;
  case eParam_in2_2:
    filter_1p_lo_blk_in(&(mixSlew[1][1]), v);
    break;
  case eParam_in2_3:
    filter_1p_lo_blk_in(&(mixSlew[1][2]), v);
    break;
  case eParam_in2_4:
    filter_1p_lo_blk_in(&(mixSlew[1][3]), v);
    break;
  case eParam_in2MixSlew:
    set_mix_slew(1, v);
    break;

  case eParam_in3_1:
    filter_1p_lo_blk_in(&(mixSlew[2][0]), v);
    break;
  case eParam_in3_2:
    filter_1p_lo_blk_in(&(mixSlew[2][1]), v);
    break;
  case eParam_in3_3:
    filter_1p_lo_blk_in(&(mixSlew[2][2]), v);
    break;
  case eParam_in3_4:
    filter_1p_lo_blk_in(&(mixSlew[2][3]), v);
    break;
  case eParam_in3MixSlew:
    set_mix_slew(2, v);
    break;

  case eParam_in4_1:
    filter_1p_lo_blk_in(&(mixSlew[3][0]), v);
    break;
  case eParam_in4_2:
    filter_1p_lo_blk_in(&(mixSlew[3][1]), v);
    break;
  case eParam_in4_3:
    filter_1p_lo_blk_in(&(mixSlew[3][2]), v);
    break;
  case eParam_in4_4:
    filter_1p_lo_blk_in(&(mixSlew[3][3]), v);
    break;
  case eParam_in4MixSlew:
    set_mix_slew(3, v);
    break;

  case eParam_out1:
    filter_1p_lo_blk_in(&(outSlew[0]), v);
    break;
  case eParam_out2:
    filter_1p_lo_blk_in(&(outSlew[1]), v);
    break;
  case eParam_out3:
    filter_1p_lo_blk_in(&(outSlew[2]), v);
    break;
  case eParam_out4:
    filter_1p_lo_blk_in(&(outSlew[3]), v);
    break;

  case eParam_out1Slew:
    filter_1p_lo_blk_set_slew(&(outSlew[0]), v);
    break;
  case eParam_out2Slew:
    filter_1p_lo_blk_set_slew(&(outSlew[1]), v);
    break;
  case eParam_out3Slew:
    filter_1p_lo_blk_set_slew(&(outSlew[2]), v);
    break;
  case eParam_out4Slew:
    filter_1p_lo_blk_set_slew(&(outSlew[3]), v);
    break;

  case eParam_out1Base:
    filter_1p_lo_blk_in(&(baseStSlew[0]), v);
    break;
  case eParam_out2Base:
    filter_1p_lo_blk_in(&(baseStSlew[1]), v);
    break;
  case eParam_out3Base:
    filter_1p_lo_blk_in(&(baseStSlew[2]), v);
    break;
  case eParam_out4Base:
    filter_1p_lo_blk_in(&(baseStSlew[3]), v);
    break;

  case eParam_out1Width:
    filter_1p_lo_blk_in(&(widthStSlew[0]), v);
    break;
  case eParam_out2Width:
    filter_1p_lo_blk_in(&(widthStSlew[1]), v);
    break;
  case eParam_out3Width:
    filter_1p_lo_blk_in(&(widthStSlew[2]), v);
    break;
  case eParam_out4Width:
    filter_1p_lo_blk_in(&(widthStSlew[3]), v);
    break;

  case eParam_out1BWSlew:
    set_bw_slew(0, v);
    break;
  case eParam_out2BWSlew:
    set_bw_slew(1, v);
    break;
  case eParam_out3BWSlew:
    set_bw_slew(2, v);
    break;
  case eParam_out4BWSlew:
    set_bw_slew(3, v);
    break;

  case eParam_cv1:
    filter_1p_lo_blk_in(&(cvSlew[0]), v);
    break;
  case eParam_cv2:
    filter_1p_lo_blk_in(&(cvSlew[1]), v);
    break;
  case eParam_cv3:
    filter_1p_lo_blk_in(&(cvSlew[2]), v);
    break;
  case eParam_cv4:
    filter_1p_lo_blk_in(&(cvSlew[3]), v);
    break;

  case eParam_cvSlew1:
    filter_1p_lo_blk_set_slew(&(cvSlew[0]), v);
    break;
  case eParam_cvSlew2:
    filter_1p_lo_blk_set_slew(&(cvSlew[1]), v);
    break;
  case eParam_cvSlew3:
    filter_1p_lo_blk_set_slew(&(cvSlew[2]), v);
    break;
  case eParam_cvSlew4:
    filter_1p_lo_blk_set_slew(&(cvSlew[3]), v);
    break;

  default:
    break;
  }
}
