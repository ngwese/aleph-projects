#ifndef DEVICE_TEST_HID_GAMEPAD_H
#define DEVICE_TEST_HID_GAMEPAD_H

#include "compiler.h"
#include "types.h"

typedef struct {
  s8 x;
  s8 y;
  s8 z;
  s8 rz;
  u8 rx;
  u8 ry;
  u8 hat; /* 0-7 direction, 8 = center/none */
  u32 buttons;
} hid_gamepad_state_t;

/* TinyUSB-style packed report: 12 bytes (+ optional Report ID, + pad to 16). */
bool hid_gamepad_parse(const u8 *data, u8 size, hid_gamepad_state_t *out);

#endif
