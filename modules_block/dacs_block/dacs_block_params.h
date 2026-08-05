#ifndef _ALEPH_DACS_BLOCK_PARAMS_H_
#define _ALEPH_DACS_BLOCK_PARAMS_H_

#include "fix.h"
#include "param_common.h"

#define PARAM_DAC_MIN 0
#define PARAM_DAC_MAX FR32_MAX
#define PARAM_DAC_RADIX 16

#define PARAM_AMP_0 (FR32_MAX)
#define PARAM_AMP_6 (FR32_MAX >> 1)
#define PARAM_AMP_12 (FR32_MAX >> 2)

#define PARAM_CV_SLEW_DEFAULT 0x77000000

// cv output
#define PARAM_CV_VAL_DEFAULT PARAM_AMP_6

// enumerate parameters (same indices/labels as modules/dacs)
enum params {
  eParam_cvVal0,
  eParam_cvVal1,
  eParam_cvVal2,
  eParam_cvVal3,
  eParam_cvSlew0,
  eParam_cvSlew1,
  eParam_cvSlew2,
  eParam_cvSlew3,

  eParamNumParams
};

extern void fill_param_desc(ParamDesc *desc);

#endif // header guard
