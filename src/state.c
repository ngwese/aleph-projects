#include "state.h"

#include <string.h>

#include "preset_io.h"

#include "bfin.h"

#include "files_ensure.h"
#include "module_load.h"
#include "play_maps.h"
#include "preset_file.h"
#include "render.h"
#include "setup_file.h"

static ParamValue banks[MORPH2D_SLOTS][BETWEEN_PARAMS_MAX];
Slots g_slots;
PlayMaps g_play_maps;
char g_setup_name[BETWEEN_NAME_LEN];

static void set_param_cb(u16 index, ParamValue value, void *ctx) {
  (void)ctx;
  bfin_set_param((u8)index, value);
}

void state_init(void) {
  ParamValue *ptrs[MORPH2D_SLOTS] = {banks[0], banks[1], banks[2], banks[3]};
  slots_init(&g_slots, BETWEEN_PARAMS_MAX, g_module.desc, ptrs, set_param_cb,
	     NULL);
  play_maps_set_defaults(&g_play_maps);
  g_setup_name[0] = '\0';
}

u8 state_load_module(const char *name, u8 keep_slots) {
  if(!module_load(name)) {
    return 0;
  }
  files_ensure_preset_module_dir(g_module.name);
  slots_set_module(&g_slots, g_module.name, &g_module.version);
  slots_set_num_params(&g_slots, g_module.num_params);
  /* re-bind desc pointer after module_load refreshed g_module */
  g_slots.desc = g_module.desc;
  slots_fill_defaults(&g_slots, g_module.defaults);
  if(!keep_slots) {
    slots_clear_all(&g_slots);
    slots_set_morph(&g_slots, 0, 0);
  }
  play_maps_clear_invalid(&g_play_maps, g_module.desc, g_module.num_params);
  return 1;
}

typedef struct {
  MorphSlot slot;
} PresetLoadCtx;

static u8 on_preset_param(const char *label, s32 value, void *ctx) {
  PresetLoadCtx *c = (PresetLoadCtx *)ctx;
  s16 idx = module_find_param(label);
  if(idx < 0) {
    return 1; /* ignore unknown */
  }
  if(g_slots.values[c->slot] != NULL) {
    g_slots.values[c->slot][idx] = (ParamValue)value;
  }
  return 1;
}

u8 state_load_preset(MorphSlot slot, const char *stem) {
  PresetMeta meta;
  PresetLoadCtx ctx;
  u16 i;

  if(!g_module.loaded || stem == NULL) {
    return 0;
  }
  ctx.slot = slot;
  /* start from defaults so missing params stay at module default */
  for(i = 0; i < g_module.num_params; ++i) {
    banks[slot][i] = g_module.defaults[i];
  }
  if(preset_file_load(g_module.name, stem, &meta, on_preset_param, &ctx) !=
     ePresetIoOk) {
    return 0;
  }
  if(strcmp(meta.module, g_module.name) != 0) {
    slots_clear_slot(&g_slots, slot);
    return 0;
  }
  slots_assign_stem(&g_slots, slot, stem);
  g_slots.dirty[slot] = 0;
  state_apply();
  return 1;
}

typedef struct {
  MorphSlot slot;
  u16 idx;
} PresetSaveCtx;

static u8 next_preset_param(const char **label, s32 *value, void *ctx) {
  PresetSaveCtx *c = (PresetSaveCtx *)ctx;
  if(c->idx >= g_module.num_params) {
    return 0;
  }
  *label = g_module.desc[c->idx].label;
  *value = banks[c->slot][c->idx];
  c->idx++;
  return 1;
}

u8 state_save_preset(MorphSlot slot, const char *stem) {
  PresetMeta meta;
  PresetSaveCtx ctx;

  if(!g_module.loaded || !g_slots.occupied[slot] || stem == NULL) {
    return 0;
  }
  if(!files_ensure_preset_module_dir(g_module.name)) {
    return 0;
  }
  memset(&meta, 0, sizeof(meta));
  meta.format = PRESET_IO_FORMAT;
  strncpy(meta.module, g_module.name, MODULE_NAME_LEN - 1);
  meta.version = g_module.version;
  ctx.slot = slot;
  ctx.idx = 0;
  if(preset_file_save(g_module.name, stem, &meta, next_preset_param, &ctx) !=
     ePresetIoOk) {
    return 0;
  }
  slots_assign_stem(&g_slots, slot, stem);
  g_slots.dirty[slot] = 0;
  return 1;
}

u8 state_new_preset(MorphSlot slot, const char *stem) {
  u16 i;

  if(!g_module.loaded || stem == NULL || stem[0] == '\0') {
    return 0;
  }
  for(i = 0; i < g_module.num_params; ++i) {
    banks[slot][i] = g_module.defaults[i];
  }
  g_slots.occupied[slot] = 1;
  if(!state_save_preset(slot, stem)) {
    return 0;
  }
  state_apply();
  return 1;
}

