#ifndef BETWEEN_PAGES_H
#define BETWEEN_PAGES_H

#include "types.h"

typedef enum {
  ePageSetups = 0,
  ePageModules,
  ePageSlots,
  ePageSlotA,
  ePageSlotB,
  ePageSlotC,
  ePageSlotD,
  ePagePlayMaps,
  ePageInfo,
  ePagePlay,
  ePageInspect,
  eNumPages
} PageId;

typedef enum {
  eAppModeEdit = 0,
  eAppModePlay,
  eAppModeInspect
} AppMode;

typedef void (*page_select_fn)(void);
typedef void (*page_redraw_fn)(void);

typedef struct {
  const char *name;
  page_select_fn select_fn;
  page_redraw_fn redraw_fn;
} Page;

extern Page g_pages[eNumPages];
extern s8 g_page_idx;
extern AppMode g_app_mode;
extern u8 g_alt_mode;
extern u8 g_new_setup_flow;

u8 app_mode_is_play(void);
u8 app_mode_is_edit(void);
u8 app_mode_is_inspect(void);

void pages_init(void);
void pages_set(PageId id);
void pages_next(s8 dir);
void pages_enter_play(void);
void pages_enter_edit(void);
void pages_enter_inspect(void);
/* short MODE release: inspect→prior mode/page; else play↔edit. */
void pages_mode_short_release(void);
void pages_redraw(void);

void page_setups_init(void);
void page_modules_init(void);
void page_slots_init(void);
void page_slot_init(void);
void page_play_maps_init(void);
void page_info_init(void);
void page_play_init(void);
void page_inspect_init(void);

void select_setups(void);
void select_modules(void);
void select_slots(void);
void select_slot_a(void);
void select_slot_b(void);
void select_slot_c(void);
void select_slot_d(void);
void select_play_maps(void);
void select_info(void);
void select_play(void);
void select_inspect(void);

void redraw_setups(void);
void redraw_modules(void);
void redraw_slots(void);
void redraw_slot_a(void);
void redraw_slot_b(void);
void redraw_slot_c(void);
void redraw_slot_d(void);
void redraw_play_maps(void);
void redraw_info(void);
void redraw_play(void);
void redraw_inspect(void);

#endif
