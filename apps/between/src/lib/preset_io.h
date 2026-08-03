/* preset_io — streaming load/save of between preset text files */

#ifndef BETWEEN_PRESET_IO_H
#define BETWEEN_PRESET_IO_H

#include "lineio.h"
#include "module_common.h"
#include "types.h"

#define PRESET_IO_FORMAT 1
#define PRESET_IO_LINE_MAX 160

typedef enum {
  ePresetIoOk = 0,
  ePresetIoEof,
  ePresetIoBadFormat,
  ePresetIoMissingMeta,
  ePresetIoMalformed,
  ePresetIoWriteFail
} PresetIoStatus;

typedef struct {
  u8 format;
  char module[MODULE_NAME_LEN];
  ModuleVersion version;
} PresetMeta;

/* called for each param line after metadata. return 0 to abort. */
typedef u8 (*preset_on_param_fn)(const char *label, s32 value, void *ctx);

/* called to emit each param while writing. return 0 when done.
 * set *label / *value for the next param; return 1 to continue. */
typedef u8 (*preset_next_param_fn)(const char **label, s32 *value, void *ctx);

PresetIoStatus preset_io_read(LineIO *io, PresetMeta *meta,
                              preset_on_param_fn on_param, void *ctx);

PresetIoStatus preset_io_write(LineIO *io, const PresetMeta *meta,
                               preset_next_param_fn next_param, void *ctx);

/* helpers for version strings "maj.min.rev" */
u8 preset_io_parse_version(const char *s, ModuleVersion *out);
void preset_io_format_version(char *buf, u32 buf_size, const ModuleVersion *v);

#endif
