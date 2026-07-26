#ifndef BETWEEN_APP_TIMERS_H
#define BETWEEN_APP_TIMERS_H

#include "types.h"

void init_app_timers(void);

/* start/stop 1 ms USB-MIDI poll (midi_read). */
void timers_set_midi(void);
void timers_unset_midi(void);

/* start/stop ADC poll (adc_poll → kEventAdc0..3). period in ms. */
void timers_set_adc(u32 period);
void timers_unset_adc(void);

#endif
