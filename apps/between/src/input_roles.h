/* input_roles — edit-mode encoder/switch role bindings */

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

typedef enum {
  eInputSwRoleUnmapped = 0,
  eInputSwRoleAction, /* softkey; page fn gets press/release */
  eInputSwRoleAlt     /* sets g_alt_mode; optional fn for redraw */
} InputSwRole;

typedef void (*input_role_fn)(s32 data);

typedef struct {
  InputRole role;
  /* page callback; NULL ok for Unmapped.
   * PageSelect: if fn non-NULL it is called (e.g. modal-aware); else
   * pages_next. */
  input_role_fn fn;
} InputEncBinding;

typedef struct {
  InputSwRole role;
  /* Action: page softkey handler. Alt: optional after-set (usually redraw).
   * NULL ok for Unmapped / Alt-without-redraw. */
  input_role_fn fn;
} InputSwBinding;

/* Install enc0..enc3 and sw0..sw3. Live play does not use this. */
void input_roles_bind(const InputEncBinding enc[4], const InputSwBinding sw[4]);

#endif
