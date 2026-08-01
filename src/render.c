#include "render.h"

#include <string.h>

#include "app.h"
#include "font.h"
#include "meters.h"
#include "morph2d.h"
#include "pages.h"
#include "region.h"
#include "screen.h"
#include "state.h"
#include "timers.h"

static region bootScrollRegion = {.w = 128, .h = 64, .x = 0, .y = 0};
static scroll bootScroll;

/* header 8; content 48 (6 rows); log overlays last content row when active;
 * four bees-style SW label cells at y=56 */

/* the header row is five side-by-side regions: the title area plus one per
 * status glyph, so each glyph redraws and flushes independently. they tile
 * L→R to 128. screen_draw_region packs 2px per byte, so every origin and
 * width here must stay even. */
#define HEAD_TITLE_W 92
#define HEAD_XRUN_X HEAD_TITLE_W
#define HEAD_XRUN_W 8
#define HEAD_MIDI_X (HEAD_XRUN_X + HEAD_XRUN_W)
#define HEAD_MIDI_W 8
#define HEAD_VU_X (HEAD_MIDI_X + HEAD_MIDI_W)
#define HEAD_VU_W 12
#define HEAD_IND_X (HEAD_VU_X + HEAD_VU_W)
#define HEAD_IND_W 8

static region regHead = {.w = HEAD_TITLE_W, .h = 8, .x = 0, .y = 0};
static region regXrun = {.w = HEAD_XRUN_W, .h = 8, .x = HEAD_XRUN_X, .y = 0};
static region regMidi = {.w = HEAD_MIDI_W, .h = 8, .x = HEAD_MIDI_X, .y = 0};
static region regVu = {.w = HEAD_VU_W, .h = 8, .x = HEAD_VU_X, .y = 0};
static region regMorph = {.w = HEAD_IND_W, .h = 8, .x = HEAD_IND_X, .y = 0};
static region regMain = {.w = 128, .h = 48, .x = 0, .y = 8};
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
/* titles clip to the title region; the glyph slots are always reserved */
#define HEAD_TITLE_MAX_X (HEAD_TITLE_W - HEAD_GAP_W)
/* vu boxes, drawn region-local: 4 cols of 2px with 1px gaps = 11px */
#define HEAD_VU_BOX 2
#define HEAD_VU_H_GAP 1
#define HEAD_VU_V_GAP 2
#define HEAD_VU_COLS 4
#define HEAD_VU_Y0 1 /* 1px pad; rows at y=1 and y=5 */
#define HEAD_MARGIN 2
#define HEAD_TEXT_X (HEAD_BAR_W + HEAD_GAP_W)
#define HEAD_DIRTY_GAP_W 2
#define HEAD_DIRTY_DOT_W 3

/* peak threshold → grey; search high→low for first peak >= thresh */
typedef struct {
  fract32 thresh;
  u8 grey;
} head_vu_lut_t;

static const head_vu_lut_t head_vu_lut[] = {
    {(fract32)0x60000000, 0xf}, {(fract32)0x40000000, 0xc},
    {(fract32)0x20000000, 0xa}, {(fract32)0x10000000, 0x7},
    {(fract32)0x08000000, 0x5}, {(fract32)0x04000000, 0x3},
    {(fract32)0x01000000, 0x2}, {(fract32)0x00000000, 0x0},
};

static char log_buf[22];
static u8 log_active = 0;
static u32 log_age_ms = 0;

static u8 midi_connected = 0;
static u8 midi_flash = 0;
static u32 midi_flash_age_ms = 0;
static u8 xrun_warn = 0;

/* one bit per status glyph; each is redrawn only when its own bit is set */
#define STATUS_XRUN 0x1
#define STATUS_MIDI 0x2
#define STATUS_VU 0x4
#define STATUS_MORPH 0x8
#define STATUS_ALL 0xf
static u8 status_dirty = 0;

/* frame scheduler: page content stale, and time of the last flush */
static u8 page_dirty = 0;
static u32 last_frame_ticks = 0;

/* fill w full-height columns of a header-row region, clipped to its width. */
static void reg_fill_col(region *r, u8 x0, u8 w, u8 color) {
  u8 x;
  u8 y;
  for(y = 0; y < r->h; ++y) {
    for(x = 0; x < w; ++x) {
      if((u16)x0 + x >= r->w) {
	break;
      }
      r->data[(u32)y * (u32)r->w + (u32)(x0 + x)] = color;
    }
  }
}

