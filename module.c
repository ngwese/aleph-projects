/*
  module.c
  aleph-bfin (block)

  mx44 — 4×4 matrix mixer for bfin_lib_block.

  adc[X] * in[X] → bus; bus * in[X]-Y summed into out mix Y; * out[Y] → dac[Y].
  each amplitude has a 1-pole slew; matrix sends from input X share inXMixSlew.
*/

#include <string.h>

#include "audio_channels.h"
#include "filter_1p.h"
#include "fract_math.h"
#include "module.h"
#include "params.h"

ModuleData *gModuleData;

static ModuleData super;
static ParamData mParamData[eParamNumParams];

static filter_1p_lo inSlew[4];
static filter_1p_lo outSlew[4];
static filter_1p_lo mixSlew[4][4]; /* [in][out] */

static fract32 inVal[4];
static fract32 outVal[4];
static fract32 mixVal[4][4];

static inline void param_setup(u32 id, ParamValue v) {
  gModuleData->paramData[id].value = v;
  module_set_param(id, v);
}

static void set_mix_slew(u8 xin, fract32 slew) {
  u8 y;
  for(y = 0; y < 4; ++y) {
    filter_1p_lo_set_slew(&(mixSlew[xin][y]), slew);
  }
}

void module_init(void) {
  u8 x;
  u8 y;

  gModuleData = &super;
  strcpy(gModuleData->name, "mx44");
  gModuleData->paramData = mParamData;
  gModuleData->numParams = eParamNumParams;

  for(x = 0; x < 4; ++x) {
    filter_1p_lo_init(&(inSlew[x]), 0);
    filter_1p_lo_init(&(outSlew[x]), 0);
    for(y = 0; y < 4; ++y) {
      filter_1p_lo_init(&(mixSlew[x][y]), 0);
    }
  }

  /* identity matrix at unity; input/output levels unity; default slews */
  param_setup(eParam_in0, PARAM_AMP_MAX);
  param_setup(eParam_in1, PARAM_AMP_MAX);
  param_setup(eParam_in2, PARAM_AMP_MAX);
  param_setup(eParam_in3, PARAM_AMP_MAX);

  param_setup(eParam_in0Slew, PARAM_SLEW_DEFAULT);
  param_setup(eParam_in1Slew, PARAM_SLEW_DEFAULT);
  param_setup(eParam_in2Slew, PARAM_SLEW_DEFAULT);
  param_setup(eParam_in3Slew, PARAM_SLEW_DEFAULT);

  param_setup(eParam_in0_0, PARAM_AMP_MAX);
  param_setup(eParam_in0_1, 0);
  param_setup(eParam_in0_2, 0);
  param_setup(eParam_in0_3, 0);
  param_setup(eParam_in0MixSlew, PARAM_SLEW_DEFAULT);

  param_setup(eParam_in1_0, 0);
  param_setup(eParam_in1_1, PARAM_AMP_MAX);
  param_setup(eParam_in1_2, 0);
  param_setup(eParam_in1_3, 0);
  param_setup(eParam_in1MixSlew, PARAM_SLEW_DEFAULT);

  param_setup(eParam_in2_0, 0);
  param_setup(eParam_in2_1, 0);
  param_setup(eParam_in2_2, PARAM_AMP_MAX);
  param_setup(eParam_in2_3, 0);
  param_setup(eParam_in2MixSlew, PARAM_SLEW_DEFAULT);

  param_setup(eParam_in3_0, 0);
  param_setup(eParam_in3_1, 0);
  param_setup(eParam_in3_2, 0);
  param_setup(eParam_in3_3, PARAM_AMP_MAX);
  param_setup(eParam_in3MixSlew, PARAM_SLEW_DEFAULT);

  param_setup(eParam_out0, PARAM_AMP_MAX);
  param_setup(eParam_out1, PARAM_AMP_MAX);
  param_setup(eParam_out2, PARAM_AMP_MAX);
  param_setup(eParam_out3, PARAM_AMP_MAX);

  param_setup(eParam_out0Slew, PARAM_SLEW_DEFAULT);
  param_setup(eParam_out1Slew, PARAM_SLEW_DEFAULT);
  param_setup(eParam_out2Slew, PARAM_SLEW_DEFAULT);
  param_setup(eParam_out3Slew, PARAM_SLEW_DEFAULT);
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

  for(frame = 0; frame < MODULE_BLOCKSIZE; frame++) {
    for(x = 0; x < 4; ++x) {
      inVal[x] = filter_1p_lo_next(&(inSlew[x]));
      outVal[x] = filter_1p_lo_next(&(outSlew[x]));
      for(y = 0; y < 4; ++y) {
        mixVal[x][y] = filter_1p_lo_next(&(mixSlew[x][y]));
      }
      bus[x] = mult_fr1x32x32(inCh[x][frame], inVal[x]);
    }

    for(y = 0; y < 4; ++y) {
      mix = 0;
      for(x = 0; x < 4; ++x) {
        mix = add_fr1x32(mix, mult_fr1x32x32(bus[x], mixVal[x][y]));
      }
      outCh[y][frame] = mult_fr1x32x32(mix, outVal[y]);
    }
  }
}

