#ifndef DEVICE_TEST_HID_KBD_H
#define DEVICE_TEST_HID_KBD_H

#include "compiler.h"
#include "types.h"

#define HID_KBD_KEYS_MAX 14

typedef struct {
  u8 modifiers;
  u8 key_count;
  u8 keys[HID_KBD_KEYS_MAX];
  u8 has_report_id;
  u8 report_id;
} hid_kbd_state_t;

/* Boot: 8-byte mod|reserved|key[6].
 * Report: same array shape; optional leading Report ID; longer key arrays. */
bool hid_kbd_parse(const u8 *data, u8 size, bool report_protocol,
                   hid_kbd_state_t *out);

#endif
