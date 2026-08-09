#include "hid_kbd.h"

#include <string.h>

/* Boot keyboard: [mod][reserved=0][key0..key5]. Host often reports
 * wMaxPacketSize (e.g. 64), so only the first 8 payload bytes matter. */
static bool fill_from_boot(const u8 *p, hid_kbd_state_t *out) {
  u8 i;

  out->modifiers = p[0];
  for (i = 0; i < 6; i++) {
    if (p[2 + i] != 0) {
      out->keys[out->key_count++] = p[2 + i];
    }
  }
  return true;
}

static bool looks_boot_at(const u8 *p) {
  /* reserved/OEM byte is normally 0 in boot and boot-compatible reports */
  return p[1] == 0;
}

bool hid_kbd_parse(const u8 *data, u8 size, bool report_protocol,
                   hid_kbd_state_t *out) {
  const u8 *p;
  u8 rem;
  u8 i;
  u8 nkeys;

  if (data == NULL || out == NULL || size < 8) {
    return false;
  }

  memset(out, 0, sizeof(*out));

  if (!report_protocol) {
    return fill_from_boot(data, out);
  }

  /* Report protocol: prefer boot-compatible layout at offset 0. Only treat
   * byte0 as Report ID when [id][mod][0][...] and offset 0 is not boot. */
  if (size >= 9 && !looks_boot_at(data) && looks_boot_at(data + 1)) {
    out->has_report_id = 1;
    out->report_id = data[0];
    p = data + 1;
    rem = (u8)(size - 1);
  } else if (looks_boot_at(data)) {
    return fill_from_boot(data, out);
  } else {
    p = data;
    rem = size;
  }

  if (rem < 2) {
    return false;
  }

  out->modifiers = p[0];
  nkeys = (u8)(rem - 2);
  if (nkeys > HID_KBD_KEYS_MAX) {
    nkeys = HID_KBD_KEYS_MAX;
  }
  /* Cap to a sensible boot-ish window when the buffer is a max-packet pad */
  if (nkeys > 6 && rem >= 8 && p[1] == 0) {
    nkeys = 6;
  }
  for (i = 0; i < nkeys; i++) {
    if (p[2 + i] != 0) {
      out->keys[out->key_count++] = p[2 + i];
    }
  }
  return true;
}
