/*
  mx44_params.c — mx44 parameter descriptors (descriptor helper only).
*/

#include <string.h>

#include "module.h"
#include "mx44_params.h"

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

static void fill_hz(ParamDesc *d, const char *label, ParamValue min,
		    ParamValue max) {
  strcpy(d->label, label);
  d->type = eParamTypeFix;
  d->min = min;
  d->max = max;
  d->radix = 16;
}

void fill_param_desc(ParamDesc *desc) {
  fill_amp(&desc[eParam_in1], "in1");
  fill_amp(&desc[eParam_in2], "in2");
  fill_amp(&desc[eParam_in3], "in3");
  fill_amp(&desc[eParam_in4], "in4");

  fill_slew(&desc[eParam_in1Slew], "in1Slew");
  fill_slew(&desc[eParam_in2Slew], "in2Slew");
  fill_slew(&desc[eParam_in3Slew], "in3Slew");
  fill_slew(&desc[eParam_in4Slew], "in4Slew");

  fill_amp(&desc[eParam_in1_1], "in1-1");
  fill_amp(&desc[eParam_in1_2], "in1-2");
  fill_amp(&desc[eParam_in1_3], "in1-3");
  fill_amp(&desc[eParam_in1_4], "in1-4");
  fill_slew(&desc[eParam_in1MixSlew], "in1MixSlew");

  fill_amp(&desc[eParam_in2_1], "in2-1");
  fill_amp(&desc[eParam_in2_2], "in2-2");
  fill_amp(&desc[eParam_in2_3], "in2-3");
  fill_amp(&desc[eParam_in2_4], "in2-4");
  fill_slew(&desc[eParam_in2MixSlew], "in2MixSlew");

  fill_amp(&desc[eParam_in3_1], "in3-1");
  fill_amp(&desc[eParam_in3_2], "in3-2");
  fill_amp(&desc[eParam_in3_3], "in3-3");
  fill_amp(&desc[eParam_in3_4], "in3-4");
  fill_slew(&desc[eParam_in3MixSlew], "in3MixSlew");

  fill_amp(&desc[eParam_in4_1], "in4-1");
  fill_amp(&desc[eParam_in4_2], "in4-2");
  fill_amp(&desc[eParam_in4_3], "in4-3");
  fill_amp(&desc[eParam_in4_4], "in4-4");
  fill_slew(&desc[eParam_in4MixSlew], "in4MixSlew");

  fill_amp(&desc[eParam_out1], "out1");
  fill_slew(&desc[eParam_out1Slew], "out1Slew");
  fill_hz(&desc[eParam_out1Base], "out1Base", PARAM_HZ_MIN, PARAM_HZ_MAX);
  fill_hz(&desc[eParam_out1Width], "out1Width", PARAM_WIDTH_MIN,
	  PARAM_WIDTH_MAX);
  fill_amp(&desc[eParam_out1Wet], "out1Wet");
  fill_slew(&desc[eParam_out1WetSlew], "out1WetSlew");

  fill_amp(&desc[eParam_out2], "out2");
  fill_slew(&desc[eParam_out2Slew], "out2Slew");
  fill_hz(&desc[eParam_out2Base], "out2Base", PARAM_HZ_MIN, PARAM_HZ_MAX);
  fill_hz(&desc[eParam_out2Width], "out2Width", PARAM_WIDTH_MIN,
	  PARAM_WIDTH_MAX);
  fill_amp(&desc[eParam_out2Wet], "out2Wet");
  fill_slew(&desc[eParam_out2WetSlew], "out2WetSlew");

  fill_amp(&desc[eParam_out3], "out3");
  fill_slew(&desc[eParam_out3Slew], "out3Slew");
  fill_hz(&desc[eParam_out3Base], "out3Base", PARAM_HZ_MIN, PARAM_HZ_MAX);
  fill_hz(&desc[eParam_out3Width], "out3Width", PARAM_WIDTH_MIN,
	  PARAM_WIDTH_MAX);
  fill_amp(&desc[eParam_out3Wet], "out3Wet");
  fill_slew(&desc[eParam_out3WetSlew], "out3WetSlew");

  fill_amp(&desc[eParam_out4], "out4");
  fill_slew(&desc[eParam_out4Slew], "out4Slew");
  fill_hz(&desc[eParam_out4Base], "out4Base", PARAM_HZ_MIN, PARAM_HZ_MAX);
  fill_hz(&desc[eParam_out4Width], "out4Width", PARAM_WIDTH_MIN,
	  PARAM_WIDTH_MAX);
  fill_amp(&desc[eParam_out4Wet], "out4Wet");
  fill_slew(&desc[eParam_out4WetSlew], "out4WetSlew");
}
