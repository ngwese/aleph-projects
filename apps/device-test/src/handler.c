#include "handler.h"

#include <string.h>

#include "delay.h"
#include "gpio.h"
#include "print_funcs.h"

#include "app.h"
#include "conf_board.h"
#include "events.h"
#include "monome.h"

#include "app_timers.h"
#include "render.h"

static void take_focus(focus_class_t f) {
  render_set_focus(f);
  render_mark_dirty();
}

static void clear_focus_if(focus_class_t f) {
  if (render_get_focus() == f) {
    render_set_focus(FOCUS_NONE);
    render_mark_dirty();
  }
}

static void handle_Switch5(s32 data) {
  (void)data;
  delay_ms(100);
  gpio_clr_gpio_pin(POWER_CTL_PIN);
}

static void handle_MidiConnect(s32 data) {
  (void)data;
  timers_set_midi();
  take_focus(FOCUS_MIDI);
  render_log("midi up");
}

static void handle_MidiDisconnect(s32 data) {
  (void)data;
  timers_unset_midi();
  clear_focus_if(FOCUS_MIDI);
  render_log("midi down");
}

static void handle_MidiPacket(s32 data) { render_midi_packet((u32)data); }

static void append_u8(char *buf, u8 buf_len, u8 v) {
  char tmp[4];
  u8 n = 0;
  if (v >= 100) {
    tmp[n++] = (char)('0' + (v / 100));
    v = (u8)(v % 100);
    tmp[n++] = (char)('0' + (v / 10));
    tmp[n++] = (char)('0' + (v % 10));
  } else if (v >= 10) {
    tmp[n++] = (char)('0' + (v / 10));
    tmp[n++] = (char)('0' + (v % 10));
  } else {
    tmp[n++] = (char)('0' + v);
  }
  tmp[n] = '\0';
  strncat(buf, tmp, buf_len - strlen(buf) - 1);
}

static void handle_MonomeConnect(s32 data) {
  char buf[22];

  (void)data;
  timers_set_monome();
  render_monome_connect();
  take_focus(FOCUS_MONOME);

  if (monome_device() == eDeviceGrid) {
    strncpy(buf, "monome grid ", sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    append_u8(buf, sizeof(buf), monome_size_x());
    strncat(buf, "x", sizeof(buf) - strlen(buf) - 1);
    append_u8(buf, sizeof(buf), monome_size_y());
    render_log(buf);
  } else if (monome_device() == eDeviceArc) {
    strncpy(buf, "monome arc ", sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    append_u8(buf, sizeof(buf), monome_encs());
    render_log(buf);
  } else {
    render_log("monome up");
  }
}

static void handle_MonomeDisconnect(s32 data) {
  (void)data;
  timers_unset_monome();
  render_monome_clear();
  clear_focus_if(FOCUS_MONOME);
  render_log("monome down");
}

static void handle_MonomeGridKey(s32 data) {
  u8 x;
  u8 y;
  u8 z;
  monome_grid_key_parse_event_data((u32)data, &x, &y, &z);
  render_monome_grid_key(x, y, z);
}

static void handle_MonomeRingEnc(s32 data) {
  u8 n;
  s8 delta;
  monome_ring_enc_parse_event_data((u32)data, &n, &delta);
  render_monome_ring_enc(n, delta);
}

static void handle_HidConnect(s32 data) {
  (void)data;
  timers_set_hid();
  take_focus(FOCUS_HID);
  render_log("hid up");
}

static void handle_HidDisconnect(s32 data) {
  (void)data;
  timers_unset_hid();
  clear_focus_if(FOCUS_HID);
  render_log("hid down");
}

static void handle_HidPacket(s32 data) {
  (void)data;
  render_hid_frame();
}

static void handle_MscConnect(s32 data) {
  (void)data;
  take_focus(FOCUS_MSC);
  render_log("msc up");
}

static void handle_MscDisconnect(s32 data) {
  (void)data;
  clear_focus_if(FOCUS_MSC);
  render_log("msc down");
}

static void handle_ScreenRefresh(s32 data) {
  (void)data;
  render_tick();
}

void assign_event_handlers(void) {
  app_event_handlers[kEventSwitch5] = handle_Switch5;
  app_event_handlers[kEventMidiConnect] = handle_MidiConnect;
  app_event_handlers[kEventMidiDisconnect] = handle_MidiDisconnect;
  app_event_handlers[kEventMidiPacket] = handle_MidiPacket;
  app_event_handlers[kEventMonomeConnect] = handle_MonomeConnect;
  app_event_handlers[kEventMonomeDisconnect] = handle_MonomeDisconnect;
  app_event_handlers[kEventMonomeGridKey] = handle_MonomeGridKey;
  app_event_handlers[kEventMonomeRingEnc] = handle_MonomeRingEnc;
  app_event_handlers[kEventHidConnect] = handle_HidConnect;
  app_event_handlers[kEventHidDisconnect] = handle_HidDisconnect;
  app_event_handlers[kEventHidPacket] = handle_HidPacket;
  app_event_handlers[kEventMscConnect] = handle_MscConnect;
  app_event_handlers[kEventMscDisconnect] = handle_MscDisconnect;
  app_event_handlers[kEventScreenRefresh] = handle_ScreenRefresh;

  app_idle_handler = render_frame_service;
}
