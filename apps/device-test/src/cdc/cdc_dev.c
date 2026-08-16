#include "cdc_dev.h"

#include "events.h"
#include "monome.h"
#include "print_funcs.h"
#include "uhc.h"

static cdc_dev_info_t info;

void cdc_dev_clear(void) {
  info.connected = 0;
  info.vid = 0;
  info.pid = 0;
  info.kind = CDC_KIND_UNKNOWN;
}

void cdc_dev_on_connect(s32 event_data) {
  uhc_device_t *dev = (uhc_device_t *)event_data;

  cdc_dev_clear();

  if (dev == NULL) {
    print_dbg("\r\ncdc_dev: null uhc_device");
    return;
  }

  info.connected = 1;
  info.vid = le16_to_cpu(dev->dev_desc.idVendor);
  info.pid = le16_to_cpu(dev->dev_desc.idProduct);
  info.kind = cdc_classify_ids(info.vid, info.pid);

  print_dbg("\r\ncdc_dev ");
  print_dbg_hex(info.vid);
  print_dbg(":");
  print_dbg_hex(info.pid);
  print_dbg(" ids=");
  print_dbg_ulong(info.kind);

  if (info.kind == CDC_KIND_MONOME) {
    print_dbg("\r\ncdc_dev -> monome_setup_mext");
    monome_setup_mext();
  }
}

void cdc_dev_on_disconnect(void) {
  event_t e;

  if (info.kind == CDC_KIND_MONOME) {
    e.type = kEventMonomeDisconnect;
    e.data = 0;
    event_post(&e);
  }
  cdc_dev_clear();
}

const cdc_dev_info_t *cdc_dev_info(void) { return &info; }
