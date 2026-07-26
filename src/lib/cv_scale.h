/* cv_scale — ADC (0–10 V) ↔ fract32 */

#ifndef BETWEEN_CV_SCALE_H
#define BETWEEN_CV_SCALE_H

/* module_common.h lives in common/, so its "types.h" is common/types.h (fract32). */
#include "module_common.h"

/* ADC full-scale (4095) = 10 V → FR32_MAX (0x7fffffff). */
fract32 cv_in_adc_to_fr32(u16 adc12);

#endif
