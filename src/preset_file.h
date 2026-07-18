#ifndef BETWEEN_PRESET_FILE_H
#define BETWEEN_PRESET_FILE_H

#include "preset_io.h"
#include "types.h"

PresetIoStatus preset_file_load(const char *module, const char *stem,
				PresetMeta *meta, preset_on_param_fn on_param,
				void *ctx);

PresetIoStatus preset_file_save(const char *module, const char *stem,
				const PresetMeta *meta,
				preset_next_param_fn next_param, void *ctx);

u8 preset_file_delete(const char *module, const char *stem);

/* true if /data/between/presets/<module>/<stem>.txt exists. */
u8 preset_file_exists(const char *module, const char *stem);

#endif
