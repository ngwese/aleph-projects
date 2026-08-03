#include "render.h"

#include <string.h>

#include "app.h"
#include "font.h"
#include "hid.h"
#include "monome.h"
#include "region.h"
#include "timers.h"

#define HEAD_W 128
#define HEAD_H 8
#define MAIN_W 128
#define MAIN_H 48
#define LOG_W 128
#define LOG_H 8
#define FOOT_H 8

#define COL_BLACK 0x0
#define COL_GREY 0x5
#define COL_WHITE 0xf
#define LED_ON 0xf

static region regHead = {.w = HEAD_W, .h = HEAD_H, .x = 0, .y = 0};
static region regMain = {.w = MAIN_W, .h = MAIN_H, .x = 0, .y = 8};
static region regLog = {.w = LOG_W, .h = LOG_H, .x = 0, .y = 48};
static region regFoot = {.w = HEAD_W, .h = FOOT_H, .x = 0, .y = 56};
static region bootScrollRegion = {.w = 128, .h = 32, .x = 0, .y = 16};
static scroll bootScroll;

static focus_class_t focus = FOCUS_NONE;
static u8 page_dirty = 1;
static u32 last_frame_ticks = 0;

static u8 heartbeat_on = 1;
static u32 heartbeat_age_ms = 0;

static char log_buf[22];
static u8 log_active = 0;
static u32 log_age_ms = 0;

static char midi_lines[MIDI_LOG_LINES][22];
static u8 midi_len = 0;
static u8 midi_head = 0;

static char hid_lines[HID_LOG_LINES][22];
static u8 hid_len = 0;
static u8 hid_head = 0;

static u8 grid_press[GRID_MAP_SIZE][GRID_MAP_SIZE];
static s16 arc_accum[ARC_MAX_ENCS];
static u8 arc_led_idx[ARC_MAX_ENCS];

static void reg_put_px(region *r, u8 x, u8 y, u8 color) {
  if (x < r->w && y < r->h) {
    r->data[(u32)y * (u32)r->w + (u32)x] = color;
  }
}

static void format_u32_dec(char *dst, u8 dst_len, s16 val) {
  char tmp[8];
  u8 n = 0;
  u8 i;
  u16 v;
  u8 neg = 0;

  if (dst_len < 2) {
    return;
  }
  if (val < 0) {
    neg = 1;
    v = (u16)(-val);
  } else {
    v = (u16)val;
  }
  if (v == 0) {
    tmp[n++] = '0';
  } else {
    while (v > 0 && n < sizeof(tmp)) {
      tmp[n++] = (char)('0' + (v % 10));
      v /= 10;
    }
  }
  i = 0;
  if (neg && i + 1 < dst_len) {
    dst[i++] = '-';
  }
  while (n > 0 && i + 1 < dst_len) {
    dst[i++] = tmp[--n];
  }
  dst[i] = '\0';
}

static void format_midi_line(char *dst, u8 dst_len, u32 data) {
  static const char hex[] = "0123456789ABCDEF";
  u8 b0 = (u8)((data >> 24) & 0xff);
  u8 b1 = (u8)((data >> 16) & 0xff);
  u8 b2 = (u8)((data >> 8) & 0xff);
  u8 i = 0;

  if (dst_len < 12) {
    dst[0] = '\0';
    return;
  }
  dst[i++] = hex[(b0 >> 4) & 0xf];
  dst[i++] = ' ';
  dst[i++] = hex[(b0 >> 4) & 0xf];
  dst[i++] = hex[b0 & 0xf];
  dst[i++] = ' ';
  dst[i++] = hex[(b1 >> 4) & 0xf];
  dst[i++] = hex[b1 & 0xf];
  dst[i++] = ' ';
  dst[i++] = hex[(b2 >> 4) & 0xf];
  dst[i++] = hex[b2 & 0xf];
  dst[i] = '\0';
}

static const char *focus_label(void) {
  switch (focus) {
  case FOCUS_MIDI:
    return "MIDI";
  case FOCUS_MONOME:
    if (monome_device() == eDeviceGrid) {
      return "MONOME GRID";
    }
    if (monome_device() == eDeviceArc) {
      return "MONOME ARC";
    }
    return "MONOME";
  case FOCUS_HID:
    return "HID";
  case FOCUS_MSC:
    return "MSC";
  default:
    return "";
  }
}

