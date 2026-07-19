#ifndef _ALEPH_MX44_PARAMS_H_
#define _ALEPH_MX44_PARAMS_H_

#include "param_common.h"

#define PARAM_AMP_MAX 0x7fffffff
#define PARAM_SLEW_MAX 0x7fffffff
#define PARAM_SLEW_MIN 0

/* something pretty fast, but noticeable (matrix send slews) */
#define PARAM_SLEW_DEFAULT 0x7ffecccc

/*
  order matches SPEC.md suggested enum grouping (1-based labels):
  in1..in4, in1Slew..in4Slew,
  inX-1..inX-4 + inXMixSlew per input,
  out1..out4, out1Slew..out4Slew
*/
enum params {
  eParam_in1,
  eParam_in2,
  eParam_in3,
  eParam_in4,

  eParam_in1Slew,
  eParam_in2Slew,
  eParam_in3Slew,
  eParam_in4Slew,

  eParam_in1_1,
  eParam_in1_2,
  eParam_in1_3,
  eParam_in1_4,
  eParam_in1MixSlew,

  eParam_in2_1,
  eParam_in2_2,
  eParam_in2_3,
  eParam_in2_4,
  eParam_in2MixSlew,

  eParam_in3_1,
  eParam_in3_2,
  eParam_in3_3,
  eParam_in3_4,
  eParam_in3MixSlew,

  eParam_in4_1,
  eParam_in4_2,
  eParam_in4_3,
  eParam_in4_4,
  eParam_in4MixSlew,

  eParam_out1,
  eParam_out2,
  eParam_out3,
  eParam_out4,

  eParam_out1Slew,
  eParam_out2Slew,
  eParam_out3Slew,
  eParam_out4Slew,

  eParamNumParams
};

extern void fill_param_desc(ParamDesc *desc);

#endif
