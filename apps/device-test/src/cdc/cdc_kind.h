#ifndef DEVICE_TEST_CDC_KIND_H
#define DEVICE_TEST_CDC_KIND_H

#include "compiler.h"
#include "types.h"

/* known CDC USB IDs */
#define CDC_VID_CROW 0x0483
#define CDC_PID_CROW 0x5740

#define CDC_VID_MONOME_III 0xCAFE
#define CDC_PID_MONOME_III 0x1110

typedef enum {
  CDC_KIND_UNKNOWN = 0,
  CDC_KIND_MONOME,
  CDC_KIND_CROW,
} cdc_kind_t;

/* classify from USB VID/PID only (no probe). */
static inline cdc_kind_t cdc_classify_ids(u16 vid, u16 pid) {
  if (vid == CDC_VID_CROW && pid == CDC_PID_CROW) {
    return CDC_KIND_CROW;
  }
  if (vid == CDC_VID_MONOME_III && pid == CDC_PID_MONOME_III) {
    return CDC_KIND_MONOME;
  }
  return CDC_KIND_UNKNOWN;
}

#endif
