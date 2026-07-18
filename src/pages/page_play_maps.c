#include "pages.h"

#include <string.h>

#include "app.h"
#include "events.h"

#include "module_load.h"
#include "param_scaler.h"
#include "play_maps.h"
#include "render.h"
#include "scaler_tables.h"
#include "state.h"

/* 0..3 = enc, 4..7 = sw */
static s16 sel;
static u8 field; /* 0=kind, 1=slot, 2=param, 3=value (as applicable) */

static u8 is_sw(void) { return sel >= 4; }
static u8 ctrl_i(void) { return (u8)(is_sw() ? (sel - 4) : sel); }

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

static u8 enc_field_max(const PlayEncMap *m) {
  switch(m->kind) {
  case ePlayEncParamSlot:
    return 2; /* kind, slot, param */
  case ePlayEncParamAll:
    return 1; /* kind, param */
  default:
    return 0;
  }
}

static u8 sw_field_max(const PlaySwMap *m) {
  switch(m->kind) {
  case ePlaySwSetSlot:
  case ePlaySwMomSlot:
    return 3; /* kind, slot, param, value */
  case ePlaySwSetAll:
  case ePlaySwMomAll:
    return 2; /* kind, param, value */
  default:
    return 0;
  }
}

static void clamp_field(void) {
  u8 maxf;
  if(is_sw()) {
    maxf = sw_field_max(&g_play_maps.sw[ctrl_i()]);
  } else {
    maxf = enc_field_max(&g_play_maps.enc[ctrl_i()]);
  }
  if(field > maxf) {
    field = maxf;
  }
}

static const char *field_tag(void) {
  if(is_sw()) {
    switch(field) {
    case 0:
      return "kind";
    case 1:
      if(g_play_maps.sw[ctrl_i()].kind == ePlaySwSetAll ||
	 g_play_maps.sw[ctrl_i()].kind == ePlaySwMomAll) {
	return "param";
      }
      return "slot";
    case 2:
      if(g_play_maps.sw[ctrl_i()].kind == ePlaySwSetAll ||
	 g_play_maps.sw[ctrl_i()].kind == ePlaySwMomAll) {
	return "val";
      }
      return "param";
    default:
      return "val";
    }
  }
  switch(field) {
  case 0:
    return "kind";
  case 1:
    return (g_play_maps.enc[ctrl_i()].kind == ePlayEncParamAll) ? "param"
							       : "slot";
  default:
    return "param";
  }
}

static void redraw(void) {
  char line[24];
  char sum[20];
  u16 i;
  u16 start;
  u8 idx;

  clamp_field();
  render_clear();
  render_header("play", 0);

  start = (sel > 3) ? (u16)(sel - 3) : 0;
  for(i = 0; i < 4; ++i) {
    idx = (u8)(start + i);
    if(idx >= 8) {
      break;
    }
    line[0] = (idx == (u8)sel) ? '>' : ' ';
    line[1] = ' ';
    line[2] = '\0';
    if(idx < 4) {
      strcat(line, "enc");
      {
	char n[2] = {(char)('0' + idx), '\0'};
	strcat(line, n);
      }
      strcat(line, ":");
      play_maps_summary_enc(sum, sizeof(sum), &g_play_maps.enc[idx]);
    } else {
      strcat(line, "sw");
      {
	char n[2] = {(char)('0' + (idx - 4)), '\0'};
	strcat(line, n);
      }
      strcat(line, ":");
      play_maps_summary_sw(sum, sizeof(sum), &g_play_maps.sw[idx - 4]);
    }
    strncat(line, sum, sizeof(line) - strlen(line) - 1);
    render_line((u8)i, line);
  }

  line[0] = '\0';
  strcpy(line, "fld:");
  strcat(line, field_tag());
  render_line(4, line);
  render_footer("reset", "rst all", "-", "alt");
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
  if(m->kind == ePlaySwSetSlot || m->kind == ePlaySwMomSlot ||
     m->kind == ePlaySwSetAll || m->kind == ePlaySwMomAll) {
    ensure_param_label(m->label);
  }
}

