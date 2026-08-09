#include "hid_mouse.h"

#include <string.h>

static s16 read_s16_le(const u8 *p) {
  return (s16)((u16)p[0] | ((u16)p[1] << 8));
}

static bool parse_boot_body(const u8 *p, u8 rem, hid_mouse_state_t *out) {
  if (rem < 3) {
    return false;
  }
  out->buttons = p[0];
  out->dx = (s16)(s8)p[1];
  out->dy = (s16)(s8)p[2];
  if (rem >= 4) {
    out->wheel = (s8)p[3];
  }
  return true;
}

static bool parse_body(const u8 *p, u8 rem, hid_mouse_state_t *out) {
  if (rem < 3) {
    return false;
  }

  /* Large buffers are almost always boot-shaped with max-packet padding. */
  if (rem > 8) {
    return parse_boot_body(p, 4, out);
  }

  out->buttons = p[0];

  if (rem >= 5 && rem <= 7) {
    /* buttons + 16-bit dx/dy [+ wheel] */
    out->dx = read_s16_le(p + 1);
    out->dy = read_s16_le(p + 3);
    if (rem >= 6) {
      out->wheel = (s8)p[5];
    }
    return true;
  }

  return parse_boot_body(p, rem, out);
}

bool hid_mouse_parse(const u8 *data, u8 size, bool report_protocol,
                     hid_mouse_state_t *out) {
  if (data == NULL || out == NULL) {
    return false;
  }

  memset(out, 0, sizeof(*out));

  if (!report_protocol) {
    return parse_boot_body(data, size > 4 ? 4 : size, out);
  }

  /* Report ID only for compact reports (not padded max-packet buffers). */
  if (size >= 4 && size <= 8) {
    /* Try without ID first when size is classic boot 3/4. */
    if (size <= 4) {
      return parse_body(data, size, out);
    }
    /* size 5 or 7 often means ID + boot/16-bit body */
    if (size == 5 || size == 7) {
      out->has_report_id = 1;
      out->report_id = data[0];
      if (parse_body(data + 1, (u8)(size - 1), out)) {
        return true;
      }
      memset(out, 0, sizeof(*out));
    }
  }

  return parse_body(data, size, out);
}
