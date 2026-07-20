#include "pages.h"

#include "app.h"
#include "events.h"
#include "render.h"
#include "xruns.h"

#ifndef VERSIONSTRING
#define VERSIONSTRING "0.0.0"
#endif
#ifndef GIT_HASH
#define GIT_HASH ""
#endif

static void handle_noop(s32 data) { (void)data; }

static void handle_enc1(s32 data) { pages_next(data > 0 ? 1 : -1); }

static void format_u32(char *buf, u8 buf_len, u32 val) {
  char reverse[10];
  u8 digits = 0;
  u8 i;

  if(buf_len < 5) {
    if(buf_len > 0) {
      buf[0] = '\0';
    }
    return;
  }
  if(val > 99999) {
    buf[0] = '>';
    buf[1] = '9';
    buf[2] = '9';
    buf[3] = 'k';
    buf[4] = '\0';
    return;
  }
  do {
    reverse[digits++] = (char)('0' + (val % 10));
    val /= 10;
  } while(val);
  for(i = 0; i < digits; ++i) {
    buf[i] = reverse[digits - 1 - i];
  }
  buf[digits] = '\0';
}

static void xrun_line(char *dst, u8 dst_len, const char *label, u32 val) {
  char num[8];
  u8 i = 0;
  u8 j;

  while(label[i] != '\0' && i + 1 < dst_len) {
    dst[i] = label[i];
    ++i;
  }
  if(i + 1 < dst_len) {
    dst[i++] = ' ';
  }
  format_u32(num, sizeof(num), val);
  j = 0;
  while(num[j] != '\0' && i + 1 < dst_len) {
    dst[i++] = num[j++];
  }
  dst[i] = '\0';
}

void redraw_info(void) {
  const bfin_xrun_t *xr = xruns_get();
  char line[22];
  u8 i = 0;
  const char *ver = VERSIONSTRING;
  const char *git = GIT_HASH;

  render_header("info", 0);
  render_clear();

  /* version + git hash */
  while(ver[i] != '\0' && i + 1 < (u8)sizeof(line)) {
    line[i] = ver[i];
    ++i;
  }
  if(git != NULL && git[0] != '\0' && i + 1 < (u8)sizeof(line)) {
    line[i++] = ' ';
    while(*git != '\0' && i + 1 < (u8)sizeof(line)) {
      line[i++] = *git++;
    }
  }
  line[i] = '\0';
  render_line(0, line);

  xrun_line(line, sizeof(line), "winRx", xr->windowRx);
  render_line(1, line);
  xrun_line(line, sizeof(line), "winTx", xr->windowTx);
  render_line(2, line);
  xrun_line(line, sizeof(line), "clashRx", xr->clashRx);
  render_line(3, line);
  xrun_line(line, sizeof(line), "clashTx", xr->clashTx);
  render_line(4, line);

  render_footer("", "", "", "");
}

void select_info(void) {
  app_event_handlers[kEventEncoder0] = handle_noop;
  app_event_handlers[kEventEncoder1] = handle_enc1;
  app_event_handlers[kEventEncoder2] = handle_enc1;
  app_event_handlers[kEventEncoder3] = handle_enc1;
  app_event_handlers[kEventSwitch0] = handle_noop;
  app_event_handlers[kEventSwitch1] = handle_noop;
  app_event_handlers[kEventSwitch2] = handle_noop;
  app_event_handlers[kEventSwitch3] = handle_noop;
}

void page_info_init(void) {}
