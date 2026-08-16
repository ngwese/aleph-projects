#include "cdc_probe.h"

#include "cdc.h"
#include "delay.h"
#include "print_funcs.h"
#include "types.h"

#define PROBE_SLICE_US 100
#define PROBE_MAX_SLICES 200 /* 20 ms per wait */
#define PROBE_TRIES 8

static u8 wait_tx_idle(void) {
  u16 n = 0;

  while (cdc_tx_busy()) {
    if (!cdc_connected()) {
      return 0;
    }
    if (++n > PROBE_MAX_SLICES) {
      return 0;
    }
    delay_us(PROBE_SLICE_US);
  }
  return 1;
}

static u8 wait_rx_idle(void) {
  u16 n = 0;

  while (cdc_rx_busy()) {
    if (!cdc_connected()) {
      return 0;
    }
    if (++n > PROBE_MAX_SLICES) {
      return 0;
    }
    delay_us(PROBE_SLICE_US);
  }
  return 1;
}

u8 cdc_probe_mext_identity(void) {
  u8 cmd;
  u8 *prx;
  u8 tries;

  if (!cdc_connected()) {
    print_dbg("\r\ncdc_probe: not connected");
    return 0;
  }

  print_dbg("\r\ncdc_probe mext query");

  for (tries = 0; tries < PROBE_TRIES && cdc_connected(); tries++) {
    cmd = 0x00; /* mext query */
    if (!wait_tx_idle()) {
      print_dbg("\r\ncdc_probe tx idle timeout");
      return 0;
    }
    cdc_write(&cmd, 1);
    if (!wait_tx_idle()) {
      continue;
    }
    cdc_read();
    if (!wait_rx_idle()) {
      continue;
    }
    if (cdc_rx_bytes() < 3) {
      continue;
    }
    prx = cdc_rx_buf();
    /* CDC mext: 0x00, type, count — type 1=grid, 5=arc */
    if (prx[0] != 0x00) {
      continue;
    }
    if (prx[1] == 1 || prx[1] == 5) {
      print_dbg("\r\ncdc_probe type=");
      print_dbg_ulong(prx[1]);
      return 1;
    }
  }
  return 0;
}
