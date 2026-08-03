#include "pages.h"

#include "font.h"
#include "input_roles.h"
#include "render.h"
#include "xruns.h"

#ifndef MAJ
#define MAJ 0
#endif
#ifndef MIN
#define MIN 0
#endif
#ifndef REV
#define REV 0
#endif
#ifndef GIT_HASH
#define GIT_HASH ""
#endif

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

/* small unsigned decimal into dst; returns length. */
static u8 format_u8(char *dst, u8 dst_len, u8 val) {
  char reverse[3];
  u8 digits = 0;
  u8 i;

  if(dst_len == 0) {
    return 0;
  }
  do {
    reverse[digits++] = (char)('0' + (val % 10));
    val /= 10;
  } while(val && digits < 3);
  if(digits >= dst_len) {
    digits = (u8)(dst_len - 1);
  }
  for(i = 0; i < digits; ++i) {
    dst[i] = reverse[digits - 1 - i];
  }
  dst[digits] = '\0';
  return digits;
}

static void format_version(char *dst, u8 dst_len) {
  u8 i = 0;

  if(dst_len < 6) {
    if(dst_len > 0) {
      dst[0] = '\0';
    }
    return;
  }
  i = format_u8(dst, dst_len, (u8)MAJ);
  if(i + 1 < dst_len) {
    dst[i++] = '.';
  }
  i = (u8)(i + format_u8(dst + i, (u8)(dst_len - i), (u8)MIN));
  if(i + 1 < dst_len) {
    dst[i++] = '.';
  }
  (void)format_u8(dst + i, (u8)(dst_len - i), (u8)REV);
}

/* dark-grey label, white value — e.g. "version 0.1.0" */
static void info_line(u8 row, const char *label, const char *value) {
  char lab[14];
  const char *p;
  u16 i = 0;
  u8 x;
  u8 y;

  if(row >= RENDER_CONTENT_ROWS || label == NULL) {
    return;
  }
  /* walk via pointer: avr32-gcc -Warray-bounds false-positives on
   * label[i] when inlining a short string literal into a larger lab[]. */
  p = label;
  while(*p != '\0' && i + 2 < sizeof(lab)) {
    lab[i++] = *p++;
  }
  lab[i++] = ' ';
  lab[i] = '\0';
  y = (u8)(row * 8);
  render_string_xy(0, y, lab, RENDER_PLAY_GREY_DARK);
  x = font_string_pixels(lab);
  if(value != NULL && value[0] != '\0') {
    render_string_xy(x, y, value, 0xf);
  }
}

void redraw_info(void) {
  const bfin_xrun_t *xr = xruns_get();
  char ver[12];
  char num[8];
  const char *git = GIT_HASH;

  render_header("info", 0);
  render_clear();

  format_version(ver, sizeof(ver));
  info_line(0, "version", ver);
  info_line(1, "build", (git != NULL && git[0] != '\0') ? git : "-");

  format_u32(num, sizeof(num), (u32)xr->windowRx);
  info_line(2, "winRx", num);
  format_u32(num, sizeof(num), (u32)xr->windowTx);
  info_line(3, "winTx", num);
  format_u32(num, sizeof(num), (u32)xr->clashRx);
  info_line(4, "clashRx", num);
  format_u32(num, sizeof(num), (u32)xr->clashTx);
  info_line(5, "clashTx", num);

  render_footer("", "", "", "");
}

void select_info(void) {
  static const InputEncBinding enc[4] = {
    {eInputRoleUnmapped, NULL},
    {eInputRolePageSelect, NULL},
    {eInputRoleUnmapped, NULL},
    {eInputRoleUnmapped, NULL},
  };
  static const InputSwBinding sw[4] = {
    {eInputSwRoleUnmapped, NULL},
    {eInputSwRoleUnmapped, NULL},
    {eInputSwRoleUnmapped, NULL},
    {eInputSwRoleUnmapped, NULL},
  };
  input_roles_bind(enc, sw);
}

void page_info_init(void) {}
