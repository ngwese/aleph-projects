#include "render.h"

#include <string.h>

#include "app.h"
#include "font.h"
#include "hid.h"
#include "hid_dev.h"
#include "hid_gamepad.h"
#include "hid_kbd.h"
#include "hid_mouse.h"
#include "kbd.h"
#include "app_timers.h"
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
#define LED_SIDE 0x6
#define ARC_LED_SHIFT 2
#define GRID_WAVE_MS_MIN 10
#define GRID_WAVE_MS_MAX 350
#define GRID_WAVE_MS_DEFAULT 250
#define GRID_ENC_SHIFT 2
#define GRID_SAW_MAX 11
#define GRID_MODE_KEYS 0
#define GRID_MODE_WAVE 1

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

static hid_kind_t hid_view_kind = HID_KIND_UNKNOWN;
static u8 hid_have_report = 0;
static hid_kbd_state_t hid_kbd;
static hid_mouse_state_t hid_mouse;
static hid_gamepad_state_t hid_pad;

static u8 grid_press[GRID_MAP_SIZE][GRID_MAP_SIZE];
static s16 arc_accum[ARC_MAX_ENCS];
static u8 arc_led_idx[ARC_MAX_ENCS];
static u8 grid_mode = GRID_MODE_KEYS;
static s16 grid_wave_shift_acc = 0;
static s16 grid_wave_mul_acc = (1 << GRID_ENC_SHIFT);
static s16 grid_wave_rate_acc = GRID_WAVE_MS_DEFAULT;
static u8 grid_wave_phase = 0;
static u16 grid_wave_period_ms = GRID_WAVE_MS_DEFAULT;

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
    switch (hid_view_kind) {
    case HID_KIND_KEYBOARD:
      return "HID KBD";
    case HID_KIND_MOUSE:
      return "HID MOUSE";
    case HID_KIND_GAMEPAD:
      return "HID PAD";
    default:
      return "HID";
    }
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

static void append_hex_u8(char *dst, u8 dst_len, u8 v) {
  static const char hex[] = "0123456789ABCDEF";
  u8 n = (u8)strlen(dst);
  if (n + 2 >= dst_len) {
    return;
  }
  dst[n++] = hex[(v >> 4) & 0xf];
  dst[n++] = hex[v & 0xf];
  dst[n] = '\0';
}

