#include "pages.h"

#include <stddef.h>

#include "cv_in.h"
#include "input_roles.h"
#include "play_maps.h"
#include "render.h"

#define INSPECT_SUB_IO 0
#define INSPECT_SUB_CV 1
#define INSPECT_SUB_N 2

/* "cvN " (4) + " 0.00" (5) + 2px gap → spark from x=56 */
#define INSPECT_SPARK_X 56
#define INSPECT_SPARK_N (128 - INSPECT_SPARK_X)

static u8 sub;
static u8 hist[PLAY_MAPS_CV_COUNT][INSPECT_SPARK_N];
static u8 hist_pos[PLAY_MAPS_CV_COUNT];
static u8 hist_len[PLAY_MAPS_CV_COUNT];

static void fmt_volts(char *dst, fract32 fr) {
  u32 h;
  u32 whole;
  u32 frac;

  if(fr <= 0) {
    dst[0] = ' ';
    dst[1] = '0';
    dst[2] = '.';
    dst[3] = '0';
    dst[4] = '0';
    dst[5] = '\0';
    return;
  }
  /* hundredths of a volt: 0..1000 → 0.00..10.00 */
  h = ((u32)fr * 1000u) / 0x7fffffffu;
  if(h > 1000u) {
    h = 1000u;
  }
  if(h >= 1000u) {
    dst[0] = '1';
    dst[1] = '0';
    dst[2] = '.';
    dst[3] = '0';
    dst[4] = '0';
    dst[5] = '\0';
    return;
  }
  whole = h / 100u;
  frac = h % 100u;
  dst[0] = ' ';
  dst[1] = (char)('0' + whole);
  dst[2] = '.';
  dst[3] = (char)('0' + (frac / 10u));
  dst[4] = (char)('0' + (frac % 10u));
  dst[5] = '\0';
}

static void hist_ordered(u8 ch, u8 *out) {
  u8 i;
  u8 n = hist_len[ch];
  u8 start;

  if(n == 0) {
    return;
  }
  if(n < INSPECT_SPARK_N) {
    for(i = 0; i < n; ++i) {
      out[i] = hist[ch][i];
    }
    return;
  }
  start = hist_pos[ch];
  for(i = 0; i < INSPECT_SPARK_N; ++i) {
    out[i] = hist[ch][(u8)((start + i) % INSPECT_SPARK_N)];
  }
}

static void redraw_io(void) {
  render_clear();
  render_header_with_name("inspect", "i/o", 0);
  render_inspect_vu_bars();
  render_footer("-", "-", "-", "-");
}

static void redraw_cv(void) {
  const fract32 *v = cv_in_values();
  u8 i;
  char lab[5];
  char volts[6];
  u8 spark[INSPECT_SPARK_N];

  render_clear();
  render_header_with_name("inspect", "cv in", 0);

  lab[0] = 'c';
  lab[1] = 'v';
  lab[3] = ' ';
  lab[4] = '\0';
  for(i = 0; i < PLAY_MAPS_CV_COUNT; ++i) {
    lab[2] = (char)('1' + i);
    fmt_volts(volts, v[i]);
    hist_ordered(i, spark);
    render_inspect_cv_row(i, lab, volts, spark, hist_len[i]);
  }
  render_footer("-", "-", "-", "-");
}

static void redraw(void) {
  if(sub == INSPECT_SUB_CV) {
    redraw_cv();
  } else {
    redraw_io();
  }
}

void redraw_inspect(void) { redraw(); }

u8 inspect_on_io(void) {
  return (u8)(app_mode_is_inspect() && sub == INSPECT_SUB_IO);
}

void inspect_cv_hist_push(u8 ch, u8 height) {
  u8 pos;

  if(ch >= PLAY_MAPS_CV_COUNT) {
    return;
  }
  if(height > 7) {
    height = 7;
  }
  pos = hist_pos[ch];
  hist[ch][pos] = height;
  hist_pos[ch] = (u8)((pos + 1) % INSPECT_SPARK_N);
  if(hist_len[ch] < INSPECT_SPARK_N) {
    ++hist_len[ch];
  }
}

static void handle_enc1(s32 data) {
  if(data == 0) {
    return;
  }
  if(data > 0) {
    sub = (u8)((sub + 1) % INSPECT_SUB_N);
  } else {
    sub = (u8)((sub + INSPECT_SUB_N - 1) % INSPECT_SUB_N);
  }
  render_mark_dirty();
}

void select_inspect(void) {
  static const InputEncBinding enc[4] = {
    {eInputRoleUnmapped, NULL},
    {eInputRolePageSelect, handle_enc1},
    {eInputRoleUnmapped, NULL},
    {eInputRoleUnmapped, NULL},
  };
  static const InputSwBinding sw[4] = {
    {eInputSwRoleUnmapped, NULL},
    {eInputSwRoleUnmapped, NULL},
    {eInputSwRoleUnmapped, NULL},
    {eInputSwRoleUnmapped, NULL},
  };
  /* keep last i/o vs cv-in subpage across enter/exit */
  input_roles_bind(enc, sw);
  render_mark_dirty();
}

void page_inspect_init(void) {
  u8 i;
  u8 j;

  sub = INSPECT_SUB_IO;
  for(i = 0; i < PLAY_MAPS_CV_COUNT; ++i) {
    hist_pos[i] = 0;
    hist_len[i] = 0;
    for(j = 0; j < INSPECT_SPARK_N; ++j) {
      hist[i][j] = 0;
    }
  }
}
