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

  fill_amp(&desc[eParam_in0], "in0");
  fill_amp(&desc[eParam_in1], "in1");
  fill_slew(&desc[eParam_in0Slew], "in0Slew");
  fill_slew(&desc[eParam_in1Slew], "in1Slew");

  fill_amp(&desc[eParam_send0_0], "send0-0");
  fill_amp(&desc[eParam_send0_1], "send0-1");
  fill_slew(&desc[eParam_send0Slew], "send0Slew");
  fill_amp(&desc[eParam_send1_0], "send1-0");
  fill_amp(&desc[eParam_send1_1], "send1-1");
  fill_slew(&desc[eParam_send1Slew], "send1Slew");

  fill_amp(&desc[eParam_fb0], "fb0");
  fill_slew(&desc[eParam_fb0Slew], "fb0Slew");
  fill_amp(&desc[eParam_fb1], "fb1");
  fill_slew(&desc[eParam_fb1Slew], "fb1Slew");

  strcpy(desc[eParam_delay0].label, "delay0");
  desc[eParam_delay0].type = eParamTypeFix;
  desc[eParam_delay0].min = 0;
  desc[eParam_delay0].max = PARAM_DELAY_MAX;
  desc[eParam_delay0].radix = PARAM_DELAY_RADIX;

  strcpy(desc[eParam_delay1].label, "delay1");
  desc[eParam_delay1].type = eParamTypeFix;
  desc[eParam_delay1].min = 0;
  desc[eParam_delay1].max = PARAM_DELAY_MAX;
  desc[eParam_delay1].radix = PARAM_DELAY_RADIX;

  strcpy(desc[eParam_fade0].label, "fade0");
  desc[eParam_fade0].type = eParamTypeFix;
  desc[eParam_fade0].min = PARAM_FADE_MIN;
  desc[eParam_fade0].max = PARAM_FADE_MAX;
  desc[eParam_fade0].radix = PARAM_FADE_RADIX;

  strcpy(desc[eParam_fade1].label, "fade1");
  desc[eParam_fade1].type = eParamTypeFix;
  desc[eParam_fade1].min = PARAM_FADE_MIN;
  desc[eParam_fade1].max = PARAM_FADE_MAX;
  desc[eParam_fade1].radix = PARAM_FADE_RADIX;

  strcpy(desc[eParam_cut0].label, "cut0");
  desc[eParam_cut0].type = eParamTypeSvfFreq;
  desc[eParam_cut0].min = 0;
  desc[eParam_cut0].max = PARAM_CUT_MAX;
  desc[eParam_cut0].radix = 32;

  strcpy(desc[eParam_rq0].label, "rq0");
  desc[eParam_rq0].type = eParamTypeFix;
  desc[eParam_rq0].min = PARAM_RQ_MIN;
  desc[eParam_rq0].max = PARAM_RQ_MAX;
  desc[eParam_rq0].radix = 2;

  fill_amp(&desc[eParam_low0], "low0");
  fill_amp(&desc[eParam_high0], "high0");
  fill_amp(&desc[eParam_band0], "band0");
  fill_amp(&desc[eParam_notch0], "notch0");
  fill_amp(&desc[eParam_fdry0], "fdry0");
  fill_amp(&desc[eParam_fwet0], "fwet0");
  fill_svf_slew(&desc[eParam_cut0Slew], "cut0Slew");
  fill_svf_slew(&desc[eParam_rq0Slew], "rq0Slew");
  fill_svf_slew(&desc[eParam_fdry0Slew], "fdry0Slew");
  fill_svf_slew(&desc[eParam_fwet0Slew], "fwet0Slew");

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

  fill_amp(&desc[eParam_ret0_0], "ret0-0");
  fill_amp(&desc[eParam_ret0_1], "ret0-1");
  fill_slew(&desc[eParam_ret0Slew], "ret0Slew");
  fill_amp(&desc[eParam_ret1_0], "ret1-0");
  fill_amp(&desc[eParam_ret1_1], "ret1-1");
  fill_slew(&desc[eParam_ret1Slew], "ret1Slew");
}
