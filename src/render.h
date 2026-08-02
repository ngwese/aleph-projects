#ifndef DEVICE_TEST_RENDER_H
#define DEVICE_TEST_RENDER_H

#include "monome.h"
#include "types.h"

#define RENDER_TICK_MS 50
#define RENDER_MIN_FRAME_MS 50
#define RENDER_LOG_CLEAR_MS 2000
#define HEARTBEAT_HALF_MS 500

#define MIDI_LOG_LINES 5
#define HID_LOG_LINES 5
#define ARC_MAX_ENCS 4
#define GRID_MAP_SIZE 16
/* hex pairs + spaces that fit a 21-char content line */
#define HID_BYTES_PER_LINE 7

typedef enum {
  FOCUS_NONE = 0,
  FOCUS_MIDI,
  FOCUS_MONOME,
  FOCUS_HID,
  FOCUS_MSC
} focus_class_t;

void render_init(void);
void render_boot(const char *str);
void render_update(void);
void render_mark_dirty(void);
void render_frame_service(void);
void render_tick(void);

void render_log(const char *str);
void render_log_tick(void);

void render_set_focus(focus_class_t focus);
focus_class_t render_get_focus(void);

void render_midi_packet(u32 data);
void render_hid_frame(void);

void render_monome_connect(void);
void render_monome_grid_key(u8 x, u8 y, u8 z);
void render_monome_ring_enc(u8 n, s8 delta);
void render_monome_clear(void);

#endif
