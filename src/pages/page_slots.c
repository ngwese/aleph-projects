#include "pages.h"

#include <string.h>

#include "app.h"
#include "events.h"

#include "between_limits.h"
#include "dirlist.h"
#include "font.h"
#include "module_load.h"
#include "morph2d.h"
#include "render.h"
#include "state.h"

static MorphSlot sel_slot;
static u8 modal;
static DirList presets;
static s16 preset_sel;

#define SLOT_COL_B_X 64

static void slot_name(MorphSlot s, char *name) {
  if(g_slots.occupied[s]) {
    strncpy(name, g_slots.stem[s], 14);
    name[14] = '\0';
  } else {
    strcpy(name, "empty");
  }
}

static void draw_slot_pair(u8 row_lab, u8 row_name, MorphSlot left,
			   MorphSlot right) {
  const char *pref_l = (left == sel_slot) ? "> " : "  ";
  const char *pref_r = (right == sel_slot) ? "> " : "  ";
  char letter[2];
  char name[16];
  u8 lx;
  u8 rx;

  render_line_at(row_lab, 0, pref_l);
  lx = font_string_pixels(pref_l);
  letter[0] = (char)('a' + (u8)left);
  letter[1] = '\0';
  render_line_at(row_lab, lx, letter);
  slot_name(left, name);
  render_line_at(row_name, lx, name);

  render_line_at(row_lab, SLOT_COL_B_X, pref_r);
  rx = (u8)(SLOT_COL_B_X + font_string_pixels(pref_r));
  letter[0] = (char)('a' + (u8)right);
  render_line_at(row_lab, rx, letter);
  slot_name(right, name);
  render_line_at(row_name, rx, name);
}

static void redraw_grid(void) {
  render_clear();
  render_header("slots", 0);
  draw_slot_pair(0, 1, eMorphSlotA, eMorphSlotB);
  draw_slot_pair(2, 3, eMorphSlotC, eMorphSlotD);
  render_footer("preset", "edit", "clear", "alt");
}

static void redraw_modal(void) {
  u16 i;
  char line[24];
  render_clear();
  render_header("preset", 0);
  if(presets.count == 0) {
    render_line(0, "(none)");
  } else {
    for(i = 0; i < 5 && (u16)(preset_sel + (s16)i) < presets.count; ++i) {
      u16 idx = (u16)(preset_sel + (s16)i);
      line[0] = (idx == (u16)preset_sel) ? '>' : ' ';
      line[1] = ' ';
      line[2] = '\0';
      strncat(line, presets.names[idx], 20);
      render_line((u8)i, line);
    }
  }
  render_footer("load", "cancel", "new", "alt");
}

static void redraw(void) {
  if(modal) {
    redraw_modal();
  } else {
    redraw_grid();
  }
}

void redraw_slots(void) { redraw(); }

static void open_modal(void) {
  char path[BETWEEN_PATH_MAX];
  if(!g_module.loaded) {
    pages_set(ePageModules);
    return;
  }
  strcpy(path, BETWEEN_PRESET_PATH);
  strcat(path, g_module.name);
  dirlist_scan(&presets, path, ".txt");
  preset_sel = 0;
  modal = 1;
  redraw();
  render_update();
}

static void handle_enc0(s32 data) {
  if(modal) {
    if(presets.count == 0) {
      return;
    }
    preset_sel += (data > 0) ? 1 : -1;
    if(preset_sel < 0) {
      preset_sel = 0;
    }
    if(preset_sel >= (s16)presets.count) {
      preset_sel = (s16)presets.count - 1;
    }
  } else {
    sel_slot = (MorphSlot)((sel_slot + (data > 0 ? 1 : 3)) % 4);
  }
  redraw();
  render_update();
}

static void handle_enc1(s32 data) {
  if(!modal) {
    pages_next(data > 0 ? 1 : -1);
  }
}

static void handle_enc2(s32 data) {
  s32 step = data > 0 ? (s32)(MORPH2D_ONE / 64) : -(s32)(MORPH2D_ONE / 64);
  s32 nx = (s32)g_slots.x + step;
  if(nx < 0) {
    nx = 0;
  }
  if(nx > (s32)MORPH2D_ONE) {
    nx = (s32)MORPH2D_ONE;
  }
  slots_set_morph(&g_slots, (u16)nx, g_slots.y);
  state_apply();
  redraw();
  render_update();
}

static void handle_enc3(s32 data) {
  s32 step = data > 0 ? (s32)(MORPH2D_ONE / 64) : -(s32)(MORPH2D_ONE / 64);
  s32 ny = (s32)g_slots.y + step;
  if(ny < 0) {
    ny = 0;
  }
  if(ny > (s32)MORPH2D_ONE) {
    ny = (s32)MORPH2D_ONE;
  }
  slots_set_morph(&g_slots, g_slots.x, (u16)ny);
  state_apply();
  redraw();
  render_update();
}

static void handle_sw0(s32 data) {
  if(data <= 0) {
    return;
  }
  if(modal) {
    if(presets.count > 0) {
      if(state_load_preset(sel_slot, presets.names[preset_sel])) {
	render_log("preset loaded");
      } else {
	render_log("load fail");
      }
      modal = 0;
    }
  } else {
    open_modal();
    return;
  }
  redraw();
  render_update();
}

static void handle_sw1(s32 data) {
  if(data <= 0) {
    return;
  }
  if(modal) {
    modal = 0;
  } else {
    pages_set((PageId)(ePageSlotA + sel_slot));
    return;
  }
  redraw();
  render_update();
}

static void handle_sw2(s32 data) {
  char stem[BETWEEN_NAME_LEN];
  if(data <= 0) {
    return;
  }
  if(modal) {
    if(!state_unique_preset_stem(stem, sizeof(stem))) {
      render_log("name fail");
      return;
    }
    if(state_new_preset(sel_slot, stem)) {
      render_log("preset new");
      modal = 0;
    } else {
      render_log("new fail");
    }
  } else if(g_alt_mode) {
    slots_clear_all(&g_slots);
    state_apply();
    render_log("cleared");
  } else {
    slots_clear_slot(&g_slots, sel_slot);
    state_apply();
    render_log("slot empty");
  }
  redraw();
  render_update();
}

static void handle_sw3(s32 data) {
  g_alt_mode = data > 0 ? 1 : 0;
  redraw();
  render_update();
}

void select_slots(void) {
  modal = 0;
  app_event_handlers[kEventEncoder0] = handle_enc0;
  app_event_handlers[kEventEncoder1] = handle_enc1;
  app_event_handlers[kEventEncoder2] = handle_enc2;
  app_event_handlers[kEventEncoder3] = handle_enc3;
  app_event_handlers[kEventSwitch0] = handle_sw0;
  app_event_handlers[kEventSwitch1] = handle_sw1;
  app_event_handlers[kEventSwitch2] = handle_sw2;
  app_event_handlers[kEventSwitch3] = handle_sw3;
}

void page_slots_init(void) {
  sel_slot = eMorphSlotA;
  modal = 0;
}