static void reg_put_px(region *r, u8 x, u8 y, u8 color) {
  if(x < r->w && y < r->h) {
    r->data[(u32)y * (u32)r->w + (u32)x] = color;
  }
}

static void head_fill_col(u8 x0, u8 w, u8 color) {
  reg_fill_col(&regHead, x0, w, color);
}

/* light-grey 3×3 circle after a title/name box; x0 is the left edge. */
static void head_draw_dirty_dot(u8 x0) {
  const u8 y0 = 2;
  reg_put_px(&regHead, (u8)(x0 + 1), y0, HEAD_GREY_LIGHT);
  reg_put_px(&regHead, x0, (u8)(y0 + 1), HEAD_GREY_LIGHT);
  reg_put_px(&regHead, (u8)(x0 + 1), (u8)(y0 + 1), HEAD_GREY_LIGHT);
  reg_put_px(&regHead, (u8)(x0 + 2), (u8)(y0 + 1), HEAD_GREY_LIGHT);
  reg_put_px(&regHead, (u8)(x0 + 1), (u8)(y0 + 2), HEAD_GREY_LIGHT);
}

static u8 head_draw_dirty_after(u8 x, u8 x_max, u8 dirty) {
  if(!dirty) {
    return x;
  }
  if((u16)x + HEAD_DIRTY_GAP_W + HEAD_DIRTY_DOT_W > x_max) {
    return x;
  }
  x = (u8)(x + HEAD_DIRTY_GAP_W);
  head_draw_dirty_dot(x);
  return (u8)(x + HEAD_DIRTY_DOT_W);
}

