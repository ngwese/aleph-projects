#ifndef BETWEEN_METERS_H
#define BETWEEN_METERS_H

#include "bfin.h"
#include "types.h"

void meters_init(void);
/* Fetch IN + OUT banks over SPI; return 1 if either bank changed. */
u8 meters_poll(void);
const bfin_meter_bank_t *meters_in(void);
const bfin_meter_bank_t *meters_out(void);

#endif
