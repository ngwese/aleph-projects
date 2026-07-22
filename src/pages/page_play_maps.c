#include "pages.h"

#include <string.h>

#include "app.h"
#include "events.h"
#include "fix.h"

#include "font.h"
#include "input_roles.h"
#include "module_load.h"
#include "param_scaler.h"
#include "play_maps.h"
#include "render.h"
#include "scaler_tables.h"
#include "state.h"

/* field focus for the selected control */
enum {
  eFieldKind = 0,
  eFieldSlot,
  eFieldParam,
  eFieldValue
};

/* 0..3 = enc, 4..7 = sw, 8..9 = fs, 10..21 = cc1..cc12 */
#define PLAY_MAPS_CTRL_COUNT                                                     \
  (PLAY_MAPS_ENC_COUNT + PLAY_MAPS_SW_COUNT + PLAY_MAPS_FS_COUNT +               \
   PLAY_MAPS_CC_COUNT)
#define PLAY_MAPS_CC_BASE                                                        \
  (PLAY_MAPS_ENC_COUNT + PLAY_MAPS_SW_COUNT + PLAY_MAPS_FS_COUNT)

static s16 sel;
static u8 field;

/* value column matches slot page (horizontal midpoint). */
#define PLAY_MAPS_VAL_X 64

static u8 is_enc(void) { return sel < PLAY_MAPS_ENC_COUNT; }
static u8 is_panel_sw(void) {
  return sel >= PLAY_MAPS_ENC_COUNT &&
	 sel < (PLAY_MAPS_ENC_COUNT + PLAY_MAPS_SW_COUNT);
}
static u8 is_fs(void) {
  return sel >= (PLAY_MAPS_ENC_COUNT + PLAY_MAPS_SW_COUNT) &&
	 sel < PLAY_MAPS_CC_BASE;
}
static u8 is_sw_like(void) { return is_panel_sw() || is_fs(); }
static u8 is_cc(void) { return sel >= PLAY_MAPS_CC_BASE; }

static PlaySwMap *cur_sw_map(void) {
  if(is_enc() || is_cc()) {
    return NULL;
  }
  if(is_panel_sw()) {
    return &g_play_maps.sw[sel - PLAY_MAPS_ENC_COUNT];
  }
  return &g_play_maps.fs[sel - PLAY_MAPS_ENC_COUNT - PLAY_MAPS_SW_COUNT];
}

static PlayCcMap *cur_cc_map(void) {
  if(!is_cc()) {
    return NULL;
  }
  return &g_play_maps.cc[sel - PLAY_MAPS_CC_BASE];
}

static u8 enc_i(void) { return (u8)sel; }
static u8 cc_i(void) { return (u8)(sel - PLAY_MAPS_CC_BASE); }

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

static s16 find_param(const char *label) {
  return module_find_param(label);
}

static void ensure_param_label(char *label) {
  if(!g_module.loaded || g_module.num_params == 0) {
    label[0] = '\0';
    return;
  }
  if(label[0] == '\0' || find_param(label) < 0) {
    strncpy(label, g_module.desc[0].label, PARAM_LABEL_LEN - 1);
    label[PARAM_LABEL_LEN - 1] = '\0';
  }
}

static u8 sw_has_value_kind(PlaySwKind k) {
  return k == ePlaySwSetSlot || k == ePlaySwMomSlot || k == ePlaySwSetAll ||
	 k == ePlaySwMomAll;
}

static u8 sw_has_value(const PlaySwMap *m) {
  return sw_has_value_kind(m->kind);
}

static void seed_sw_value(PlaySwMap *m) {
  s16 idx;
  ensure_param_label(m->label);
  idx = find_param(m->label);
  if(idx < 0 || !g_module.loaded) {
    m->value = 0;
    return;
  }
  m->value = g_module.defaults[idx];
}

