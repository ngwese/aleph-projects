/*
  parallel_params.c — parallel parameter descriptors (descriptor helper).
*/

#include <string.h>

#include "module.h"
#include "parallel_params.h"

static void fill_amp(ParamDesc *d, const char *label) {
  strcpy(d->label, label);
  d->type = eParamTypeAmp;
  d->min = 0;
  d->max = PARAM_AMP_MAX;
  d->radix = 16;
}

static void fill_slew(ParamDesc *d, const char *label) {
  strcpy(d->label, label);
  d->type = eParamTypeIntegrator;
  d->min = 0;
  d->max = PARAM_SLEW_MAX;
  d->radix = 16;
}

static void fill_svf_slew(ParamDesc *d, const char *label) {
  strcpy(d->label, label);
  d->type = eParamTypeIntegrator;
  d->min = SLEW_SECONDS_MIN;
  d->max = SLEW_SECONDS_MAX;
  d->radix = SLEW_SECONDS_RADIX;
}

void fill_param_desc(ParamDesc *desc) {
  strcpy(desc[eParam_timescale].label, "timescale");
  desc[eParam_timescale].type = eParamTypeFix;
  desc[eParam_timescale].min = 0;
  desc[eParam_timescale].max = 0x00040000;
  desc[eParam_timescale].radix = 4;

  fill_amp(&desc[eParam_in1], "in1");
  fill_amp(&desc[eParam_in2], "in2");
  fill_slew(&desc[eParam_in1Slew], "in1Slew");
  fill_slew(&desc[eParam_in2Slew], "in2Slew");

  fill_amp(&desc[eParam_send1_1], "send1-1");
  fill_amp(&desc[eParam_send1_2], "send1-2");
  fill_slew(&desc[eParam_send1Slew], "send1Slew");
  fill_amp(&desc[eParam_send2_1], "send2-1");
  fill_amp(&desc[eParam_send2_2], "send2-2");
  fill_slew(&desc[eParam_send2Slew], "send2Slew");

  fill_amp(&desc[eParam_fb1], "fb1");
  fill_slew(&desc[eParam_fb1Slew], "fb1Slew");
  fill_amp(&desc[eParam_fb2], "fb2");
  fill_slew(&desc[eParam_fb2Slew], "fb2Slew");

  strcpy(desc[eParam_delay1].label, "delay1");
  desc[eParam_delay1].type = eParamTypeFix;
  desc[eParam_delay1].min = 0;
  desc[eParam_delay1].max = PARAM_DELAY_MAX;
  desc[eParam_delay1].radix = PARAM_DELAY_RADIX;

  strcpy(desc[eParam_delay2].label, "delay2");
  desc[eParam_delay2].type = eParamTypeFix;
  desc[eParam_delay2].min = 0;
  desc[eParam_delay2].max = PARAM_DELAY_MAX;
  desc[eParam_delay2].radix = PARAM_DELAY_RADIX;

  strcpy(desc[eParam_fade1].label, "fade1");
  desc[eParam_fade1].type = eParamTypeFix;
  desc[eParam_fade1].min = PARAM_FADE_MIN;
  desc[eParam_fade1].max = PARAM_FADE_MAX;
  desc[eParam_fade1].radix = PARAM_FADE_RADIX;

  strcpy(desc[eParam_fade2].label, "fade2");
  desc[eParam_fade2].type = eParamTypeFix;
  desc[eParam_fade2].min = PARAM_FADE_MIN;
  desc[eParam_fade2].max = PARAM_FADE_MAX;
  desc[eParam_fade2].radix = PARAM_FADE_RADIX;

  strcpy(desc[eParam_cut1].label, "cut1");
  desc[eParam_cut1].type = eParamTypeSvfFreq;
  desc[eParam_cut1].min = 0;
  desc[eParam_cut1].max = PARAM_CUT_MAX;
  desc[eParam_cut1].radix = 32;

  strcpy(desc[eParam_rq1].label, "rq1");
  desc[eParam_rq1].type = eParamTypeFix;
  desc[eParam_rq1].min = PARAM_RQ_MIN;
  desc[eParam_rq1].max = PARAM_RQ_MAX;
  desc[eParam_rq1].radix = 2;

  fill_amp(&desc[eParam_low1], "low1");
  fill_amp(&desc[eParam_high1], "high1");
  fill_amp(&desc[eParam_band1], "band1");
  fill_amp(&desc[eParam_notch1], "notch1");
  fill_amp(&desc[eParam_fdry1], "fdry1");
  fill_amp(&desc[eParam_fwet1], "fwet1");
  fill_svf_slew(&desc[eParam_cut1Slew], "cut1Slew");
  fill_svf_slew(&desc[eParam_rq1Slew], "rq1Slew");
  fill_svf_slew(&desc[eParam_fdry1Slew], "fdry1Slew");
  fill_svf_slew(&desc[eParam_fwet1Slew], "fwet1Slew");

  strcpy(desc[eParam_cut2].label, "cut2");
  desc[eParam_cut2].type = eParamTypeSvfFreq;
  desc[eParam_cut2].min = 0;
  desc[eParam_cut2].max = PARAM_CUT_MAX;
  desc[eParam_cut2].radix = 32;

  strcpy(desc[eParam_rq2].label, "rq2");
  desc[eParam_rq2].type = eParamTypeFix;
  desc[eParam_rq2].min = PARAM_RQ_MIN;
  desc[eParam_rq2].max = PARAM_RQ_MAX;
  desc[eParam_rq2].radix = 2;

  fill_amp(&desc[eParam_low2], "low2");
  fill_amp(&desc[eParam_high2], "high2");
  fill_amp(&desc[eParam_band2], "band2");
  fill_amp(&desc[eParam_notch2], "notch2");
  fill_amp(&desc[eParam_fdry2], "fdry2");
  fill_amp(&desc[eParam_fwet2], "fwet2");
  fill_svf_slew(&desc[eParam_cut2Slew], "cut2Slew");
  fill_svf_slew(&desc[eParam_rq2Slew], "rq2Slew");
  fill_svf_slew(&desc[eParam_fdry2Slew], "fdry2Slew");
  fill_svf_slew(&desc[eParam_fwet2Slew], "fwet2Slew");

  fill_amp(&desc[eParam_ret1_1], "ret1-1");
  fill_amp(&desc[eParam_ret1_2], "ret1-2");
  fill_slew(&desc[eParam_ret1Slew], "ret1Slew");
  fill_amp(&desc[eParam_ret2_1], "ret2-1");
  fill_amp(&desc[eParam_ret2_2], "ret2-2");
  fill_slew(&desc[eParam_ret2Slew], "ret2Slew");
}
