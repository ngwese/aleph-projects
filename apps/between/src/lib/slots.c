#include "slots.h"

#include <string.h>

/* Earlier entries are sent first. Types omitted here are appended
 * afterward in ascending parameter index order (stable). */
static const ParamType k_slots_apply_type_order[] = {
  eParamTypeIntegrator,
  eParamTypeIntegratorShort,
};

static void slots_rebuild_apply_order(Slots *s) {
  u16 n;
  u16 i;
  u16 t;
  u16 out;
  u8 placed[BETWEEN_PARAMS_MAX];

  if(s == NULL) {
    return;
  }
  n = s->num_params;
  if(n > BETWEEN_PARAMS_MAX) {
    n = BETWEEN_PARAMS_MAX;
  }
  if(n == 0 || s->desc == NULL) {
    for(i = 0; i < n; ++i) {
      s->apply_order[i] = i;
    }
    s->apply_order_len = n;
    return;
  }

  memset(placed, 0, sizeof(placed));
  out = 0;

  for(t = 0; t < (u16)(sizeof(k_slots_apply_type_order) /
                       sizeof(k_slots_apply_type_order[0]));
      ++t) {
    ParamType want = k_slots_apply_type_order[t];
    for(i = 0; i < n; ++i) {
      if(!placed[i] && s->desc[i].type == want) {
        s->apply_order[out++] = i;
        placed[i] = 1;
      }
    }
  }

  for(i = 0; i < n; ++i) {
    if(!placed[i]) {
      s->apply_order[out++] = i;
      placed[i] = 1;
    }
  }

  s->apply_order_len = out;
}

u8 slots_param_is_discrete(ParamType type) {
  return (u8)(type == eParamTypeBool || type == eParamTypeLabel);
}

void slots_init(Slots *s, u16 max_params, ParamDesc *desc,
                ParamValue *banks[MORPH2D_SLOTS], slots_set_param_fn set_param,
                void *ctx) {
  u32 i;
  if(s == NULL) {
    return;
  }
  memset(s, 0, sizeof(*s));
  s->max_params = max_params;
  s->desc = desc;
  s->set_param = set_param;
  s->set_param_ctx = ctx;
  for(i = 0; i < MORPH2D_SLOTS; ++i) {
    s->values[i] = banks != NULL ? banks[i] : NULL;
  }
}

void slots_clear_all(Slots *s) {
  u32 i;
  if(s == NULL) {
    return;
  }
  for(i = 0; i < MORPH2D_SLOTS; ++i) {
    slots_clear_slot(s, (MorphSlot)i);
  }
  s->x = 0;
  s->y = 0;
}

void slots_clear_slot(Slots *s, MorphSlot slot) {
  if(s == NULL || slot >= MORPH2D_SLOTS) {
    return;
  }
  s->occupied[slot] = 0;
  s->dirty[slot] = 0;
  s->stem[slot][0] = '\0';
}

void slots_set_module(Slots *s, const char *module, const ModuleVersion *ver) {
  if(s == NULL) {
    return;
  }
  if(module != NULL) {
    strncpy(s->module, module, MODULE_NAME_LEN - 1);
    s->module[MODULE_NAME_LEN - 1] = '\0';
  } else {
    s->module[0] = '\0';
  }
  if(ver != NULL) {
    s->version = *ver;
  } else {
    memset(&s->version, 0, sizeof(s->version));
  }
}

void slots_set_num_params(Slots *s, u16 n) {
  if(s == NULL) {
    return;
  }
  if(n > s->max_params) {
    n = s->max_params;
  }
  if(n > BETWEEN_PARAMS_MAX) {
    n = BETWEEN_PARAMS_MAX;
  }
  s->num_params = n;
  slots_rebuild_apply_order(s);
}

void slots_fill_defaults(Slots *s, const ParamValue *defaults) {
  u32 i;
  u32 p;
  if(s == NULL || defaults == NULL) {
    return;
  }
  for(i = 0; i < MORPH2D_SLOTS; ++i) {
    if(s->values[i] == NULL) {
      continue;
    }
    for(p = 0; p < s->num_params; ++p) {
      s->values[i][p] = defaults[p];
    }
  }
}

u8 slots_assign_stem(Slots *s, MorphSlot slot, const char *stem) {
  if(s == NULL || slot >= MORPH2D_SLOTS) {
    return 0;
  }
  if(stem == NULL || stem[0] == '\0') {
    slots_clear_slot(s, slot);
    return 1;
  }
  strncpy(s->stem[slot], stem, SETUP_STEM_MAX - 1);
  s->stem[slot][SETUP_STEM_MAX - 1] = '\0';
  s->occupied[slot] = 1;
  s->dirty[slot] = 0;
  return 1;
}

