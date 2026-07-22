#ifndef BETWEEN_NAME_EDIT_H
#define BETWEEN_NAME_EDIT_H

#include "types.h"

typedef enum {
  eNameEditSetup = 0,
  eNameEditPreset
} NameEditKind;

typedef void (*name_edit_done_fn)(const char *stem, void *ctx);

/* open modal; installs enc/sw handlers until ok/cancel. */
void name_edit_open(NameEditKind kind, const char *initial,
		    name_edit_done_fn on_ok, void *ctx);
u8 name_edit_active(void);
/* close without ok/cancel callbacks; caller must rebind input handlers. */
void name_edit_abort(void);

#endif
