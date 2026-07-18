/*
  parallel_module.c — dual send delay with parallel returns (SPEC.md).
*/

#include <string.h>

#include "audio_channels.h"
#include "buffer.h"
#include "delayFadeN.h"
#include "filter_1p.h"
#include "filter_ramp.h"
#include "filter_svf.h"
#include "fract_math.h"
#include "module.h"
#include "parallel_params.h"
#include "types.h"

/* ---- module / SDRAM ----------------------------------------------------- */

ModuleData *gModuleData;

static ModuleData super;
static ParamData mParamData[eParamNumParams];

/* delay buffers only — ~7.3 MB in SDRAM */
typedef struct _parallelSdram {
  fract32 delayBuf[PARALLEL_N_DELAYS][PARALLEL_BUF_FRAMES];
} parallelSdram;

static parallelSdram *sdram;

/* ---- DSP state ---------------------------------------------------------- */

static delayFadeN delay[PARALLEL_N_DELAYS];
static filter_svf svf[PARALLEL_N_DELAYS];

static filter_ramp lpFadeRd[PARALLEL_N_DELAYS];
static u8 fadeTargetRd[PARALLEL_N_DELAYS];

/* amp / feedback / dry-wet / cut-rq slews */
static filter_1p_lo inSlew[2];
static filter_1p_lo sendSlew[2][2]; /* [send][from mono] */
static filter_1p_lo fbSlew[2];
static filter_1p_lo retSlew[2][2]; /* [ret][to mono] */
static filter_1p_lo fdrySlew[2];
static filter_1p_lo fwetSlew[2];
static filter_1p_lo cutSlew[2];
static filter_1p_lo rqSlew[2];

/* previous-frame feedback blend into send (lines out_del → mix_del_del) */
static fract32 fbSig[PARALLEL_N_DELAYS];

/* lines-style timescale: 3.12 after >> 6 */
static volatile s16 globalTimescale;

/* ---- helpers ------------------------------------------------------------ */

static inline void param_setup(u32 id, ParamValue v) {
  gModuleData->paramData[id].value = v;
  module_set_param(id, v);
}

/* lines calc_ms: ticks are s16 (high half of delay param), timescale 3.12 */
static s32 calc_ms(s16 ticks) {
  s32 ret = mult_fr1x32(ticks, globalTimescale);
  ret = add_fr1x32(ret, shr_fr1x32(globalTimescale, 2));
  ret = shr_fr1x32(ret, 12);
  return ret;
}

static u8 start_fade_rd(u8 id) {
  u8 newTarget;
  u8 oldTarget;

  if (lpFadeRd[id].sync) {
    oldTarget = fadeTargetRd[id];
    newTarget = !oldTarget;
    buffer_tapN_copy(&(delay[id].tapRd[oldTarget]),
                     &(delay[id].tapRd[newTarget]));
    fadeTargetRd[id] = newTarget;
    filter_ramp_start(&(lpFadeRd[id]));
    return 1;
  }
  return 0;
}

static void set_send_slew(u8 s, fract32 slew) {
  filter_1p_lo_set_slew(&(sendSlew[s][0]), slew);
  filter_1p_lo_set_slew(&(sendSlew[s][1]), slew);
}

static void set_ret_slew(u8 r, fract32 slew) {
  filter_1p_lo_set_slew(&(retSlew[r][0]), slew);
  filter_1p_lo_set_slew(&(retSlew[r][1]), slew);
}

/* ---- module API --------------------------------------------------------- */

