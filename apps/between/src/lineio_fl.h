#ifndef BETWEEN_LINEIO_FL_H
#define BETWEEN_LINEIO_FL_H

#include "types.h"

#include "lineio.h"

/* bind LineIO to an open fat_io_lib file handle (void* from fl_fopen). */
void lineio_fl_bind(LineIO *io, void *fp);

/* Flush buffered sectors, confirm success, then close. Use after writes so the
 * next open cannot race a dirty FAT/file buffer. Returns 1 on success. */
u8 lineio_fl_close_written(void *fp);

#endif
