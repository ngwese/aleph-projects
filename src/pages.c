#include "pages.h"

#include "app.h"
#include "conf_board.h"
#include "events.h"
#include "gpio.h"

#include "name_edit.h"
#include "render.h"

Page g_pages[eNumPages];
s8 g_page_idx = ePageSetups;
AppMode g_app_mode = eAppModeEdit;
u8 g_alt_mode = 0;
u8 g_new_setup_flow = 0;

static s8 last_edit_page = ePageSetups;
static AppMode pre_inspect_mode = eAppModeEdit;

static void handle_sw_noop(s32 data) { (void)data; }

static u8 page_is_live_mode(PageId id) {
  return (u8)(id == ePagePlay || id == ePageInspect);
}

static void app_mode_apply_led(void) {
  if(g_app_mode == eAppModePlay) {
    gpio_set_gpio_pin(LED_MODE_PIN);
  } else {
    gpio_clr_gpio_pin(LED_MODE_PIN);
  }
}

u8 app_mode_is_play(void) { return (u8)(g_app_mode == eAppModePlay); }
u8 app_mode_is_edit(void) { return (u8)(g_app_mode == eAppModeEdit); }
u8 app_mode_is_inspect(void) {
  return (u8)(g_app_mode == eAppModeInspect);
}

void pages_init(void) {
  page_setups_init();
  page_modules_init();
  page_slots_init();
  page_slot_init();
  page_play_maps_init();
  page_info_init();
  page_play_init();
  page_inspect_init();

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
  g_pages[ePageInspect] =
    (Page){.name = "inspect", .select_fn = select_inspect,
	   .redraw_fn = redraw_inspect};

  g_page_idx = ePageSetups;
  g_app_mode = eAppModeEdit;
  pages_set(ePageSetups);
}

void pages_set(PageId id) {
  if(id >= eNumPages) {
    return;
  }
  if(name_edit_active()) {
    /* MODE / page change while naming: drop modal; select_fn rebinds. */
    name_edit_abort();
  }
  g_page_idx = (s8)id;
  if(!page_is_live_mode(id)) {
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
  if(!app_mode_is_edit()) {
    return;
  }
  next = g_page_idx + dir;
  /* skip live play / inspect in edit ring; wrap at info */
  if(next < ePageSetups) {
    next = ePageInfo;
  }
  if(next > ePageInfo) {
    next = ePageSetups;
  }
  pages_set((PageId)next);
}

void pages_enter_edit(void) {
  g_app_mode = eAppModeEdit;
  pages_set((PageId)last_edit_page);
  app_mode_apply_led();
}

void pages_enter_play(void) {
  g_app_mode = eAppModePlay;
  pages_set(ePagePlay);
  app_mode_apply_led();
}

void pages_enter_inspect(void) {
  if(g_app_mode != eAppModeInspect) {
    if(g_app_mode == eAppModePlay || g_app_mode == eAppModeEdit) {
      pre_inspect_mode = g_app_mode;
    } else {
      pre_inspect_mode = eAppModeEdit;
    }
  }
  g_app_mode = eAppModeInspect;
  pages_set(ePageInspect);
  app_mode_apply_led();
}

void pages_mode_short_release(void) {
  if(g_app_mode == eAppModeInspect) {
    if(pre_inspect_mode == eAppModePlay) {
      pages_enter_play();
    } else {
      pages_enter_edit();
    }
    return;
  }
  if(g_app_mode == eAppModePlay) {
    pages_enter_edit();
  } else {
    pages_enter_play();
  }
}

void pages_redraw(void) {
  if(g_page_idx >= 0 && g_page_idx < eNumPages) {
    g_pages[g_page_idx].redraw_fn();
  }
}