void module_init(void) {
  u8 i;

  gModuleData = &super;
  strcpy(gModuleData->name, "parallel");
  gModuleData->paramData = mParamData;
  gModuleData->numParams = eParamNumParams;

  sdram = (parallelSdram *)SDRAM_ADDRESS;

  for (i = 0; i < PARALLEL_N_DELAYS; i++) {
    memset((void *)sdram->delayBuf[i], 0,
           PARALLEL_BUF_FRAMES * sizeof(fract32));

    delayFadeN_init(&(delay[i]), sdram->delayBuf[i], PARALLEL_BUF_FRAMES);
    delayFadeN_set_run_write(&(delay[i]), 1);
    delayFadeN_set_run_read(&(delay[i]), 1);
    delayFadeN_set_pre(&(delay[i]), 0);
    delayFadeN_set_write(&(delay[i]), 1);

    filter_svf_init(&(svf[i]));
    filter_ramp_init(&(lpFadeRd[i]));
    fadeTargetRd[i] = 0;
    fbSig[i] = 0;

    filter_1p_lo_init(&(cutSlew[i]), 0);
    filter_1p_lo_init(&(rqSlew[i]), 0);
    filter_1p_lo_init(&(fdrySlew[i]), 0);
    filter_1p_lo_init(&(fwetSlew[i]), 0);
    filter_1p_lo_init(&(fbSlew[i]), 0);
  }

  filter_1p_lo_init(&(inSlew[0]), 0);
  filter_1p_lo_init(&(inSlew[1]), 0);
  filter_1p_lo_init(&(sendSlew[0][0]), 0);
  filter_1p_lo_init(&(sendSlew[0][1]), 0);
  filter_1p_lo_init(&(sendSlew[1][0]), 0);
  filter_1p_lo_init(&(sendSlew[1][1]), 0);
  filter_1p_lo_init(&(retSlew[0][0]), 0);
  filter_1p_lo_init(&(retSlew[0][1]), 0);
  filter_1p_lo_init(&(retSlew[1][0]), 0);
  filter_1p_lo_init(&(retSlew[1][1]), 0);

  /* SPEC: in* = unity; send* / fb* / ret* = 0; delays / SVF as lines */
  param_setup(eParam_timescale, PARAM_TIMESCALE_DEFAULT);

  param_setup(eParam_in0, PARAM_AMP_MAX);
  param_setup(eParam_in1, PARAM_AMP_MAX);
  param_setup(eParam_in0Slew, PARAM_SLEW_DEFAULT);
  param_setup(eParam_in1Slew, PARAM_SLEW_DEFAULT);

  param_setup(eParam_send0_0, 0);
  param_setup(eParam_send0_1, 0);
  param_setup(eParam_send0Slew, PARAM_SLEW_DEFAULT);
  param_setup(eParam_send1_0, 0);
  param_setup(eParam_send1_1, 0);
  param_setup(eParam_send1Slew, PARAM_SLEW_DEFAULT);

  param_setup(eParam_fb0, 0);
  param_setup(eParam_fb0Slew, PARAM_SLEW_DEFAULT);
  param_setup(eParam_fb1, 0);
  param_setup(eParam_fb1Slew, PARAM_SLEW_DEFAULT);

  param_setup(eParam_delay0, PARAM_DELAY_DEFAULT);
  param_setup(eParam_delay1, PARAM_DELAY_DEFAULT);
  param_setup(eParam_fade0, PARAM_FADE_DEFAULT);
  param_setup(eParam_fade1, PARAM_FADE_DEFAULT);

  param_setup(eParam_cut0, PARAM_CUT_DEFAULT);
  param_setup(eParam_rq0, PARAM_RQ_DEFAULT);
  param_setup(eParam_low0, PARAM_AMP_6);
  param_setup(eParam_high0, 0);
  param_setup(eParam_band0, 0);
  param_setup(eParam_notch0, 0);
  param_setup(eParam_fdry0, PARAM_AMP_6);
  param_setup(eParam_fwet0, PARAM_AMP_6);
  param_setup(eParam_cut0Slew, PARAM_SLEW_DEFAULT);
  param_setup(eParam_rq0Slew, PARAM_SLEW_DEFAULT);
  param_setup(eParam_fdry0Slew, PARAM_SLEW_DEFAULT);
  param_setup(eParam_fwet0Slew, PARAM_SLEW_DEFAULT);

  param_setup(eParam_cut1, PARAM_CUT_DEFAULT);
  param_setup(eParam_rq1, PARAM_RQ_DEFAULT);
  param_setup(eParam_low1, PARAM_AMP_6);
  param_setup(eParam_high1, 0);
  param_setup(eParam_band1, 0);
  param_setup(eParam_notch1, 0);
  param_setup(eParam_fdry1, PARAM_AMP_6);
  param_setup(eParam_fwet1, PARAM_AMP_6);
  param_setup(eParam_cut1Slew, PARAM_SLEW_DEFAULT);
  param_setup(eParam_rq1Slew, PARAM_SLEW_DEFAULT);
  param_setup(eParam_fdry1Slew, PARAM_SLEW_DEFAULT);
  param_setup(eParam_fwet1Slew, PARAM_SLEW_DEFAULT);

  param_setup(eParam_ret0_0, 0);
  param_setup(eParam_ret0_1, 0);
  param_setup(eParam_ret0Slew, PARAM_SLEW_DEFAULT);
  param_setup(eParam_ret1_0, 0);
  param_setup(eParam_ret1_1, 0);
  param_setup(eParam_ret1Slew, PARAM_SLEW_DEFAULT);
}

