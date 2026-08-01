#include "state.h"

#include <string.h>

#include "app.h"
#include "preset_io.h"

#include "bfin.h"

#include "files_ensure.h"
#include "cv_in.h"
#include "module_load.h"
#include "play_maps.h"
#include "preset_file.h"
#include "render.h"
#include "setup_file.h"

static ParamValue banks[MORPH2D_SLOTS][BETWEEN_PARAMS_MAX];
static u8 g_exclude_manual[BETWEEN_PARAMS_MAX];
static u8 g_exclude_bound[BETWEEN_PARAMS_MAX];
Slots g_slots;
PlayMaps g_play_maps;
char g_setup_name[BETWEEN_NAME_LEN];
static u8 g_setup_dirty;

void state_setup_mark_dirty(void) { g_setup_dirty = 1; }

void state_setup_clear_dirty(void) { g_setup_dirty = 0; }

u8 state_setup_dirty(void) {
  MorphSlot i;
  if(g_setup_dirty) {
    return 1;
  }
  for(i = 0; i < MORPH2D_SLOTS; ++i) {
    if(g_slots.occupied[i] && g_slots.dirty[i]) {
      return 1;
    }
  }
  return 0;
}

void state_rename_setup(const char *stem) {
  if(stem == NULL || stem[0] == '\0') {
    return;
  }
  if(strcmp(g_setup_name, stem) == 0) {
    return;
  }
  strncpy(g_setup_name, stem, BETWEEN_NAME_LEN - 1);
  g_setup_name[BETWEEN_NAME_LEN - 1] = '\0';
  g_setup_dirty = 1;
}

void state_rename_preset(MorphSlot slot, const char *stem) {
  if(slot >= MORPH2D_SLOTS || !g_slots.occupied[slot]) {
    return;
  }
  if(stem == NULL || stem[0] == '\0') {
    return;
  }
  if(strcmp(g_slots.stem[slot], stem) == 0) {
    return;
  }
  strncpy(g_slots.stem[slot], stem, SETUP_STEM_MAX - 1);
  g_slots.stem[slot][SETUP_STEM_MAX - 1] = '\0';
  g_slots.dirty[slot] = 1;
  g_setup_dirty = 1;
}

void state_set_morph(u16 x, u16 y) {
  u16 ox = g_slots.x;
  u16 oy = g_slots.y;
  slots_set_morph(&g_slots, x, y);
  if(g_slots.x != ox || g_slots.y != oy) {
    g_setup_dirty = 1;
  }
}

void state_snap_to(MorphSlot slot) {
  u16 ox = g_slots.x;
  u16 oy = g_slots.y;
  slots_snap_to(&g_slots, slot);
  if(g_slots.x != ox || g_slots.y != oy) {
    g_setup_dirty = 1;
  }
}

static void set_param_cb(u16 index, ParamValue value, void *ctx) {
  (void)ctx;
  bfin_set_param((u8)index, value);
}

void state_exclude_rebuild(void) {
  u16 i;
  u16 n = g_slots.num_params;
  if(n > BETWEEN_PARAMS_MAX) {
    n = BETWEEN_PARAMS_MAX;
  }
  play_maps_fill_bound(&g_play_maps, g_module.desc, n, g_exclude_bound);
  for(i = 0; i < BETWEEN_PARAMS_MAX; ++i) {
    u8 man = (i < n) ? g_exclude_manual[i] : 0;
    u8 bound = (i < n) ? g_exclude_bound[i] : 0;
    g_slots.exclude[i] = (u8)(man | bound);
  }
}

void state_exclude_clear(void) {
  memset(g_exclude_manual, 0, sizeof(g_exclude_manual));
  memset(g_exclude_bound, 0, sizeof(g_exclude_bound));
  memset(g_slots.exclude, 0, sizeof(g_slots.exclude));
}

u8 state_param_excluded(u16 idx) {
  if(idx >= g_slots.num_params || idx >= BETWEEN_PARAMS_MAX) {
    return 0;
  }
  return g_slots.exclude[idx];
}

u8 state_param_play_bound(u16 idx) {
  if(idx >= g_slots.num_params || idx >= BETWEEN_PARAMS_MAX) {
    return 0;
  }
  return g_exclude_bound[idx];
}

