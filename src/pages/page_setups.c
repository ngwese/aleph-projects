#include "pages.h"

#include <string.h>

#include "app.h"
#include "events.h"

#include "between_limits.h"
#include "dirlist.h"
#include "input_roles.h"
#include "name_edit.h"
#include "render.h"
#include "setup_file.h"
#include "state.h"

static DirList list;
static s16 sel;
static u8 scanned;

static void do_scan(void) {
  dirlist_scan(&list, BETWEEN_SETUP_PATH, ".txt");
  if(sel >= (s16)list.count) {
    sel = list.count ? (s16)list.count - 1 : 0;
  }
  scanned = 1;
}

static void on_save_as_ok(const char *stem, void *ctx) {
  (void)ctx;
  if(state_save_setup(stem)) {
    render_log("setup saved");
    do_scan();
  } else {
    render_log("save fail");
  }
  redraw_setups();
  render_update();
}

static void redraw(void) {
  u16 i;
  char line[24];
  render_clear();
  render_header_with_name("setup", g_setup_name[0] ? g_setup_name : NULL, 0);
  if(list.count == 0) {
    render_line(0, "(none)");
  } else {
    for(i = 0; i < 5 && (u16)(sel + (s16)i) < list.count; ++i) {
      u16 idx = (u16)(sel + (s16)i);
      line[0] = (idx == (u16)sel) ? '>' : ' ';
      line[1] = ' ';
      line[2] = '\0';
      strncat(line, list.names[idx], 20);
      render_line((u8)i, line);
    }
  }
  if(g_alt_mode) {
    render_footer("delete", "save as", "scan", "alt");
  } else {
    render_footer("load", "save", "new", "alt");
  }
}

void redraw_setups(void) { redraw(); }

static void handle_enc0(s32 data) {
  if(list.count == 0) {
    return;
  }
  sel += (data > 0) ? 1 : -1;
  if(sel < 0) {
    sel = 0;
  }
  if(sel >= (s16)list.count) {
    sel = (s16)list.count - 1;
  }
  redraw();
  render_update();
}

static void handle_sw0(s32 data) {
  if(data <= 0 || list.count == 0) {
    return;
  }
  if(g_alt_mode) {
    setup_file_delete(list.names[sel]);
    do_scan();
    render_log("deleted");
  } else {
    if(state_load_setup(list.names[sel])) {
      render_log("setup loaded");
    }
    /* on failure state_load_setup already logged a specific reason */
  }
  redraw();
  render_update();
}

static void handle_sw1(s32 data) {
  char stem[BETWEEN_NAME_LEN];
  if(data <= 0) {
    return;
  }
  if(g_alt_mode) {
    if(g_setup_name[0] != '\0') {
      strncpy(stem, g_setup_name, BETWEEN_NAME_LEN - 1);
      stem[BETWEEN_NAME_LEN - 1] = '\0';
    } else if(!state_unique_setup_stem(stem, sizeof(stem))) {
      render_log("name fail");
      return;
    }
    name_edit_open(eNameEditSetup, stem, on_save_as_ok, NULL);
    return;
  }
  if(g_setup_name[0] != '\0') {
    strncpy(stem, g_setup_name, BETWEEN_NAME_LEN - 1);
    stem[BETWEEN_NAME_LEN - 1] = '\0';
  } else if(!state_unique_setup_stem(stem, sizeof(stem))) {
    render_log("name fail");
    return;
  }
  if(state_save_setup(stem)) {
    render_log("setup saved");
    do_scan();
  } else {
    render_log("save fail");
  }
  redraw();
  render_update();
}

static void handle_sw2(s32 data) {
  if(data <= 0) {
    return;
  }
  if(g_alt_mode) {
    do_scan();
    render_log("scanned");
    redraw();
    render_update();
    return;
  }
  if(!state_unique_setup_stem(g_setup_name, sizeof(g_setup_name))) {
    render_log("name fail");
    redraw();
    render_update();
    return;
  }
  g_new_setup_flow = 1;
  pages_set(ePageModules);
}

static void handle_sw3(s32 data) {
  g_alt_mode = data > 0 ? 1 : 0;
  redraw();
  render_update();
}

void select_setups(void) {
  static const InputEncBinding enc[4] = {
    {eInputRoleListSelect, handle_enc0},
    {eInputRolePageSelect, NULL},
    {eInputRoleUnmapped, NULL},
    {eInputRoleUnmapped, NULL},
  };
  if(!scanned) {
    do_scan();
  }
  if(sel >= (s16)list.count) {
    sel = 0;
  }
  input_roles_bind(enc);
  app_event_handlers[kEventSwitch0] = handle_sw0;
  app_event_handlers[kEventSwitch1] = handle_sw1;
  app_event_handlers[kEventSwitch2] = handle_sw2;
  app_event_handlers[kEventSwitch3] = handle_sw3;
}

void page_setups_init(void) {
  sel = 0;
  list.count = 0;
  scanned = 0;
}
