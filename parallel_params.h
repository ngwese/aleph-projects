#ifndef _ALEPH_PARALLEL_PARAMS_H_
#define _ALEPH_PARALLEL_PARAMS_H_

#include "param_common.h"

#define PARALLEL_BUF_FRAMES (20 * 48000)

#define PARALLEL_N_DELAYS 2

#define PARAM_AMP_MAX 0x7fffffff
#define PARAM_AMP_6 (PARAM_AMP_MAX >> 1)

#define PARAM_SLEW_MAX 0x7fffffff
/* lines default (also used for SVF / fdry / fwet slews) */
#define PARAM_SLEW_DEFAULT 0x77000000

/* delay time units match lines (ms in high half); cap at 20 s */
#define PARAM_DELAY_MAX (20000 << 16)
#define PARAM_DELAY_RADIX 32
#define PARAM_DELAY_DEFAULT (1000 << 16)

#define PARAM_FADE_MIN 0x2000
#define PARAM_FADE_MAX 0x2000000
#define PARAM_FADE_RADIX 12
#define PARAM_FADE_DEFAULT 0x100000

#define PARAM_CUT_MAX 0x7fffffff
#define PARAM_CUT_DEFAULT 0x43D0A8EC

#define PARAM_RQ_MIN 0x00000000
#define PARAM_RQ_MAX 0x0000ffff
#define PARAM_RQ_DEFAULT 0x0000FFF0

#define SLEW_SECONDS_MIN 0x2000
#define SLEW_SECONDS_MAX 0x400000
#define SLEW_SECONDS_RADIX 7

#define PARAM_TIMESCALE_DEFAULT (1 << 16)

enum params {
  eParam_timescale,

  eParam_in0,
  eParam_in1,
  eParam_in0Slew,
  eParam_in1Slew,

  eParam_send0_0,
  eParam_send0_1,
  eParam_send0Slew,
  eParam_send1_0,
  eParam_send1_1,
  eParam_send1Slew,

  eParam_fb0,
  eParam_fb0Slew,
  eParam_fb1,
  eParam_fb1Slew,

  eParam_delay0,
  eParam_delay1,
  eParam_fade0,
  eParam_fade1,

  eParam_cut0,
  eParam_rq0,
  eParam_low0,
  eParam_high0,
  eParam_band0,
  eParam_notch0,
  eParam_fdry0,
  eParam_fwet0,
  eParam_cut0Slew,
  eParam_rq0Slew,
  eParam_fdry0Slew,
  eParam_fwet0Slew,

  eParam_cut1,
  eParam_rq1,
  eParam_low1,
  eParam_high1,
  eParam_band1,
  eParam_notch1,
  eParam_fdry1,
  eParam_fwet1,
  eParam_cut1Slew,
  eParam_rq1Slew,
  eParam_fdry1Slew,
  eParam_fwet1Slew,

  eParam_ret0_0,
  eParam_ret0_1,
  eParam_ret0Slew,
  eParam_ret1_0,
  eParam_ret1_1,
  eParam_ret1Slew,

  eParamNumParams
};

extern void fill_param_desc(ParamDesc *desc);

#endif
