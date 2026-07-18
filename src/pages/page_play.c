#include "pages.h"

#include <string.h>

#include "app.h"
#include "events.h"

#include "module_load.h"
#include "morph2d.h"
#include "render.h"
#include "state.h"

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

static void redraw(void) {
  char line[24];
  char num[8];
  u16 px;
  u16 py;

  render_clear();
  render_header_clear();

  px = (g_slots.x * 100) / MORPH2D_ONE;
  py = (g_slots.y * 100) / MORPH2D_ONE;
  if(g_module.loaded) {
    line[0] = '\0';
    strncat(line, g_module.name, 10);
  } else {
    strcpy(line, "no module");
  }
  strcat(line, " ");
  fmt_u(num, px);
  strcat(line, num);
  strcat(line, ",");
  fmt_u(num, py);
  strcat(line, num);
  render_line(0, line);

  line[0] = 'a';
  line[1] = ':';
  line[2] = '\0';
  strncat(line, g_slots.occupied[eMorphSlotA] ? g_slots.stem[eMorphSlotA] : "-",
	  12);
  render_line(1, line);
  line[0] = 'b';
  line[1] = ':';
  line[2] = '\0';
  strncat(line, g_slots.occupied[eMorphSlotB] ? g_slots.stem[eMorphSlotB] : "-",
	  12);
  render_line(2, line);
  line[0] = 'c';
  line[1] = ':';
  line[2] = '\0';
  strncat(line, g_slots.occupied[eMorphSlotC] ? g_slots.stem[eMorphSlotC] : "-",
	  12);
  render_line(3, line);
  line[0] = 'd';
  line[1] = ':';
  line[2] = '\0';
  strncat(line, g_slots.occupied[eMorphSlotD] ? g_slots.stem[eMorphSlotD] : "-",
	  12);
  render_line(4, line);

  render_footer("snapA", "snapB", "snapC", "snapD");
}

void redraw_play(void) { redraw(); }

static void nudge(u16 *axis, s32 data) {
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

static void handle_enc0(s32 data) {
  u16 x = g_slots.x;
  nudge(&x, data);
  slots_set_morph(&g_slots, x, g_slots.y);
  state_apply();
  redraw();
  render_update();
}

static void handle_enc1(s32 data) {
  u16 y = g_slots.y;
  nudge(&y, data);
  slots_set_morph(&g_slots, g_slots.x, y);
  state_apply();
  redraw();
  render_update();
}

static void snap(MorphSlot s) {
  slots_snap_to(&g_slots, s);
  state_apply();
  redraw();
  render_update();
}

static void handle_sw0(s32 data) {
  if(data > 0) {
    snap(eMorphSlotA);
  }
}
static void handle_sw1(s32 data) {
  if(data > 0) {
    snap(eMorphSlotB);
  }
}
static void handle_sw2(s32 data) {
  if(data > 0) {
    snap(eMorphSlotC);
  }
}
static void handle_sw3(s32 data) {
  if(data > 0) {
    snap(eMorphSlotD);
  }
}

void select_play(void) {
  app_event_handlers[kEventEncoder0] = handle_enc0;
  app_event_handlers[kEventEncoder1] = handle_enc1;
  app_event_handlers[kEventEncoder2] = handle_enc0;
  app_event_handlers[kEventEncoder3] = handle_enc1;
  app_event_handlers[kEventSwitch0] = handle_sw0;
  app_event_handlers[kEventSwitch1] = handle_sw1;
  app_event_handlers[kEventSwitch2] = handle_sw2;
  app_event_handlers[kEventSwitch3] = handle_sw3;
  redraw();
  render_update();
}

void page_play_init(void) {}