static void draw_header(void) {
  const char *label;
  u8 x;
  u8 y;

  region_fill(&regHead, COL_BLACK);

  label = focus_label();
  if (label[0] != '\0') {
    font_string_region_clip(&regHead, label, 0, 0, COL_WHITE, COL_BLACK);
  }

  if (heartbeat_on) {
    for (y = 0; y < 2; y++) {
      for (x = 0; x < 2; x++) {
        reg_put_px(&regHead, (u8)(HEAD_W - 2 + x), y, COL_WHITE);
      }
    }
  }
  regHead.dirty = 1;
}

static void draw_idle(void) {
  region_fill(&regMain, COL_BLACK);
  region_string(&regMain, "none", 0, 0, COL_WHITE, COL_BLACK, 0);
  regMain.dirty = 1;
}

static void draw_midi(void) {
  u8 i;
  u8 idx;
  u8 start;

  region_fill(&regMain, COL_BLACK);
  start = (u8)((midi_head + MIDI_LOG_LINES - midi_len) % MIDI_LOG_LINES);
  for (i = 0; i < midi_len; i++) {
    idx = (u8)((start + i) % MIDI_LOG_LINES);
    region_string(&regMain, midi_lines[idx], 0, (u8)(i * 8), COL_WHITE,
                  COL_BLACK, 0);
  }
  regMain.dirty = 1;
}

static void draw_hid(void) {
  u8 i;
  u8 idx;
  u8 start;

  region_fill(&regMain, COL_BLACK);
  start = (u8)((hid_head + HID_LOG_LINES - hid_len) % HID_LOG_LINES);
  for (i = 0; i < hid_len; i++) {
    idx = (u8)((start + i) % HID_LOG_LINES);
    region_string(&regMain, hid_lines[idx], 0, (u8)(i * 8), COL_WHITE,
                  COL_BLACK, 0);
  }
  regMain.dirty = 1;
}

static void draw_grid(void) {
  u8 x;
  u8 y;
  u8 px;
  u8 py;

  region_fill(&regMain, COL_BLACK);
  for (y = 0; y < GRID_MAP_SIZE; y++) {
    for (x = 0; x < GRID_MAP_SIZE; x++) {
      px = x;
      py = y;
      if (grid_press[y][x]) {
        reg_put_px(&regMain, px, py, COL_WHITE);
      }
    }
  }
  regMain.dirty = 1;
}

static void draw_arc(void) {
  char line[22];
  char num[8];
  u8 i;
  u8 nenc = monome_encs();

  region_fill(&regMain, COL_BLACK);
  for (i = 0; i < ARC_MAX_ENCS; i++) {
    line[0] = 'e';
    line[1] = (char)('0' + i);
    line[2] = ':';
    line[3] = ' ';
    line[4] = '\0';
    if (i < nenc) {
      format_u32_dec(num, sizeof(num), arc_accum[i]);
      strncat(line, num, sizeof(line) - strlen(line) - 1);
      strncat(line, " @", sizeof(line) - strlen(line) - 1);
      format_u32_dec(num, sizeof(num), (s16)arc_led_idx[i]);
      strncat(line, num, sizeof(line) - strlen(line) - 1);
    } else {
      strncat(line, "--", sizeof(line) - strlen(line) - 1);
    }
    region_string(&regMain, line, 0, (u8)(i * 8), COL_WHITE, COL_BLACK, 0);
  }
  regMain.dirty = 1;
}

static void draw_placeholder(const char *msg) {
  region_fill(&regMain, COL_BLACK);
  region_string(&regMain, msg, 0, 0, COL_WHITE, COL_BLACK, 0);
  regMain.dirty = 1;
}

static void draw_content(void) {
  switch (focus) {
  case FOCUS_MIDI:
    draw_midi();
    break;
  case FOCUS_MONOME:
    if (monome_device() == eDeviceGrid) {
      draw_grid();
    } else if (monome_device() == eDeviceArc) {
      draw_arc();
    } else {
      draw_placeholder("MONOME");
    }
    break;
  case FOCUS_HID:
    draw_hid();
    break;
  case FOCUS_MSC:
    draw_placeholder("TBD");
    break;
  default:
    draw_idle();
    break;
  }
}

