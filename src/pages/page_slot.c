#include "pages.h"

#include <string.h>

#include "app.h"
#include "events.h"

#include "between_limits.h"
#include "module_load.h"
#include "morph2d.h"
#include "param_scaler.h"
#include "render.h"
#include "scaler_tables.h"
#include "state.h"

static MorphSlot cur_slot;
static s16 param_sel;

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

static void fmt_param_value(char *dst, u16 idx, ParamValue raw) {
  ParamScaler *sc;
  io_t io;
  if(param_scaler_usable(idx)) {
    sc = &g_scalers[idx];
    io = scaler_get_in(sc, (s32)raw);
    scaler_get_str(dst, sc, io);
  } else {
    fmt_s32(dst, (s32)raw);
  }
}

static void redraw_slot(MorphSlot slot) {
  char line[24];
  char num[16];
  u16 i;
  u16 start;

  render_clear();
  if(!g_slots.occupied[slot]) {
    render_header_slot((char)('A' + (u8)slot), NULL, 0);
    render_line(2, "empty");
    render_footer("new", "-", "-", "-");
    return;
  }

  render_header_slot((char)('A' + (u8)slot), g_slots.stem[slot],
		     g_slots.dirty[slot]);

  start = (param_sel > 3) ? (u16)(param_sel - 3) : 0;
  for(i = 0; i < 5; ++i) {
    u16 idx = start + i;
    if(idx >= g_slots.num_params) {
      break;
    }
    line[0] = (idx == (u16)param_sel) ? '>' : ' ';
    line[1] = ' ';
    line[2] = '\0';
    strncat(line, g_module.desc[idx].label, 8);
    strcat(line, ":");
    fmt_param_value(num, idx, g_slots.values[slot][idx]);
    strncat(line, num, 10);
    render_line((u8)i, line);
  }
  render_footer("save", "reset", "new", "alt");
}

void redraw_slot_a(void) { redraw_slot(eMorphSlotA); }
void redraw_slot_b(void) { redraw_slot(eMorphSlotB); }
void redraw_slot_c(void) { redraw_slot(eMorphSlotC); }
void redraw_slot_d(void) { redraw_slot(eMorphSlotD); }

static void handle_enc0(s32 data) {
  if(!g_slots.occupied[cur_slot] || g_slots.num_params == 0) {
    return;
  }
  param_sel += (data > 0) ? 1 : -1;
  if(param_sel < 0) {
    param_sel = 0;
  }
  if(param_sel >= (s16)g_slots.num_params) {
    param_sel = (s16)g_slots.num_params - 1;
  }
  redraw_slot(cur_slot);
  render_update();
}

static void handle_enc1(s32 data) { pages_next(data > 0 ? 1 : -1); }

static void bump_param_raw(s32 inc) {
  ParamValue v;
  ParamDesc *d;
  d = &g_module.desc[param_sel];
  v = slots_get_value(&g_slots, cur_slot, (u16)param_sel);
  if(inc > 0) {
    if(v < d->max - inc) {
      v += inc;
    } else {
      v = d->max;
    }
  } else {
    if(v > d->min - inc) {
      v += inc;
    } else {
      v = d->min;
    }
  }
  slots_set_value(&g_slots, cur_slot, (u16)param_sel, v);
}

static void bump_param_scaled(io_t delta) {
  ParamScaler *sc = &g_scalers[param_sel];
  ParamValue raw = slots_get_value(&g_slots, cur_slot, (u16)param_sel);
  io_t io = scaler_get_in(sc, (s32)raw);
  ParamValue next = (ParamValue)scaler_inc(sc, &io, delta);
  slots_set_value(&g_slots, cur_slot, (u16)param_sel, next);
}

static void bump_param(io_t fine_or_coarse, u8 coarse) {
  if(!g_slots.occupied[cur_slot] || g_slots.num_params == 0) {
    return;
  }
  if(param_scaler_usable((u16)param_sel)) {
    bump_param_scaled(fine_or_coarse);
  } else {
    bump_param_raw(coarse ? (fine_or_coarse > 0 ? 64 : -64)
			  : (fine_or_coarse > 0 ? 1 : -1));
  }
  state_apply();
  redraw_slot(cur_slot);
  render_update();
}

