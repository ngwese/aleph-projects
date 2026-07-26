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

static fract32 g_cv_fr[PLAY_MAPS_CV_COUNT];
static u8 g_inspect_hold;
static u8 g_adc_running;

static u16 fr32_to_morph(fract32 fr) {
  if(fr <= 0) {
    return 0;
  }
  if((u32)fr >= CV_IN_FR32_MAX) {
    return MORPH2D_ONE;
  }
  return (u16)(((u32)fr * (u32)MORPH2D_ONE) / CV_IN_FR32_MAX);
}

static u16 adc_to_v14(u16 adc12) {
  u16 a = (u16)(adc12 & 0xfffu);
  if(a >= CV_IN_ADC_MAX) {
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

  if(m->label[0] == '\0') {
    return;
  }
  idx = module_find_param(m->label);
  if(idx < 0) {
    return;
  }
  raw = between_midi_v14_to_raw((u16)idx, adc_to_v14(adc12));

  if(m->kind == ePlayEncParamSlot) {
    s = m->slot;
    if(!g_slots.occupied[s]) {
      return;
    }
    slots_set_value(&g_slots, s, (u16)idx, raw);
    wrote = 1;
  } else if(m->kind == ePlayEncParamAll) {
    for(i = 0; i < MORPH2D_SLOTS; ++i) {
      if(g_slots.occupied[i]) {
	slots_set_value(&g_slots, (MorphSlot)i, (u16)idx, raw);
	wrote = 1;
      }
    }
  }
  if(wrote) {
    state_send_param((u16)idx, raw);
  }
}

static void apply_map(u8 ch, u16 adc12, fract32 fr) {
  const PlayEncMap *m;
  u16 axis;

  if(ch >= PLAY_MAPS_CV_COUNT) {
    return;
  }
  m = &g_play_maps.cv[ch];
  switch(m->kind) {
  case ePlayEncNone:
    break;
  case ePlayEncMorphX:
    axis = fr32_to_morph(fr);
    state_set_morph(axis, g_slots.y);
    state_apply();
    break;
  case ePlayEncMorphY:
    axis = fr32_to_morph(fr);
    state_set_morph(g_slots.x, axis);
    state_apply();
    break;
  case ePlayEncParamSlot:
  case ePlayEncParamAll:
    apply_param(m, adc12);
    break;
  default:
    break;
  }
}

void cv_in_init(void) {
  u8 i;
  for(i = 0; i < PLAY_MAPS_CV_COUNT; ++i) {
    g_cv_fr[i] = 0;
  }
  g_inspect_hold = 0;
  g_adc_running = 0;
}

const fract32 *cv_in_values(void) { return g_cv_fr; }

void cv_in_sync_poll(void) {
  u8 want =
    (u8)(play_maps_cv_any_bound(&g_play_maps) || g_inspect_hold);
  if(want) {
    if(!g_adc_running) {
      timers_set_adc(CV_IN_POLL_MS);
      g_adc_running = 1;
    }
  } else if(g_adc_running) {
    timers_unset_adc();
    g_adc_running = 0;
  }
}

void cv_in_set_inspect(u8 on) {
  g_inspect_hold = on ? 1 : 0;
  cv_in_sync_poll();
}

void cv_in_handle_adc(u8 ch, u16 adc12) {
  fract32 fr;
  if(ch >= PLAY_MAPS_CV_COUNT) {
    return;
  }
  fr = cv_in_adc_to_fr32(adc12);
  g_cv_fr[ch] = fr;
  apply_map(ch, adc12, fr);
  /* adc_poll posts ch 0..3 together; redraw once per frame */
  if(app_mode_is_inspect() && ch == (PLAY_MAPS_CV_COUNT - 1)) {
    pages_redraw();
    render_update();
  }
}
