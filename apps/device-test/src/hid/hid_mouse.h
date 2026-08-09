#ifndef DEVICE_TEST_HID_MOUSE_H
#define DEVICE_TEST_HID_MOUSE_H

#include "compiler.h"
#include "types.h"

typedef struct {
  u8 buttons;
  s16 dx;
  s16 dy;
  s8 wheel;
  u8 has_report_id;
  u8 report_id;
} hid_mouse_state_t;

/* Boot: buttons|dx|dy[|wheel] (8-bit deltas).
 * Report: optional Report ID; 8- or 16-bit deltas by remaining length. */
bool hid_mouse_parse(const u8 *data, u8 size, bool report_protocol,
                     hid_mouse_state_t *out);

#endif
