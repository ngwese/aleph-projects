#include "midi_between.h"

#include "midi_common.h"
#include "midi_nrpn.h"
#include "module_load.h"
#include "morph2d.h"
#include "pages.h"
#include "param_scaler.h"
#include "play_maps.h"
#include "render.h"
#include "scaler_tables.h"
#include "state.h"

/* MIDI channel 16 (1-based) → ch index 15.
 * CC14 / CC15 → morph x / y (SPEC).
 * channels 1–4 (ch 0–3): NRPN → that slot.
 * channel 16: NRPN → all occupied slots. */

#define MIDI_CH_SETUP 15
#define MIDI_CC_MORPH_X 14
#define MIDI_CC_MORPH_Y 15
#define MIDI_CC_DATA_MSB 6
#define MIDI_CC_DATA_LSB 38
#define MIDI_CC_NRPN_LSB 98
#define MIDI_CC_NRPN_MSB 99

typedef struct {
  u8 nrpn_msb;
  u8 nrpn_lsb;
  u8 data_msb;
  u8 data_lsb;
} NrpnRun;

static NrpnRun nrpn_ch[16];

static u16 cc_to_morph(u8 val) {
  if(val >= 127) {
    return MORPH2D_ONE;
  }
  return (u16)(((u32)val * MORPH2D_ONE) / 127u);
}

static u8 ch_accepts_nrpn(u8 ch) {
  return (u8)(ch <= 3 || ch == MIDI_CH_SETUP);
}

static u8 param_scaler_usable(u16 idx) {
  ParamType t;
  if(idx >= g_module.num_params) {
    return 0;
  }
  t = g_module.desc[idx].type;
  if(t >= eParamNumTypes) {
    return 0;
  }
  return scaler_tables_ok(t);
}

ParamValue between_midi_v14_to_raw(u16 param_idx, u16 v14) {
  const ParamDesc *d;
  ParamScaler *sc;
  io_t io;

  if(!g_module.loaded || param_idx >= g_module.num_params) {
    return 0;
  }
  d = &g_module.desc[param_idx];
  if(param_scaler_usable(param_idx)) {
    sc = &g_scalers[param_idx];
    io = (io_t)midi_nrpn_v14_to_range((s32)sc->inMin, (s32)sc->inMax, v14);
    return (ParamValue)scaler_get_value(sc, io);
  }
  return midi_nrpn_map_v14(d, v14);
}

u16 between_midi_raw_to_v14(u16 param_idx, ParamValue raw) {
  const ParamDesc *d;
  ParamScaler *sc;
  io_t io;

  if(!g_module.loaded || param_idx >= g_module.num_params) {
    return 0;
  }
  d = &g_module.desc[param_idx];
  if(param_scaler_usable(param_idx)) {
    sc = &g_scalers[param_idx];
    io = scaler_get_in(sc, (s32)raw);
    return midi_nrpn_range_to_v14((s32)sc->inMin, (s32)sc->inMax, (s32)io);
  }
  return midi_nrpn_raw_to_v14(d, raw);
}

ParamValue between_midi_cc7_to_raw(u16 param_idx, u8 cc7) {
  u16 v14;
  if(cc7 >= 127) {
    v14 = MIDI_NRPN_V14_MAX;
  } else {
    v14 = (u16)(((u32)cc7 * (u32)MIDI_NRPN_V14_MAX) / 127u);
  }
  return between_midi_v14_to_raw(param_idx, v14);
}

static void apply_play_cc(u8 ch, u8 cc_num, u8 val) {
  const PlayCcMap *m;
  s16 idx;
  ParamValue raw;
  MorphSlot s;
  u8 wrote = 0;
  u8 i;

  if(cc_num < 1 || cc_num > PLAY_MAPS_CC_COUNT) {
    return;
  }
  m = &g_play_maps.cc[cc_num - 1];
  if(m->kind != ePlayCcParam || m->label[0] == '\0') {
    return;
  }
  if(!ch_accepts_nrpn(ch)) {
    return;
  }
  idx = module_find_param(m->label);
  if(idx < 0) {
    return;
  }
  raw = between_midi_cc7_to_raw((u16)idx, val);

  if(ch <= 3) {
    s = (MorphSlot)ch;
    if(!g_slots.occupied[s]) {
      return;
    }
    slots_set_value(&g_slots, s, (u16)idx, raw);
    wrote = 1;
  } else if(ch == MIDI_CH_SETUP) {
    for(i = 0; i < MORPH2D_SLOTS; ++i) {
      if(g_slots.occupied[i]) {
	slots_set_value(&g_slots, (MorphSlot)i, (u16)idx, raw);
	wrote = 1;
      }
    }
  }
  if(!wrote) {
    return;
  }
  state_apply();
  pages_redraw();
  render_update();
}

