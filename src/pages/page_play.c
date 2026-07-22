#include "pages.h"

#include <string.h>

#include "app.h"
#include "events.h"

#include "input_roles.h"
#include "module_load.h"
#include "morph2d.h"
#include "param_scaler.h"
#include "play_maps.h"
#include "render.h"
#include "scaler_tables.h"
#include "state.h"

#define PLAY_ENC_MARGIN 8
#define PLAY_ENC_ROW_GAP 3
#define PLAY_ENC_LINE_H 8
#define PLAY_CONTENT_H 40
#define PLAY_CONTENT_W 128

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
    strncpy(dst, "morph.x", dst_len - 1);
    break;
  case ePlayEncMorphY:
    strncpy(dst, "morph.y", dst_len - 1);
    break;
  case ePlayEncParamSlot:
    /* e.g. "a/amp" so slot changes are visible on play */
    if(dst_len < 4) {
      strncpy(dst, "?", dst_len - 1);
    } else {
      dst[0] = (char)('a' + (u8)m->slot);
      dst[1] = '/';
      dst[2] = '\0';
      strncat(dst, m->label, dst_len - 3);
    }
    break;
  case ePlayEncParamAll:
    if(dst_len < 5) {
      strncpy(dst, "?", dst_len - 1);
    } else {
      strcpy(dst, "*/");
      strncat(dst, m->label, dst_len - 3);
    }
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
  u8 area_l;
  u8 area_r;
  u8 area_w;
  u8 col_w;
  u8 col0_x;
  u8 col1_x;
  u8 block_h;
  u8 y0;
  u8 y_lab_top;
  u8 y_val_top;
  u8 y_lab_bot;
  u8 y_val_bot;

  render_clear();
  render_header(g_setup_name[0] ? g_setup_name : "none", state_setup_dirty());
  render_play_morph(g_slots.x, g_slots.y);

  /* 2×2 encoder grid between morph and right edge, with side margins */
  area_l = (u8)(RENDER_PLAY_MORPH_OX + RENDER_PLAY_MORPH_SZ + PLAY_ENC_MARGIN);
  area_r = (u8)(PLAY_CONTENT_W - PLAY_ENC_MARGIN);
  area_w = (u8)(area_r - area_l);
  col_w = (u8)(area_w / 2);
  col0_x = area_l;
  col1_x = (u8)(area_l + col_w);

  block_h = (u8)(PLAY_ENC_LINE_H * 4 + PLAY_ENC_ROW_GAP);
  y0 = (u8)((PLAY_CONTENT_H - block_h) / 2);
  y_lab_top = y0;
  y_val_top = (u8)(y0 + PLAY_ENC_LINE_H);
  y_lab_bot = (u8)(y_val_top + PLAY_ENC_LINE_H + PLAY_ENC_ROW_GAP);
  y_val_bot = (u8)(y_lab_bot + PLAY_ENC_LINE_H);

  /* clear each column band so shorter labels/values do not ghost */
  render_fill_rect(col0_x, y_lab_top, col_w, block_h, 0);
  render_fill_rect(col1_x, y_lab_top, (u8)(area_r - col1_x), block_h, 0);

  enc_label(lab, sizeof(lab), &g_play_maps.enc[0]);
  render_string_xy(col0_x, y_lab_top, lab, RENDER_PLAY_GREY);
  enc_value_str(val, sizeof(val), &g_play_maps.enc[0]);
  render_string_xy(col0_x, y_val_top, val, 0xf);

  enc_label(lab, sizeof(lab), &g_play_maps.enc[1]);
  render_string_xy(col1_x, y_lab_top, lab, RENDER_PLAY_GREY);
  enc_value_str(val, sizeof(val), &g_play_maps.enc[1]);
  render_string_xy(col1_x, y_val_top, val, 0xf);

  enc_label(lab, sizeof(lab), &g_play_maps.enc[2]);
  render_string_xy(col0_x, y_lab_bot, lab, RENDER_PLAY_GREY);
  enc_value_str(val, sizeof(val), &g_play_maps.enc[2]);
  render_string_xy(col0_x, y_val_bot, val, 0xf);

  enc_label(lab, sizeof(lab), &g_play_maps.enc[3]);
  render_string_xy(col1_x, y_lab_bot, lab, RENDER_PLAY_GREY);
  enc_value_str(val, sizeof(val), &g_play_maps.enc[3]);
  render_string_xy(col1_x, y_val_bot, val, 0xf);

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
  /* enc timer posts accumulated ticks; scale step by |data| so fast
   * turns are not collapsed to a single detent. */
  s32 step = (s32)(MORPH2D_ONE / 128) * data;
  s32 v;
  if(data == 0) {
    return;
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
  s32 delta32;

  if(data == 0) {
    return raw;
  }
  delta32 = (s32)0x100 * data;
  if(delta32 > 32767) {
    delta32 = 32767;
  } else if(delta32 < -32768) {
    delta32 = -32768;
  }
  if(param_scaler_usable(idx)) {
    ParamScaler *sc = &g_scalers[idx];
    io_t io = scaler_get_in(sc, (s32)raw);
    /* play has one encoder per binding (no separate fine/coarse). use
     * slot-page coarse step (±0x100) per accumulated tick. */
    return (ParamValue)scaler_inc(sc, &io, (io_t)delta32);
  }
  return bump_raw(raw, d, delta32);
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
    state_set_morph(x, g_slots.y);
    state_apply();
    break;
  case ePlayEncMorphY:
    y = g_slots.y;
    nudge_axis(&y, data);
    state_set_morph(g_slots.x, y);
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
    state_send_param((u16)idx, next);
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
    state_send_param((u16)idx, next);
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
  state_send_param((u16)idx, v);
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
  state_send_param((u16)idx, v);
}

