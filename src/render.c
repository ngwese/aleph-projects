#include "render.h"

#include <string.h>

#include "app.h"
#include "font.h"
#include "region.h"
#include "screen.h"

static region bootScrollRegion = {.w = 128, .h = 64, .x = 0, .y = 0};
static scroll bootScroll;

/* header 8; content 40 (5 rows); log 8; four bees-style SW label cells */
static region regHead = {.w = 128, .h = 8, .x = 0, .y = 0};
static region regMain = {.w = 128, .h = 40, .x = 0, .y = 8};
static region regLog = {.w = 128, .h = 8, .x = 0, .y = 48};
static region regFoot[4] = {
    {.w = 32, .h = 8, .x = 0, .y = 56},
    {.w = 32, .h = 8, .x = 32, .y = 56},
    {.w = 32, .h = 8, .x = 64, .y = 56},
    {.w = 32, .h = 8, .x = 96, .y = 56},
};

static char log_buf[22];
static u8 log_active = 0;
static u32 log_age_ms = 0;

#define HEAD_GREY 0x5
#define HEAD_WHITE 0xf
#define HEAD_BLACK 0x0
#define HEAD_BAR_W 2
#define HEAD_GAP_W 1
#define HEAD_IND_W 8
#define HEAD_IND_X (128 - HEAD_IND_W)
#define HEAD_MARGIN 2
#define HEAD_TEXT_X (HEAD_BAR_W + HEAD_GAP_W)

static void head_fill_col(u8 x0, u8 w, u8 color) {
  u8 x;
  u8 y;
  for(y = 0; y < 8; ++y) {
    for(x = 0; x < w; ++x) {
      regHead.data[(u32)y * 128u + (u32)(x0 + x)] = color;
    }
  }
}

static void head_put_px(u8 x, u8 y, u8 color) {
  if(x < 128 && y < 8) {
    regHead.data[(u32)y * 128u + (u32)x] = color;
  }
}

void render_init(void) {
  u8 i;
  region_alloc(&bootScrollRegion);
  scroll_init(&bootScroll, &bootScrollRegion);
  region_alloc(&regHead);
  region_alloc(&regMain);
  region_alloc(&regLog);
  for(i = 0; i < 4; ++i) {
    region_alloc(&regFoot[i]);
  }
  log_buf[0] = '\0';
  log_active = 0;
  region_fill(&regHead, HEAD_BLACK);
  region_fill(&regLog, 0);
  region_draw(&regHead);
  region_draw(&regLog);
}

void render_boot(const char *str) {
  int i;
  u8 *p = bootScroll.reg->data;
  for(i = 0; i < bootScroll.reg->len; i++) {
    if(*p > 0x4) {
      *p = 0x4;
    }
    p++;
  }
  scroll_string_front(&bootScroll, (char *)str);
  region_draw(bootScroll.reg);
}

void render_clear(void) {
  region_fill(&regMain, 0);
  regMain.dirty = 1;
}

void render_line(u8 row, const char *str) {
  if(row >= RENDER_CONTENT_ROWS || str == NULL) {
    return;
  }
  region_string(&regMain, str, 0, (u8)(row * 8), 0xf, 0, 0);
  regMain.dirty = 1;
}

void render_line_inv(u8 row, const char *str) {
  if(row >= RENDER_CONTENT_ROWS || str == NULL) {
    return;
  }
  region_string(&regMain, str, 0, (u8)(row * 8), 0, 0xf, 0);
  regMain.dirty = 1;
}

void render_line_at(u8 row, u8 x, const char *str) {
  if(row >= RENDER_CONTENT_ROWS || str == NULL || x >= 128) {
    return;
  }
  font_string_region_clip(&regMain, str, x, (u8)(row * 8), 0xf, 0);
  regMain.dirty = 1;
}

void render_status_line(u8 row, const char *name) {
  u8 y;
  u8 x;
  u8 y0;

  if(row >= RENDER_CONTENT_ROWS) {
    return;
  }
  if(name == NULL || name[0] == '\0') {
    name = "none";
  }
  y0 = (u8)(row * 8);
  /* clear the row, then 2px mid-grey + 3px black + name (glyphs at x=5,
   * matching header title text after its 2px margin). */
  for(y = 0; y < 8; ++y) {
    for(x = 0; x < 128; ++x) {
      regMain.data[(u32)(y0 + y) * 128u + x] = 0;
    }
  }
  for(y = 0; y < 8; ++y) {
    regMain.data[(u32)(y0 + y) * 128u + 0] = HEAD_GREY;
    regMain.data[(u32)(y0 + y) * 128u + 1] = HEAD_GREY;
  }
  font_string_region_clip(&regMain, name, 5, y0, 0xf, 0);
  regMain.dirty = 1;
}

/* draw a white text box starting at x; returns x just past the box.
 * text has HEAD_MARGIN px padding on left and right. clipped to x_max. */
