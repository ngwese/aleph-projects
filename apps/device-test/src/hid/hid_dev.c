#include "hid_dev.h"

#include <string.h>

#include "hid.h"
#include "uhc.h"
#include "uhd.h"
#include "usb_protocol.h"
#include "usb_protocol_hid.h"

typedef struct {
  u8 valid;
  u8 iface_num;
  u8 protocol;
  u8 subclass;
  u8 ep_addr;
  u8 report_size;
  usb_ep_desc_t ep_desc;
  u8 allocated; /* we called uhd_ep_alloc (not iface 0 / uhi_hid) */
  u8 running;
  u8 dirty;
  u8 size;
  u8 boot_mode; /* parse as boot after SET_PROTOCOL(BOOT) */
  u8 boot_set;  /* already attempted SET_PROTOCOL */
} hid_iface_slot_t;

static hid_dev_info_t info;
static hid_iface_slot_t slots[HID_IFACE_MAX];
static COMPILER_WORD_ALIGNED u8 slot_reports[HID_IFACE_MAX][HID_FRAME_MAX_BYTES];
static uhc_device_t *hid_uhc_dev;
static usb_add_t hid_usb_addr;

static volatile bool setup_done;
static volatile bool setup_ok;

static void apply_selected(void);
static void start_slot_rx(u8 idx);
static void maybe_set_boot_protocol(u8 idx);
static void slot_rx_callback(usb_add_t add, usb_ep_t ep,
                             uhd_trans_status_t status,
                             iram_size_t nb_transfered);

void hid_dev_clear(void) {
  u8 i;

  for (i = 0; i < HID_IFACE_MAX; i++) {
    if (slots[i].allocated && hid_uhc_dev != NULL) {
      uhd_ep_abort(hid_usb_addr, slots[i].ep_addr);
      uhd_ep_free(hid_usb_addr, slots[i].ep_addr);
    }
  }
  memset(slots, 0, sizeof(slots));
  memset(&info, 0, sizeof(info));
  hid_uhc_dev = NULL;
  hid_usb_addr = 0;
}

static void walk_hid_ifaces(uhc_device_t *dev) {
  uint16_t conf_desc_lgt;
  usb_iface_desc_t *ptr_iface;
  s8 cur = -1;
  u8 got_ep = 0;

  if (dev == NULL || dev->conf_desc == NULL) {
    return;
  }

  conf_desc_lgt = le16_to_cpu(dev->conf_desc->wTotalLength);
  ptr_iface = (usb_iface_desc_t *)dev->conf_desc;

  while (conf_desc_lgt) {
    switch (ptr_iface->bDescriptorType) {
    case USB_DT_INTERFACE:
      got_ep = 0;
      cur = -1;
      if (ptr_iface->bInterfaceClass == HID_CLASS &&
          info.hid_iface_count < HID_IFACE_MAX) {
        cur = (s8)info.hid_iface_count;
        slots[cur].valid = 1;
        slots[cur].iface_num = ptr_iface->bInterfaceNumber;
        slots[cur].protocol = ptr_iface->bInterfaceProtocol;
        slots[cur].subclass = ptr_iface->bInterfaceSubClass;
        info.hid_iface_count++;
      }
      break;

    case USB_DT_ENDPOINT:
      if (cur >= 0 && !got_ep) {
        usb_ep_desc_t *ep = (usb_ep_desc_t *)ptr_iface;
        if (ep->bEndpointAddress & USB_EP_DIR_IN) {
          slots[cur].ep_addr = ep->bEndpointAddress;
          slots[cur].report_size = (u8)le16_to_cpu(ep->wMaxPacketSize);
          if (slots[cur].report_size > HID_FRAME_MAX_BYTES) {
            slots[cur].report_size = HID_FRAME_MAX_BYTES;
          }
          slots[cur].ep_desc = *ep;
          slots[cur].size = slots[cur].report_size;
          got_ep = 1;
        }
      }
      break;

    default:
      break;
    }

    if (conf_desc_lgt < ptr_iface->bLength) {
      break;
    }
    conf_desc_lgt = (uint16_t)(conf_desc_lgt - ptr_iface->bLength);
    ptr_iface =
        (usb_iface_desc_t *)((uint8_t *)ptr_iface + ptr_iface->bLength);
  }
}