static u8 field_ok(u8 f) {
  if(f == eFieldKind) {
    return 1;
  }
  if(is_cc()) {
    PlayCcMap *m = cur_cc_map();
    if(f == eFieldParam) {
      return m != NULL && m->kind == ePlayCcParam;
    }
    return 0;
  }
  if(is_sw_like()) {
    PlaySwMap *m = cur_sw_map();
    PlaySwKind k = m->kind;
    switch(f) {
    case eFieldSlot:
      return k == ePlaySwSetSlot || k == ePlaySwMomSlot;
    case eFieldParam:
      return sw_has_value(m);
    case eFieldValue:
      return sw_has_value(m);
    default:
      return 0;
    }
  } else {
    PlayEncKind k = g_play_maps.enc[enc_i()].kind;
    switch(f) {
    case eFieldSlot:
      return k == ePlayEncParamSlot;
    case eFieldParam:
      return k == ePlayEncParamSlot || k == ePlayEncParamAll;
    default:
      return 0;
    }
  }
}

static void clamp_field(void) {
  if(!field_ok(field)) {
    field = eFieldKind;
  }
}

static const char *field_tag(void) {
  switch(field) {
  case eFieldSlot:
    return "slot";
  case eFieldParam:
    return "param";
  case eFieldValue:
    return "value";
  default:
    return "kind";
  }
}

static void fmt_s32(char *dst, s32 v) {
  char tmp[12];
  u8 n = 0;
  u8 i;
  u8 neg = 0;
  u32 x;

  if(v < 0) {
    neg = 1;
    x = (u32)(-(v + 1)) + 1;
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
    /* print_fix16 writes FIX_DIG_TOTAL chars and does not NUL-terminate. */
    if(dst_len > FIX_DIG_TOTAL) {
      dst[FIX_DIG_TOTAL] = '\0';
    } else {
      dst[dst_len - 1] = '\0';
    }
  } else {
    fmt_s32(dst, (s32)raw);
  }
}

