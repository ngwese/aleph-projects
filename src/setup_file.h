#ifndef BETWEEN_SETUP_FILE_H
#define BETWEEN_SETUP_FILE_H

#include "setup_io.h"
#include "types.h"

SetupIoStatus setup_file_load(const char *stem, SetupData *out);
SetupIoStatus setup_file_save(const char *stem, const SetupData *data);
u8 setup_file_delete(const char *stem);

u8 setup_file_write_state(const char *stem);
u8 setup_file_read_state(char *stem, u32 stem_size);

#endif
