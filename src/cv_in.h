/* cv_in — panel CV ADC sample cache, play-map apply, poll gating */

#ifndef BETWEEN_CV_IN_H
#define BETWEEN_CV_IN_H

#include "cv_scale.h"
#include "types.h"

void cv_in_init(void);
void cv_in_handle_adc(u8 ch, u16 adc12);
const fract32 *cv_in_values(void); /* length PLAY_MAPS_CV_COUNT */

/* start/stop ADC soft-timer from play.cv maps | inspect hold. */
void cv_in_sync_poll(void);
void cv_in_set_inspect(u8 on);

#endif
