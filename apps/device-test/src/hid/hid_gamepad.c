#include "hid_gamepad.h"

#include <string.h>

#define HID_GAMEPAD_CORE_BYTES 11

static void decode_core(const u8 *p, hid_gamepad_state_t *out) {
  out->x = (s8)p[0];
  out->y = (s8)p[1];
  out->z = (s8)p[2];
  out->rz = (s8)p[3];
  out->rx = p[4];
  out->ry = p[5];
  out->hat = p[6];
  out->buttons = (u32)p[7] | ((u32)p[8] << 8) | ((u32)p[9] << 16) |
                 ((u32)p[10] << 24);
}

bool hid_gamepad_parse(const u8 *data, u8 size, hid_gamepad_state_t *out) {
  if (data == NULL || out == NULL) {
    return false;
  }

  memset(out, 0, sizeof(*out));

  /* 12-16 byte report: core 11 + padding; ignore trailing pad */
  if (size >= 12 && size <= 16) {
    decode_core(data, out);
    return true;
  }

  /* optional Report ID + 12-byte core (+ pad): sizes 13-17 */
  if (size >= 13 && size <= 17) {
    decode_core(data + 1, out);
    return true;
  }

  return false;
}