void module_set_param(u32 idx, ParamValue v) {
  switch(idx) {
  case eParam_in0 :
    filter_1p_lo_in(&(inSlew[0]), v);
    break;
  case eParam_in1 :
    filter_1p_lo_in(&(inSlew[1]), v);
    break;
  case eParam_in2 :
    filter_1p_lo_in(&(inSlew[2]), v);
    break;
  case eParam_in3 :
    filter_1p_lo_in(&(inSlew[3]), v);
    break;

  case eParam_in0Slew :
    filter_1p_lo_set_slew(&(inSlew[0]), v);
    break;
  case eParam_in1Slew :
    filter_1p_lo_set_slew(&(inSlew[1]), v);
    break;
  case eParam_in2Slew :
    filter_1p_lo_set_slew(&(inSlew[2]), v);
    break;
  case eParam_in3Slew :
    filter_1p_lo_set_slew(&(inSlew[3]), v);
    break;

  case eParam_in0_0 :
    filter_1p_lo_in(&(mixSlew[0][0]), v);
    break;
  case eParam_in0_1 :
    filter_1p_lo_in(&(mixSlew[0][1]), v);
    break;
  case eParam_in0_2 :
    filter_1p_lo_in(&(mixSlew[0][2]), v);
    break;
  case eParam_in0_3 :
    filter_1p_lo_in(&(mixSlew[0][3]), v);
    break;
  case eParam_in0MixSlew :
    set_mix_slew(0, v);
    break;

  case eParam_in1_0 :
    filter_1p_lo_in(&(mixSlew[1][0]), v);
    break;
  case eParam_in1_1 :
    filter_1p_lo_in(&(mixSlew[1][1]), v);
    break;
  case eParam_in1_2 :
    filter_1p_lo_in(&(mixSlew[1][2]), v);
    break;
  case eParam_in1_3 :
    filter_1p_lo_in(&(mixSlew[1][3]), v);
    break;
  case eParam_in1MixSlew :
    set_mix_slew(1, v);
    break;

  case eParam_in2_0 :
    filter_1p_lo_in(&(mixSlew[2][0]), v);
    break;
  case eParam_in2_1 :
    filter_1p_lo_in(&(mixSlew[2][1]), v);
    break;
  case eParam_in2_2 :
    filter_1p_lo_in(&(mixSlew[2][2]), v);
    break;
  case eParam_in2_3 :
    filter_1p_lo_in(&(mixSlew[2][3]), v);
    break;
  case eParam_in2MixSlew :
    set_mix_slew(2, v);
    break;

  case eParam_in3_0 :
    filter_1p_lo_in(&(mixSlew[3][0]), v);
    break;
  case eParam_in3_1 :
    filter_1p_lo_in(&(mixSlew[3][1]), v);
    break;
  case eParam_in3_2 :
    filter_1p_lo_in(&(mixSlew[3][2]), v);
    break;
  case eParam_in3_3 :
    filter_1p_lo_in(&(mixSlew[3][3]), v);
    break;
  case eParam_in3MixSlew :
    set_mix_slew(3, v);
    break;

  case eParam_out0 :
    filter_1p_lo_in(&(outSlew[0]), v);
    break;
  case eParam_out1 :
    filter_1p_lo_in(&(outSlew[1]), v);
    break;
  case eParam_out2 :
    filter_1p_lo_in(&(outSlew[2]), v);
    break;
  case eParam_out3 :
    filter_1p_lo_in(&(outSlew[3]), v);
    break;

  case eParam_out0Slew :
    filter_1p_lo_set_slew(&(outSlew[0]), v);
    break;
  case eParam_out1Slew :
    filter_1p_lo_set_slew(&(outSlew[1]), v);
    break;
  case eParam_out2Slew :
    filter_1p_lo_set_slew(&(outSlew[2]), v);
    break;
  case eParam_out3Slew :
    filter_1p_lo_set_slew(&(outSlew[3]), v);
    break;

  default :
    break;
  }
}
