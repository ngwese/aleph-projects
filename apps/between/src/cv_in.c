#include "cv_in.h"

#include "app_timers.h"
#include "midi_between.h"
#include "midi_nrpn.h"
#include "module_load.h"
#include "morph2d.h"
#include "pages.h"
#include "play_maps.h"
#include "render.h"
#include "slots.h"
#include "state.h"

#define CV_IN_ADC_MAX 4095u
#define CV_IN_FR32_MAX 0x7fffffffu
#define CV_IN_POLL_MS 50u
/* the low bits of the AD7923 reading dither constantly. applying a map costs
 * a param SPI write, and a morph map costs a full state_apply() sweep, so
 * ignore movement below this many counts. */
#define CV_IN_DEADBAND 8

static fract32 g_cv_fr[PLAY_MAPS_CV_COUNT];
static u16 g_cv_applied[PLAY_MAPS_CV_COUNT];
static u8 g_cv_have[PLAY_MAPS_CV_COUNT];
static u8 g_inspect_hold;
static u8 g_adc_running;

static u16 fr32_to_morph(fract32 fr) {
  if (fr <= 0) {
    return 0;
  }
  if ((u32)fr >= CV_IN_FR32_MAX) {
    return MORPH2D_ONE;
  }
  /* shift so (fr>>15)*MORPH2D_ONE fits in u32 */
  return (u16)((((u32)fr >> 15) * MORPH2D_ONE) / (CV_IN_FR32_MAX >> 15));
}

static u16 adc_to_v14(u16 adc12) {
  u16 a = (u16)(adc12 & 0xfffu);
  if (a >= CV_IN_ADC_MAX) {
    return MIDI_NRPN_V14_MAX;
  }
  return (u16)(((u32)a * (u32)MIDI_NRPN_V14_MAX) / CV_IN_ADC_MAX);
}

static void apply_param(const PlayEncMap *m, u16 adc12) {
  s16 idx;
  ParamValue raw;
  MorphSlot s;
  u8 wrote = 0;
  u8 i;

  if (m->label[0] == '\0') {
    return;
  }
  idx = module_find_param(m->label);
  if (idx < 0) {
    return;
  }
  raw = between_midi_v14_to_raw((u16)idx, adc_to_v14(adc12));

  if (m->kind == ePlayEncParamSlot) {
    s = m->slot;
    if (!g_slots.occupied[s]) {
      return;
    }
    slots_set_value(&g_slots, s, (u16)idx, raw);
    wrote = 1;
  } else if (m->kind == ePlayEncParamAll) {
    for (i = 0; i < MORPH2D_SLOTS; ++i) {
      if (g_slots.occupied[i]) {
        slots_set_value(&g_slots, (MorphSlot)i, (u16)idx, raw);
        wrote = 1;
      }
    }
  }
  if (wrote) {
    state_send_param((u16)idx, raw);
  }
}

/* returns non-zero when the sample drove a mapped target, so the caller
 * knows the on-screen morph point / values are now stale. */
static u8 apply_map(u8 ch, u16 adc12, fract32 fr) {
  const PlayEncMap *m;
  u16 axis;

  if (ch >= PLAY_MAPS_CV_COUNT) {
    return 0;
  }
  m = &g_play_maps.cv[ch];
  switch (m->kind) {
  case ePlayEncNone:
    break;
  case ePlayEncMorphX:
    axis = fr32_to_morph(fr);
    state_set_morph(axis, g_slots.y);
    state_apply();
    return 1;
  case ePlayEncMorphY:
    axis = fr32_to_morph(fr);
    state_set_morph(g_slots.x, axis);
    state_apply();
    return 1;
  case ePlayEncParamSlot:
  case ePlayEncParamAll:
    apply_param(m, adc12);
    return 1;
  default:
    break;
  }
  return 0;
}

void cv_in_init(void) {
  u8 i;
  for (i = 0; i < PLAY_MAPS_CV_COUNT; ++i) {
    g_cv_fr[i] = 0;
    g_cv_applied[i] = 0;
    g_cv_have[i] = 0;
  }
  g_inspect_hold = 0;
  g_adc_running = 0;
}

const fract32 *cv_in_values(void) { return g_cv_fr; }

void cv_in_sync_poll(void) {
  u8 want = (u8)(play_maps_cv_any_bound(&g_play_maps) || g_inspect_hold);
  if (want) {
    if (!g_adc_running) {
      timers_set_adc(CV_IN_POLL_MS);
      g_adc_running = 1;
    }
  } else if (g_adc_running) {
    timers_unset_adc();
    g_adc_running = 0;
  }
}

void cv_in_set_inspect(u8 on) {
  g_inspect_hold = on ? 1 : 0;
  cv_in_sync_poll();
}

/* movement since the last value we acted on, in ADC counts */
static u8 past_deadband(u8 ch, u16 adc12) {
  u16 prev;

  if (!g_cv_have[ch]) {
    return 1;
  }
  prev = g_cv_applied[ch];
  if (adc12 > prev) {
    return (u8)((adc12 - prev) >= CV_IN_DEADBAND);
  }
  return (u8)((prev - adc12) >= CV_IN_DEADBAND);
}

void cv_in_handle_adc(u8 ch, u16 adc12) {
  fract32 fr;
  u8 applied;

  if (ch >= PLAY_MAPS_CV_COUNT) {
    return;
  }
  adc12 = (u16)(adc12 & 0xfffu);
  if (!past_deadband(ch, adc12)) {
    return;
  }
  g_cv_applied[ch] = adc12;
  g_cv_have[ch] = 1;

  fr = cv_in_adc_to_fr32(adc12);
  g_cv_fr[ch] = fr;
  applied = apply_map(ch, adc12, fr);
  if (app_mode_is_inspect()) {
    /* 0–7 px spark height; keep history warm on both inspect subpages */
    inspect_cv_hist_push(ch,
                         (u8)((((u32)fr >> 10) * 7u) / (0x7fffffffu >> 10)));
    render_mark_dirty();
  } else if (applied) {
    /* morph point / play values moved; frame service is rate-capped */
    render_mark_dirty();
  }
}
