#include "modal.h"

#include <stddef.h>

#include "encoders.h"
#include "events.h"

#include "render.h"

#define MODAL_ENC_N 4
#define MODAL_SW_N 4

static const Modal *g_modal = NULL;

static void (*saved_enc_fn[MODAL_ENC_N])(s32 data);
static void (*saved_sw_fn[MODAL_SW_N])(s32 data);
static s16 saved_enc_thresh[MODAL_ENC_N];

void modal_open(const Modal *m) {
  u8 i;

  if(m == NULL || m->redraw_fn == NULL || g_modal != NULL) {
    return;
  }
  for(i = 0; i < MODAL_ENC_N; ++i) {
    saved_enc_fn[i] = app_event_handlers[kEventEncoder0 + i];
    saved_enc_thresh[i] = enc[i].thresh;
  }
  for(i = 0; i < MODAL_SW_N; ++i) {
    saved_sw_fn[i] = app_event_handlers[kEventSwitch0 + i];
  }
  g_modal = m;
  render_mark_dirty();
}

void modal_close(void) {
  u8 i;

  if(g_modal == NULL) {
    return;
  }
  g_modal = NULL;
  for(i = 0; i < MODAL_ENC_N; ++i) {
    app_event_handlers[kEventEncoder0 + i] = saved_enc_fn[i];
    set_enc_thresh(i, (u8)saved_enc_thresh[i]);
  }
  for(i = 0; i < MODAL_SW_N; ++i) {
    app_event_handlers[kEventSwitch0 + i] = saved_sw_fn[i];
  }
  render_mark_dirty();
}

void modal_abort(void) {
  const Modal *m = g_modal;

  if(m == NULL) {
    return;
  }
  g_modal = NULL;
  if(m->abort_fn != NULL) {
    m->abort_fn();
  }
  render_mark_dirty();
}

u8 modal_active(void) { return (u8)(g_modal != NULL); }

const Modal *modal_current(void) { return g_modal; }
