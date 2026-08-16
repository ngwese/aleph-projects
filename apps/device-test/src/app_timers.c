#include "app_timers.h"

#include "encoders.h"
#include "events.h"
#include "hid_dev.h"
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
static softTimer_t gridWaveTimer = {.next = NULL, .prev = NULL};

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
  /* TC IRQ context — do not call USB here. */
  e.type = kEventMonomePoll;
  e.data = 0;
  event_post(&e);
}

static void monome_refresh_timer_callback(void *obj) {
  (void)obj;
  if (monomeFrameDirty > 0) {
    e.type = kEventMonomeRefresh;
    e.data = 0;
    event_post(&e);
  }
}

static void grid_wave_timer_callback(void *obj) {
  (void)obj;
  e.type = kEventAppCustom;
  e.data = 0;
  event_post(&e);
}

static void hid_poll_timer_callback(void *obj) {
  (void)obj;
  if (hid_dev_frame_dirty()) {
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

void timers_unset_midi(void) { timer_remove(&midiPollTimer); }

void timers_set_monome(void) {
  timer_add(&monomePollTimer, 5, &monome_poll_timer_callback, NULL);
  timer_add(&monomeRefreshTimer, 12, &monome_refresh_timer_callback, NULL);
}

void timers_unset_monome(void) {
  timer_remove(&monomePollTimer);
  timer_remove(&monomeRefreshTimer);
  timers_unset_grid_wave();
}

void timers_set_hid(void) {
  timer_add(&hidPollTimer, 20, &hid_poll_timer_callback, NULL);
}

void timers_unset_hid(void) { timer_remove(&hidPollTimer); }

void timers_set_grid_wave(u16 ms) {
  if (ms < 10) {
    ms = 10;
  }
  if (ms > 350) {
    ms = 350;
  }
  timer_remove(&gridWaveTimer);
  timer_add(&gridWaveTimer, ms, &grid_wave_timer_callback, NULL);
}

void timers_set_grid_wave_period(u16 ms) {
  if (ms < 10) {
    ms = 10;
  }
  if (ms > 350) {
    ms = 350;
  }
  timer_set(&gridWaveTimer, ms);
}

void timers_unset_grid_wave(void) { timer_remove(&gridWaveTimer); }
