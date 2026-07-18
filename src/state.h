#ifndef BETWEEN_STATE_H
#define BETWEEN_STATE_H

#include "slots.h"
#include "types.h"

#include "between_limits.h"

extern Slots g_slots;
/* stem of the last loaded/saved setup; empty if none. */
extern char g_setup_name[BETWEEN_NAME_LEN];

void state_init(void);

/* load module into g_module and reset slots (unless keep_slots). */
u8 state_load_module(const char *name, u8 keep_slots);

/* load/save preset stem into a slot. */
u8 state_load_preset(MorphSlot slot, const char *stem);
u8 state_save_preset(MorphSlot slot, const char *stem);

/* create new preset from slot values (or effective if from_eff). */
u8 state_new_preset(MorphSlot slot, const char *stem, u8 from_eff);

/* write a unique pNNN stem into out (not on disk or in any slot). */
u8 state_unique_preset_stem(char *out, u32 out_size);

u8 state_load_setup(const char *stem);
u8 state_save_setup(const char *stem);

/* persist last setup stem path for boot. */
u8 state_write_last_setup(const char *stem);
u8 state_read_last_setup(char *stem, u32 stem_size);

void state_apply(void);

#endif