static u8 stem_in_memory(const char *stem) {
  MorphSlot i;
  for(i = 0; i < MORPH2D_SLOTS; ++i) {
    if(g_slots.occupied[i] && g_slots.stem[i][0] != '\0' &&
       strcmp(g_slots.stem[i], stem) == 0) {
      return 1;
    }
  }
  return 0;
}

u8 state_unique_preset_stem(char *out, u32 out_size) {
  u16 n;
  char cand[BETWEEN_NAME_LEN];

  if(out == NULL || out_size < 5 || !g_module.loaded) {
    return 0;
  }

  for(n = 0; n < 1000; ++n) {
    cand[0] = 'p';
    cand[1] = (char)('0' + (n / 100) % 10);
    cand[2] = (char)('0' + (n / 10) % 10);
    cand[3] = (char)('0' + (n % 10));
    cand[4] = '\0';
    if(stem_in_memory(cand)) {
      continue;
    }
    if(preset_file_exists(g_module.name, cand)) {
      continue;
    }
    strncpy(out, cand, out_size - 1);
    out[out_size - 1] = '\0';
    return 1;
  }
  return 0;
}

u8 state_unique_setup_stem(char *out, u32 out_size) {
  u16 n;
  char cand[BETWEEN_NAME_LEN];

  if(out == NULL || out_size < 5) {
    return 0;
  }

  for(n = 0; n < 1000; ++n) {
    cand[0] = 's';
    cand[1] = (char)('0' + (n / 100) % 10);
    cand[2] = (char)('0' + (n / 10) % 10);
    cand[3] = (char)('0' + (n % 10));
    cand[4] = '\0';
    if(setup_file_exists(cand)) {
      continue;
    }
    strncpy(out, cand, out_size - 1);
    out[out_size - 1] = '\0';
    return 1;
  }
  return 0;
}

u8 state_load_setup(const char *stem) {
  SetupData data;
  SetupIoStatus st;
  MorphSlot i;

  st = setup_file_load(stem, &data);
  if(st != eSetupIoOk) {
    if(st == eSetupIoMissingMeta) {
      render_log("setup meta");
    } else if(st == eSetupIoBadFormat) {
      render_log("setup fmt");
    } else {
      render_log("setup io");
    }
    return 0;
  }
  if(data.module[0] == '\0') {
    render_log("no module");
    return 0;
  }
  if(!g_module.loaded || strcmp(data.module, g_module.name) != 0) {
    if(!state_load_module(data.module, 0)) {
      render_log("mod fail");
      return 0;
    }
  }
  slots_from_setup_meta(&g_slots, &data);
  for(i = 0; i < MORPH2D_SLOTS; ++i) {
    if(data.slot_occupied[i]) {
      if(!state_load_preset(i, data.slot_stem[i])) {
	slots_clear_slot(&g_slots, i);
      }
    }
  }
  slots_set_morph(&g_slots, data.x, data.y);
  g_play_maps = data.maps;
  play_maps_clear_invalid(&g_play_maps, g_module.desc, g_module.num_params);
  state_apply();
  strncpy(g_setup_name, stem, BETWEEN_NAME_LEN - 1);
  g_setup_name[BETWEEN_NAME_LEN - 1] = '\0';
  state_write_last_setup(stem);
  return 1;
}

u8 state_save_setup(const char *stem) {
  SetupData data;
  MorphSlot i;

  if(!g_module.loaded) {
    render_log("no module");
    return 0;
  }
  if(!files_ensure_data_dirs()) {
    return 0;
  }
  /* persist any modified slot presets before writing the setup */
  for(i = 0; i < MORPH2D_SLOTS; ++i) {
    if(g_slots.occupied[i] && g_slots.dirty[i] && g_slots.stem[i][0] != '\0') {
      if(!state_save_preset(i, g_slots.stem[i])) {
	return 0;
      }
    }
  }
  slots_to_setup(&g_slots, &data);
  /* always stamp the currently loaded module (source of truth) */
  strncpy(data.module, g_module.name, MODULE_NAME_LEN - 1);
  data.module[MODULE_NAME_LEN - 1] = '\0';
  data.version = g_module.version;
  data.maps = g_play_maps;
  if(setup_file_save(stem, &data) != eSetupIoOk) {
    return 0;
  }
  strncpy(g_setup_name, stem, BETWEEN_NAME_LEN - 1);
  g_setup_name[BETWEEN_NAME_LEN - 1] = '\0';
  state_write_last_setup(stem);
  return 1;
}

u8 state_write_last_setup(const char *stem) {
  return setup_file_write_state(stem);
}

u8 state_read_last_setup(char *stem, u32 stem_size) {
  return setup_file_read_state(stem, stem_size);
}

void state_apply(void) { slots_apply(&g_slots); }
