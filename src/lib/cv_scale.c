#include "cv_scale.h"

#define CV_IN_ADC_MAX 4095u
#define CV_IN_FR32_MAX 0x7fffffffu

fract32 cv_in_adc_to_fr32(u16 adc12) {
  u16 a = (u16)(adc12 & 0xfffu);
  if(a >= CV_IN_ADC_MAX) {
    return (fract32)CV_IN_FR32_MAX;
  }
  return (fract32)(((s64)a * (s64)CV_IN_FR32_MAX) / (s64)CV_IN_ADC_MAX);
}
