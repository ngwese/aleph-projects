#include "pages.h"

#include <string.h>

#include "app.h"
#include "events.h"
#include "fix.h"

#include "between_limits.h"
#include "font.h"
#include "input_roles.h"
#include "midi_between.h"
#include "midi_nrpn.h"
#include "module_load.h"
#include "morph2d.h"
#include "name_edit.h"
#include "param_scaler.h"
#include "render.h"
#include "scaler_tables.h"
#include "state.h"

static MorphSlot cur_slot;
/* shared across slot A–D pages so selection/scroll survive page changes */
static s16 param_sel;
static MorphSlot save_as_slot;

static void clamp_param_sel(void) {
  if(g_slots.num_params == 0) {
    param_sel = 0;
    return;
  }
  if(param_sel < 0) {
    param_sel = 0;
  }
  if(param_sel >= (s16)g_slots.num_params) {
    param_sel = (s16)g_slots.num_params - 1;
  }
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

/* value column starts at horizontal midpoint (128/2). */
#define SLOT_VAL_X 64

static void redraw_slot(MorphSlot slot) {
  char line[24];
  char num[FIX_DIG_TOTAL + 1];
  char nrpn_s[8];
  char val14_s[8];
  u16 i;
  u16 start;
  u8 status_y;
  u8 nrpn_x;
  u8 val_lab_w;
  u8 val_lab_x;
  ParamValue raw;
  u16 v14;

  render_clear();
  if(!g_slots.occupied[slot]) {
    render_header_slot((char)('A' + (u8)slot), NULL, 0);
    render_line(2, "empty");
    render_footer("new", "-", "-", "-");
    return;
  }

  clamp_param_sel();
  render_header_slot((char)('A' + (u8)slot), g_slots.stem[slot],
		     g_slots.dirty[slot]);

  /* four list rows; status line above the diagnostic log (row 4) */
  start = (param_sel > 3) ? (u16)(param_sel - 3) : 0;
  for(i = 0; i < 4; ++i) {
    u16 idx = start + i;
    u8 y;
    u8 excl;
    u8 fg;
    if(idx >= g_slots.num_params) {
      break;
    }
    excl = state_param_excluded(idx);
    fg = excl ? RENDER_PLAY_GREY : 0xf;
    y = (u8)(i * 8);
    line[0] = (idx == (u16)param_sel) ? '>' : ' ';
    line[1] = ' ';
    line[2] = '\0';
    strncat(line, g_module.desc[idx].label, 8);
    strcat(line, ":");
    render_fill_rect(0, y, 128, 8, 0);
    render_string_xy(0, y, line, fg);
    if(excl) {
      render_string_xy(SLOT_VAL_X, y, "-", fg);
    } else {
      fmt_param_value(num, sizeof(num), idx, g_slots.values[slot][idx]);
      render_string_xy(SLOT_VAL_X, y, num, fg);
    }
  }

  status_y = (u8)(4 * 8);
  midi_nrpn_fmt_msb_lsb(nrpn_s, sizeof(nrpn_s), (u16)param_sel);
  if(state_param_excluded((u16)param_sel)) {
    val14_s[0] = '-';
    val14_s[1] = '\0';
  } else {
    raw = slots_get_value(&g_slots, slot, (u16)param_sel);
    v14 = between_midi_raw_to_v14((u16)param_sel, raw);
    midi_nrpn_fmt_msb_lsb(val14_s, sizeof(val14_s), v14);
  }

  render_string_xy(0, status_y, "nrpn ", RENDER_PLAY_GREY_DARK);
  nrpn_x = (u8)font_string_pixels("nrpn ");
  render_string_xy(nrpn_x, status_y, nrpn_s, RENDER_PLAY_GREY_LIGHT);

  val_lab_w = (u8)font_string_pixels("value ");
  val_lab_x = (SLOT_VAL_X > val_lab_w) ? (u8)(SLOT_VAL_X - val_lab_w) : 0;
  render_fill_rect(val_lab_x, status_y, (u8)(128 - val_lab_x), 8, 0);
  render_string_xy(val_lab_x, status_y, "value ", RENDER_PLAY_GREY_DARK);
  render_string_xy(SLOT_VAL_X, status_y, val14_s, RENDER_PLAY_GREY_LIGHT);

  if(g_alt_mode) {
    render_footer("save as", "capture", "focus", "alt");
  } else {
    render_footer("save", "reset", "new", "alt");
  }
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
  clamp_param_sel();
  redraw_slot(cur_slot);
  render_update();
}

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
  /* get_in returns the table-bucket base (e.g. amp inRshift==5). a fine
   * +1 stays in the same bucket so raw/display do not move, while -1
   * crosses into the previous bucket — only decrement appears to work.
   * promote sub-bucket steps to one table index (0x20). */
  if(delta > 0 && delta < (io_t)0x20) {
    delta = (io_t)0x20;
  } else if(delta < 0 && delta > (io_t)-0x20) {
    delta = (io_t)-0x20;
  }
  {
    ParamValue next = (ParamValue)scaler_inc(sc, &io, delta);
    slots_set_value(&g_slots, cur_slot, (u16)param_sel, next);
  }
}

