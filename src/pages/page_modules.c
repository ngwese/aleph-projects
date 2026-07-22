#include "pages.h"

#include <string.h>

#include "app.h"
#include "events.h"

#include "between_limits.h"
#include "dirlist.h"
#include "input_roles.h"
#include "module_load.h"
#include "render.h"
#include "state.h"

static DirList list;
static s16 sel;
static u8 scanned;

static void do_scan(void) {
  dirlist_scan(&list, BETWEEN_MOD_PATH, ".ldr");
  if(sel >= (s16)list.count) {
    sel = list.count ? (s16)list.count - 1 : 0;
  }
  scanned = 1;
}

static void redraw(void) {
  u16 i;
  char line[24];
  render_clear();
  render_header_with_name("module", g_module.loaded ? g_module.name : NULL, 0);
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
    render_footer("-", "-", "scan", "alt");
  } else {
    render_footer("load", "-", "-", "alt");
  }
}

void redraw_modules(void) { redraw(); }

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
  if(data <= 0 || g_alt_mode || list.count == 0) {
    return;
  }
  if(state_load_module(list.names[sel], 0)) {
    if(g_new_setup_flow) {
      g_new_setup_flow = 0;
      pages_set(ePageSlots);
      return;
    }
  } else {
    render_log("load fail");
  }
  redraw();
  render_update();
}

static void handle_sw_noop(s32 data) { (void)data; }

static void handle_sw2(s32 data) {
  if(data <= 0 || !g_alt_mode) {
    return;
  }
  do_scan();
  render_log("scanned");
  redraw();
  render_update();
}

static void handle_sw3(s32 data) {
  g_alt_mode = data > 0 ? 1 : 0;
  redraw();
  render_update();
}

void select_modules(void) {
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
  app_event_handlers[kEventSwitch1] = handle_sw_noop;
  app_event_handlers[kEventSwitch2] = handle_sw2;
  app_event_handlers[kEventSwitch3] = handle_sw3;
}

void page_modules_init(void) {
  sel = 0;
  list.count = 0;
  scanned = 0;
}
