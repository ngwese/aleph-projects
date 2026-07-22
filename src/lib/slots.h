/* slots — four corner preset banks and morph application */

#ifndef BETWEEN_SLOTS_H
#define BETWEEN_SLOTS_H

#include "between_limits.h"
#include "module_common.h"
#include "morph2d.h"
#include "param_common.h"
#include "setup_io.h"
#include "types.h"

typedef void (*slots_set_param_fn)(u16 index, ParamValue value, void *ctx);

/* discrete if type is bool or label; otherwise continuous. */
u8 slots_param_is_discrete(ParamType type);

typedef struct {
  u16 max_params;
  u16 num_params;
  ParamDesc *desc;                   /* caller-owned, length max_params */
  ParamValue *values[MORPH2D_SLOTS]; /* caller-owned banks */
  u8 occupied[MORPH2D_SLOTS];
  u8 dirty[MORPH2D_SLOTS];
  char stem[MORPH2D_SLOTS][SETUP_STEM_MAX];
  char module[MODULE_NAME_LEN];
  ModuleVersion version;
  u16 x;
  u16 y;
  /* send order for slots_apply only (descriptor index order elsewhere) */
  u16 apply_order[BETWEEN_PARAMS_MAX];
  u16 apply_order_len;
  /* 1 = skip in slots_apply (morph exclusion); owned/filled by caller */
  u8 exclude[BETWEEN_PARAMS_MAX];
  slots_set_param_fn set_param;
  void *set_param_ctx;
} Slots;

void slots_init(Slots *s, u16 max_params, ParamDesc *desc,
		ParamValue *banks[MORPH2D_SLOTS], slots_set_param_fn set_param,
		void *ctx);

void slots_clear_all(Slots *s);
void slots_clear_slot(Slots *s, MorphSlot slot);

void slots_set_module(Slots *s, const char *module, const ModuleVersion *ver);
void slots_set_num_params(Slots *s, u16 n);

/* fill all banks from defaults (e.g. after module load). */
void slots_fill_defaults(Slots *s, const ParamValue *defaults);

u8 slots_assign_stem(Slots *s, MorphSlot slot, const char *stem);
void slots_set_value(Slots *s, MorphSlot slot, u16 index, ParamValue v);
ParamValue slots_get_value(const Slots *s, MorphSlot slot, u16 index);

void slots_set_morph(Slots *s, u16 x, u16 y);
void slots_snap_to(Slots *s, MorphSlot slot);

/* recompute and send every non-excluded parameter for current morph point. */
void slots_apply(Slots *s);

/* send one parameter value to the module (play/MIDI path for excluded params). */
void slots_send_param(Slots *s, u16 index, ParamValue value);

/* bake current effective values into a slot bank. */
void slots_capture_effective(Slots *s, MorphSlot slot);

void slots_to_setup(const Slots *s, SetupData *out);
void slots_from_setup_meta(Slots *s, const SetupData *in);

#endif