static void mom_release(u8 sw) {
  const PlaySwMap *m;
  s16 idx;
  MorphSlot s;
  ParamValue send = 0;
  u8 have = 0;

  if(!mom.active || mom.sw != sw) {
    return;
  }
  m = play_maps_sw_total_at_const(&g_play_maps, sw);
  idx = (m != NULL) ? module_find_param(m->label) : -1;
  mom.active = 0;
  if(idx < 0) {
    return;
  }
  for(s = 0; s < MORPH2D_SLOTS; ++s) {
    if(mom.occupied[s]) {
      slots_set_value(&g_slots, s, (u16)idx, mom.saved[s]);
      if(!have) {
	send = mom.saved[s];
	have = 1;
      }
    }
  }
  if(have) {
    state_send_param((u16)idx, send);
  }
}

static void apply_sw(u8 i, s32 data) {
  const PlaySwMap *m = play_maps_sw_total_at_const(&g_play_maps, i);
  MorphSlot snap;

  if(m == NULL) {
    return;
  }

  if(data > 0) {
    if(play_maps_sw_snap_slot(m->kind, &snap)) {
      state_snap_to(snap);
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
static void handle_fs0(s32 data) { apply_sw(4, data); }
static void handle_fs1(s32 data) { apply_sw(5, data); }

void select_play(void) {
  /* ParamFine → thresh 0 and forward to play map handlers (same install
   * path as edit pages, so prior Unmapped/PageSelect wrappers cannot stick). */
  static const InputEncBinding enc[4] = {
    {eInputRoleParamFine, handle_enc0},
    {eInputRoleParamFine, handle_enc1},
    {eInputRoleParamFine, handle_enc2},
    {eInputRoleParamFine, handle_enc3},
  };
  static const InputSwBinding sw[4] = {
    {eInputSwRoleAction, handle_sw0},
    {eInputSwRoleAction, handle_sw1},
    {eInputSwRoleAction, handle_sw2},
    {eInputSwRoleAction, handle_sw3},
  };
  mom.active = 0;
  input_roles_bind(enc, sw);
  app_event_handlers[kEventSwitch6] = handle_fs0;
  app_event_handlers[kEventSwitch7] = handle_fs1;
  redraw();
  render_update();
}

void page_play_init(void) { mom.active = 0; }