static void apply_selected(void) {
  hid_iface_slot_t *s;

  if (info.iface_index >= info.hid_iface_count) {
    info.iface_index = 0;
  }
  if (info.hid_iface_count == 0) {
    info.iface_protocol = 0;
    info.iface_subclass = 0;
    info.report_size = 0;
    info.kind = HID_KIND_UNKNOWN;
    return;
  }

  s = &slots[info.iface_index];
  info.iface_protocol = s->protocol;
  info.iface_subclass = s->subclass;
  info.report_size = s->report_size;
  info.kind = hid_classify(info.iface_protocol, info.report_size);
}

static void setup_request_end(usb_add_t add, uhd_trans_status_t status,
                              uint16_t payload_trans) {
  (void)add;
  (void)payload_trans;
  setup_ok = (status == UHD_TRANS_NOERROR);
  setup_done = true;
}

static bool hid_set_protocol_boot(u8 iface_num) {
  usb_setup_req_t req;

  if (hid_uhc_dev == NULL) {
    return false;
  }

  /* HID class SET_PROTOCOL(BOOT), interface recipient, host-to-device */
  req.bmRequestType = USB_REQ_TYPE_CLASS | USB_REQ_RECIP_INTERFACE;
  req.bRequest = USB_REQ_HID_SET_PROTOCOL;
  req.wValue = USB_HID_PROCOTOL_BOOT;
  req.wIndex = iface_num;
  req.wLength = 0;

  setup_done = false;
  setup_ok = false;
  if (!uhd_setup_request(hid_usb_addr, &req, NULL, 0, NULL,
                         setup_request_end)) {
    return false;
  }
  while (!setup_done) {
    /* USB host IRQs complete the control transfer */
  }
  return setup_ok;
}

static void maybe_set_boot_protocol(u8 idx) {
  hid_iface_slot_t *s;

  if (idx >= info.hid_iface_count || idx >= HID_IFACE_MAX) {
    return;
  }
  s = &slots[idx];
  if (!s->valid) {
    return;
  }
  if (s->protocol != HID_IFACE_PROTO_KEYBOARD &&
      s->protocol != HID_IFACE_PROTO_MOUSE) {
    s->boot_mode = 0;
    return;
  }
  if (s->boot_set) {
    return;
  }

  s->boot_set = 1;
  if (hid_set_protocol_boot(s->iface_num)) {
    s->boot_mode = 1;
  } else {
    s->boot_mode = 0;
  }
}

static void start_slot_rx(u8 idx) {
  hid_iface_slot_t *s;

  if (idx == 0 || idx >= info.hid_iface_count || hid_uhc_dev == NULL) {
    return;
  }

  s = &slots[idx];
  if (!s->valid || s->ep_addr == 0) {
    return;
  }

  if (!s->allocated) {
    if (!uhd_ep_alloc(hid_usb_addr, &s->ep_desc)) {
      return;
    }
    s->allocated = 1;
  }

  if (s->running) {
    return;
  }

  s->running = 1;
  uhd_ep_run(hid_usb_addr, s->ep_addr, true, slot_reports[idx], s->report_size,
             0, slot_rx_callback);
}

static void slot_rx_callback(usb_add_t add, usb_ep_t ep,
                             uhd_trans_status_t status,
                             iram_size_t nb_transfered) {
  u8 i;
  u8 idx = 0;
  hid_iface_slot_t *s = NULL;

  for (i = 1; i < info.hid_iface_count && i < HID_IFACE_MAX; i++) {
    if (slots[i].valid && slots[i].ep_addr == ep) {
      s = &slots[i];
      idx = i;
      break;
    }
  }

  if (s == NULL) {
    return;
  }

  if ((status == UHD_TRANS_NOERROR) && (nb_transfered >= 1)) {
    s->size = s->report_size;
    s->dirty = 1;
  }

  if (s->running && info.connected) {
    uhd_ep_run(add, s->ep_addr, true, slot_reports[idx], s->report_size, 0,
               slot_rx_callback);
  }
}