static void redraw(void) {
  char line[24];
  char sum[20];
  char num[FIX_DIG_TOTAL + 1];
  u16 i;
  u16 start;
  u8 idx;
  u8 show_value;
  PlaySwMap *swm;
  u8 status_y;
  u8 edit_x;
  const char *tag;

  clamp_field();
  swm = cur_sw_map();
  show_value = (swm != NULL) && sw_has_value(swm);

  render_clear();
  render_header("play", 0);

  /* always four list rows — status line shares row 4 with optional value */
  start = (sel > 3) ? (u16)(sel - 3) : 0;
  for(i = 0; i < 4; ++i) {
    idx = (u8)(start + i);
    if(idx >= PLAY_MAPS_CTRL_COUNT) {
      break;
    }
    line[0] = (idx == (u8)sel) ? '>' : ' ';
    line[1] = ' ';
    line[2] = '\0';
    if(idx < PLAY_MAPS_ENC_COUNT) {
      strcat(line, "enc");
      {
	char n[2] = {(char)('0' + idx), '\0'};
	strcat(line, n);
      }
      strcat(line, ": ");
      play_maps_summary_enc(sum, sizeof(sum), &g_play_maps.enc[idx]);
    } else if(idx < PLAY_MAPS_ENC_COUNT + PLAY_MAPS_SW_COUNT) {
      strcat(line, "sw");
      {
	char n[2] = {(char)('0' + (idx - PLAY_MAPS_ENC_COUNT)), '\0'};
	strcat(line, n);
      }
      strcat(line, ": ");
      play_maps_summary_sw(sum, sizeof(sum),
			   &g_play_maps.sw[idx - PLAY_MAPS_ENC_COUNT]);
    } else if(idx < PLAY_MAPS_CC_BASE) {
      strcat(line, "fs");
      {
	char n[2] = {
	  (char)('0' + (idx - PLAY_MAPS_ENC_COUNT - PLAY_MAPS_SW_COUNT)),
	  '\0'};
	strcat(line, n);
      }
      strcat(line, ": ");
      play_maps_summary_sw(
	sum, sizeof(sum),
	&g_play_maps.fs[idx - PLAY_MAPS_ENC_COUNT - PLAY_MAPS_SW_COUNT]);
    } else {
      u8 cc_n = (u8)(idx - PLAY_MAPS_CC_BASE + 1);
      strcat(line, "cc");
      if(cc_n >= 10) {
	char n[3] = {(char)('0' + (cc_n / 10)), (char)('0' + (cc_n % 10)),
		     '\0'};
	strcat(line, n);
      } else {
	char n[2] = {(char)('0' + cc_n), '\0'};
	strcat(line, n);
      }
      strcat(line, ": ");
      play_maps_summary_cc(sum, sizeof(sum),
			   &g_play_maps.cc[idx - PLAY_MAPS_CC_BASE]);
    }
    strncat(line, sum, sizeof(line) - strlen(line) - 1);
    render_line((u8)i, line);
  }

  status_y = (u8)(4 * 8);
  tag = field_tag();
  render_string_xy(0, status_y, "edit ", RENDER_PLAY_GREY_DARK);
  edit_x = (u8)font_string_pixels("edit ");
  render_string_xy(edit_x, status_y, tag, RENDER_PLAY_GREY_LIGHT);

  if(show_value) {
    u8 val_lab_w;
    u8 val_lab_x;
    s16 pidx = find_param(swm->label);
    val_lab_w = (u8)font_string_pixels("value ");
    val_lab_x = (PLAY_MAPS_VAL_X > val_lab_w)
		  ? (u8)(PLAY_MAPS_VAL_X - val_lab_w)
		  : 0;
    /* keep value block readable if edit text runs long */
    render_fill_rect(val_lab_x, status_y, (u8)(128 - val_lab_x), 8, 0);
    render_string_xy(val_lab_x, status_y, "value ", RENDER_PLAY_GREY_DARK);
    if(pidx >= 0) {
      fmt_param_value(num, sizeof(num), (u16)pidx, swm->value);
    } else {
      fmt_s32(num, (s32)swm->value);
    }
    render_string_xy(PLAY_MAPS_VAL_X, status_y, num, RENDER_PLAY_GREY_LIGHT);
  }

  if(g_alt_mode) {
    render_footer("reset", "rst all", "-", "alt");
  } else {
    render_footer("slot", "param", "value", "alt");
  }
}

void redraw_play_maps(void) { redraw(); }

static void cycle_enc_kind(PlayEncMap *m, s8 dir) {
  static const PlayEncKind seq_no_mod[] = {
    ePlayEncNone, ePlayEncMorphX, ePlayEncMorphY};
  static const PlayEncKind seq_mod[] = {
    ePlayEncNone, ePlayEncMorphX, ePlayEncMorphY, ePlayEncParamSlot,
    ePlayEncParamAll};
  const PlayEncKind *seq;
  u8 n;
  u8 i;
  u8 cur = 0;

  if(g_module.loaded && g_module.num_params > 0) {
    seq = seq_mod;
    n = 5;
  } else {
    seq = seq_no_mod;
    n = 3;
  }
  for(i = 0; i < n; ++i) {
    if(seq[i] == m->kind) {
      cur = i;
      break;
    }
  }
  if(dir > 0) {
    cur = (u8)((cur + 1) % n);
  } else {
    cur = (u8)((cur + n - 1) % n);
  }
  m->kind = seq[cur];
  if(m->kind == ePlayEncParamSlot || m->kind == ePlayEncParamAll) {
    ensure_param_label(m->label);
  }
}

