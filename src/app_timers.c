#include "app_timers.h"

#include "encoders.h"
#include "events.h"
#include "midi.h"
#include "timers.h"

#include "render.h"

static event_t e;
static softTimer_t screenTimer = {.next = NULL, .prev = NULL};
static softTimer_t encTimer = {.next = NULL, .prev = NULL};
static softTimer_t midiPollTimer = {.next = NULL, .prev = NULL};

static void screen_timer_callback(void *obj) {
  (void)obj;
  render_log_tick();
  render_update();
}

static void enc_timer_callback(void *obj) {
  static s16 val, valAbs;
  u8 i;
  (void)obj;

  for(i = 0; i < NUM_ENC; i++) {
    val = enc[i].val;
    valAbs = (val & 0x8000 ? (val ^ 0xffff) + 1 : val);
    if(valAbs > enc[i].thresh) {
      e.type = enc[i].event;
      e.data = val;
      enc[i].val = 0;
      event_post(&e);
    }
  }
}

static void midi_poll_timer_callback(void *obj) {
  (void)obj;
  /* asynchronous, non-blocking; UHC callbacks post MIDI events */
  midi_read();
}

void init_app_timers(void) {
  timer_add(&screenTimer, 50, &screen_timer_callback, NULL);
  timer_add(&encTimer, 50, &enc_timer_callback, NULL);
}

void timers_set_midi(void) {
  timer_add(&midiPollTimer, 1, &midi_poll_timer_callback, NULL);
}

void timers_unset_midi(void) {
  timer_remove(&midiPollTimer);
}