u8 state_exclude_manual_set(u16 idx, u8 on) {
  if(idx >= g_slots.num_params || idx >= BETWEEN_PARAMS_MAX) {
    return 0;
  }
  if(!on && g_exclude_bound[idx]) {
    /* play-bound force stays; cannot clear */
    return 0;
  }
  {
    u8 next = on ? 1 : 0;
    if(g_exclude_manual[idx] != next) {
      g_exclude_manual[idx] = next;
      g_setup_dirty = 1;
    }
  }
  g_slots.exclude[idx] = (u8)(g_exclude_manual[idx] | g_exclude_bound[idx]);
  return 1;
}

void state_exclude_manual_from_list(const char *list) {
  char tmp[SETUP_IO_LINE_MAX];
  char *p;
  char *tok;
  u16 i;

  memset(g_exclude_manual, 0, sizeof(g_exclude_manual));
  if(list == NULL || list[0] == '\0') {
    state_exclude_rebuild();
    return;
  }
  strncpy(tmp, list, sizeof(tmp) - 1);
  tmp[sizeof(tmp) - 1] = '\0';
  p = tmp;
  while(p != NULL && *p != '\0') {
    while(*p == ' ' || *p == '\t') {
      p++;
    }
    if(*p == '\0') {
      break;
    }
    tok = p;
    while(*p != '\0' && *p != ',') {
      p++;
    }
    if(*p == ',') {
      *p = '\0';
      p++;
    } else {
      p = NULL;
    }
    /* trim trailing space on tok */
    {
      char *e = tok + strlen(tok);
      while(e > tok && (e[-1] == ' ' || e[-1] == '\t')) {
	*--e = '\0';
      }
    }
    for(i = 0; i < g_module.num_params && i < BETWEEN_PARAMS_MAX; ++i) {
      if(strncmp(g_module.desc[i].label, tok, PARAM_LABEL_LEN) == 0) {
	g_exclude_manual[i] = 1;
	break;
      }
    }
  }
  state_exclude_rebuild();
}

void state_exclude_manual_to_list(char *buf, u32 buf_size) {
  u16 i;
  u8 first = 1;
  u32 len = 0;
  if(buf == NULL || buf_size == 0) {
    return;
  }
  buf[0] = '\0';
  for(i = 0; i < g_module.num_params && i < BETWEEN_PARAMS_MAX; ++i) {
    const char *lab;
    u32 lab_len;
    if(!g_exclude_manual[i]) {
      continue;
    }
    lab = g_module.desc[i].label;
    lab_len = (u32)strlen(lab);
    if(len + lab_len + (first ? 0u : 2u) + 1u > buf_size) {
      break;
    }
    if(!first) {
      buf[len++] = ',';
      buf[len++] = ' ';
      buf[len] = '\0';
    }
    memcpy(buf + len, lab, lab_len);
    len += lab_len;
    buf[len] = '\0';
    first = 0;
  }
}

void state_send_param(u16 idx, ParamValue value) {
  slots_send_param(&g_slots, idx, value);
}

void state_send_excluded(void) {
  u16 i;
  MorphSlot s;
  MorphSlot src;
  u8 have;

  if(!g_module.loaded) {
    return;
  }
  for(i = 0; i < g_slots.num_params && i < BETWEEN_PARAMS_MAX; ++i) {
    if(!g_slots.exclude[i]) {
      continue;
    }
    have = 0;
    src = eMorphSlotA;
    for(s = 0; s < MORPH2D_SLOTS; ++s) {
      if(g_slots.occupied[s]) {
	src = s;
	have = 1;
	break;
      }
    }
    if(!have) {
      continue;
    }
    /* prefer a ParamSlot binding's slot when this label is play-mapped */
    for(s = 0; s < PLAY_MAPS_ENC_COUNT; ++s) {
      const PlayEncMap *m = &g_play_maps.enc[s];
      if(m->kind == ePlayEncParamSlot &&
	 strncmp(m->label, g_module.desc[i].label, PARAM_LABEL_LEN) == 0 &&
	 g_slots.occupied[m->slot]) {
	src = m->slot;
	break;
      }
    }
    slots_send_param(&g_slots, i, slots_get_value(&g_slots, src, i));
  }
}

