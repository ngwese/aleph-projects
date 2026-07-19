#include "pages.h"

#include <string.h>

#include "app.h"
#include "events.h"

#include "module_load.h"
#include "morph2d.h"
#include "param_scaler.h"
#include "play_maps.h"
#include "render.h"
#include "scaler_tables.h"
#include "state.h"

#define PLAY_ENC_COL 64

static struct {
  u8 active;
  u8 sw;
  ParamValue saved[MORPH2D_SLOTS];
  u8 occupied[MORPH2D_SLOTS];
} mom;

static void fmt_u(char *dst, u32 v) {
  char tmp[8];
  u8 n = 0;
  u8 i;
  if(v == 0) {
    dst[0] = '0';
    dst[1] = '\0';
    return;
  }
  while(v && n < 7) {
    tmp[n++] = (char)('0' + (v % 10));
    v /= 10;
  }
  for(i = 0; i < n; ++i) {
    dst[i] = tmp[n - 1 - i];
  }
  dst[n] = '\0';
}

static void fmt_s32(char *dst, s32 v) {
  char tmp[12];
  u8 n = 0;
  u8 neg = 0;
  u8 i;
  u32 x;
  if(v < 0) {
    neg = 1;
    x = (u32)(-v);
  } else {
    x = (u32)v;
  }
  if(x == 0) {
    dst[0] = '0';
    dst[1] = '\0';
    return;
  }
  while(x && n < 11) {
    tmp[n++] = (char)('0' + (x % 10));
    x /= 10;
  }
  i = 0;
  if(neg) {
    dst[i++] = '-';
  }
  while(n) {
    dst[i++] = tmp[--n];
  }
  dst[i] = '\0';
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

static void fmt_param_value(char *dst, u16 dst_len, u16 idx, ParamValue raw) {
  ParamScaler *sc;
  io_t io;

  if(dst == NULL || dst_len == 0) {
    return;
  }
  if(param_scaler_usable(idx)) {
    sc = &g_scalers[idx];
    io = scaler_get_in(sc, (s32)raw);
    scaler_get_str(dst, sc, io);
    if(dst_len > FIX_DIG_TOTAL) {
      dst[FIX_DIG_TOTAL] = '\0';
    } else {
      dst[dst_len - 1] = '\0';
    }
  } else {
    fmt_s32(dst, (s32)raw);
  }
}

static void enc_label(char *dst, u32 dst_len, const PlayEncMap *m) {
  if(dst == NULL || dst_len == 0) {
    return;
  }
  dst[0] = '\0';
  if(m == NULL) {
    return;
  }
  switch(m->kind) {
  case ePlayEncNone:
    strncpy(dst, "-", dst_len - 1);
    break;
  case ePlayEncMorphX:
    strncpy(dst, "morph x", dst_len - 1);
    break;
  case ePlayEncMorphY:
    strncpy(dst, "morph y", dst_len - 1);
    break;
  case ePlayEncParamSlot:
  case ePlayEncParamAll:
    strncpy(dst, m->label, dst_len - 1);
    break;
  default:
    strncpy(dst, "?", dst_len - 1);
    break;
  }
  dst[dst_len - 1] = '\0';
}

static void enc_value_str(char *dst, u32 dst_len, const PlayEncMap *m) {
  s16 idx;
  MorphSlot s;
  ParamValue raw = 0;

  if(dst == NULL || dst_len == 0) {
    return;
  }
  dst[0] = '\0';
  if(m == NULL) {
    return;
  }
  switch(m->kind) {
  case ePlayEncNone:
    strncpy(dst, "-", dst_len - 1);
    break;
  case ePlayEncMorphX:
    fmt_u(dst, (g_slots.x * 100u) / MORPH2D_ONE);
    if(strlen(dst) + 1 < dst_len) {
      strcat(dst, "%");
    }
    break;
  case ePlayEncMorphY:
    fmt_u(dst, (g_slots.y * 100u) / MORPH2D_ONE);
    if(strlen(dst) + 1 < dst_len) {
      strcat(dst, "%");
    }
    break;
  case ePlayEncParamSlot:
    idx = module_find_param(m->label);
    if(idx < 0 || !g_slots.occupied[m->slot]) {
      strncpy(dst, "-", dst_len - 1);
    } else {
      fmt_param_value(dst, (u16)dst_len, (u16)idx,
		      slots_get_value(&g_slots, m->slot, (u16)idx));
    }
    break;
  case ePlayEncParamAll:
    idx = module_find_param(m->label);
    if(idx < 0) {
      strncpy(dst, "-", dst_len - 1);
      break;
    }
    for(s = 0; s < MORPH2D_SLOTS; ++s) {
      if(g_slots.occupied[s]) {
	raw = slots_get_value(&g_slots, s, (u16)idx);
	fmt_param_value(dst, (u16)dst_len, (u16)idx, raw);
	break;
      }
    }
    if(dst[0] == '\0') {
      strncpy(dst, "-", dst_len - 1);
    }
    break;
  default:
    strncpy(dst, "-", dst_len - 1);
    break;
  }
  dst[dst_len - 1] = '\0';
}

static void redraw(void) {
  char lab[12];
  char val[FIX_DIG_TOTAL + 1];
  char foot[4][8];
  MorphSlot tri;
  u8 i;

  render_clear();
  render_header(g_setup_name[0] ? g_setup_name : "none", 0);
  render_play_morph(g_slots.x, g_slots.y);

  /* 2×2 encoder grid: left enc0/enc2, right enc1/enc3 */
  enc_label(lab, sizeof(lab), &g_play_maps.enc[0]);
  render_line_at(0, PLAY_ENC_COL, lab);
  enc_value_str(val, sizeof(val), &g_play_maps.enc[0]);
  render_line_at(1, PLAY_ENC_COL, val);

  enc_label(lab, sizeof(lab), &g_play_maps.enc[1]);
  render_line_at(0, (u8)(PLAY_ENC_COL + 32), lab);
  enc_value_str(val, sizeof(val), &g_play_maps.enc[1]);
  render_line_at(1, (u8)(PLAY_ENC_COL + 32), val);

  enc_label(lab, sizeof(lab), &g_play_maps.enc[2]);
  render_line_at(2, PLAY_ENC_COL, lab);
  enc_value_str(val, sizeof(val), &g_play_maps.enc[2]);
  render_line_at(3, PLAY_ENC_COL, val);

  enc_label(lab, sizeof(lab), &g_play_maps.enc[3]);
  render_line_at(2, (u8)(PLAY_ENC_COL + 32), lab);
  enc_value_str(val, sizeof(val), &g_play_maps.enc[3]);
  render_line_at(3, (u8)(PLAY_ENC_COL + 32), val);

  for(i = 0; i < 4; ++i) {
    play_maps_footer_sw(foot[i], sizeof(foot[i]), &g_play_maps.sw[i]);
  }
  render_footer(foot[0], foot[1], foot[2], foot[3]);
  for(i = 0; i < 4; ++i) {
    if(play_maps_sw_single_slot(&g_play_maps.sw[i], &tri)) {
      render_footer_slot_tri(i, tri);
    }
  }
}

void redraw_play(void) { redraw(); }

static void nudge_axis(u16 *axis, s32 data) {
  s32 step = (s32)(MORPH2D_ONE / 128);
  s32 v;
  if(data < 0) {
    step = -step;
  }
  v = (s32)(*axis) + step;
  if(v < 0) {
    v = 0;
  }
  if(v > (s32)MORPH2D_ONE) {
    v = (s32)MORPH2D_ONE;
  }
  *axis = (u16)v;
}

static ParamValue bump_raw(ParamValue v, const ParamDesc *d, s32 inc) {
  if(inc > 0) {
    if(v < d->max - inc) {
      return v + inc;
    }
    return d->max;
  }
  if(v > d->min - inc) {
    return v + inc;
  }
  return d->min;
}

static ParamValue bump_one(u16 idx, ParamValue raw, s32 data) {
  ParamDesc *d = &g_module.desc[idx];
  if(param_scaler_usable(idx)) {
    ParamScaler *sc = &g_scalers[idx];
    io_t io = scaler_get_in(sc, (s32)raw);
    io_t delta = (data > 0) ? (io_t)1 : (io_t)-1;
    return (ParamValue)scaler_inc(sc, &io, delta);
  }
  return bump_raw(raw, d, data > 0 ? 1 : -1);
}

static void apply_enc(u8 i, s32 data) {
  const PlayEncMap *m = &g_play_maps.enc[i];
  s16 idx;
  MorphSlot s;
  ParamValue next;
  u16 x;
  u16 y;

  switch(m->kind) {
  case ePlayEncNone:
    return;
  case ePlayEncMorphX:
    x = g_slots.x;
    nudge_axis(&x, data);
    slots_set_morph(&g_slots, x, g_slots.y);
    state_apply();
    break;
  case ePlayEncMorphY:
    y = g_slots.y;
    nudge_axis(&y, data);
    slots_set_morph(&g_slots, g_slots.x, y);
    state_apply();
    break;
  case ePlayEncParamSlot:
    idx = module_find_param(m->label);
    if(idx < 0 || !g_slots.occupied[m->slot]) {
      return;
    }
    next = bump_one((u16)idx, slots_get_value(&g_slots, m->slot, (u16)idx),
		    data);
    slots_set_value(&g_slots, m->slot, (u16)idx, next);
    state_apply();
    break;
  case ePlayEncParamAll:
    idx = module_find_param(m->label);
    if(idx < 0) {
      return;
    }
    next = 0;
    {
      u8 have = 0;
      for(s = 0; s < MORPH2D_SLOTS; ++s) {
	if(g_slots.occupied[s]) {
	  if(!have) {
	    next = bump_one((u16)idx,
			    slots_get_value(&g_slots, s, (u16)idx), data);
	    have = 1;
	  }
	  slots_set_value(&g_slots, s, (u16)idx, next);
	}
      }
      if(!have) {
	return;
      }
    }
    state_apply();
    break;
  default:
    return;
  }
  redraw();
  render_update();
}

static void write_param_slots(MorphSlot slot, u8 all, const char *label,
			      ParamValue v) {
  s16 idx = module_find_param(label);
  MorphSlot s;
  if(idx < 0) {
    return;
  }
  if(all) {
    for(s = 0; s < MORPH2D_SLOTS; ++s) {
      if(g_slots.occupied[s]) {
	slots_set_value(&g_slots, s, (u16)idx, v);
      }
    }
  } else if(g_slots.occupied[slot]) {
    slots_set_value(&g_slots, slot, (u16)idx, v);
  }
  state_apply();
}

static void mom_press(u8 sw, MorphSlot slot, u8 all, const char *label,
		      ParamValue v) {
  s16 idx = module_find_param(label);
  MorphSlot s;
  if(idx < 0) {
    return;
  }
  mom.active = 1;
  mom.sw = sw;
  memset(mom.saved, 0, sizeof(mom.saved));
  memset(mom.occupied, 0, sizeof(mom.occupied));
  if(all) {
    for(s = 0; s < MORPH2D_SLOTS; ++s) {
      if(g_slots.occupied[s]) {
	mom.occupied[s] = 1;
	mom.saved[s] = slots_get_value(&g_slots, s, (u16)idx);
	slots_set_value(&g_slots, s, (u16)idx, v);
      }
    }
  } else if(g_slots.occupied[slot]) {
    mom.occupied[slot] = 1;
    mom.saved[slot] = slots_get_value(&g_slots, slot, (u16)idx);
    slots_set_value(&g_slots, slot, (u16)idx, v);
  }
  state_apply();
}

static void mom_release(u8 sw) {
  const PlaySwMap *m;
  s16 idx;
  MorphSlot s;

  if(!mom.active || mom.sw != sw) {
    return;
  }
  m = &g_play_maps.sw[sw];
  idx = module_find_param(m->label);
  mom.active = 0;
  if(idx < 0) {
    return;
  }
  for(s = 0; s < MORPH2D_SLOTS; ++s) {
    if(mom.occupied[s]) {
      slots_set_value(&g_slots, s, (u16)idx, mom.saved[s]);
    }
  }
  state_apply();
}

static void apply_sw(u8 i, s32 data) {
  const PlaySwMap *m = &g_play_maps.sw[i];
  MorphSlot snap;

  if(data > 0) {
    if(play_maps_sw_snap_slot(m->kind, &snap)) {
      slots_snap_to(&g_slots, snap);
      state_apply();
      redraw();
      render_update();
      return;
    }
    switch(m->kind) {
    case ePlaySwSetSlot:
      write_param_slots(m->slot, 0, m->label, m->value);
      redraw();
      render_update();
      break;
    case ePlaySwSetAll:
      write_param_slots(0, 1, m->label, m->value);
      redraw();
      render_update();
      break;
    case ePlaySwMomSlot:
      mom_press(i, m->slot, 0, m->label, m->value);
      redraw();
      render_update();
      break;
    case ePlaySwMomAll:
      mom_press(i, 0, 1, m->label, m->value);
      redraw();
      render_update();
      break;
    default:
      break;
    }
  } else if(data <= 0) {
    if(m->kind == ePlaySwMomSlot || m->kind == ePlaySwMomAll) {
      mom_release(i);
      redraw();
      render_update();
    }
  }
}

static void handle_enc0(s32 data) { apply_enc(0, data); }
static void handle_enc1(s32 data) { apply_enc(1, data); }
static void handle_enc2(s32 data) { apply_enc(2, data); }
static void handle_enc3(s32 data) { apply_enc(3, data); }
static void handle_sw0(s32 data) { apply_sw(0, data); }
static void handle_sw1(s32 data) { apply_sw(1, data); }
static void handle_sw2(s32 data) { apply_sw(2, data); }
static void handle_sw3(s32 data) { apply_sw(3, data); }

void select_play(void) {
  mom.active = 0;
  app_event_handlers[kEventEncoder0] = handle_enc0;
  app_event_handlers[kEventEncoder1] = handle_enc1;
  app_event_handlers[kEventEncoder2] = handle_enc2;
  app_event_handlers[kEventEncoder3] = handle_enc3;
  app_event_handlers[kEventSwitch0] = handle_sw0;
  app_event_handlers[kEventSwitch1] = handle_sw1;
  app_event_handlers[kEventSwitch2] = handle_sw2;
  app_event_handlers[kEventSwitch3] = handle_sw3;
  redraw();
  render_update();
}

void page_play_init(void) { mom.active = 0; }