static void cycle_sw_kind(PlaySwMap *m, s8 dir) {
  static const PlaySwKind seq_no_mod[] = {
    ePlaySwNone, ePlaySwSnapA, ePlaySwSnapB, ePlaySwSnapC, ePlaySwSnapD};
  static const PlaySwKind seq_mod[] = {
    ePlaySwNone,   ePlaySwSnapA,   ePlaySwSnapB,  ePlaySwSnapC,
    ePlaySwSnapD,  ePlaySwSetSlot, ePlaySwMomSlot, ePlaySwSetAll,
    ePlaySwMomAll};
  const PlaySwKind *seq;
  u8 n;
  u8 i;
  u8 cur = 0;
  PlaySwKind prev = m->kind;

  if(g_module.loaded && g_module.num_params > 0) {
    seq = seq_mod;
    n = 9;
  } else {
    seq = seq_no_mod;
    n = 5;
  }
  for(i = 0; i < n; ++i) {
    if(seq[i] == m->kind) {
      cur = i;
      break;
    }
  }
  if(dir > 0) {
    cur = (u8)((cur + 1) % n);
  } else {
    cur = (u8)((cur + n - 1) % n);
  }
  m->kind = seq[cur];
  if(sw_has_value(m)) {
    ensure_param_label(m->label);
    /* seed stored binding value when entering a set/mom kind */
    if(!sw_has_value_kind(prev)) {
      seed_sw_value(m);
    }
  }
}

static void cycle_cc_kind(PlayCcMap *m, s8 dir) {
  PlayCcKind next;
  if(m == NULL) {
    return;
  }
  if(!g_module.loaded || g_module.num_params == 0) {
    m->kind = ePlayCcNone;
    m->label[0] = '\0';
    return;
  }
  if(dir > 0) {
    next = (m->kind == ePlayCcNone) ? ePlayCcParam : ePlayCcNone;
  } else {
    next = (m->kind == ePlayCcParam) ? ePlayCcNone : ePlayCcParam;
  }
  m->kind = next;
  if(m->kind == ePlayCcParam) {
    ensure_param_label(m->label);
  } else {
    m->label[0] = '\0';
  }
}

static void cycle_param_label(char *label, s8 dir, u8 coarse) {
  s16 idx;
  s16 step;
  if(!g_module.loaded || g_module.num_params == 0) {
    return;
  }
  step = coarse ? (s16)8 : (s16)1;
  if(dir < 0) {
    step = (s16)-step;
  }
  idx = find_param(label);
  if(idx < 0) {
    idx = 0;
  } else {
    idx = (s16)(idx + step);
    while(idx < 0) {
      idx = (s16)(idx + (s16)g_module.num_params);
    }
    while(idx >= (s16)g_module.num_params) {
      idx = (s16)(idx - (s16)g_module.num_params);
    }
  }
  strncpy(label, g_module.desc[idx].label, PARAM_LABEL_LEN - 1);
  label[PARAM_LABEL_LEN - 1] = '\0';
}

static void bump_sw_value(PlaySwMap *m, s8 dir, u8 coarse) {
  s16 idx = find_param(m->label);
  io_t delta;

  if(idx < 0) {
    return;
  }
  if(param_scaler_usable((u16)idx)) {
    ParamScaler *sc = &g_scalers[idx];
    io_t io = scaler_get_in(sc, (s32)m->value);
    if(coarse) {
      delta = (dir > 0) ? (io_t)0x100 : (io_t)-0x100;
    } else {
      delta = (dir > 0) ? (io_t)1 : (io_t)-1;
    }
    m->value = (ParamValue)scaler_inc(sc, &io, delta);
  } else {
    ParamDesc *d = &g_module.desc[idx];
    s32 step = coarse ? 64 : 1;
    s32 v = (s32)m->value + (dir > 0 ? step : -step);
    if(v < d->min) {
      v = d->min;
    }
    if(v > d->max) {
      v = d->max;
    }
    m->value = (ParamValue)v;
  }
}

