#include "handler.h"

#include "gpio.h"
#include "delay.h"
#include "print_funcs.h"

#include "conf_board.h"
#include "app.h"
#include "events.h"
#include "timers.h"

#include "app_timers.h"
#include "cv_in.h"
#include "meters.h"
#include "midi_between.h"
#include "pages.h"
#include "render.h"
#include "xruns.h"

#define MODE_LONG_TICKS 200u

static u8 mode_held;
static u32 mode_press_ticks;

static void handle_Switch4(s32 data) {
  u32 held;

  if(data > 0) {
    mode_held = 1;
    mode_press_ticks = time_now();
    return;
  }
  if(!mode_held) {
    return;
  }
  mode_held = 0;
  held = time_now() - mode_press_ticks;
  if(held >= MODE_LONG_TICKS) {
    pages_enter_inspect();
  } else {
    pages_mode_short_release();
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
  render_midi_set_connected(1);
}

static void handle_MidiDisconnect(s32 data) {
  (void)data;
  timers_unset_midi();
  render_midi_set_connected(0);
}

static void handle_MidiPacket(s32 data) {
  render_midi_pulse_activity();
  between_midi_handle_packet((u32)data);
}

/* xrun / meter SPI poll (posted from soft timer; run on main loop) */
static void handle_AppCustom(s32 data) {
  (void)data;
  (void)xruns_poll();
  (void)meters_poll();
}

/* frame tick: age the MIDI flash and the log, then redraw + flush if anything
   is pending. also runs from main-loop idle via app_idle_handler, which skips
   the aging — this event is the fixed RENDER_TICK_MS cadence. */
static void handle_ScreenRefresh(s32 data) {
  (void)data;
  render_status_tick();
  render_log_tick();
  render_frame_service();
}

static void handle_Adc0(s32 data) { cv_in_handle_adc(0, (u16)data); }
static void handle_Adc1(s32 data) { cv_in_handle_adc(1, (u16)data); }
static void handle_Adc2(s32 data) { cv_in_handle_adc(2, (u16)data); }
static void handle_Adc3(s32 data) { cv_in_handle_adc(3, (u16)data); }

void assign_event_handlers(void) {
  /* page select installs enc/sw0-3; keep mode + power + MIDI global */
  app_event_handlers[kEventSwitch4] = handle_Switch4;
  app_event_handlers[kEventSwitch5] = handle_Switch5;
  app_event_handlers[kEventMidiConnect] = handle_MidiConnect;
  app_event_handlers[kEventMidiDisconnect] = handle_MidiDisconnect;
  app_event_handlers[kEventMidiPacket] = handle_MidiPacket;
  app_event_handlers[kEventAppCustom] = handle_AppCustom;
  app_event_handlers[kEventScreenRefresh] = handle_ScreenRefresh;
  app_event_handlers[kEventAdc0] = handle_Adc0;
  app_event_handlers[kEventAdc1] = handle_Adc1;
  app_event_handlers[kEventAdc2] = handle_Adc2;
  app_event_handlers[kEventAdc3] = handle_Adc3;

  /* drain-driven frame: renders as soon as the queue empties, still
     rate-capped by RENDER_MIN_FRAME_MS */
  app_idle_handler = render_frame_service;
}
