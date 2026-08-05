#ifndef BETWEEN_FILES_ENSURE_H
#define BETWEEN_FILES_ENSURE_H

#include "types.h"

/* create between data directories if missing; never delete or clear contents.
 * returns 1 if all required dirs exist afterward. */
u8 files_ensure_data_dirs(void);

/* ensure /data/between/presets/<module>/ exists (after module load / before
 * save). */
u8 files_ensure_preset_module_dir(const char *module);

#endif
