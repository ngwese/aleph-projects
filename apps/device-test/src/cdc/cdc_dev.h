#ifndef DEVICE_TEST_CDC_DEV_H
#define DEVICE_TEST_CDC_DEV_H

#include "cdc_kind.h"
#include "types.h"

typedef struct {
  u8 connected;
  u16 vid;
  u16 pid;
  cdc_kind_t kind;
} cdc_dev_info_t;

void cdc_dev_clear(void);

/*
 * Snapshot identity from the uhc_device_t* posted on kEventSerialConnect,
 * classify (IDs then bounded mext probe), and for MONOME call
 * monome_setup_mext().
 */
void cdc_dev_on_connect(s32 event_data);
void cdc_dev_on_disconnect(void);

const cdc_dev_info_t *cdc_dev_info(void);

#endif