static void redraw_all(void) {
  draw_header();
  draw_content();
}

void render_init(void) {
  u8 i;

  region_alloc(&bootScrollRegion);
  scroll_init(&bootScroll, &bootScrollRegion);
  region_alloc(&regHead);
  region_alloc(&regMain);
  region_alloc(&regLog);
  region_alloc(&regFoot);

  log_buf[0] = '\0';
  midi_len = 0;
  midi_head = 0;
  for (i = 0; i < MIDI_LOG_LINES; i++) {
    midi_lines[i][0] = '\0';
  }
  hid_len = 0;
  hid_head = 0;
  for (i = 0; i < HID_LOG_LINES; i++) {
    hid_lines[i][0] = '\0';
  }
  memset(grid_press, 0, sizeof(grid_press));
  memset(arc_accum, 0, sizeof(arc_accum));
  memset(arc_led_idx, 0, sizeof(arc_led_idx));

  region_fill(&regHead, COL_BLACK);
  region_fill(&regMain, COL_BLACK);
  region_fill(&regLog, COL_BLACK);
  region_fill(&regFoot, COL_BLACK);
  region_draw(&regHead);
  region_draw(&regMain);
  region_draw(&regFoot);
  page_dirty = 1;
}

void render_boot(const char *str) {
  int i;
  u8 *p = bootScroll.reg->data;
  for (i = 0; i < (int)bootScroll.reg->len; i++) {
    if (*p > 0x4) {
      *p = 0x4;
    }
    p++;
  }
  scroll_string_front(&bootScroll, (char *)str);
  region_draw(bootScroll.reg);
}

void render_update(void) {
  app_pause();
  if (regHead.dirty) {
    region_draw(&regHead);
  }
  if (regMain.dirty) {
    region_draw(&regMain);
    if (log_active) {
      regLog.dirty = 1;
    }
  }
  if (log_active && regLog.dirty) {
    region_draw(&regLog);
  }
  if (regFoot.dirty) {
    region_draw(&regFoot);
  }
  app_resume();
}

void render_mark_dirty(void) { page_dirty = 1; }

void render_frame_service(void) {
  u32 now;

  if (!page_dirty && !regHead.dirty && !regMain.dirty &&
      !(log_active && regLog.dirty)) {
    return;
  }
  now = time_now();
  if ((now - last_frame_ticks) < RENDER_MIN_FRAME_MS) {
    return;
  }
  last_frame_ticks = now;

  if (page_dirty) {
    page_dirty = 0;
    redraw_all();
  }
  render_update();
}

void render_tick(void) {
  heartbeat_age_ms += RENDER_TICK_MS;
  if (heartbeat_age_ms >= HEARTBEAT_HALF_MS) {
    heartbeat_age_ms = 0;
    heartbeat_on = (u8)!heartbeat_on;
    draw_header();
  }
  render_log_tick();
  render_frame_service();
}

void render_log(const char *str) {
  region_fill(&regLog, COL_BLACK);
  log_buf[0] = '\0';
  if (str != NULL && str[0] != '\0') {
    strncpy(log_buf, str, sizeof(log_buf) - 1);
    log_buf[sizeof(log_buf) - 1] = '\0';
    region_string(&regLog, log_buf, 0, 0, COL_GREY, COL_BLACK, 0);
    log_active = 1;
  } else {
    log_active = 0;
  }
  log_age_ms = 0;
  regLog.dirty = 1;
  app_pause();
  region_draw(&regLog);
  app_resume();
}

void render_log_tick(void) {
  if (!log_active) {
    return;
  }
  log_age_ms += RENDER_TICK_MS;
  if (log_age_ms >= RENDER_LOG_CLEAR_MS) {
    log_buf[0] = '\0';
    log_active = 0;
    log_age_ms = 0;
    region_fill(&regLog, COL_BLACK);
    regLog.dirty = 0;
    regMain.dirty = 1;
    page_dirty = 1;
  }
}

