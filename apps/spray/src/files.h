#ifndef _ALEPH_APP_SPRAY_FILES_H_
#define _ALEPH_APP_SPRAY_FILES_H_

#include "types.h"

#define DSP_PATH "/mod/"
#define DEFAULT_LDR "spray.ldr"

// Load a Blackfin LDR from /mod/ by filename (with or without .ldr).
// Returns 1 on success, 0 on failure.
extern u8 files_load_dsp(const char* name);

#endif