void module_process_block(buffer_t *inChannels, buffer_t *outChannels) {
  u16 frame;
  u8 s;
  fract32 dry0, dry1;
  fract32 send_in;
  fract32 tap;
  fract32 svfOut;
  fract32 fdry, fwet;
  fract32 ret0, ret1;
  fract32 *inCh[4];
  fract32 *outCh[4];

  inCh[0] = audio_in_channel(inChannels, 0);
  inCh[1] = audio_in_channel(inChannels, 1);
  inCh[2] = audio_in_channel(inChannels, 2);
  inCh[3] = audio_in_channel(inChannels, 3);

  outCh[0] = audio_out_channel(outChannels, 0);
  outCh[1] = audio_out_channel(outChannels, 1);
  outCh[2] = audio_out_channel(outChannels, 2);
  outCh[3] = audio_out_channel(outChannels, 3);

  for (frame = 0; frame < MODULE_BLOCKSIZE; frame++) {
    dry0 = mult_fr1x32x32(inCh[0][frame], filter_1p_lo_next(&(inSlew[0])));
    dry1 = mult_fr1x32x32(inCh[1][frame], filter_1p_lo_next(&(inSlew[1])));

    for (s = 0; s < PARALLEL_N_DELAYS; s++) {
      /* fade integrator (lines) */
      delay[s].fadeRd = filter_ramp_next(&(lpFadeRd[s]));
      if (fadeTargetRd[s] == 0) {
        delay[s].fadeRd = FR32_MAX - delay[s].fadeRd;
      }

      send_in = add_fr1x32(
          mult_fr1x32x32(dry0, filter_1p_lo_next(&(sendSlew[s][0]))),
          mult_fr1x32x32(dry1, filter_1p_lo_next(&(sendSlew[s][1]))));
      send_in = add_fr1x32(
          send_in,
          mult_fr1x32x32(fbSig[s], filter_1p_lo_next(&(fbSlew[s]))));

      tap = delayFadeN_next(&(delay[s]), send_in);

      filter_svf_set_coeff(&(svf[s]), filter_1p_lo_next(&(cutSlew[s])));
      filter_svf_set_rq(&(svf[s]), filter_1p_lo_next(&(rqSlew[s])));
      svfOut = filter_svf_next(&(svf[s]), tap);

      fdry = filter_1p_lo_next(&(fdrySlew[s]));
      fwet = filter_1p_lo_next(&(fwetSlew[s]));
      fbSig[s] = add_fr1x32(mult_fr1x32x32(tap, fdry),
                            mult_fr1x32x32(svfOut, fwet));

      outCh[2 + s][frame] = tap; /* raw tap → hardware send */
    }

    ret0 = add_fr1x32(
        mult_fr1x32x32(inCh[2][frame], filter_1p_lo_next(&(retSlew[0][0]))),
        mult_fr1x32x32(inCh[3][frame], filter_1p_lo_next(&(retSlew[1][0]))));
    ret1 = add_fr1x32(
        mult_fr1x32x32(inCh[2][frame], filter_1p_lo_next(&(retSlew[0][1]))),
        mult_fr1x32x32(inCh[3][frame], filter_1p_lo_next(&(retSlew[1][1]))));

    outCh[0][frame] = add_fr1x32(dry0, ret0);
    outCh[1][frame] = add_fr1x32(dry1, ret1);
  }
}

