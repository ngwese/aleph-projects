#include "pages.h"

#include "app.h"
#include "events.h"

#include "render.h"

Page g_pages[eNumPages];
s8 g_page_idx = ePageSetups;
u8 g_play_mode = 0;
u8 g_alt_mode = 0;
u8 g_new_setup_flow = 0;

static s8 last_edit_page = ePageSetups;

static void handle_sw_noop(s32 data) { (void)data; }

void pages_init(void) {
  page_setups_init();
  page_modules_init();
  page_slots_init();
  page_slot_init();
  page_play_maps_init();
  page_info_init();
  page_play_init();

  g_pages[ePageSetups] =
    (Page){.name = "setups", .select_fn = select_setups, .redraw_fn = redraw_setups};
  g_pages[ePageModules] =
    (Page){.name = "modules", .select_fn = select_modules, .redraw_fn = redraw_modules};
  g_pages[ePageSlots] =
    (Page){.name = "slots", .select_fn = select_slots, .redraw_fn = redraw_slots};
  g_pages[ePageSlotA] =
    (Page){.name = "slot a", .select_fn = select_slot_a, .redraw_fn = redraw_slot_a};
  g_pages[ePageSlotB] =
    (Page){.name = "slot b", .select_fn = select_slot_b, .redraw_fn = redraw_slot_b};
  g_pages[ePageSlotC] =
    (Page){.name = "slot c", .select_fn = select_slot_c, .redraw_fn = redraw_slot_c};
  g_pages[ePageSlotD] =
    (Page){.name = "slot d", .select_fn = select_slot_d, .redraw_fn = redraw_slot_d};
  g_pages[ePagePlayMaps] =
    (Page){.name = "play", .select_fn = select_play_maps, .redraw_fn = redraw_play_maps};
  g_pages[ePageInfo] =
    (Page){.name = "info", .select_fn = select_info, .redraw_fn = redraw_info};
  g_pages[ePagePlay] =
    (Page){.name = "play", .select_fn = select_play, .redraw_fn = redraw_play};

  g_page_idx = ePageSetups;
  pages_set(ePageSetups);
}

void pages_set(PageId id) {
  if(id >= eNumPages) {
    return;
  }
  g_page_idx = (s8)id;
  if(id != ePagePlay) {
    last_edit_page = (s8)id;
  }
  g_alt_mode = 0;
  g_pages[id].select_fn();
  /* footswitches only mapped in live play; clear when leaving */
  if(id != ePagePlay) {
    app_event_handlers[kEventSwitch6] = handle_sw_noop;
    app_event_handlers[kEventSwitch7] = handle_sw_noop;
  }
  g_pages[id].redraw_fn();
  render_update();
}

void pages_next(s8 dir) {
  s8 next;
  if(g_play_mode) {
    return;
  }
  next = g_page_idx + dir;
  /* skip live play in edit ring; wrap at info */
  if(next < ePageSetups) {
    next = ePageInfo;
  }
  if(next > ePageInfo) {
    next = ePageSetups;
  }
  pages_set((PageId)next);
}

u8 pages_toggle_play(void) {
  if(g_play_mode) {
    g_play_mode = 0;
    pages_set((PageId)last_edit_page);
    return 0;
  }
  g_play_mode = 1;
  pages_set(ePagePlay);
  return 1;
}

void pages_redraw(void) {
  if(g_page_idx >= 0 && g_page_idx < eNumPages) {
    g_pages[g_page_idx].redraw_fn();
  }
}
