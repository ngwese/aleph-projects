#ifndef DEVICE_TEST_HID_DEV_H
#define DEVICE_TEST_HID_DEV_H

#include "hid_report.h"
#include "types.h"

#define HID_IFACE_MAX 4

typedef struct {
  u8 connected;
  u16 vid;
  u16 pid;
  u8 iface_protocol;
  u8 iface_subclass;
  u8 report_size; /* wMaxPacketSize of selected HID IN EP */
  u8 hid_iface_count;
  u8 iface_index; /* 0 .. hid_iface_count-1 */
  hid_kind_t kind;
} hid_dev_info_t;

void hid_dev_clear(void);
/* Snapshot identity from the uhc_device_t* posted on kEventHidConnect. */
void hid_dev_on_connect(s32 event_data);
void hid_dev_on_disconnect(void);

const hid_dev_info_t *hid_dev_info(void);
bool hid_dev_report_protocol(void);
hid_kind_t hid_dev_kind_for_size(u8 report_size);
hid_kind_t hid_dev_guess_kind(const u8 *data, u8 size);

/* Cycle to the next HID interface when count > 1. Returns true if changed. */
bool hid_dev_next_iface(void);

/* Frame accessors for the selected interface (iface 0 uses stock hid_*). */
u8 hid_dev_frame_dirty(void);
const volatile u8 *hid_dev_frame_data(void);
u8 hid_dev_frame_size(void);
void hid_dev_clear_frame_dirty(void);

#endif
