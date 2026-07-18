/* setup_io — streaming load/save of between setup text files */

#ifndef BETWEEN_SETUP_IO_H
#define BETWEEN_SETUP_IO_H

#include "lineio.h"
#include "module_common.h"
#include "morph2d.h"
#include "types.h"

#define SETUP_IO_FORMAT 1
#define SETUP_IO_LINE_MAX 160
#define SETUP_STEM_MAX 32

typedef enum {
  eSetupIoOk = 0,
  eSetupIoBadFormat,
  eSetupIoMissingMeta,
  eSetupIoMalformed,
  eSetupIoWriteFail
} SetupIoStatus;

typedef struct {
  u8 format;
  char module[MODULE_NAME_LEN];
  ModuleVersion version;
  char slot_stem[MORPH2D_SLOTS][SETUP_STEM_MAX];
  u8 slot_occupied[MORPH2D_SLOTS];
  u16 x; /* 0..MORPH2D_ONE */
  u16 y;
} SetupData;

SetupIoStatus setup_io_read(LineIO *io, SetupData *out);
SetupIoStatus setup_io_write(LineIO *io, const SetupData *data);

#endif
