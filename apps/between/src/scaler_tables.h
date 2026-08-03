#ifndef BETWEEN_SCALER_TABLES_H
#define BETWEEN_SCALER_TABLES_H

#include "param_common.h"
#include "types.h"

/* load Bees scaler .dat files from SD into RAM. call after SD is ready. */
void scaler_tables_init(void);

/* 1 if this type needs no table, or all required tables loaded. */
u8 scaler_tables_ok(ParamType p);

/* base of PARAM_SCALER_DATA_SIZE byte blob (for NV accessors). */
u8 *scaler_tables_bytes(void);

#endif