static void draw_hid_hex_fallback(void) {
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

static void draw_hid_kbd(void) {
  char line[22];
  char num[8];
  char ch[2];
  u8 i;
  u8 c;

  region_fill(&regMain, COL_BLACK);

  strncpy(line, "mod ", sizeof(line) - 1);
  line[sizeof(line) - 1] = '\0';
  if (hid_kbd.modifiers & CTRL) {
    strncat(line, "C", sizeof(line) - strlen(line) - 1);
  }
  if (hid_kbd.modifiers & SHIFT) {
    strncat(line, "S", sizeof(line) - strlen(line) - 1);
  }
  if (hid_kbd.modifiers & ALT) {
    strncat(line, "A", sizeof(line) - strlen(line) - 1);
  }
  if (hid_kbd.modifiers & META) {
    strncat(line, "M", sizeof(line) - strlen(line) - 1);
  }
  if ((hid_kbd.modifiers & (CTRL | SHIFT | ALT | META)) == 0) {
    strncat(line, "-", sizeof(line) - strlen(line) - 1);
  }
  region_string(&regMain, line, 0, 0, COL_WHITE, COL_BLACK, 0);

  strncpy(line, "keys ", sizeof(line) - 1);
  line[sizeof(line) - 1] = '\0';
  if (hid_kbd.key_count == 0) {
    strncat(line, "-", sizeof(line) - strlen(line) - 1);
  } else {
    for (i = 0; i < hid_kbd.key_count && i < 6; i++) {
      c = hid_to_ascii(hid_kbd.keys[i], hid_kbd.modifiers);
      if (c >= 0x20 && c < 0x7f) {
        ch[0] = (char)c;
        ch[1] = '\0';
        strncat(line, ch, sizeof(line) - strlen(line) - 1);
      } else {
        append_hex_u8(line, sizeof(line), hid_kbd.keys[i]);
      }
      if (i + 1 < hid_kbd.key_count && i + 1 < 6) {
        strncat(line, " ", sizeof(line) - strlen(line) - 1);
      }
    }
  }
  region_string(&regMain, line, 0, 8, COL_WHITE, COL_BLACK, 0);

  if (hid_have_report) {
    strncpy(line, "n=", sizeof(line) - 1);
    line[sizeof(line) - 1] = '\0';
    format_u32_dec(num, sizeof(num), (s16)hid_kbd.key_count);
    strncat(line, num, sizeof(line) - strlen(line) - 1);
    region_string(&regMain, line, 0, 16, COL_WHITE, COL_BLACK, 0);
  }

  regMain.dirty = 1;
}

static void draw_hid_mouse(void) {
  char line[22];
  char num[8];

  region_fill(&regMain, COL_BLACK);

  strncpy(line, "btn ", sizeof(line) - 1);
  line[sizeof(line) - 1] = '\0';
  append_hex_u8(line, sizeof(line), hid_mouse.buttons);
  region_string(&regMain, line, 0, 0, COL_WHITE, COL_BLACK, 0);

  strncpy(line, "dx ", sizeof(line) - 1);
  line[sizeof(line) - 1] = '\0';
  format_u32_dec(num, sizeof(num), hid_mouse.dx);
  strncat(line, num, sizeof(line) - strlen(line) - 1);
  strncat(line, " dy ", sizeof(line) - strlen(line) - 1);
  format_u32_dec(num, sizeof(num), hid_mouse.dy);
  strncat(line, num, sizeof(line) - strlen(line) - 1);
  region_string(&regMain, line, 0, 8, COL_WHITE, COL_BLACK, 0);

  strncpy(line, "wh ", sizeof(line) - 1);
  line[sizeof(line) - 1] = '\0';
  format_u32_dec(num, sizeof(num), (s16)hid_mouse.wheel);
  strncat(line, num, sizeof(line) - strlen(line) - 1);
  region_string(&regMain, line, 0, 16, COL_WHITE, COL_BLACK, 0);

  regMain.dirty = 1;
}

static void draw_hid_pad(void) {
  char line[22];
  char num[8];

  region_fill(&regMain, COL_BLACK);

  strncpy(line, "xy ", sizeof(line) - 1);
  line[sizeof(line) - 1] = '\0';
  format_u32_dec(num, sizeof(num), (s16)hid_pad.x);
  strncat(line, num, sizeof(line) - strlen(line) - 1);
  strncat(line, " ", sizeof(line) - strlen(line) - 1);
  format_u32_dec(num, sizeof(num), (s16)hid_pad.y);
  strncat(line, num, sizeof(line) - strlen(line) - 1);
  region_string(&regMain, line, 0, 0, COL_WHITE, COL_BLACK, 0);

  strncpy(line, "zr ", sizeof(line) - 1);
  line[sizeof(line) - 1] = '\0';
  format_u32_dec(num, sizeof(num), (s16)hid_pad.z);
  strncat(line, num, sizeof(line) - strlen(line) - 1);
  strncat(line, " ", sizeof(line) - strlen(line) - 1);
  format_u32_dec(num, sizeof(num), (s16)hid_pad.rz);
  strncat(line, num, sizeof(line) - strlen(line) - 1);
  region_string(&regMain, line, 0, 8, COL_WHITE, COL_BLACK, 0);

  strncpy(line, "hat ", sizeof(line) - 1);
  line[sizeof(line) - 1] = '\0';
  format_u32_dec(num, sizeof(num), (s16)hid_pad.hat);
  strncat(line, num, sizeof(line) - strlen(line) - 1);
  strncat(line, " rx ", sizeof(line) - strlen(line) - 1);
  format_u32_dec(num, sizeof(num), (s16)hid_pad.rx);
  strncat(line, num, sizeof(line) - strlen(line) - 1);
  region_string(&regMain, line, 0, 16, COL_WHITE, COL_BLACK, 0);

  strncpy(line, "btn ", sizeof(line) - 1);
  line[sizeof(line) - 1] = '\0';
  append_hex_u8(line, sizeof(line), (u8)((hid_pad.buttons >> 24) & 0xff));
  append_hex_u8(line, sizeof(line), (u8)((hid_pad.buttons >> 16) & 0xff));
  append_hex_u8(line, sizeof(line), (u8)((hid_pad.buttons >> 8) & 0xff));
  append_hex_u8(line, sizeof(line), (u8)(hid_pad.buttons & 0xff));
  region_string(&regMain, line, 0, 24, COL_WHITE, COL_BLACK, 0);

  regMain.dirty = 1;
}

static void draw_hid(void) {
  char line[22];
  const hid_dev_info_t *dev;

  /*
   * Known kinds: show the empty decode view immediately so a quiet mouse
   * (no interrupt until movement) is not stuck on "waiting". Unknown kinds
   * still wait for the first frame / hex fallback.
   */
  if (!hid_have_report && hid_len == 0) {
    if (hid_view_kind == HID_KIND_KEYBOARD) {
      draw_hid_kbd();
      return;
    }
    if (hid_view_kind == HID_KIND_MOUSE) {
      draw_hid_mouse();
      return;
    }
    if (hid_view_kind == HID_KIND_GAMEPAD) {
      draw_hid_pad();
      return;
    }

    region_fill(&regMain, COL_BLACK);
    region_string(&regMain, "waiting", 0, 0, COL_WHITE, COL_BLACK, 0);
    dev = hid_dev_info();
    strncpy(line, "p", sizeof(line) - 1);
    line[sizeof(line) - 1] = '\0';
    {
      char num[8];
      format_u32_dec(num, sizeof(num), (s16)dev->iface_protocol);
      strncat(line, num, sizeof(line) - strlen(line) - 1);
      strncat(line, " s", sizeof(line) - strlen(line) - 1);
      format_u32_dec(num, sizeof(num), (s16)dev->report_size);
      strncat(line, num, sizeof(line) - strlen(line) - 1);
      if (dev->hid_iface_count > 1) {
        strncat(line, " ", sizeof(line) - strlen(line) - 1);
        format_u32_dec(num, sizeof(num), (s16)(dev->iface_index + 1));
        strncat(line, num, sizeof(line) - strlen(line) - 1);
        strncat(line, "/", sizeof(line) - strlen(line) - 1);
        format_u32_dec(num, sizeof(num), (s16)dev->hid_iface_count);
        strncat(line, num, sizeof(line) - strlen(line) - 1);
      }
    }
    region_string(&regMain, line, 0, 8, COL_WHITE, COL_BLACK, 0);
    regMain.dirty = 1;
    return;
  }

  if (!hid_have_report) {
    draw_hid_hex_fallback();
    return;
  }

  switch (hid_view_kind) {
  case HID_KIND_KEYBOARD:
    draw_hid_kbd();
    break;
  case HID_KIND_MOUSE:
    draw_hid_mouse();
    break;
  case HID_KIND_GAMEPAD:
    draw_hid_pad();
    break;
  default:
    draw_hid_hex_fallback();
    break;
  }
}

static u8 grid_saw_at(u8 i) {
  return (u8)(((i & 7) * GRID_SAW_MAX) / 7);
}

static void grid_wave_apply(void) {
  u8 x;
  u8 y;
  u8 cols;
  u8 rows;
  u8 z;
  s16 t;

  if (monome_device() != eDeviceGrid) {
    return;
  }
  cols = monome_size_x();
  rows = monome_size_y();
  if (cols > GRID_MAP_SIZE) {
    cols = GRID_MAP_SIZE;
  }
  if (rows > GRID_MAP_SIZE) {
    rows = GRID_MAP_SIZE;
  }
  for (y = 0; y < rows; y++) {
    for (x = 0; x < cols; x++) {
      t = (s16)x + (grid_wave_shift_acc >> GRID_ENC_SHIFT) +
          (s16)y * (grid_wave_mul_acc >> GRID_ENC_SHIFT) +
          (s16)grid_wave_phase;
      z = grid_saw_at((u8)(t & 15));
      if (grid_press[y][x]) {
        z = LED_ON;
      }
      monome_led_set(x, y, z);
    }
  }
  page_dirty = 1;
}

static void draw_grid(void) {
  u8 x;
  u8 y;
  u8 z;
  u8 px;
  u8 py;

  region_fill(&regMain, COL_BLACK);
  for (y = 0; y < GRID_MAP_SIZE; y++) {
    for (x = 0; x < GRID_MAP_SIZE; x++) {
      z = monomeLedBuffer[monome_xy_idx(x, y)];
      if (z == 0) {
        continue;
      }
      if (z > COL_WHITE) {
        z = COL_WHITE;
      }
      px = (u8)(x * 3);
      py = (u8)(y * 3);
      reg_put_px(&regMain, px, py, z);
      reg_put_px(&regMain, (u8)(px + 1), py, z);
      reg_put_px(&regMain, px, (u8)(py + 1), z);
      reg_put_px(&regMain, (u8)(px + 1), (u8)(py + 1), z);
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
  hid_view_kind = HID_KIND_UNKNOWN;
  hid_have_report = 0;
  memset(&hid_kbd, 0, sizeof(hid_kbd));
  memset(&hid_mouse, 0, sizeof(hid_mouse));
  memset(&hid_pad, 0, sizeof(hid_pad));
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

static void hid_push_hex_frame(const volatile u8 *frame, u8 size) {
  static const char hex[] = "0123456789ABCDEF";
  char line[22];
  u8 i;
  u8 n = 0;
  u8 li = 0;
  u8 show;

  /* Host reports wMaxPacketSize; trim trailing zeros for display. */
  show = size;
  if (show > 14) {
    show = 14;
  }
  while (show > 8) {
    if (frame[show - 1] != 0) {
      break;
    }
    show--;
  }

  hid_len = 0;
  hid_head = 0;

  line[0] = '\0';
  for (i = 0; i < show; i++) {
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
}

void render_hid_connect(void) {
  const hid_dev_info_t *dev = hid_dev_info();

  hid_view_kind = dev->kind;
  hid_have_report = 0;
  hid_len = 0;
  hid_head = 0;
  memset(&hid_kbd, 0, sizeof(hid_kbd));
  memset(&hid_mouse, 0, sizeof(hid_mouse));
  memset(&hid_pad, 0, sizeof(hid_pad));
  page_dirty = 1;
}

void render_hid_iface_changed(void) {
  render_hid_connect();
}

void render_hid_disconnect(void) {
  hid_view_kind = HID_KIND_UNKNOWN;
  hid_have_report = 0;
  hid_len = 0;
  hid_head = 0;
  page_dirty = 1;
}

void render_hid_frame(void) {
  const volatile u8 *frame = hid_dev_frame_data();
  u8 size = hid_dev_frame_size();
  bool report_proto = hid_dev_report_protocol();
  u8 local[HID_FRAME_MAX_BYTES];
  u8 i;
  bool ok = false;
  hid_kind_t kind;

  if (size > HID_FRAME_MAX_BYTES) {
    size = HID_FRAME_MAX_BYTES;
  }
  for (i = 0; i < size; i++) {
    local[i] = frame[i];
  }

  kind = hid_dev_kind_for_size(size);
  if (kind == HID_KIND_UNKNOWN) {
    kind = hid_dev_guess_kind(local, size);
  }

  switch (kind) {
  case HID_KIND_KEYBOARD:
    ok = hid_kbd_parse(local, size, report_proto, &hid_kbd);
    break;
  case HID_KIND_MOUSE:
    ok = hid_mouse_parse(local, size, report_proto, &hid_mouse);
    break;
  case HID_KIND_GAMEPAD:
    ok = hid_gamepad_parse(local, size, &hid_pad);
    break;
  default:
    /* Last resort: try boot-compatible keyboard, then mouse. */
    if (hid_kbd_parse(local, size, report_proto, &hid_kbd)) {
      kind = HID_KIND_KEYBOARD;
      ok = true;
    } else if (hid_mouse_parse(local, size, report_proto, &hid_mouse)) {
      kind = HID_KIND_MOUSE;
      ok = true;
    } else if (hid_gamepad_parse(local, size, &hid_pad)) {
      kind = HID_KIND_GAMEPAD;
      ok = true;
    }
    break;
  }

  if (ok) {
    hid_view_kind = kind;
    hid_have_report = 1;
  } else {
    hid_have_report = 0;
    hid_push_hex_frame(frame, size);
  }

  hid_dev_clear_frame_dirty();
  if (focus == FOCUS_HID) {
    page_dirty = 1;
  }
}

static u8 arc_led_pos(s16 accum) {
  return (u8)((accum >> ARC_LED_SHIFT) & 63);
}

static void arc_paint_triplet(u8 enc, u8 idx, u8 on) {
  u8 side = on ? LED_SIDE : 0;
  u8 cen = on ? LED_ON : 0;

  monome_arc_led_set(enc, (u8)((idx + 63) & 63), side);
  monome_arc_led_set(enc, idx, cen);
  monome_arc_led_set(enc, (u8)((idx + 1) & 63), side);
}

void render_monome_connect(void) {
  u8 i;
  u8 nenc;

  memset(grid_press, 0, sizeof(grid_press));
  memset(arc_accum, 0, sizeof(arc_accum));
  memset(arc_led_idx, 0, sizeof(arc_led_idx));

  grid_mode = GRID_MODE_KEYS;
  grid_wave_shift_acc = 0;
  grid_wave_mul_acc = (1 << GRID_ENC_SHIFT);
  grid_wave_rate_acc = GRID_WAVE_MS_DEFAULT;
  grid_wave_phase = 0;
  grid_wave_period_ms = GRID_WAVE_MS_DEFAULT;
  timers_unset_grid_wave();

  memset(monomeLedBuffer, 0, MONOME_MAX_LED_BYTES);
  monomeFrameDirty = 0;

  if (monome_device() == eDeviceArc) {
    nenc = monome_encs();
    for (i = 0; i < nenc && i < ARC_MAX_ENCS; i++) {
      arc_paint_triplet(i, 0, 1);
      arc_led_idx[i] = 0;
    }
  } else {
    monomeFrameDirty = 0x0f;
  }

  page_dirty = 1;
}

void render_monome_grid_key(u8 x, u8 y, u8 z) {
  if (x >= GRID_MAP_SIZE || y >= GRID_MAP_SIZE) {
    return;
  }
  grid_press[y][x] = z ? 1 : 0;
  if (grid_mode == GRID_MODE_WAVE) {
    grid_wave_apply();
  } else {
    monome_led_set(x, y, z ? LED_ON : 0);
  }
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
  arc_accum[n] = (s16)(arc_accum[n] + delta);
  arc_led_idx[n] = arc_led_pos(arc_accum[n]);
  if (arc_led_idx[n] != prev) {
    arc_paint_triplet(n, prev, 0);
    arc_paint_triplet(n, arc_led_idx[n], 1);
  }
  if (focus == FOCUS_MONOME) {
    page_dirty = 1;
  }
}

void render_monome_clear(void) {
  memset(grid_press, 0, sizeof(grid_press));
  memset(arc_accum, 0, sizeof(arc_accum));
  memset(arc_led_idx, 0, sizeof(arc_led_idx));
  grid_mode = GRID_MODE_KEYS;
  grid_wave_shift_acc = 0;
  grid_wave_mul_acc = (1 << GRID_ENC_SHIFT);
  grid_wave_rate_acc = GRID_WAVE_MS_DEFAULT;
  grid_wave_phase = 0;
  grid_wave_period_ms = GRID_WAVE_MS_DEFAULT;
  timers_unset_grid_wave();
  monomeFrameDirty = 0;
  page_dirty = 1;
}

static u16 grid_wave_period_from_acc(void) {
  return (u16)grid_wave_rate_acc;
}

void render_monome_grid_mode(u8 mode) {
  u8 x;
  u8 y;

  if (mode > 3) {
    return;
  }
  grid_mode = mode;
  if (mode == GRID_MODE_WAVE && monome_device() == eDeviceGrid) {
    grid_wave_period_ms = grid_wave_period_from_acc();
    timers_set_grid_wave(grid_wave_period_ms);
    grid_wave_apply();
    return;
  }
  timers_unset_grid_wave();
  if (monome_device() == eDeviceGrid) {
    for (y = 0; y < GRID_MAP_SIZE; y++) {
      for (x = 0; x < GRID_MAP_SIZE; x++) {
        monome_led_set(x, y, grid_press[y][x] ? LED_ON : 0);
      }
    }
  }
  page_dirty = 1;
}

void render_monome_enc(u8 n, s16 delta) {
  u16 period;

  if (render_get_focus() != FOCUS_MONOME ||
      monome_device() != eDeviceGrid || grid_mode != GRID_MODE_WAVE) {
    return;
  }
  if (n == 0) {
    grid_wave_shift_acc = (s16)(grid_wave_shift_acc + delta);
  } else if (n == 1) {
    grid_wave_mul_acc = (s16)(grid_wave_mul_acc + delta);
  } else if (n == 2) {
    grid_wave_rate_acc = (s16)(grid_wave_rate_acc + delta);
    if (grid_wave_rate_acc < GRID_WAVE_MS_MIN) {
      grid_wave_rate_acc = GRID_WAVE_MS_MIN;
    }
    if (grid_wave_rate_acc > GRID_WAVE_MS_MAX) {
      grid_wave_rate_acc = GRID_WAVE_MS_MAX;
    }
    period = grid_wave_period_from_acc();
    if (period != grid_wave_period_ms) {
      grid_wave_period_ms = period;
      timers_set_grid_wave_period(period);
    }
    return;
  } else {
    return;
  }
  grid_wave_apply();
}

void render_monome_grid_wave_tick(void) {
  if (grid_mode != GRID_MODE_WAVE || monome_device() != eDeviceGrid) {
    return;
  }
  grid_wave_phase = (u8)((grid_wave_phase + 1) & 15);
  grid_wave_apply();
}