static void handle_enc2(s32 data) {
  bump_param(data > 0 ? (io_t)1 : (io_t)-1, 0);
}

static void handle_enc3(s32 data) {
  /* Bees-like coarse step in io_t domain */
  bump_param(data > 0 ? (io_t)0x100 : (io_t)-0x100, 1);
}

static void handle_sw0(s32 data) {
  char stem[BETWEEN_NAME_LEN];
  if(data <= 0) {
    return;
  }
  /* empty slot: sw0 creates a new preset from defaults */
  if(!g_slots.occupied[cur_slot]) {
    if(!g_module.loaded) {
      render_log("no module");
      return;
    }
    if(!state_unique_preset_stem(stem, sizeof(stem))) {
      render_log("name fail");
      return;
    }
    if(state_new_preset(cur_slot, stem, 0)) {
      render_log("preset new");
    } else {
      render_log("new fail");
    }
    redraw_slot(cur_slot);
    render_update();
    return;
  }
  if(g_alt_mode) {
    if(!state_unique_preset_stem(stem, sizeof(stem))) {
      render_log("name fail");
      return;
    }
    if(state_save_preset(cur_slot, stem)) {
      render_log("save as");
    } else {
      render_log("save fail");
    }
  } else {
    if(g_slots.stem[cur_slot][0]) {
      strncpy(stem, g_slots.stem[cur_slot], BETWEEN_NAME_LEN - 1);
      stem[BETWEEN_NAME_LEN - 1] = '\0';
    } else if(!state_unique_preset_stem(stem, sizeof(stem))) {
      render_log("name fail");
      return;
    }
    if(state_save_preset(cur_slot, stem)) {
      render_log("preset saved");
    } else {
      render_log("save fail");
    }
  }
  redraw_slot(cur_slot);
  render_update();
}

static void handle_sw1(s32 data) {
  if(data <= 0 || !g_slots.occupied[cur_slot] || !g_slots.stem[cur_slot][0]) {
    return;
  }
  if(state_load_preset(cur_slot, g_slots.stem[cur_slot])) {
    render_log("reset");
  } else {
    render_log("reset fail");
  }
  redraw_slot(cur_slot);
  render_update();
}

static void handle_sw2(s32 data) {
  char stem[BETWEEN_NAME_LEN];
  if(data <= 0) {
    return;
  }
  if(!g_slots.occupied[cur_slot]) {
    return;
  }
  if(g_alt_mode) {
    slots_capture_effective(&g_slots, cur_slot);
    render_log("captured");
  } else {
    if(!state_unique_preset_stem(stem, sizeof(stem))) {
      render_log("name fail");
      return;
    }
    if(state_new_preset(cur_slot, stem, 0)) {
      render_log("preset new");
    } else {
      render_log("new fail");
    }
  }
  redraw_slot(cur_slot);
  render_update();
}

static void handle_sw3(s32 data) {
  g_alt_mode = data > 0 ? 1 : 0;
  redraw_slot(cur_slot);
  render_update();
}

static void select_slot(MorphSlot slot) {
  cur_slot = slot;
  param_sel = 0;
  app_event_handlers[kEventEncoder0] = handle_enc0;
  app_event_handlers[kEventEncoder1] = handle_enc1;
  app_event_handlers[kEventEncoder2] = handle_enc2;
  app_event_handlers[kEventEncoder3] = handle_enc3;
  app_event_handlers[kEventSwitch0] = handle_sw0;
  app_event_handlers[kEventSwitch1] = handle_sw1;
  app_event_handlers[kEventSwitch2] = handle_sw2;
  app_event_handlers[kEventSwitch3] = handle_sw3;
}

void select_slot_a(void) { select_slot(eMorphSlotA); }
void select_slot_b(void) { select_slot(eMorphSlotB); }
void select_slot_c(void) { select_slot(eMorphSlotC); }
void select_slot_d(void) { select_slot(eMorphSlotD); }

void page_slot_init(void) {
  cur_slot = eMorphSlotA;
  param_sel = 0;
}