void module_set_param(u32 idx, ParamValue v) {
  switch (idx) {
  case eParam_timescale:
    /* lines: globalTimescale = v >> 6 */
    globalTimescale = (s16)(v >> 6);
    break;

  case eParam_in0:
    filter_1p_lo_in(&(inSlew[0]), v);
    break;
  case eParam_in1:
    filter_1p_lo_in(&(inSlew[1]), v);
    break;
  case eParam_in0Slew:
    filter_1p_lo_set_slew(&(inSlew[0]), v);
    break;
  case eParam_in1Slew:
    filter_1p_lo_set_slew(&(inSlew[1]), v);
    break;

  case eParam_send0_0:
    filter_1p_lo_in(&(sendSlew[0][0]), v);
    break;
  case eParam_send0_1:
    filter_1p_lo_in(&(sendSlew[0][1]), v);
    break;
  case eParam_send0Slew:
    set_send_slew(0, v);
    break;
  case eParam_send1_0:
    filter_1p_lo_in(&(sendSlew[1][0]), v);
    break;
  case eParam_send1_1:
    filter_1p_lo_in(&(sendSlew[1][1]), v);
    break;
  case eParam_send1Slew:
    set_send_slew(1, v);
    break;

  case eParam_fb0:
    filter_1p_lo_in(&(fbSlew[0]), v);
    break;
  case eParam_fb0Slew:
    filter_1p_lo_set_slew(&(fbSlew[0]), v);
    break;
  case eParam_fb1:
    filter_1p_lo_in(&(fbSlew[1]), v);
    break;
  case eParam_fb1Slew:
    filter_1p_lo_set_slew(&(fbSlew[1]), v);
    break;

  case eParam_delay0:
    if (start_fade_rd(0)) {
      delayFadeN_set_delay_ms(&(delay[0]), calc_ms(trunc_fr1x32(v)),
                              fadeTargetRd[0]);
    }
    break;
  case eParam_delay1:
    if (start_fade_rd(1)) {
      delayFadeN_set_delay_ms(&(delay[1]), calc_ms(trunc_fr1x32(v)),
                              fadeTargetRd[1]);
    }
    break;

  case eParam_fade0:
    if (v > PARAM_FADE_MIN) {
      filter_ramp_set_inc(&(lpFadeRd[0]), v);
    }
    break;
  case eParam_fade1:
    if (v > PARAM_FADE_MIN) {
      filter_ramp_set_inc(&(lpFadeRd[1]), v);
    }
    break;

  case eParam_cut0:
    filter_1p_lo_in(&(cutSlew[0]), v);
    break;
  case eParam_rq0:
    filter_1p_lo_in(&(rqSlew[0]), v);
    break;
  case eParam_low0:
    filter_svf_set_low(&(svf[0]), v);
    break;
  case eParam_high0:
    filter_svf_set_high(&(svf[0]), v);
    break;
  case eParam_band0:
    filter_svf_set_band(&(svf[0]), v);
    break;
  case eParam_notch0:
    filter_svf_set_notch(&(svf[0]), v);
    break;
  case eParam_fdry0:
    filter_1p_lo_in(&(fdrySlew[0]), v);
    break;
  case eParam_fwet0:
    filter_1p_lo_in(&(fwetSlew[0]), v);
    break;
  case eParam_cut0Slew:
    filter_1p_lo_set_slew(&(cutSlew[0]), v);
    break;
  case eParam_rq0Slew:
    filter_1p_lo_set_slew(&(rqSlew[0]), v);
    break;
  case eParam_fdry0Slew:
    filter_1p_lo_set_slew(&(fdrySlew[0]), v);
    break;
  case eParam_fwet0Slew:
    filter_1p_lo_set_slew(&(fwetSlew[0]), v);
    break;

  case eParam_cut1:
    filter_1p_lo_in(&(cutSlew[1]), v);
    break;
  case eParam_rq1:
    filter_1p_lo_in(&(rqSlew[1]), v);
    break;
  case eParam_low1:
    filter_svf_set_low(&(svf[1]), v);
    break;
  case eParam_high1:
    filter_svf_set_high(&(svf[1]), v);
    break;
  case eParam_band1:
    filter_svf_set_band(&(svf[1]), v);
    break;
  case eParam_notch1:
    filter_svf_set_notch(&(svf[1]), v);
    break;
  case eParam_fdry1:
    filter_1p_lo_in(&(fdrySlew[1]), v);
    break;
  case eParam_fwet1:
    filter_1p_lo_in(&(fwetSlew[1]), v);
    break;
  case eParam_cut1Slew:
    filter_1p_lo_set_slew(&(cutSlew[1]), v);
    break;
  case eParam_rq1Slew:
    filter_1p_lo_set_slew(&(rqSlew[1]), v);
    break;
  case eParam_fdry1Slew:
    filter_1p_lo_set_slew(&(fdrySlew[1]), v);
    break;
  case eParam_fwet1Slew:
    filter_1p_lo_set_slew(&(fwetSlew[1]), v);
    break;

  case eParam_ret0_0:
    filter_1p_lo_in(&(retSlew[0][0]), v);
    break;
  case eParam_ret0_1:
    filter_1p_lo_in(&(retSlew[0][1]), v);
    break;
  case eParam_ret0Slew:
    set_ret_slew(0, v);
    break;
  case eParam_ret1_0:
    filter_1p_lo_in(&(retSlew[1][0]), v);
    break;
  case eParam_ret1_1:
    filter_1p_lo_in(&(retSlew[1][1]), v);
    break;
  case eParam_ret1Slew:
    set_ret_slew(1, v);
    break;

  default:
    break;
  }
}
