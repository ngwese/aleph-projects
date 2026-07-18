/*
  params.c — mx44 parameter descriptors (descriptor helper only).
*/

#include <string.h>

#include "module.h"
#include "params.h"

static void fill_amp(ParamDesc *d, const char *label) {
  strcpy(d->label, label);
  d->type = eParamTypeAmp;
  d->min = 0x00000000;
  d->max = PARAM_AMP_MAX;
  d->radix = 16;
}

static void fill_slew(ParamDesc *d, const char *label) {
  strcpy(d->label, label);
  d->type = eParamTypeIntegrator;
  d->min = 0x00000000;
  d->max = PARAM_SLEW_MAX;
  d->radix = 16;
}

void fill_param_desc(ParamDesc *desc) {
  fill_amp(&desc[eParam_in0], "in0");
  fill_amp(&desc[eParam_in1], "in1");
  fill_amp(&desc[eParam_in2], "in2");
  fill_amp(&desc[eParam_in3], "in3");

  fill_slew(&desc[eParam_in0Slew], "in0Slew");
  fill_slew(&desc[eParam_in1Slew], "in1Slew");
  fill_slew(&desc[eParam_in2Slew], "in2Slew");
  fill_slew(&desc[eParam_in3Slew], "in3Slew");

  fill_amp(&desc[eParam_in0_0], "in0-0");
  fill_amp(&desc[eParam_in0_1], "in0-1");
  fill_amp(&desc[eParam_in0_2], "in0-2");
  fill_amp(&desc[eParam_in0_3], "in0-3");
  fill_slew(&desc[eParam_in0MixSlew], "in0MixSlew");

  fill_amp(&desc[eParam_in1_0], "in1-0");
  fill_amp(&desc[eParam_in1_1], "in1-1");
  fill_amp(&desc[eParam_in1_2], "in1-2");
  fill_amp(&desc[eParam_in1_3], "in1-3");
  fill_slew(&desc[eParam_in1MixSlew], "in1MixSlew");

  fill_amp(&desc[eParam_in2_0], "in2-0");
  fill_amp(&desc[eParam_in2_1], "in2-1");
  fill_amp(&desc[eParam_in2_2], "in2-2");
  fill_amp(&desc[eParam_in2_3], "in2-3");
  fill_slew(&desc[eParam_in2MixSlew], "in2MixSlew");

  fill_amp(&desc[eParam_in3_0], "in3-0");
  fill_amp(&desc[eParam_in3_1], "in3-1");
  fill_amp(&desc[eParam_in3_2], "in3-2");
  fill_amp(&desc[eParam_in3_3], "in3-3");
  fill_slew(&desc[eParam_in3MixSlew], "in3MixSlew");

  fill_amp(&desc[eParam_out0], "out0");
  fill_amp(&desc[eParam_out1], "out1");
  fill_amp(&desc[eParam_out2], "out2");
  fill_amp(&desc[eParam_out3], "out3");

  fill_slew(&desc[eParam_out0Slew], "out0Slew");
  fill_slew(&desc[eParam_out1Slew], "out1Slew");
  fill_slew(&desc[eParam_out2Slew], "out2Slew");
  fill_slew(&desc[eParam_out3Slew], "out3Slew");
}
