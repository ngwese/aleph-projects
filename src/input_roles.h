/* input_roles — edit-mode encoder role bindings (thresh + handler) */

#ifndef BETWEEN_INPUT_ROLES_H
#define BETWEEN_INPUT_ROLES_H

#include "types.h"

typedef enum {
  eInputRoleUnmapped = 0,
  eInputRolePageSelect,
  eInputRoleListSelect,
  eInputRoleParamFine,
  eInputRoleParamCoarse
} InputRole;

typedef void (*input_role_fn)(s32 data);

typedef struct {
  InputRole role;
  /* page callback; NULL ok for Unmapped.
   * PageSelect: if fn non-NULL it is called (e.g. modal-aware); else pages_next. */
  input_role_fn fn;
} InputEncBinding;

/* Install handlers + thresholds for enc0..enc3. */
void input_roles_bind(const InputEncBinding enc[4]);

#endif
