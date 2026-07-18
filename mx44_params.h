#ifndef _ALEPH_MX44_PARAMS_H_
#define _ALEPH_MX44_PARAMS_H_

#include "param_common.h"

#define PARAM_AMP_MAX 0x7fffffff
#define PARAM_SLEW_MAX 0x7fffffff

/* something pretty fast, but noticeable */
#define PARAM_SLEW_DEFAULT 0x7ffecccc

/*
  order matches SPEC.md suggested enum grouping:
  in0..in3, in0Slew..in3Slew,
  inX-0..inX-3 + inXMixSlew per input,
  out0..out3, out0Slew..out3Slew
*/
enum params {
  eParam_in0,
  eParam_in1,
  eParam_in2,
  eParam_in3,

  eParam_in0Slew,
  eParam_in1Slew,
  eParam_in2Slew,
  eParam_in3Slew,

  eParam_in0_0,
  eParam_in0_1,
  eParam_in0_2,
  eParam_in0_3,
  eParam_in0MixSlew,

  eParam_in1_0,
  eParam_in1_1,
  eParam_in1_2,
  eParam_in1_3,
  eParam_in1MixSlew,

  eParam_in2_0,
  eParam_in2_1,
  eParam_in2_2,
  eParam_in2_3,
  eParam_in2MixSlew,

  eParam_in3_0,
  eParam_in3_1,
  eParam_in3_2,
  eParam_in3_3,
  eParam_in3MixSlew,

  eParam_out0,
  eParam_out1,
  eParam_out2,
  eParam_out3,

  eParam_out0Slew,
  eParam_out1Slew,
  eParam_out2Slew,
  eParam_out3Slew,

  eParamNumParams
};

extern void fill_param_desc(ParamDesc *desc);

#endif
