#include "xruns.h"

#include "pages.h"
#include "render.h"

/* must match dspPollTimer interval in app_timers.c */
#define XRUN_POLL_MS 100
#define XRUN_WARN_HOLD_MS 5000

static bfin_xrun_t g_xruns = {0, 0, 0, 0};
static u16 g_xrun_quiet_ms = 0;
static u8 g_xrun_warn = 0;

const bfin_xrun_t *xruns_get(void) {
  return &g_xruns;
}

u8 xruns_any(void) {
  return (g_xruns.windowRx | g_xruns.windowTx | g_xruns.clashRx |
	  g_xruns.clashTx) != 0;
}

/* true if any counter rose (wrap not treated as activity). */
static u8 xrun_increased(const bfin_xrun_t *cur, const bfin_xrun_t *prev) {
  return cur->windowRx > prev->windowRx || cur->windowTx > prev->windowTx ||
	 cur->clashRx > prev->clashRx || cur->clashTx > prev->clashTx;
}

static void xrun_set_warn(u8 on) {
  u8 next = on ? 1 : 0;
  if(g_xrun_warn == next) {
    return;
  }
  g_xrun_warn = next;
  render_xrun_set_warn(next);
}

u8 xruns_poll(void) {
  bfin_xrun_t cur;
  u8 changed;
  u8 increased;

  bfin_get_xruns(&cur);
  changed = (cur.windowRx != g_xruns.windowRx ||
	     cur.windowTx != g_xruns.windowTx ||
	     cur.clashRx != g_xruns.clashRx ||
	     cur.clashTx != g_xruns.clashTx);
  increased = xrun_increased(&cur, &g_xruns);
  g_xruns = cur;

  if(increased) {
    g_xrun_quiet_ms = 0;
    if(!g_xrun_warn) {
      xrun_set_warn(1);
      changed = 1;
    }
  } else if(g_xrun_warn) {
    if(g_xrun_quiet_ms < XRUN_WARN_HOLD_MS) {
      g_xrun_quiet_ms = (u16)(g_xrun_quiet_ms + XRUN_POLL_MS);
    }
    if(g_xrun_quiet_ms >= XRUN_WARN_HOLD_MS) {
      xrun_set_warn(0);
      g_xrun_quiet_ms = 0;
      changed = 1;
    }
  }

  if(changed && g_page_idx == ePageInfo) {
    render_mark_dirty();
  }
  return changed;
}

void xruns_clear_local(void) {
  g_xruns.windowRx = 0;
  g_xruns.windowTx = 0;
  g_xruns.clashRx = 0;
  g_xruns.clashTx = 0;
  g_xrun_quiet_ms = 0;
  if(g_xrun_warn) {
    xrun_set_warn(0);
  }
}
