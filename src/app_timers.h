#ifndef BETWEEN_APP_TIMERS_H
#define BETWEEN_APP_TIMERS_H

void init_app_timers(void);

/* start/stop 1 ms USB-MIDI poll (midi_read). */
void timers_set_midi(void);
void timers_unset_midi(void);

#endif
