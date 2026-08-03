#include "pages.h"

#include <string.h>

#include "between_limits.h"
#include "dirlist.h"
#include "font.h"
#include "input_roles.h"
#include "module_load.h"
#include "preset_file.h"
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
  render_string_xy(lx, (u8)(row_lab * 8), letter, RENDER_PLAY_GREY_DARK);
  slot_name(left, name);
  render_line_at(row_name, lx, name);

  render_line_at(row_lab, SLOT_COL_B_X, pref_r);
  rx = (u8)(SLOT_COL_B_X + font_string_pixels(pref_r));
  letter[0] = (char)('a' + (u8)right);
  render_string_xy(rx, (u8)(row_lab * 8), letter, RENDER_PLAY_GREY_DARK);
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
  if(g_alt_mode) {
    render_footer("delete", "-", "-", "alt");
  } else {
    render_footer("load", "cancel", "new", "alt");
  }
}

static void rescan_presets(void) {
  char path[BETWEEN_PATH_MAX];
  strcpy(path, BETWEEN_PRESET_PATH);
  strcat(path, g_module.name);
  dirlist_scan(&presets, path, ".txt");
  if(preset_sel >= (s16)presets.count) {
    preset_sel = presets.count ? (s16)presets.count - 1 : 0;
  }
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
  if(!g_module.loaded) {
    pages_set(ePageModules);
    return;
  }
  preset_sel = 0;
  rescan_presets();
  modal = 1;
  render_mark_dirty();
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
  render_mark_dirty();
}

static void handle_enc1(s32 data) {
  if(!modal) {
    pages_next(data > 0 ? 1 : -1);
  }
}


static void handle_sw0(s32 data) {
  if(data <= 0) {
    return;
  }
  if(modal) {
    if(g_alt_mode) {
      if(presets.count == 0) {
        return;
      }
      if(preset_file_delete(g_module.name, presets.names[preset_sel])) {
        rescan_presets();
        render_log("deleted");
      } else {
        render_log("delete fail");
      }
    } else if(presets.count > 0) {
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
  render_mark_dirty();
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
  render_mark_dirty();
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
    state_setup_mark_dirty();
    state_apply();
    render_log("cleared");
  } else {
    slots_clear_slot(&g_slots, sel_slot);
    state_setup_mark_dirty();
    state_apply();
    render_log("slot empty");
  }
  render_mark_dirty();
}

static void handle_sw3(s32 data) {
  (void)data;
  render_mark_dirty();
}

void select_slots(void) {
  static const InputEncBinding enc[4] = {
    {eInputRoleListSelect, handle_enc0},
    {eInputRolePageSelect, handle_enc1},
    {eInputRoleUnmapped, NULL},
    {eInputRoleUnmapped, NULL},
  };
  static const InputSwBinding sw[4] = {
    {eInputSwRoleAction, handle_sw0},
    {eInputSwRoleAction, handle_sw1},
    {eInputSwRoleAction, handle_sw2},
    {eInputSwRoleAlt, handle_sw3},
  };
  modal = 0;
  input_roles_bind(enc, sw);
}

void page_slots_init(void) {
  sel_slot = eMorphSlotA;
  modal = 0;
}
