#include "render.h"

#include <string.h>

#include "app.h"
#include "font.h"
#include "midi_nrpn.h"
#include "morph2d.h"
#include "region.h"
#include "screen.h"
#include "state.h"

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

#define HEAD_GREY 0x5
#define HEAD_GREY_DARK 0x3
#define HEAD_GREY_LIGHT 0xa
#define HEAD_WHITE 0xf
#define HEAD_BLACK 0x0
#define HEAD_BAR_W 2
#define HEAD_GAP_W 1
#define HEAD_IND_W 8
#define HEAD_IND_X (128 - HEAD_IND_W)
#define HEAD_MIDI_W FONT_CHARW
#define HEAD_MIDI_GAP_W 2 /* black gap between m glyph and morph indicator */
#define HEAD_MIDI_X (HEAD_IND_X - HEAD_MIDI_GAP_W - HEAD_MIDI_W)
#define HEAD_NRPN_GAP_W 2 /* black gap between NRPN text and m */
#define HEAD_NRPN_MAX_CHARS 5 /* e.g. "7:127" */
#define HEAD_TITLE_MAX_X (HEAD_MIDI_X - HEAD_GAP_W)
#define HEAD_MARGIN 2
#define HEAD_TEXT_X (HEAD_BAR_W + HEAD_GAP_W)

static char log_buf[22];
static u8 log_active = 0;
static u32 log_age_ms = 0;

static u8 midi_connected = 0;
static u8 midi_flash = 0;
static u32 midi_flash_age_ms = 0;

/* slot-page NRPN address chrome (left of m); cleared on non-slot headers */
static u8 head_nrpn_on = 0;
static char head_nrpn_text[HEAD_NRPN_MAX_CHARS + 1];
static u8 head_nrpn_x = 0;

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

void render_string_xy(u8 x, u8 y, const char *str, u8 fg) {
  if(str == NULL || x >= 128 || y >= 40) {
    return;
  }
  font_string_region_clip(&regMain, str, x, y, fg, 0);
  regMain.dirty = 1;
}

void render_fill_rect(u8 x, u8 y, u8 w, u8 h, u8 color) {
  u8 xi;
  u8 yi;
  u8 x1;
  u8 y1;
  if(w == 0 || h == 0 || x >= 128 || y >= 40) {
    return;
  }
  x1 = (u8)(x + w);
  y1 = (u8)(y + h);
  if(x1 > 128) {
    x1 = 128;
  }
  if(y1 > 40) {
    y1 = 40;
  }
  for(yi = y; yi < y1; ++yi) {
    for(xi = x; xi < x1; ++xi) {
      regMain.data[(u32)yi * 128u + (u32)xi] = color;
    }
  }
  regMain.dirty = 1;
}

void render_edit_string(u8 row, const char *str, u8 cursor) {
  u8 y0;
  u8 x = 2;
  u8 i = 0;
  u8 *dst;
  u8 cols;
  char ch;

  if(row >= RENDER_CONTENT_ROWS || str == NULL) {
    return;
  }
  y0 = (u8)(row * 8);
  /* clear the row */
  {
    u8 y;
    u8 px;
    for(y = 0; y < 8; ++y) {
      for(px = 0; px < 128; ++px) {
	regMain.data[(u32)(y0 + y) * 128u + px] = 0;
      }
    }
  }

  while(x < 120) {
    ch = str[i];
    if(ch == '\0' && i != cursor) {
      break;
    }
    dst = regMain.data + x + (u32)128u * (u32)y0;
    if(i == cursor) {
      /* inverse: black glyph on white cell (fixed width pad) */
      (void)font_glyph_fixed(ch != '\0' ? ch : ' ', dst, 128, 0x0, 0xf);
      cols = FONT_CHARW + 1;
    } else if(ch != '\0') {
      cols = (u8)(font_glyph(ch, dst, 128, 0xf, 0) + 1);
    } else {
      break;
    }
    x = (u8)(x + cols);
    ++i;
    if(ch == '\0') {
      break;
    }
  }
  regMain.dirty = 1;
}

