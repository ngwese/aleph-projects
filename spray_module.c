/*
   spray_module.c
   aleph-bfin (block)

   "spray" — block-processed mix module for bfin_lib_block.

   applies attenuation with slew to each input signal,
   then mixes all attenuated signals to all outputs.

   CV params are retained (same indices as modules/mix and apps/mix)
   but are no-ops: bfin_lib_block has no CV DAC driver.
*/

#include <string.h>

#include "audio_channels.h"
#include "filter_1p.h"
#include "fract_math.h"
#include "module.h"
#include "spray_params.h"

ModuleData *gModuleData;

static ModuleData super;
static ParamData mParamData[eParamNumParams];

// slewed input attenuation
static fract32 adcVal[4];
static filter_1p_lo adcSlew[4];

static inline void param_setup(u32 id, ParamValue v) {
  gModuleData->paramData[id].value = v;
  module_set_param(id, v);
}

void module_init(void) {
  gModuleData = &super;
  strcpy(gModuleData->name, "spray");
  gModuleData->paramData = mParamData;
  gModuleData->numParams = eParamNumParams;

  filter_1p_lo_init(&(adcSlew[0]), 0);
  filter_1p_lo_init(&(adcSlew[1]), 0);
  filter_1p_lo_init(&(adcSlew[2]), 0);
  filter_1p_lo_init(&(adcSlew[3]), 0);

  // CV params recorded for SPI sync / apps/mix compatibility; set_param is no-op
  param_setup(eParam_cv0, 0);
  param_setup(eParam_cv1, 0);
  param_setup(eParam_cv2, 0);
  param_setup(eParam_cv3, 0);

  // amp = 1/4 (-12dB)
  param_setup(eParam_adc0, PARAM_AMP_MAX >> 2);
  param_setup(eParam_adc1, PARAM_AMP_MAX >> 2);
  param_setup(eParam_adc2, PARAM_AMP_MAX >> 2);
  param_setup(eParam_adc3, PARAM_AMP_MAX >> 2);

  param_setup(eParam_adcSlew0, PARAM_SLEW_DEFAULT);
  param_setup(eParam_adcSlew1, PARAM_SLEW_DEFAULT);
  param_setup(eParam_adcSlew2, PARAM_SLEW_DEFAULT);
  param_setup(eParam_adcSlew3, PARAM_SLEW_DEFAULT);
  param_setup(eParam_cvSlew0, PARAM_SLEW_DEFAULT);
  param_setup(eParam_cvSlew1, PARAM_SLEW_DEFAULT);
  param_setup(eParam_cvSlew2, PARAM_SLEW_DEFAULT);
  param_setup(eParam_cvSlew3, PARAM_SLEW_DEFAULT);
}

void module_process_block(buffer_t *inChannels, buffer_t *outChannels) {
  u16 frame;
  fract32 outBus;

  fract32 *in0 = audio_in_channel(inChannels, 0);
  fract32 *in1 = audio_in_channel(inChannels, 1);
  fract32 *in2 = audio_in_channel(inChannels, 2);
  fract32 *in3 = audio_in_channel(inChannels, 3);

  fract32 *out0 = audio_out_channel(outChannels, 0);
  fract32 *out1 = audio_out_channel(outChannels, 1);
  fract32 *out2 = audio_out_channel(outChannels, 2);
  fract32 *out3 = audio_out_channel(outChannels, 3);

  for(frame = 0; frame < MODULE_BLOCKSIZE; frame++) {
    // advance amp slews once per sample (same rate as frame mix)
    adcVal[0] = filter_1p_lo_next(&(adcSlew[0]));
    adcVal[1] = filter_1p_lo_next(&(adcSlew[1]));
    adcVal[2] = filter_1p_lo_next(&(adcSlew[2]));
    adcVal[3] = filter_1p_lo_next(&(adcSlew[3]));

    outBus = 0;
    outBus = add_fr1x32(outBus, mult_fr1x32x32(in0[frame], adcVal[0]));
    outBus = add_fr1x32(outBus, mult_fr1x32x32(in1[frame], adcVal[1]));
    outBus = add_fr1x32(outBus, mult_fr1x32x32(in2[frame], adcVal[2]));
    outBus = add_fr1x32(outBus, mult_fr1x32x32(in3[frame], adcVal[3]));

    out0[frame] = outBus;
    out1[frame] = outBus;
    out2[frame] = outBus;
    out3[frame] = outBus;
  }
}

void module_set_param(u32 idx, ParamValue v) {
  switch(idx) {
  // CV — no-op (kept for param-index compatibility with apps/mix)
  case eParam_cv0 :
  case eParam_cv1 :
  case eParam_cv2 :
  case eParam_cv3 :
  case eParam_cvSlew0 :
  case eParam_cvSlew1 :
  case eParam_cvSlew2 :
  case eParam_cvSlew3 :
    break;

  case eParam_adc0 :
    filter_1p_lo_in(&(adcSlew[0]), v);
    break;
  case eParam_adc1 :
    filter_1p_lo_in(&(adcSlew[1]), v);
    break;
  case eParam_adc2 :
    filter_1p_lo_in(&(adcSlew[2]), v);
    break;
  case eParam_adc3 :
    filter_1p_lo_in(&(adcSlew[3]), v);
    break;

  case eParam_adcSlew0 :
    filter_1p_lo_set_slew(&(adcSlew[0]), v);
    break;
  case eParam_adcSlew1 :
    filter_1p_lo_set_slew(&(adcSlew[1]), v);
    break;
  case eParam_adcSlew2 :
    filter_1p_lo_set_slew(&(adcSlew[2]), v);
    break;
  case eParam_adcSlew3 :
    filter_1p_lo_set_slew(&(adcSlew[3]), v);
    break;

  default :
    break;
  }
}
