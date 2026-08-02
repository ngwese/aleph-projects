#include "app_timers.h"

#include "encoders.h"
#include "events.h"
#include "hid.h"
#include "midi.h"
#include "monome.h"
#include "timers.h"

static event_t e;
static softTimer_t screenTimer = {.next = NULL, .prev = NULL};
static softTimer_t encTimer = {.next = NULL, .prev = NULL};
static softTimer_t midiPollTimer = {.next = NULL, .prev = NULL};
static softTimer_t monomePollTimer = {.next = NULL, .prev = NULL};
static softTimer_t monomeRefreshTimer = {.next = NULL, .prev = NULL};
static softTimer_t hidPollTimer = {.next = NULL, .prev = NULL};

static void screen_timer_callback(void *obj) {
  (void)obj;
  e.type = kEventScreenRefresh;
  e.data = 0;
  event_post(&e);
}

static void enc_timer_callback(void *obj) {
  static s16 val, valAbs;
  u8 i;
  (void)obj;

  for (i = 0; i < NUM_ENC; i++) {
    val = enc[i].val;
    valAbs = (val & 0x8000 ? (val ^ 0xffff) + 1 : val);
    if (valAbs > enc[i].thresh) {
      e.type = enc[i].event;
      e.data = val;
      enc[i].val = 0;
      event_post(&e);
    }
  }
}

static void midi_poll_timer_callback(void *obj) {
  (void)obj;
  midi_read();
}

static void monome_poll_timer_callback(void *obj) {
  (void)obj;
  if (serial_read != NULL) {
    serial_read();
  }
}

static void monome_refresh_timer_callback(void *obj) {
  (void)obj;
  if (monomeFrameDirty > 0) {
    e.type = kEventMonomeRefresh;
    e.data = 0;
    event_post(&e);
  }
}

static void hid_poll_timer_callback(void *obj) {
  (void)obj;
  /* HID stack updates the frame + dirty flags; post so the main loop logs */
  if (hid_get_frame_dirty()) {
    e.type = kEventHidPacket;
    e.data = 0;
    event_post(&e);
  }
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

void timers_set_monome(void) {
  timer_add(&monomePollTimer, 1, &monome_poll_timer_callback, NULL);
  timer_add(&monomeRefreshTimer, 50, &monome_refresh_timer_callback, NULL);
}

void timers_unset_monome(void) {
  timer_remove(&monomePollTimer);
  timer_remove(&monomeRefreshTimer);
}

void timers_set_hid(void) {
  timer_add(&hidPollTimer, 20, &hid_poll_timer_callback, NULL);
}

void timers_unset_hid(void) {
  timer_remove(&hidPollTimer);
}