void render_set_focus(focus_class_t f) {
  focus = f;
  page_dirty = 1;
}

focus_class_t render_get_focus(void) { return focus; }

void render_midi_packet(u32 data) {
  format_midi_line(midi_lines[midi_head], sizeof(midi_lines[midi_head]), data);
  midi_head = (u8)((midi_head + 1) % MIDI_LOG_LINES);
  if (midi_len < MIDI_LOG_LINES) {
    midi_len++;
  }
  if (focus == FOCUS_MIDI) {
    page_dirty = 1;
  }
}

static void hid_push_line(const char *line) {
  strncpy(hid_lines[hid_head], line, sizeof(hid_lines[hid_head]) - 1);
  hid_lines[hid_head][sizeof(hid_lines[hid_head]) - 1] = '\0';
  hid_head = (u8)((hid_head + 1) % HID_LOG_LINES);
  if (hid_len < HID_LOG_LINES) {
    hid_len++;
  }
}

void render_hid_frame(void) {
  static const char hex[] = "0123456789ABCDEF";
  const volatile u8 *frame = hid_get_frame_data();
  u8 size = (u8)hid_get_frame_size();
  char line[22];
  u8 i;
  u8 n = 0;
  u8 li = 0;

  if (size > HID_FRAME_MAX_BYTES) {
    size = HID_FRAME_MAX_BYTES;
  }

  line[0] = '\0';
  for (i = 0; i < size; i++) {
    if (n >= HID_BYTES_PER_LINE) {
      hid_push_line(line);
      n = 0;
      li = 0;
      line[0] = '\0';
    }
    if (n > 0) {
      line[li++] = ' ';
    }
    line[li++] = hex[(frame[i] >> 4) & 0xf];
    line[li++] = hex[frame[i] & 0xf];
    line[li] = '\0';
    n++;
  }
  if (n > 0) {
    hid_push_line(line);
  }

  hid_clear_frame_dirty();
  if (focus == FOCUS_HID) {
    page_dirty = 1;
  }
}

void render_monome_connect(void) {
  u8 i;
  u8 nenc;

  memset(grid_press, 0, sizeof(grid_press));
  memset(arc_accum, 0, sizeof(arc_accum));
  memset(arc_led_idx, 0, sizeof(arc_led_idx));

  for (i = 0; i < MONOME_MAX_LED_BYTES; i++) {
    monomeLedBuffer[i] = 0;
  }
  monomeFrameDirty = 0x0f;

  if (monome_device() == eDeviceArc) {
    nenc = monome_encs();
    for (i = 0; i < nenc && i < ARC_MAX_ENCS; i++) {
      monome_arc_led_set(i, 0, LED_ON);
      arc_led_idx[i] = 0;
    }
  }

  page_dirty = 1;
}

void render_monome_grid_key(u8 x, u8 y, u8 z) {
  if (x >= GRID_MAP_SIZE || y >= GRID_MAP_SIZE) {
    return;
  }
  grid_press[y][x] = z ? 1 : 0;
  monome_led_set(x, y, z ? LED_ON : 0);
  if (focus == FOCUS_MONOME) {
    page_dirty = 1;
  }
}

void render_monome_ring_enc(u8 n, s8 delta) {
  u8 prev;

  if (n >= ARC_MAX_ENCS || n >= monome_encs()) {
    return;
  }
  prev = arc_led_idx[n];
  monome_arc_led_set(n, prev, 0);
  arc_accum[n] = (s16)(arc_accum[n] + delta);
  arc_led_idx[n] = (u8)(arc_accum[n] & 63);
  monome_arc_led_set(n, arc_led_idx[n], LED_ON);
  if (focus == FOCUS_MONOME) {
    page_dirty = 1;
  }
}

void render_monome_clear(void) {
  u8 i;

  memset(grid_press, 0, sizeof(grid_press));
  memset(arc_accum, 0, sizeof(arc_accum));
  memset(arc_led_idx, 0, sizeof(arc_led_idx));
  for (i = 0; i < MONOME_MAX_LED_BYTES; i++) {
    monomeLedBuffer[i] = 0;
  }
  monomeFrameDirty = 0;
  page_dirty = 1;
}