void hid_dev_on_connect(s32 event_data) {
  uhc_device_t *dev = (uhc_device_t *)event_data;

  hid_dev_clear();
  info.connected = 1;
  info.iface_index = 0;
  hid_uhc_dev = dev;

  if (dev != NULL) {
    info.vid = le16_to_cpu(dev->dev_desc.idVendor);
    info.pid = le16_to_cpu(dev->dev_desc.idProduct);
    hid_usb_addr = dev->address;
    walk_hid_ifaces(dev);
  }

  apply_selected();
  maybe_set_boot_protocol(info.iface_index);

  /* Drop the host's power-on "all bytes dirty" zero frame on iface 0. */
  hid_clear_frame_dirty();
}

void hid_dev_on_disconnect(void) { hid_dev_clear(); }

const hid_dev_info_t *hid_dev_info(void) { return &info; }

bool hid_dev_report_protocol(void) {
  if (info.iface_index < info.hid_iface_count) {
    /* After SET_PROTOCOL(BOOT), use boot parsers even if subclass is noboot. */
    return slots[info.iface_index].boot_mode == 0;
  }
  return info.iface_subclass != HID_IFACE_SUBCLASS_BOOT;
}

hid_kind_t hid_dev_kind_for_size(u8 report_size) {
  u8 sz = report_size;

  if (sz == 0) {
    sz = info.report_size;
  }
  if (info.iface_protocol == HID_IFACE_PROTO_KEYBOARD) {
    return HID_KIND_KEYBOARD;
  }
  if (info.iface_protocol == HID_IFACE_PROTO_MOUSE) {
    return HID_KIND_MOUSE;
  }
  return hid_classify(info.iface_protocol, sz);
}

hid_kind_t hid_dev_guess_kind(const u8 *data, u8 size) {
  if (size >= 8 && data[1] == 0) {
    u8 i;
    u8 ok = 1;
    for (i = 2; i < 8; i++) {
      if (data[i] != 0 && data[i] < 0x04) {
        ok = 0;
        break;
      }
    }
    if (ok) {
      return HID_KIND_KEYBOARD;
    }
  }
  if (size >= 12 && size <= 16) {
    return HID_KIND_GAMEPAD;
  }
  if (size >= 3 && size <= 4) {
    return HID_KIND_MOUSE;
  }
  return HID_KIND_UNKNOWN;
}

bool hid_dev_next_iface(void) {
  if (!info.connected || info.hid_iface_count < 2) {
    return false;
  }

  info.iface_index++;
  if (info.iface_index >= info.hid_iface_count) {
    info.iface_index = 0;
  }

  apply_selected();
  maybe_set_boot_protocol(info.iface_index);
  start_slot_rx(info.iface_index);

  if (info.iface_index == 0) {
    hid_clear_frame_dirty();
  } else {
    slots[info.iface_index].dirty = 0;
  }

  return true;
}

u8 hid_dev_frame_dirty(void) {
  if (!info.connected) {
    return 0;
  }
  if (info.iface_index == 0) {
    return hid_get_frame_dirty() != 0;
  }
  return slots[info.iface_index].dirty;
}

const volatile u8 *hid_dev_frame_data(void) {
  if (info.iface_index == 0) {
    return hid_get_frame_data();
  }
  return (const volatile u8 *)slot_reports[info.iface_index];
}

u8 hid_dev_frame_size(void) {
  if (info.iface_index == 0) {
    return (u8)hid_get_frame_size();
  }
  return slots[info.iface_index].size;
}

void hid_dev_clear_frame_dirty(void) {
  if (info.iface_index == 0) {
    hid_clear_frame_dirty();
  } else {
    slots[info.iface_index].dirty = 0;
  }
}
