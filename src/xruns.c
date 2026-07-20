#include "xruns.h"

#include "pages.h"
#include "render.h"

static bfin_xrun_t g_xruns = {0, 0, 0, 0};

const bfin_xrun_t *xruns_get(void) {
  return &g_xruns;
}

u8 xruns_any(void) {
  return (g_xruns.windowRx | g_xruns.windowTx | g_xruns.clashRx |
	  g_xruns.clashTx) != 0;
}

u8 xruns_poll(void) {
  bfin_xrun_t cur;
  u8 was;
  u8 now;
  u8 changed;

  bfin_get_xruns(&cur);
  changed = (cur.windowRx != g_xruns.windowRx ||
	     cur.windowTx != g_xruns.windowTx ||
	     cur.clashRx != g_xruns.clashRx ||
	     cur.clashTx != g_xruns.clashTx);
  was = xruns_any();
  g_xruns = cur;
  now = xruns_any();
  if(was != now) {
    render_xrun_set_warn(now);
    /* title max-x depends on warn; redraw whatever page owns the header */
    pages_redraw();
    render_update();
    changed = 1;
  } else if(changed && g_page_idx == ePageInfo) {
    pages_redraw();
    render_update();
  }
  return changed;
}

void xruns_clear_local(void) {
  u8 was = xruns_any();
  g_xruns.windowRx = 0;
  g_xruns.windowTx = 0;
  g_xruns.clashRx = 0;
  g_xruns.clashTx = 0;
  if(was) {
    render_xrun_set_warn(0);
    pages_redraw();
    render_update();
  }
}
