#include "app_timers.h"

#include "adc_poll.h"
#include "encoders.h"
#include "events.h"
#include "midi.h"
#include "timers.h"

#include "render.h"

static event_t e;
static softTimer_t screenTimer = {.next = NULL, .prev = NULL};
static softTimer_t encTimer = {.next = NULL, .prev = NULL};
static softTimer_t midiPollTimer = {.next = NULL, .prev = NULL};
static softTimer_t dspPollTimer = {.next = NULL, .prev = NULL};
static softTimer_t adcPollTimer = {.next = NULL, .prev = NULL};

/* runs in the TC ISR: post only, so the screen SPI stays on the main loop */
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
  /* asynchronous, non-blocking; UHC callbacks post MIDI events */
  midi_read();
}

/* post AppCustom so main loop can SPI-read xruns / meters */
static void dsp_poll_callback(void *obj) {
  (void)obj;
  e.type = kEventAppCustom;
  e.data = 0;
  event_post(&e);
}

static void adc_poll_timer_callback(void *obj) {
  (void)obj;
  adc_poll();
}

void init_app_timers(void) {
  timer_add(&screenTimer, 50, &screen_timer_callback, NULL);
  timer_add(&encTimer, 50, &enc_timer_callback, NULL);
  /* ~10 Hz — meters feel alive; xrun SPI is cheap at this rate */
  timer_add(&dspPollTimer, 100, &dsp_poll_callback, NULL);
}

void timers_set_midi(void) {
  timer_add(&midiPollTimer, 1, &midi_poll_timer_callback, NULL);
}

void timers_unset_midi(void) { timer_remove(&midiPollTimer); }

void timers_set_adc(u32 period) {
  timer_add(&adcPollTimer, period, &adc_poll_timer_callback, NULL);
}

void timers_unset_adc(void) { timer_remove(&adcPollTimer); }