void slots_set_value(Slots *s, MorphSlot slot, u16 index, ParamValue v) {
  if(s == NULL || slot >= MORPH2D_SLOTS || index >= s->num_params) {
    return;
  }
  if(s->values[slot] == NULL || !s->occupied[slot]) {
    return;
  }
  s->values[slot][index] = v;
  s->dirty[slot] = 1;
}

ParamValue slots_get_value(const Slots *s, MorphSlot slot, u16 index) {
  if(s == NULL || slot >= MORPH2D_SLOTS || index >= s->num_params) {
    return 0;
  }
  if(s->values[slot] == NULL) {
    return 0;
  }
  return s->values[slot][index];
}

void slots_set_morph(Slots *s, u16 x, u16 y) {
  if(s == NULL) {
    return;
  }
  morph2d_clamp(&x, &y);
  s->x = x;
  s->y = y;
}

void slots_snap_to(Slots *s, MorphSlot slot) {
  u16 x = 0;
  u16 y = 0;
  if(s == NULL) {
    return;
  }
  morph2d_slot_corner(slot, &x, &y);
  slots_set_morph(s, x, y);
}

static ParamValue effective_at(const Slots *s, u16 index, const u16 w[MORPH2D_SLOTS]) {
  s32 v[MORPH2D_SLOTS];
  u32 i;
  ParamType type = eParamTypeFix;

  for(i = 0; i < MORPH2D_SLOTS; ++i) {
    v[i] = s->occupied[i] && s->values[i] != NULL ? s->values[i][index] : 0;
  }

  if(s->desc != NULL && index < s->num_params) {
    type = s->desc[index].type;
  }

  if(slots_param_is_discrete(type)) {
    s8 pick = morph2d_pick_discrete(w, s->occupied);
    if(pick < 0) {
      return 0;
    }
    return v[pick];
  }
  return morph2d_blend_s32(w, v);
}

void slots_apply(Slots *s) {
  u16 w[MORPH2D_SLOTS];
  u16 i;
  u8 any = 0;

  if(s == NULL || s->set_param == NULL) {
    return;
  }
  for(i = 0; i < MORPH2D_SLOTS; ++i) {
    if(s->occupied[i]) {
      any = 1;
      break;
    }
  }
  if(!any) {
    return;
  }

  morph2d_weights(s->x, s->y, s->occupied, w);
  for(i = 0; i < s->apply_order_len; ++i) {
    u16 idx = s->apply_order[i];
    if(idx < BETWEEN_PARAMS_MAX && s->exclude[idx]) {
      continue;
    }
    s->set_param(idx, effective_at(s, idx, w), s->set_param_ctx);
  }
}

void slots_send_param(Slots *s, u16 index, ParamValue value) {
  if(s == NULL || s->set_param == NULL || index >= s->num_params) {
    return;
  }
  s->set_param(index, value, s->set_param_ctx);
}

void slots_capture_effective(Slots *s, MorphSlot slot) {
  u16 w[MORPH2D_SLOTS];
  u16 i;
  if(s == NULL || slot >= MORPH2D_SLOTS || s->values[slot] == NULL) {
    return;
  }
  morph2d_weights(s->x, s->y, s->occupied, w);
  for(i = 0; i < s->num_params; ++i) {
    s->values[slot][i] = effective_at(s, i, w);
  }
  s->occupied[slot] = 1;
  s->dirty[slot] = 1;
}

void slots_to_setup(const Slots *s, SetupData *out) {
  u32 i;
  if(s == NULL || out == NULL) {
    return;
  }
  memset(out, 0, sizeof(*out));
  out->format = SETUP_IO_FORMAT;
  strncpy(out->module, s->module, MODULE_NAME_LEN - 1);
  out->version = s->version;
  out->x = s->x;
  out->y = s->y;
  for(i = 0; i < MORPH2D_SLOTS; ++i) {
    out->slot_occupied[i] = s->occupied[i];
    if(s->occupied[i]) {
      strncpy(out->slot_stem[i], s->stem[i], SETUP_STEM_MAX - 1);
    }
  }
}

void slots_from_setup_meta(Slots *s, const SetupData *in) {
  u32 i;
  if(s == NULL || in == NULL) {
    return;
  }
  slots_set_module(s, in->module, &in->version);
  slots_set_morph(s, in->x, in->y);
  for(i = 0; i < MORPH2D_SLOTS; ++i) {
    if(in->slot_occupied[i]) {
      slots_assign_stem(s, (MorphSlot)i, in->slot_stem[i]);
    } else {
      slots_clear_slot(s, (MorphSlot)i);
    }
  }
}
