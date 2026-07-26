#ifndef _ALEPH_MX44_PARAMS_H_
#define _ALEPH_MX44_PARAMS_H_

#include "param_common.h"

#define PARAM_AMP_MAX 0x7fffffff
#define PARAM_CV_MAX 0x7fffffff
#define PARAM_SLEW_MAX 0x7fffffff
#define PARAM_SLEW_MIN 0

/* something pretty fast, but noticeable (matrix send slews) */
#define PARAM_SLEW_DEFAULT 0x7ffecccc

/* output base-width filter cutoffs (fix16 Hz, integer in high 16 bits) */
#define PARAM_HZ_MIN (20 << 16)
#define PARAM_HZ_MAX (20000 << 16)
#define PARAM_BASE_DEFAULT (20 << 16)
#define PARAM_WIDTH_DEFAULT (20000 << 16)
#define PARAM_WIDTH_MIN 0
#define PARAM_WIDTH_MAX (20000 << 16)

/* filter dry/wet: 0 = full dry (unfiltered), MAX = full wet */
#define PARAM_WET_DEFAULT 0

/*
  order matches SPEC.md suggested enum grouping (1-based labels):
  in1..in4, in1Slew..in4Slew,
  inX-1..inX-4 + inXMixSlew per input,
  per output Y: outY, outYSlew, outYBase, outYWidth, outYWet, outYWetSlew,
  cv1..cv4, cvSlew1..cvSlew4
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
  eParam_out1Slew,
  eParam_out1Base,
  eParam_out1Width,
  eParam_out1Wet,
  eParam_out1WetSlew,

  eParam_out2,
  eParam_out2Slew,
  eParam_out2Base,
  eParam_out2Width,
  eParam_out2Wet,
  eParam_out2WetSlew,

  eParam_out3,
  eParam_out3Slew,
  eParam_out3Base,
  eParam_out3Width,
  eParam_out3Wet,
  eParam_out3WetSlew,

  eParam_out4,
  eParam_out4Slew,
  eParam_out4Base,
  eParam_out4Width,
  eParam_out4Wet,
  eParam_out4WetSlew,

  eParam_cv1,
  eParam_cv2,
  eParam_cv3,
  eParam_cv4,

  eParam_cvSlew1,
  eParam_cvSlew2,
  eParam_cvSlew3,
  eParam_cvSlew4,

  eParamNumParams
};

extern void fill_param_desc(ParamDesc *desc);

#endif
