#ifndef BETWEEN_MODULE_LOAD_H
#define BETWEEN_MODULE_LOAD_H

#include "module_common.h"
#include "param_common.h"
#include "types.h"

#include "between_limits.h"

typedef struct {
  char name[MODULE_NAME_LEN];
  ModuleVersion version;
  u16 num_params;
  ParamDesc desc[BETWEEN_PARAMS_MAX];
  ParamValue defaults[BETWEEN_PARAMS_MAX];
  u8 loaded;
} ModuleState;

extern ModuleState g_module;

/* load .ldr + .dsc from /mod/, fill g_module, query defaults. return 1 ok. */
u8 module_load(const char *name);

/* lookup param index by label; -1 if missing. */
s16 module_find_param(const char *label);

#endif
