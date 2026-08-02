#ifndef DEVICE_TEST_APP_TIMERS_H
#define DEVICE_TEST_APP_TIMERS_H

#include "types.h"

void init_app_timers(void);

void timers_set_midi(void);
void timers_unset_midi(void);

void timers_set_monome(void);
void timers_unset_monome(void);

void timers_set_hid(void);
void timers_unset_hid(void);

#endif
