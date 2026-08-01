#ifndef BETWEEN_NAME_EDIT_H
#define BETWEEN_NAME_EDIT_H

#include "types.h"

typedef enum {
  eNameEditSetup = 0,
  eNameEditPreset
} NameEditKind;

typedef void (*name_edit_done_fn)(const char *stem, void *ctx);

/* open modal; captures enc/sw until ok/cancel. see modal.h for the generic
 * lifecycle (modal_active / modal_abort / modal_current). */
void name_edit_open(NameEditKind kind, const char *initial,
		    name_edit_done_fn on_ok, void *ctx);

#endif