static void adjust_field(s8 dir, u8 coarse) {
  if(is_cc()) {
    PlayCcMap *m = cur_cc_map();
    switch(field) {
    case eFieldKind:
      cycle_cc_kind(m, dir);
      break;
    case eFieldParam:
      cycle_param_label(m->label, dir, coarse);
      break;
    default:
      break;
    }
  } else if(is_sw_like()) {
    PlaySwMap *m = cur_sw_map();
    switch(field) {
    case eFieldKind:
      cycle_sw_kind(m, dir);
      break;
    case eFieldSlot:
      m->slot = (MorphSlot)(((u8)m->slot + (dir > 0 ? 1 : 3)) % 4);
      break;
    case eFieldParam:
      cycle_param_label(m->label, dir, coarse);
      break;
    case eFieldValue:
      bump_sw_value(m, dir, coarse);
      break;
    default:
      break;
    }
  } else {
    PlayEncMap *m = &g_play_maps.enc[enc_i()];
    switch(field) {
    case eFieldKind:
      cycle_enc_kind(m, dir);
      break;
    case eFieldSlot:
      m->slot = (MorphSlot)(((u8)m->slot + (dir > 0 ? 1 : 3)) % 4);
      break;
    case eFieldParam:
      cycle_param_label(m->label, dir, coarse);
      break;
    default:
      break;
    }
  }
  clamp_field();
  state_exclude_rebuild();
}

static void handle_enc0(s32 data) {
  sel += (data > 0) ? 1 : -1;
  if(sel < 0) {
    sel = 0;
  }
  if(sel > (s16)(PLAY_MAPS_CTRL_COUNT - 1)) {
    sel = (s16)(PLAY_MAPS_CTRL_COUNT - 1);
  }
  field = eFieldKind;
  redraw();
  render_update();
}

static void handle_enc2(s32 data) {
  adjust_field(data > 0 ? 1 : -1, 0);
  redraw();
  render_update();
}

static void handle_enc3(s32 data) {
  /* bees-like coarse step (same as slot page) */
  adjust_field(data > 0 ? 1 : -1, 1);
  redraw();
  render_update();
}

static void select_field_btn(u8 f) {
  if(!field_ok(f)) {
    return;
  }
  field = f;
  redraw();
  render_update();
}

static void handle_sw0(s32 data) {
  if(data <= 0) {
    return;
  }
  if(g_alt_mode) {
    if(is_enc()) {
      play_maps_reset_enc(&g_play_maps, enc_i());
    } else if(is_panel_sw()) {
      play_maps_reset_sw(&g_play_maps, (u8)(sel - PLAY_MAPS_ENC_COUNT));
    } else if(is_fs()) {
      play_maps_reset_fs(
	&g_play_maps,
	(u8)(sel - PLAY_MAPS_ENC_COUNT - PLAY_MAPS_SW_COUNT));
    } else {
      play_maps_reset_cc(&g_play_maps, cc_i());
    }
    state_exclude_rebuild();
    field = eFieldKind;
    redraw();
    render_update();
    return;
  }
  select_field_btn(eFieldSlot);
}

static void handle_sw1(s32 data) {
  if(data <= 0) {
    return;
  }
  if(g_alt_mode) {
    play_maps_set_defaults(&g_play_maps);
    state_exclude_rebuild();
    field = eFieldKind;
    redraw();
    render_update();
    return;
  }
  select_field_btn(eFieldParam);
}

static void handle_sw2(s32 data) {
  if(data <= 0) {
    return;
  }
  if(g_alt_mode) {
    return;
  }
  select_field_btn(eFieldValue);
}

static void handle_sw3(s32 data) {
  g_alt_mode = data > 0 ? 1 : 0;
  redraw();
  render_update();
}

void select_play_maps(void) {
  static const InputEncBinding enc[4] = {
    {eInputRoleListSelect, handle_enc0},
    {eInputRolePageSelect, NULL},
    {eInputRoleParamFine, handle_enc2},
    {eInputRoleParamCoarse, handle_enc3},
  };
  input_roles_bind(enc);
  app_event_handlers[kEventSwitch0] = handle_sw0;
  app_event_handlers[kEventSwitch1] = handle_sw1;
  app_event_handlers[kEventSwitch2] = handle_sw2;
  app_event_handlers[kEventSwitch3] = handle_sw3;
  redraw();
  render_update();
}

void page_play_maps_init(void) {
  sel = 0;
  field = eFieldKind;
}
