#ifndef DEVICE_TEST_CDC_PROBE_H
#define DEVICE_TEST_CDC_PROBE_H

#include "types.h"

/*
 * bounded mext query (opcode 0x00). returns 1 if the CDC peer looks like
 * a monome grid (type 1) or arc (type 5). uses hard timeouts — never spins
 * forever on tx_busy/rx_busy.
 */
u8 cdc_probe_mext_identity(void);

#endif