static u8 head_draw_text_box(u8 x, const char *text, u8 x_max) {
  u8 text_w;
  u8 bar_w;

  if(text == NULL || x >= x_max) {
    return x;
  }
  text_w = font_string_pixels(text);
  if(text_w < 1) {
    text_w = 1;
  }
  bar_w = (u8)(text_w + (2 * HEAD_MARGIN));
  if((u16)x + bar_w > x_max) {
    if(x_max <= x) {
      return x;
    }
    bar_w = (u8)(x_max - x);
  }
  head_fill_col(x, bar_w, HEAD_WHITE);
  font_string_region_clip(&regHead, text, (u8)(x + HEAD_MARGIN), 0, HEAD_BLACK,
			  HEAD_WHITE);
  return (u8)(x + bar_w);
}

static void head_draw_indicator(u8 dirty) {
  u8 ix;
  u8 iy;

  head_fill_col(HEAD_IND_X, HEAD_IND_W, HEAD_WHITE);
  if(dirty) {
    for(iy = 3; iy < 5; ++iy) {
      for(ix = 3; ix < 5; ++ix) {
	head_put_px((u8)(HEAD_IND_X + ix), iy, HEAD_BLACK);
      }
    }
  }
}

void render_header(const char *title, u8 dirty) {
  if(title == NULL) {
    title = "";
  }

  region_fill(&regHead, HEAD_BLACK);
  head_fill_col(0, HEAD_BAR_W, HEAD_GREY);
  (void)head_draw_text_box(HEAD_TEXT_X, title, (u8)(HEAD_IND_X - HEAD_GAP_W));
  head_draw_indicator(dirty);
  regHead.dirty = 1;
}

void render_header_slot(char slot_letter, const char *preset, u8 dirty) {
  char slot[2];
  u8 x;
  u8 x_max = (u8)(HEAD_IND_X - HEAD_GAP_W);

  slot[0] = slot_letter;
  slot[1] = '\0';

  region_fill(&regHead, HEAD_BLACK);
  head_fill_col(0, HEAD_BAR_W, HEAD_GREY);

  x = head_draw_text_box(HEAD_TEXT_X, slot, x_max);
  if(preset != NULL && preset[0] != '\0' && (u16)x + HEAD_GAP_W < x_max) {
    x = (u8)(x + HEAD_GAP_W);
    (void)head_draw_text_box(x, preset, x_max);
  }

  head_draw_indicator(dirty);
  regHead.dirty = 1;
}

void render_header_clear(void) {
  region_fill(&regHead, HEAD_BLACK);
  regHead.dirty = 1;
}

void render_footer(const char *a, const char *b, const char *c, const char *d) {
  const char *labels[4];
  u8 i;
  u8 tw;
  u8 x;

  labels[0] = (a != NULL && a[0] != '\0') ? a : " ";
  labels[1] = (b != NULL && b[0] != '\0') ? b : " ";
  labels[2] = (c != NULL && c[0] != '\0') ? c : " ";
  labels[3] = (d != NULL && d[0] != '\0') ? d : " ";

  /* bees-style: four equal cells, black text centered on white fill */
  for(i = 0; i < 4; ++i) {
    region_fill(&regFoot[i], 0xf);
    tw = font_string_pixels(labels[i]);
    if(tw >= 32) {
      x = 0;
    } else {
      x = (u8)((32 - tw) / 2);
    }
    font_string_region_clip(&regFoot[i], labels[i], x, 0, 0x0, 0xf);
    regFoot[i].dirty = 1;
  }
}

void render_log(const char *str) {
  region_fill(&regLog, 0);
  log_buf[0] = '\0';
  if(str != NULL && str[0] != '\0') {
    strncpy(log_buf, str, sizeof(log_buf) - 1);
    log_buf[sizeof(log_buf) - 1] = '\0';
    region_string(&regLog, log_buf, 0, 0, 0x5, 0, 0);
    log_active = 1;
  } else {
    log_active = 0;
  }
  log_age_ms = 0;
  regLog.dirty = 1;
  /* show immediately — long ops may block the event loop */
  app_pause();
  region_draw(&regLog);
  app_resume();
}

void render_log_clear(void) {
  if(!log_active && log_buf[0] == '\0') {
    return;
  }
  log_buf[0] = '\0';
  log_active = 0;
  log_age_ms = 0;
  region_fill(&regLog, 0);
  regLog.dirty = 1;
  app_pause();
  region_draw(&regLog);
  app_resume();
}

void render_log_tick(void) {
  if(!log_active) {
    return;
  }
  log_age_ms += RENDER_TICK_MS;
  if(log_age_ms >= RENDER_LOG_CLEAR_MS) {
    render_log_clear();
  }
}

void render_update(void) {
  u8 i;
  app_pause();
  if(regHead.dirty) {
    region_draw(&regHead);
  }
  if(regMain.dirty) {
    region_draw(&regMain);
  }
  if(regLog.dirty) {
    region_draw(&regLog);
  }
  for(i = 0; i < 4; ++i) {
    if(regFoot[i].dirty) {
      region_draw(&regFoot[i]);
    }
  }
  app_resume();
}
