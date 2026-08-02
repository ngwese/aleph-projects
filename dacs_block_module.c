/*
  dacs_block_module.c
  aleph-bfin (block)

  dacs_block — simplest CV output module for bfin_lib_block.

  same param surface as modules/dacs (cv0..cv3, cvSlew0..3).
  slews at block rate via filter_1p_lo_blk; commits all four DAC
  channels once per block. audio outs are silenced.
*/

#include <string.h>

#include "audio_channels.h"
#include "cv.h"
#include "filter_1p_blk.h"
#include "module.h"
#include "module_custom.h"
#include "dacs_block_params.h"

ModuleData *gModuleData;

static ModuleData super;
static ParamData mParamData[eParamNumParams];

static filter_1p_lo_blk cvSlew[4];
static fract32 cvVal[4];

static inline void param_setup(u32 id, ParamValue v) {
  gModuleData->paramData[id].value = v;
  module_set_param(id, v);
}

void module_init(void) {
  u8 x;

  gModuleData = &super;
  strcpy(gModuleData->name, "dacs_block");
  gModuleData->paramData = mParamData;
  gModuleData->numParams = eParamNumParams;

  for(x = 0; x < 4; ++x) {
    filter_1p_lo_blk_init(&(cvSlew[x]), 0, MODULE_BLOCKSIZE);
  }

  param_setup(eParam_cvSlew0, PARAM_CV_SLEW_DEFAULT);
  param_setup(eParam_cvSlew1, PARAM_CV_SLEW_DEFAULT);
  param_setup(eParam_cvSlew2, PARAM_CV_SLEW_DEFAULT);
  param_setup(eParam_cvSlew3, PARAM_CV_SLEW_DEFAULT);

  param_setup(eParam_cvVal0, PARAM_CV_VAL_DEFAULT);
  param_setup(eParam_cvVal1, PARAM_CV_VAL_DEFAULT);
  param_setup(eParam_cvVal2, PARAM_CV_VAL_DEFAULT);
  param_setup(eParam_cvVal3, PARAM_CV_VAL_DEFAULT);
}

void module_process_block(buffer_t *inChannels, buffer_t *outChannels) {
  u16 frame;
  u8 x;
  fract32 *outCh[4];

  (void)inChannels;

  outCh[0] = audio_out_channel(outChannels, 0);
  outCh[1] = audio_out_channel(outChannels, 1);
  outCh[2] = audio_out_channel(outChannels, 2);
  outCh[3] = audio_out_channel(outChannels, 3);

  for(frame = 0; frame < MODULE_BLOCKSIZE; frame++) {
    outCh[0][frame] = 0;
    outCh[1][frame] = 0;
    outCh[2][frame] = 0;
    outCh[3][frame] = 0;
  }

  /* CV: slew once per block, then one hardware flush */
  for(x = 0; x < 4; ++x) {
    filter_1p_lo_blk_prepare(&(cvSlew[x]));
    cvVal[x] = filter_1p_lo_blk_next(&(cvSlew[x]));
    cv_set(x, cvVal[x]);
  }
  cv_commit();
}

void module_set_param(u32 idx, ParamValue v) {
  switch(idx) {
  case eParam_cvVal0 :
    filter_1p_lo_blk_in(&(cvSlew[0]), v);
    break;
  case eParam_cvVal1 :
    filter_1p_lo_blk_in(&(cvSlew[1]), v);
    break;
  case eParam_cvVal2 :
    filter_1p_lo_blk_in(&(cvSlew[2]), v);
    break;
  case eParam_cvVal3 :
    filter_1p_lo_blk_in(&(cvSlew[3]), v);
    break;

  case eParam_cvSlew0 :
    filter_1p_lo_blk_set_slew(&(cvSlew[0]), v);
    break;
  case eParam_cvSlew1 :
    filter_1p_lo_blk_set_slew(&(cvSlew[1]), v);
    break;
  case eParam_cvSlew2 :
    filter_1p_lo_blk_set_slew(&(cvSlew[2]), v);
    break;
  case eParam_cvSlew3 :
    filter_1p_lo_blk_set_slew(&(cvSlew[3]), v);
    break;

  default :
    break;
  }
}
