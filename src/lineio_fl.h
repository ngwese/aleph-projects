#ifndef BETWEEN_LINEIO_FL_H
#define BETWEEN_LINEIO_FL_H

#include "lineio.h"

/* bind LineIO to an open fat_io_lib file handle (void* from fl_fopen). */
void lineio_fl_bind(LineIO *io, void *fp);

#endif