static void apply_nrpn_data(u8 ch) {
  u16 idx;
  u16 v14;
  ParamValue raw;
  MorphSlot s;
  u8 wrote = 0;

  idx = midi_nrpn_param_index(nrpn_ch[ch].nrpn_msb, nrpn_ch[ch].nrpn_lsb);
  /* null / reset NRPN */
  if(nrpn_ch[ch].nrpn_msb == 127 && nrpn_ch[ch].nrpn_lsb == 127) {
    return;
  }
  if(idx > MIDI_NRPN_PARAM_MAX) {
    return;
  }
  if(!g_module.loaded || idx >= g_module.num_params) {
    return;
  }

  v14 = midi_nrpn_v14(nrpn_ch[ch].data_msb, nrpn_ch[ch].data_lsb);
  raw = between_midi_v14_to_raw(idx, v14);

  if(ch <= 3) {
    s = (MorphSlot)ch;
    if(!g_slots.occupied[s]) {
      return;
    }
    slots_set_value(&g_slots, s, idx, raw);
    wrote = 1;
  } else if(ch == MIDI_CH_SETUP) {
    for(s = 0; s < MORPH2D_SLOTS; ++s) {
      if(g_slots.occupied[s]) {
	slots_set_value(&g_slots, s, idx, raw);
	wrote = 1;
      }
    }
  }

  if(!wrote) {
    return;
  }
  state_apply();
  pages_redraw();
  render_update();
}

static void on_control_change(u8 ch, u8 num, u8 val) {
  u16 x;
  u16 y;

  val = (u8)(val & 0x7f);

  /* NRPN address select (never play-mapped) */
  if(num == MIDI_CC_NRPN_MSB || num == MIDI_CC_NRPN_LSB) {
    if(!ch_accepts_nrpn(ch) || ch >= 16) {
      return;
    }
    if(num == MIDI_CC_NRPN_MSB) {
      nrpn_ch[ch].nrpn_msb = val;
    } else {
      nrpn_ch[ch].nrpn_lsb = val;
    }
    return;
  }

  /* play-mapped CC 1..12 take priority when bound (CC6 overlaps NRPN data
   * entry MSB — unbound CC6 still feeds NRPN below). */
  if(num >= 1 && num <= PLAY_MAPS_CC_COUNT) {
    if(g_play_maps.cc[num - 1].kind == ePlayCcParam) {
      apply_play_cc(ch, num, val);
      return;
    }
  }

  if(num == MIDI_CC_DATA_MSB || num == MIDI_CC_DATA_LSB) {
    if(!ch_accepts_nrpn(ch) || ch >= 16) {
      return;
    }
    if(num == MIDI_CC_DATA_MSB) {
      nrpn_ch[ch].data_msb = val;
    } else {
      nrpn_ch[ch].data_lsb = val;
    }
    apply_nrpn_data(ch);
    return;
  }

  if(ch != MIDI_CH_SETUP) {
    return;
  }

  x = g_slots.x;
  y = g_slots.y;
  if(num == MIDI_CC_MORPH_X) {
    x = cc_to_morph(val);
  } else if(num == MIDI_CC_MORPH_Y) {
    y = cc_to_morph(val);
  } else {
    return;
  }

  slots_set_morph(&g_slots, x, y);
  state_apply();
  pages_redraw();
  render_update();
}

static midi_behavior_t g_midi_beh = {
    .note_on = NULL,
    .note_off = NULL,
    .channel_pressure = NULL,
    .pitch_bend = NULL,
    .control_change = on_control_change,
    .program_change = NULL,
    .clock_tick = NULL,
    .seq_start = NULL,
    .seq_stop = NULL,
    .seq_continue = NULL,
    .panic = NULL,
    .aftertouch = NULL,
};

void between_midi_handle_packet(u32 data) {
  midi_packet_parse(&g_midi_beh, data);
}
