#ifndef DEVICE_TEST_HID_REPORT_H
#define DEVICE_TEST_HID_REPORT_H

#include "compiler.h"
#include "types.h"

/* USB HID interface protocol (bInterfaceProtocol) */
#define HID_IFACE_PROTO_GENERIC 0x00
#define HID_IFACE_PROTO_KEYBOARD 0x01
#define HID_IFACE_PROTO_MOUSE 0x02

/* USB HID interface subclass (bInterfaceSubClass) */
#define HID_IFACE_SUBCLASS_NOBOOT 0x00
#define HID_IFACE_SUBCLASS_BOOT 0x01

typedef enum {
  HID_KIND_UNKNOWN = 0,
  HID_KIND_KEYBOARD,
  HID_KIND_MOUSE,
  HID_KIND_GAMEPAD,
} hid_kind_t;

/*
 * Classify from interface protocol and interrupt max-packet size.
 * Note: hosts often report wMaxPacketSize (8/64), not the HID payload
 * length — protocol is the reliable signal; size is a weak hint only.
 */
static inline hid_kind_t hid_classify(u8 iface_protocol, u8 report_size) {
  (void)report_size;

  if (iface_protocol == HID_IFACE_PROTO_KEYBOARD) {
    return HID_KIND_KEYBOARD;
  }
  if (iface_protocol == HID_IFACE_PROTO_MOUSE) {
    return HID_KIND_MOUSE;
  }
  if (iface_protocol == HID_IFACE_PROTO_GENERIC) {
    /* Exact common payload sizes only — ignore 64-byte max-packet pads. */
    if (report_size >= 12 && report_size <= 16) {
      return HID_KIND_GAMEPAD;
    }
    if (report_size == 8) {
      return HID_KIND_KEYBOARD;
    }
    if (report_size >= 3 && report_size <= 4) {
      return HID_KIND_MOUSE;
    }
  }
  return HID_KIND_UNKNOWN;
}

#endif
