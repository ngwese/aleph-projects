/* modal — single-slot global overlay that captures enc0-3 / sw0-3
 *
 * a modal draws over the current page and owns the four encoders and four
 * softkeys until it closes. only one can be open at a time. the registry
 * snapshots the input bindings on open so every modal does not have to
 * reimplement save/restore.
 */

#ifndef BETWEEN_MODAL_H
#define BETWEEN_MODAL_H

#include "types.h"

typedef struct {
  const char *name;
  /* rebuild modal content into the render regions (full redraw). */
  void (*redraw_fn)(void);
  /* drop internal state without restoring input; the caller is about to
   * install a new page bind. NULL ok. */
  void (*abort_fn)(void);
} Modal;

/* snapshot enc0-3 / sw0-3 handlers + thresholds, install m as the global
 * modal, mark the frame dirty. the modal installs its own bindings after
 * this returns. */
void modal_open(const Modal *m);
/* restore the snapshot, clear the global modal, mark the frame dirty. */
void modal_close(void);
/* clear without restoring input; calls abort_fn. */
void modal_abort(void);
u8 modal_active(void);
const Modal *modal_current(void);

#endif
