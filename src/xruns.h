#ifndef BETWEEN_XRUNS_H
#define BETWEEN_XRUNS_H

#include "bfin.h"
#include "types.h"

/* cached DSP xrun counters (polled ~2 Hz). */
const bfin_xrun_t *xruns_get(void);
u8 xruns_any(void);
/* SPI-read counters; returns 1 if cache or warn flag changed. */
u8 xruns_poll(void);
/* zero local cache and clear header warn (DSP already reset on enable). */
void xruns_clear_local(void);

#endif