void render_init(void) {
  u8 i;
  region_alloc(&bootScrollRegion);
  scroll_init(&bootScroll, &bootScrollRegion);
  region_alloc(&regHead);
  region_alloc(&regXrun);
  region_alloc(&regMidi);
  region_alloc(&regVu);
  region_alloc(&regMorph);
  region_alloc(&regMain);
  region_alloc(&regLog);
  for(i = 0; i < 4; ++i) {
    region_alloc(&regFoot[i]);
  }
  log_buf[0] = '\0';
  log_active = 0;
  region_fill(&regHead, HEAD_BLACK);
  region_fill(&regXrun, HEAD_BLACK);
  region_fill(&regMidi, HEAD_BLACK);
  region_fill(&regVu, HEAD_BLACK);
  region_fill(&regMorph, HEAD_BLACK);
  region_fill(&regLog, 0);
  region_draw(&regHead);
  region_draw(&regXrun);
  region_draw(&regMidi);
  region_draw(&regVu);
  region_draw(&regMorph);
  /* paint every glyph once on the first frame */
  status_dirty = STATUS_ALL;
  /* do not draw empty log — it shares the last content row */
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
  if(str == NULL || x >= 128 || y >= 48) {
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
  if(w == 0 || h == 0 || x >= 128 || y >= 48) {
    return;
  }
  x1 = (u8)(x + w);
  y1 = (u8)(y + h);
  if(x1 > 128) {
    x1 = 128;
  }
  if(y1 > 48) {
    y1 = 48;
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
    } else {
      (void)font_glyph_fixed(ch, dst, 128, 0xf, 0);
    }
    x = (u8)(x + FONT_CHARW + 1);
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
  const u8 w = regMorph.w;
  u8 ix;
  u8 iy;
  u8 cx;
  u8 cy;
  u16 inner = (u16)(w - 2);

  /* mid-grey outline, black fill (matches play morph style, 8×8) */
  reg_fill_col(&regMorph, 0, w, HEAD_BLACK);
  for(ix = 0; ix < w; ++ix) {
    reg_put_px(&regMorph, ix, 0, HEAD_GREY);
    reg_put_px(&regMorph, ix, (u8)(w - 1), HEAD_GREY);
  }
  for(iy = 0; iy < w; ++iy) {
    reg_put_px(&regMorph, 0, iy, HEAD_GREY);
    reg_put_px(&regMorph, (u8)(w - 1), iy, HEAD_GREY);
  }

  /* 2×2 white cursor mapped into the inner (outline inset by 1) */
  if(inner > 1) {
    cx = (u8)(1 + ((u32)g_slots.x * (inner - 1)) / MORPH2D_ONE);
    cy = (u8)(1 + ((u32)g_slots.y * (inner - 1)) / MORPH2D_ONE);
  } else {
    cx = 1;
    cy = 1;
  }
  for(iy = 0; iy < 2; ++iy) {
    for(ix = 0; ix < 2; ++ix) {
      reg_put_px(&regMorph, (u8)(cx + ix), (u8)(cy + iy), HEAD_WHITE);
    }
  }
}

static u8 vu_peak_grey(fract32 peak) {
  u8 i;
  if(peak < 0) {
    peak = 0;
  }
  for(i = 0; i < (u8)(sizeof(head_vu_lut) / sizeof(head_vu_lut[0])); i++) {
    if(peak >= head_vu_lut[i].thresh) {
      return head_vu_lut[i].grey;
    }
  }
  return 0;
}

static u8 head_vu_grey(fract32 peak) { return vu_peak_grey(peak); }

static void vu_fill_box2(u8 x, u8 y, u8 color) {
  reg_put_px(&regVu, x, y, color);
  reg_put_px(&regVu, (u8)(x + 1), y, color);
  reg_put_px(&regVu, x, (u8)(y + 1), color);
  reg_put_px(&regVu, (u8)(x + 1), (u8)(y + 1), color);
}

static void head_draw_vu(void) {
  const bfin_meter_bank_t *in = meters_in();
  const bfin_meter_bank_t *out = meters_out();
  u8 i;
  u8 x;
  u8 y_in = HEAD_VU_Y0;
  u8 y_out = (u8)(HEAD_VU_Y0 + HEAD_VU_BOX + HEAD_VU_V_GAP);

  reg_fill_col(&regVu, 0, regVu.w, HEAD_BLACK);
  for(i = 0; i < HEAD_VU_COLS; i++) {
    x = (u8)(i * (HEAD_VU_BOX + HEAD_VU_H_GAP));
    vu_fill_box2(x, y_in, head_vu_grey(in->ch[i]));
    vu_fill_box2(x, y_out, head_vu_grey(out->ch[i]));
  }
}

static void head_draw_xrun(void) {
  u8 *dst;
  u8 x = 0;
  u8 i;

  reg_fill_col(&regXrun, 0, regXrun.w, HEAD_BLACK);
  if(!xrun_warn) {
    return;
  }
  /* proportional glyphs — each ! is 1px wide plus a 1px advance. drawn one
   * at a time: font_string_region_clip's padding math underflows on a
   * region this narrow. */
  for(i = 0; i < 3; ++i) {
    dst = regXrun.data + x;
    x = (u8)(x + font_glyph('!', dst, regXrun.w, HEAD_GREY_DARK, HEAD_BLACK) +
	     1);
  }
}

static void head_draw_midi(void) {
  u8 color;

  reg_fill_col(&regMidi, 0, regMidi.w, HEAD_BLACK);
  if(!midi_connected) {
    return;
  }
  color = midi_flash ? HEAD_GREY_LIGHT : HEAD_GREY_DARK;
  (void)font_glyph_fixed('m', regMidi.data, regMidi.w, color, HEAD_BLACK);
}

static void status_redraw(void) {
  if(status_dirty & STATUS_XRUN) {
    head_draw_xrun();
    regXrun.dirty = 1;
  }
  if(status_dirty & STATUS_MIDI) {
    head_draw_midi();
    regMidi.dirty = 1;
  }
  if(status_dirty & STATUS_VU) {
    head_draw_vu();
    regVu.dirty = 1;
  }
  if(status_dirty & STATUS_MORPH) {
    head_draw_morph_indicator();
    regMorph.dirty = 1;
  }
  status_dirty = 0;
}

void render_header(const char *title, u8 dirty) {
  u8 x;
  u8 x_max = HEAD_TITLE_MAX_X;

  if(title == NULL) {
    title = "";
  }

  region_fill(&regHead, HEAD_BLACK);
  head_fill_col(0, HEAD_BAR_W, HEAD_GREY);
  x = head_draw_text_box(HEAD_TEXT_X, title, x_max);
  (void)head_draw_dirty_after(x, x_max, dirty);
  regHead.dirty = 1;
}

void render_header_with_name(const char *title, const char *name, u8 dirty) {
  u8 x;
  u8 x_max = HEAD_TITLE_MAX_X;

  if(title == NULL) {
    title = "";
  }
  if(name == NULL || name[0] == '\0') {
    name = "none";
  }

  region_fill(&regHead, HEAD_BLACK);
  head_fill_col(0, HEAD_BAR_W, HEAD_GREY);
  x = head_draw_text_box(HEAD_TEXT_X, title, x_max);
  if((u16)x + HEAD_GAP_W < x_max) {
    x = (u8)(x + HEAD_GAP_W);
    x = head_draw_text_box(x, name, x_max);
  }
  (void)head_draw_dirty_after(x, x_max, dirty);
  regHead.dirty = 1;
}

void render_header_slot(char slot_letter, const char *preset, u8 dirty) {
  char slot[2];
  u8 x;
  u8 x_max = HEAD_TITLE_MAX_X;

  slot[0] = slot_letter;
  slot[1] = '\0';

  region_fill(&regHead, HEAD_BLACK);
  head_fill_col(0, HEAD_BAR_W, HEAD_GREY);

  x = head_draw_text_box(HEAD_TEXT_X, slot, x_max);
  if(preset != NULL && preset[0] != '\0' && (u16)x + HEAD_GAP_W < x_max) {
    x = (u8)(x + HEAD_GAP_W);
    x = head_draw_text_box(x, preset, x_max);
  }
  (void)head_draw_dirty_after(x, x_max, dirty);
  regHead.dirty = 1;
}

/* clears the title area only; status glyphs own their own regions. */
void render_header_clear(void) {
  region_fill(&regHead, HEAD_BLACK);
  regHead.dirty = 1;
}

void render_midi_set_connected(u8 connected) {
  midi_connected = connected ? 1 : 0;
  if(!midi_connected) {
    midi_flash = 0;
    midi_flash_age_ms = 0;
  }
  status_dirty |= STATUS_MIDI;
}

void render_midi_pulse_activity(void) {
  if(!midi_connected) {
    return;
  }
  midi_flash = 1;
  midi_flash_age_ms = 0;
  status_dirty |= STATUS_MIDI;
}

void render_xrun_set_warn(u8 warn) {
  u8 next = warn ? 1 : 0;
  if(xrun_warn == next) {
    return;
  }
  xrun_warn = next;
  status_dirty |= STATUS_XRUN;
}

void render_meters_mark_dirty(void) { status_dirty |= STATUS_VU; }

void render_morph_mark_dirty(void) { status_dirty |= STATUS_MORPH; }

void render_status_tick(void) {
  if(!midi_flash) {
    return;
  }
  midi_flash_age_ms += RENDER_TICK_MS;
  if(midi_flash_age_ms >= RENDER_MIDI_FLASH_MS) {
    midi_flash = 0;
    midi_flash_age_ms = 0;
    /* back to dark-grey m while still connected */
    status_dirty |= STATUS_MIDI;
  }
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

/* peak [0, FR32_MAX] → fill height; 0 (-inf) → 0, FS (0.0 dBFS) → bar_h_max.
 * shift before multiply so u32 math does not overflow on tall bars. */
static u8 vu_peak_fill_h(fract32 peak, u8 bar_h_max) {
  u32 peak_u;
  u8 fill_h;

  if(bar_h_max == 0 || peak <= 0) {
    return 0;
  }
  peak_u = (u32)peak;
  if(peak_u >= 0x7fffffffu) {
    return bar_h_max;
  }
  fill_h = (u8)(((peak_u >> 15) * (u32)bar_h_max) / 0xffffu);
  if(fill_h > bar_h_max) {
    fill_h = bar_h_max;
  }
  return fill_h;
}

void render_inspect_cv_row(u8 row, const char *label, const char *volts,
			   const u8 *spark, u8 spark_n) {
  const u8 y = (u8)(row * FONT_CHARH);
  const u8 label_w = (u8)(4 * FONT_CHARW); /* "cvN " */
  const u8 volts_w = (u8)(5 * FONT_CHARW); /* " 0.00" / "10.00" */
  const u8 spark_x = (u8)(label_w + volts_w + 2);
  u8 i;
  u8 h;
  u8 max_n;

  if(row >= RENDER_CONTENT_ROWS) {
    return;
  }
  if(label != NULL) {
    render_string_xy(0, y, label, HEAD_GREY);
  }
  if(volts != NULL) {
    render_string_xy(label_w, y, volts, HEAD_WHITE);
  }
  if(spark == NULL || spark_n == 0) {
    return;
  }
  max_n = (u8)(128 - spark_x);
  if(spark_n > max_n) {
    spark_n = max_n;
  }
  for(i = 0; i < spark_n; ++i) {
    h = spark[i];
    if(h > 7) {
      h = 7;
    }
    if(h > 0) {
      render_fill_rect((u8)(spark_x + i), (u8)(y + 8 - h), 1, h,
		       HEAD_GREY_LIGHT);
    }
  }
}

void render_inspect_vu_bars(void) {
  const bfin_meter_bank_t *in = meters_in();
  const bfin_meter_bank_t *out = meters_out();
  /* full content height: tags at top, digits under baseline, tall bars. */
  const u8 bar_w = 4;
  const u8 label_w = FONT_CHARW;
  const u8 col_pitch = 12;
  const u8 group_gap = 16;
  const u8 baseline_h = 1;
  const u8 gap_h = 1;
  const u8 tag_y = 0;
  const u8 label_y = 40;
  const u8 baseline_y = (u8)(label_y - baseline_h);
  const u8 meter_base_y = (u8)(baseline_y - gap_h - 1);
  const u8 bar_top = 10;
  const u8 bar_h_max =
    (meter_base_y >= bar_top) ? (u8)(meter_base_y - bar_top) : 0;
  const u8 group_w =
    (u8)((BFIN_METER_CH - 1) * col_pitch + label_w);
  const u8 total_w = (u8)(group_w + group_gap + group_w);
  const u8 x0 = (u8)((128 - total_w) / 2);
  u8 i;
  u8 x_label;
  u8 x_bar;
  u8 fill_h;
  char dig[2];

  render_string_xy(x0, tag_y, "in", HEAD_GREY);
  render_string_xy((u8)(x0 + group_w + group_gap), tag_y, "out", HEAD_GREY);

  dig[1] = '\0';
  for(i = 0; i < BFIN_METER_CH; ++i) {
    x_label = (u8)(x0 + i * col_pitch);
    x_bar = (u8)(x_label + (label_w - bar_w) / 2);
    fill_h = vu_peak_fill_h(in->ch[i], bar_h_max);
    render_fill_rect(x_bar, baseline_y, bar_w, baseline_h, HEAD_GREY_DARK);
    if(fill_h > 0) {
      render_fill_rect(x_bar, (u8)(meter_base_y + 1 - fill_h), bar_w, fill_h,
		       HEAD_GREY_LIGHT);
    }
    dig[0] = (char)('0' + i);
    render_string_xy(x_label, label_y, dig, HEAD_GREY);
  }

  for(i = 0; i < BFIN_METER_CH; ++i) {
    x_label = (u8)(x0 + group_w + group_gap + i * col_pitch);
    x_bar = (u8)(x_label + (label_w - bar_w) / 2);
    fill_h = vu_peak_fill_h(out->ch[i], bar_h_max);
    render_fill_rect(x_bar, baseline_y, bar_w, baseline_h, HEAD_GREY_DARK);
    if(fill_h > 0) {
      render_fill_rect(x_bar, (u8)(meter_base_y + 1 - fill_h), bar_w, fill_h,
		       HEAD_GREY_LIGHT);
    }
    dig[0] = (char)('0' + i);
    render_string_xy(x_label, label_y, dig, HEAD_GREY);
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
  regLog.dirty = 0;
  /* restore content row under the log overlay; flushed by the next frame */
  regMain.dirty = 1;
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
  if(regXrun.dirty) {
    region_draw(&regXrun);
  }
  if(regMidi.dirty) {
    region_draw(&regMidi);
  }
  if(regVu.dirty) {
    region_draw(&regVu);
  }
  if(regMorph.dirty) {
    region_draw(&regMorph);
  }
  if(regMain.dirty) {
    region_draw(&regMain);
    /* main includes the log overlay band; restack log if showing */
    if(log_active) {
      regLog.dirty = 1;
    }
  }
  /* log overlays the last content row; skip when inactive so row 5 shows */
  if(log_active && regLog.dirty) {
    region_draw(&regLog);
  }
  for(i = 0; i < 4; ++i) {
    if(regFoot[i].dirty) {
      region_draw(&regFoot[i]);
    }
  }
  app_resume();
}

static u8 any_region_dirty(void) {
  u8 i;
  if(regHead.dirty || regMain.dirty || (log_active && regLog.dirty)) {
    return 1;
  }
  if(regXrun.dirty || regMidi.dirty || regVu.dirty || regMorph.dirty) {
    return 1;
  }
  for(i = 0; i < 4; ++i) {
    if(regFoot[i].dirty) {
      return 1;
    }
  }
  return 0;
}

void render_mark_dirty(void) { page_dirty = 1; }

void render_frame_service(void) {
  u32 now;

  if(!page_dirty && !status_dirty && !any_region_dirty()) {
    return;
  }
  now = time_now();
  if((now - last_frame_ticks) < RENDER_MIN_FRAME_MS) {
    return;
  }
  last_frame_ticks = now;
  if(status_dirty) {
    status_redraw();
  }
  if(page_dirty) {
    page_dirty = 0;
    pages_redraw();
  }
  render_update();
}
