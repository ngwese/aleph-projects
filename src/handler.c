#include "handler.h"

#include "gpio.h"
#include "delay.h"
#include "print_funcs.h"

#include "conf_board.h"
#include "app.h"
#include "events.h"

#include "app_timers.h"
#include "midi_between.h"
#include "pages.h"
#include "render.h"

static void handle_Switch4(s32 data) {
  if(data > 0) {
    if(pages_toggle_play()) {
      gpio_set_gpio_pin(LED_MODE_PIN);
    } else {
      gpio_clr_gpio_pin(LED_MODE_PIN);
    }
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

void assign_event_handlers(void) {
  /* page select installs enc/sw0-3; keep mode + power + MIDI global */
  app_event_handlers[kEventSwitch4] = handle_Switch4;
  app_event_handlers[kEventSwitch5] = handle_Switch5;
  app_event_handlers[kEventMidiConnect] = handle_MidiConnect;
  app_event_handlers[kEventMidiDisconnect] = handle_MidiDisconnect;
  app_event_handlers[kEventMidiPacket] = handle_MidiPacket;
}