static void bump_param(io_t fine_or_coarse, u8 coarse) {
  if(!g_slots.occupied[cur_slot] || g_slots.num_params == 0) {
    return;
  }
  if(state_param_excluded((u16)param_sel)) {
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
  if(g_alt_mode) {
    /* left = enable exclusion; right = disable exclusion */
    u8 want_excl = (data < 0) ? 1 : 0;
    if(!g_slots.occupied[cur_slot] || g_slots.num_params == 0) {
      return;
    }
    if(!want_excl && state_param_play_bound((u16)param_sel)) {
      render_log("bound to play");
      return;
    }
    if(!state_exclude_manual_set((u16)param_sel, want_excl)) {
      if(state_param_play_bound((u16)param_sel)) {
	render_log("bound to play");
      }
      return;
    }
    redraw_slot(cur_slot);
    render_update();
    return;
  }
  /* Bees-like coarse step in io_t domain */
  bump_param(data > 0 ? (io_t)0x100 : (io_t)-0x100, 1);
}

static void on_save_as_ok(const char *stem, void *ctx) {
  MorphSlot slot = *(MorphSlot *)ctx;
  if(state_save_preset(slot, stem)) {
    render_log("save as");
  } else {
    render_log("save fail");
  }
  redraw_slot(slot);
  render_update();
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
    if(state_new_preset(cur_slot, stem)) {
      render_log("preset new");
    } else {
      render_log("new fail");
    }
    redraw_slot(cur_slot);
    render_update();
    return;
  }
  if(g_alt_mode) {
    if(g_slots.stem[cur_slot][0]) {
      strncpy(stem, g_slots.stem[cur_slot], BETWEEN_NAME_LEN - 1);
      stem[BETWEEN_NAME_LEN - 1] = '\0';
    } else if(!state_unique_preset_stem(stem, sizeof(stem))) {
      render_log("name fail");
      return;
    }
    save_as_slot = cur_slot;
    name_edit_open(eNameEditPreset, stem, on_save_as_ok, &save_as_slot);
    return;
  }
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
  redraw_slot(cur_slot);
  render_update();
}

static void handle_sw1(s32 data) {
  if(data <= 0 || !g_slots.occupied[cur_slot]) {
    return;
  }
  if(g_alt_mode) {
    slots_capture_effective(&g_slots, cur_slot);
    render_log("captured");
    redraw_slot(cur_slot);
    render_update();
    return;
  }
  if(!g_slots.stem[cur_slot][0]) {
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
    slots_snap_to(&g_slots, cur_slot);
    state_apply();
    render_log("focus");
    redraw_slot(cur_slot);
    render_update();
    return;
  }
  if(!state_unique_preset_stem(stem, sizeof(stem))) {
    render_log("name fail");
    return;
  }
  if(state_new_preset(cur_slot, stem)) {
    render_log("preset new");
  } else {
    render_log("new fail");
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
  static const InputEncBinding enc[4] = {
    {eInputRoleListSelect, handle_enc0},
    {eInputRolePageSelect, NULL},
    {eInputRoleParamFine, handle_enc2},
    {eInputRoleParamCoarse, handle_enc3},
  };
  cur_slot = slot;
  clamp_param_sel();
  input_roles_bind(enc);
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
