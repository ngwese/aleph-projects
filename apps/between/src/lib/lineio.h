/* lineio — abstract line-oriented read/write callbacks */

#ifndef BETWEEN_LINEIO_H
#define BETWEEN_LINEIO_H

#include "types.h"

/* read one line into buf (size n, including NUL). return 1 on success, 0 on
 * EOF/error. */
typedef u8 (*lineio_read_fn)(char *buf, u32 n, void *ctx);

/* write a null-terminated string (typically ending in \n). return 1 on success.
 */
typedef u8 (*lineio_write_fn)(const char *s, void *ctx);

typedef struct {
  lineio_read_fn read_line;
  lineio_write_fn write_line;
  void *ctx;
} LineIO;

#endif