static void cycle_param_label(char *label, s8 dir) {
  s16 idx;
  if(!g_module.loaded || g_module.num_params == 0) {
    return;
  }
  idx = find_param(label);
  if(idx < 0) {
    idx = 0;
  } else {
    idx += (dir > 0) ? 1 : -1;
    if(idx < 0) {
      idx = (s16)g_module.num_params - 1;
    }
    if(idx >= (s16)g_module.num_params) {
      idx = 0;
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
    if(coarse || g_alt_mode) {
      delta = (dir > 0) ? (io_t)0x100 : (io_t)-0x100;
    } else {
      delta = (dir > 0) ? (io_t)1 : (io_t)-1;
    }
    m->value = (ParamValue)scaler_inc(sc, &io, delta);
  } else {
    ParamDesc *d = &g_module.desc[idx];
    s32 step = (coarse || g_alt_mode) ? 64 : 1;
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

static void adjust_field(s8 dir) {
  if(is_sw()) {
    PlaySwMap *m = &g_play_maps.sw[ctrl_i()];
    switch(field) {
    case 0:
      cycle_sw_kind(m, dir);
      break;
    case 1:
      if(m->kind == ePlaySwSetSlot || m->kind == ePlaySwMomSlot) {
	m->slot = (MorphSlot)(((u8)m->slot + (dir > 0 ? 1 : 3)) % 4);
      } else {
	cycle_param_label(m->label, dir);
      }
      break;
    case 2:
      if(m->kind == ePlaySwSetSlot || m->kind == ePlaySwMomSlot) {
	cycle_param_label(m->label, dir);
      } else {
	bump_sw_value(m, dir, 0);
      }
      break;
    default:
      bump_sw_value(m, dir, 0);
      break;
    }
  } else {
    PlayEncMap *m = &g_play_maps.enc[ctrl_i()];
    switch(field) {
    case 0:
      cycle_enc_kind(m, dir);
      break;
    case 1:
      if(m->kind == ePlayEncParamSlot) {
	m->slot = (MorphSlot)(((u8)m->slot + (dir > 0 ? 1 : 3)) % 4);
      } else {
	cycle_param_label(m->label, dir);
      }
      break;
    default:
      cycle_param_label(m->label, dir);
      break;
    }
  }
  clamp_field();
}

static void handle_enc0(s32 data) {
  sel += (data > 0) ? 1 : -1;
  if(sel < 0) {
    sel = 0;
  }
  if(sel > 7) {
    sel = 7;
  }
  field = 0;
  redraw();
  render_update();
}

static void handle_enc1(s32 data) { pages_next(data > 0 ? 1 : -1); }

static void handle_enc2(s32 data) {
  u8 maxf;
  if(is_sw()) {
    maxf = sw_field_max(&g_play_maps.sw[ctrl_i()]);
  } else {
    maxf = enc_field_max(&g_play_maps.enc[ctrl_i()]);
  }
  if(maxf == 0) {
    return;
  }
  if(data > 0) {
    field = (u8)((field + 1) % (maxf + 1));
  } else {
    field = (u8)((field + maxf) % (maxf + 1));
  }
  redraw();
  render_update();
}

static void handle_enc3(s32 data) {
  adjust_field(data > 0 ? 1 : -1);
  redraw();
  render_update();
}

static void handle_sw0(s32 data) {
  if(data <= 0) {
    return;
  }
  if(is_sw()) {
    play_maps_reset_sw(&g_play_maps, ctrl_i());
  } else {
    play_maps_reset_enc(&g_play_maps, ctrl_i());
  }
  field = 0;
  redraw();
  render_update();
}

static void handle_sw1(s32 data) {
  if(data <= 0) {
    return;
  }
  play_maps_set_defaults(&g_play_maps);
  field = 0;
  redraw();
  render_update();
}

static void handle_sw2(s32 data) { (void)data; }

static void handle_sw3(s32 data) {
  g_alt_mode = data > 0 ? 1 : 0;
  redraw();
  render_update();
}

void select_play_maps(void) {
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

void page_play_maps_init(void) {
  sel = 0;
  field = 0;
}