void state_init(void) {
  ParamValue *ptrs[MORPH2D_SLOTS] = {banks[0], banks[1], banks[2], banks[3]};
  slots_init(&g_slots, BETWEEN_PARAMS_MAX, g_module.desc, ptrs, set_param_cb,
	     NULL);
  play_maps_set_defaults(&g_play_maps);
  state_exclude_clear();
  g_setup_name[0] = '\0';
  g_setup_dirty = 0;
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
    state_exclude_clear();
  }
  play_maps_clear_invalid(&g_play_maps, g_module.desc, g_module.num_params);
  state_exclude_rebuild();
  g_setup_dirty = 1;
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
  u8 stem_changed;

  if(!g_module.loaded || stem == NULL) {
    return 0;
  }
  stem_changed =
    !g_slots.occupied[slot] || strcmp(g_slots.stem[slot], stem) != 0;
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
  if(stem_changed) {
    g_setup_dirty = 1;
  }
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
  g_setup_dirty = 1;
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
  u8 fail_mask;
  char msg[22];
  u8 n;

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
  fail_mask = 0;
  for(i = 0; i < MORPH2D_SLOTS; ++i) {
    if(data.slot_occupied[i]) {
      if(!state_load_preset(i, data.slot_stem[i])) {
	slots_clear_slot(&g_slots, i);
	fail_mask |= (u8)(1u << i);
      }
    }
  }
  slots_set_morph(&g_slots, data.x, data.y);
  g_play_maps = data.maps;
  play_maps_clear_invalid(&g_play_maps, g_module.desc, g_module.num_params);
  state_exclude_manual_from_list(data.morph_exclude);
  cv_in_sync_poll();
  state_apply();
  /* play-bound / manual excludes were skipped by apply — seed DSP now */
  state_send_excluded();
  strncpy(g_setup_name, stem, BETWEEN_NAME_LEN - 1);
  g_setup_name[BETWEEN_NAME_LEN - 1] = '\0';
  state_write_last_setup(stem);
  g_setup_dirty = 0;
  if(fail_mask != 0) {
    /* e.g. "fail a,c" — caller should not auto-enter play */
    n = 0;
    msg[n++] = 'f';
    msg[n++] = 'a';
    msg[n++] = 'i';
    msg[n++] = 'l';
    msg[n++] = ' ';
    for(i = 0; i < MORPH2D_SLOTS; ++i) {
      if((fail_mask & (u8)(1u << i)) == 0) {
	continue;
      }
      if(n > 5) {
	msg[n++] = ',';
      }
      msg[n++] = (char)('a' + i);
    }
    msg[n] = '\0';
    render_log(msg);
    return 0;
  }
  return 1;
}

static void log_writing_file(const char *stem) {
  char msg[22];

  msg[0] = '\0';
  if(stem != NULL && stem[0] != '\0') {
    strncpy(msg, stem, sizeof(msg) - 5);
    msg[sizeof(msg) - 5] = '\0';
  }
  strcat(msg, ".txt");
  render_log(msg);
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
  /* Hold pause across every file so fat_io_lib is not interleaved with UI /
   * timers between preset/setup/state writes (single shared sector cache). */
  app_pause();
  /* persist any modified slot presets before writing the setup */
  for(i = 0; i < MORPH2D_SLOTS; ++i) {
    if(g_slots.occupied[i] && g_slots.dirty[i] && g_slots.stem[i][0] != '\0') {
      log_writing_file(g_slots.stem[i]);
      if(!state_save_preset(i, g_slots.stem[i])) {
	app_resume();
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
  state_exclude_manual_to_list(data.morph_exclude, sizeof(data.morph_exclude));
  log_writing_file(stem);
  if(setup_file_save(stem, &data) != eSetupIoOk) {
    app_resume();
    return 0;
  }
  strncpy(g_setup_name, stem, BETWEEN_NAME_LEN - 1);
  g_setup_name[BETWEEN_NAME_LEN - 1] = '\0';
  if(!state_write_last_setup(stem)) {
    app_resume();
    return 0;
  }
  g_setup_dirty = 0;
  app_resume();
  return 1;
}

u8 state_write_last_setup(const char *stem) {
  return setup_file_write_state(stem);
}

u8 state_read_last_setup(char *stem, u32 stem_size) {
  return setup_file_read_state(stem, stem_size);
}

void state_apply(void) { slots_apply(&g_slots); }