void render_charset_row(u8 row, const char *chars, u8 sel) {
  u8 y0;
  u8 x = 2;
  u8 i = 0;
  u8 *dst;
  char ch;

  if(row >= RENDER_CONTENT_ROWS || chars == NULL) {
    return;
  }
  y0 = (u8)(row * 8);
  {
    u8 y;
    u8 px;
    for(y = 0; y < 8; ++y) {
      for(px = 0; px < 128; ++px) {
	regMain.data[(u32)(y0 + y) * 128u + px] = 0;
      }
    }
  }

  while((ch = chars[i]) != '\0' && x < 120) {
    dst = regMain.data + x + (u32)128u * (u32)y0;
    if(i == sel) {
      (void)font_glyph_fixed(ch, dst, 128, 0x0, 0xf);
      x = (u8)(x + FONT_CHARW + 1);
    } else {
      x = (u8)(x + font_glyph(ch, dst, 128, 0xf, 0) + 1);
    }
    ++i;
  }
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

static void head_draw_morph_indicator(void) {
  u8 ix;
  u8 iy;
  u8 cx;
  u8 cy;
  u16 inner = (u16)(HEAD_IND_W - 2);

  /* mid-grey outline, black fill (matches play morph style, 8×8) */
  head_fill_col(HEAD_IND_X, HEAD_IND_W, HEAD_BLACK);
  for(ix = 0; ix < HEAD_IND_W; ++ix) {
    head_put_px((u8)(HEAD_IND_X + ix), 0, HEAD_GREY);
    head_put_px((u8)(HEAD_IND_X + ix), (u8)(HEAD_IND_W - 1), HEAD_GREY);
  }
  for(iy = 0; iy < HEAD_IND_W; ++iy) {
    head_put_px(HEAD_IND_X, iy, HEAD_GREY);
    head_put_px((u8)(HEAD_IND_X + HEAD_IND_W - 1), iy, HEAD_GREY);
  }

  /* 3×3 white cursor mapped into the inner (outline inset by 1) */
  if(inner > 2) {
    cx = (u8)(HEAD_IND_X + 1 +
	      ((u32)g_slots.x * (inner - 2)) / MORPH2D_ONE);
    cy = (u8)(1 + ((u32)g_slots.y * (inner - 2)) / MORPH2D_ONE);
  } else {
    cx = (u8)(HEAD_IND_X + 1);
    cy = 1;
  }
  for(iy = 0; iy < 3; ++iy) {
    for(ix = 0; ix < 3; ++ix) {
      head_put_px((u8)(cx + ix), (u8)(cy + iy), HEAD_WHITE);
    }
  }
}

static void head_clear_nrpn(void) {
  head_nrpn_on = 0;
  head_nrpn_text[0] = '\0';
  head_nrpn_x = 0;
}

static u8 head_title_max_x(void) {
  if(head_nrpn_on && head_nrpn_text[0] != '\0') {
    return (u8)(head_nrpn_x - HEAD_GAP_W);
  }
  return HEAD_TITLE_MAX_X;
}

static void head_set_nrpn(s16 param_index) {
  u8 len;
  u8 w;

  head_clear_nrpn();
  if(param_index < 0) {
    return;
  }
  len = midi_nrpn_fmt_addr(head_nrpn_text, sizeof(head_nrpn_text),
			   (u16)param_index);
  if(len == 0) {
    return;
  }
  w = (u8)(len * FONT_CHARW);
  if(w > HEAD_MIDI_X) {
    return;
  }
  head_nrpn_x = (u8)(HEAD_MIDI_X - HEAD_NRPN_GAP_W - w);
  head_nrpn_on = 1;
}

static void head_draw_nrpn(void) {
  if(!head_nrpn_on || head_nrpn_text[0] == '\0') {
    return;
  }
  /* full header draws region_fill first; midi refresh must not wipe titles
   * left of the NRPN field, so we only paint the glyph columns. */
  font_string_region_clip(&regHead, head_nrpn_text, head_nrpn_x, 0,
			  HEAD_GREY_DARK, HEAD_BLACK);
}

static void head_draw_midi_m(void) {
  u8 color;
  u8 *dst;
  u8 x;

  /* clear the m column (and gap to morph) so disconnect leaves black */
  for(x = HEAD_MIDI_X; x < HEAD_IND_X; ++x) {
    head_fill_col(x, 1, HEAD_BLACK);
  }
  if(!midi_connected) {
    return;
  }
  color = midi_flash ? HEAD_GREY_LIGHT : HEAD_GREY_DARK;
  dst = regHead.data + HEAD_MIDI_X;
  (void)font_glyph_fixed('m', dst, 128, color, HEAD_BLACK);
}

static void head_draw_right_chrome(void) {
  head_draw_nrpn();
  head_draw_midi_m();
  head_draw_morph_indicator();
}

void render_header(const char *title, u8 dirty) {
  (void)dirty;
  if(title == NULL) {
    title = "";
  }

  head_clear_nrpn();
  region_fill(&regHead, HEAD_BLACK);
  head_fill_col(0, HEAD_BAR_W, HEAD_GREY);
  (void)head_draw_text_box(HEAD_TEXT_X, title, HEAD_TITLE_MAX_X);
  head_draw_right_chrome();
  regHead.dirty = 1;
}

void render_header_with_name(const char *title, const char *name, u8 dirty) {
  u8 x;
  u8 x_max = HEAD_TITLE_MAX_X;

  (void)dirty;
  if(title == NULL) {
    title = "";
  }
  if(name == NULL || name[0] == '\0') {
    name = "none";
  }

  head_clear_nrpn();
  region_fill(&regHead, HEAD_BLACK);
  head_fill_col(0, HEAD_BAR_W, HEAD_GREY);
  x = head_draw_text_box(HEAD_TEXT_X, title, x_max);
  if((u16)x + HEAD_GAP_W < x_max) {
    x = (u8)(x + HEAD_GAP_W);
    (void)head_draw_text_box(x, name, x_max);
  }
  head_draw_right_chrome();
  regHead.dirty = 1;
}

void render_header_slot(char slot_letter, const char *preset, u8 dirty,
			s16 nrpn_param) {
  char slot[2];
  u8 x;
  u8 x_max;

  slot[0] = slot_letter;
  slot[1] = '\0';

  head_set_nrpn(nrpn_param);
  x_max = head_title_max_x();

  region_fill(&regHead, HEAD_BLACK);
  head_fill_col(0, HEAD_BAR_W, HEAD_GREY);

  x = head_draw_text_box(HEAD_TEXT_X, slot, x_max);
  if(preset != NULL && preset[0] != '\0' && (u16)x + HEAD_GAP_W < x_max) {
    x = (u8)(x + HEAD_GAP_W);
    x = head_draw_text_box(x, preset, x_max);
  }
  /* dirty: 1px black spacer then light-grey "*" after the name box */
  if(dirty && (u16)x + HEAD_GAP_W < x_max) {
    x = (u8)(x + HEAD_GAP_W);
    font_string_region_clip(&regHead, "*", x, 0, HEAD_GREY, HEAD_BLACK);
  }

  head_draw_right_chrome();
  regHead.dirty = 1;
}

void render_header_clear(void) {
  head_clear_nrpn();
  region_fill(&regHead, HEAD_BLACK);
  regHead.dirty = 1;
}

void render_header_midi_refresh(void) {
  head_draw_right_chrome();
  regHead.dirty = 1;
  app_pause();
  region_draw(&regHead);
  app_resume();
}

void render_midi_set_connected(u8 connected) {
  midi_connected = connected ? 1 : 0;
  if(!midi_connected) {
    midi_flash = 0;
    midi_flash_age_ms = 0;
  }
  render_header_midi_refresh();
}

void render_midi_pulse_activity(void) {
  if(!midi_connected) {
    return;
  }
  midi_flash = 1;
  midi_flash_age_ms = 0;
  render_header_midi_refresh();
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

void render_footer_slot_tri(u8 cell, MorphSlot slot) {
  u8 px[3];
  u8 py[3];
  u8 i;
  if(cell >= 4 || slot >= MORPH2D_SLOTS) {
    return;
  }
  /* 3-pixel right triangle in morph-plane corner of the footer cell */
  switch(slot) {
  case eMorphSlotA: /* top-left */
    px[0] = 0;
    py[0] = 0;
    px[1] = 1;
    py[1] = 0;
    px[2] = 0;
    py[2] = 1;
    break;
  case eMorphSlotB: /* top-right */
    px[0] = 31;
    py[0] = 0;
    px[1] = 30;
    py[1] = 0;
    px[2] = 31;
    py[2] = 1;
    break;
  case eMorphSlotC: /* bottom-left */
    px[0] = 0;
    py[0] = 7;
    px[1] = 1;
    py[1] = 7;
    px[2] = 0;
    py[2] = 6;
    break;
  case eMorphSlotD: /* bottom-right */
  default:
    px[0] = 31;
    py[0] = 7;
    px[1] = 30;
    py[1] = 7;
    px[2] = 31;
    py[2] = 6;
    break;
  }
  for(i = 0; i < 3; ++i) {
    regFoot[cell].data[(u32)py[i] * 32u + (u32)px[i]] = HEAD_GREY;
  }
  regFoot[cell].dirty = 1;
}

/* morph plane: ~36px square on the left of the content region */
void render_play_morph(u16 mx, u16 my) {
  u8 x;
  u8 y;
  u8 cx;
  u8 cy;
  u8 ix;
  u8 iy;
  u16 inner = (u16)(RENDER_PLAY_MORPH_SZ - 2);

  /* light-gray frame */
  for(x = 0; x < RENDER_PLAY_MORPH_SZ; ++x) {
    regMain.data[(u32)RENDER_PLAY_MORPH_OY * 128u +
		 (u32)(RENDER_PLAY_MORPH_OX + x)] = HEAD_GREY;
    regMain.data[(u32)(RENDER_PLAY_MORPH_OY + RENDER_PLAY_MORPH_SZ - 1) * 128u +
		 (u32)(RENDER_PLAY_MORPH_OX + x)] = HEAD_GREY;
  }
  for(y = 0; y < RENDER_PLAY_MORPH_SZ; ++y) {
    regMain.data[(u32)(RENDER_PLAY_MORPH_OY + y) * 128u +
		 (u32)RENDER_PLAY_MORPH_OX] = HEAD_GREY;
    regMain.data[(u32)(RENDER_PLAY_MORPH_OY + y) * 128u +
		 (u32)(RENDER_PLAY_MORPH_OX + RENDER_PLAY_MORPH_SZ - 1)] =
      HEAD_GREY;
  }

  /* 3×3 white cursor; map morph into inner (frame inset by 1) */
  if(inner > 2) {
    cx = (u8)(RENDER_PLAY_MORPH_OX + 1 +
	      ((u32)mx * (inner - 2)) / MORPH2D_ONE);
    cy = (u8)(RENDER_PLAY_MORPH_OY + 1 +
	      ((u32)my * (inner - 2)) / MORPH2D_ONE);
  } else {
    cx = (u8)(RENDER_PLAY_MORPH_OX + 1);
    cy = (u8)(RENDER_PLAY_MORPH_OY + 1);
  }
  for(iy = 0; iy < 3; ++iy) {
    for(ix = 0; ix < 3; ++ix) {
      x = (u8)(cx + ix);
      y = (u8)(cy + iy);
      if(x < 128 && y < 40) {
	regMain.data[(u32)y * 128u + (u32)x] = HEAD_WHITE;
      }
    }
  }
  regMain.dirty = 1;
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
  if(midi_flash) {
    midi_flash_age_ms += RENDER_TICK_MS;
    if(midi_flash_age_ms >= RENDER_MIDI_FLASH_MS) {
      midi_flash = 0;
      midi_flash_age_ms = 0;
      /* return to dark-grey m while still connected */
      head_draw_midi_m();
      regHead.dirty = 1;
    }
  }
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
