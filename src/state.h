#ifndef BETWEEN_STATE_H
#define BETWEEN_STATE_H

#include "slots.h"
#include "play_maps.h"
#include "types.h"

#include "between_limits.h"

extern Slots g_slots;
extern PlayMaps g_play_maps;
/* stem of the last loaded/saved setup; empty if none. */
extern char g_setup_name[BETWEEN_NAME_LEN];

void state_init(void);

/* setup dirty: setup-owned edits, or any occupied slot preset dirty. */
void state_setup_mark_dirty(void);
void state_setup_clear_dirty(void);
u8 state_setup_dirty(void);

/* morph point (marks setup dirty when the position changes). */
void state_set_morph(u16 x, u16 y);
void state_snap_to(MorphSlot slot);

/* load module into g_module and reset slots (unless keep_slots). */
u8 state_load_module(const char *name, u8 keep_slots);

/* load/save preset stem into a slot. */
u8 state_load_preset(MorphSlot slot, const char *stem);
u8 state_save_preset(MorphSlot slot, const char *stem);

/* create new preset from module defaults and assign to slot. */
u8 state_new_preset(MorphSlot slot, const char *stem);

/* write a unique pNNN stem into out (not on disk or in any slot). */
u8 state_unique_preset_stem(char *out, u32 out_size);

/* write a unique sNNN stem into out (not already on disk). */
u8 state_unique_setup_stem(char *out, u32 out_size);

u8 state_load_setup(const char *stem);
u8 state_save_setup(const char *stem);

/* persist last setup stem path for boot. */
u8 state_write_last_setup(const char *stem);
u8 state_read_last_setup(char *stem, u32 stem_size);

void state_apply(void);

/* morph exclusion (setup-owned; see SPEC). */
void state_exclude_rebuild(void);
void state_exclude_clear(void);
u8 state_param_excluded(u16 idx);
u8 state_param_play_bound(u16 idx);
/* set/clear manual exclude; returns 0 if play-bound blocked clear. */
u8 state_exclude_manual_set(u16 idx, u8 on);
/* parse "a, b, c" into manual excludes (unknown labels ignored). */
void state_exclude_manual_from_list(const char *list);
/* format manual excludes into buf as comma-separated labels. */
void state_exclude_manual_to_list(char *buf, u32 buf_size);

/* send one param to the DSP (for play/MIDI writes of excluded params). */
void state_send_param(u16 idx, ParamValue value);

#endif
