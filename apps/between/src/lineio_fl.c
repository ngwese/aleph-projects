#include "lineio_fl.h"

#include <string.h>

#include "compiler.h"
#include "filesystem.h"

static u8 fl_read_line(char *buf, u32 n, void *ctx) {
  void *fp = ctx;
  if (fp == NULL || buf == NULL || n == 0) {
    return 0;
  }
  if (fl_fgets(buf, (int)n, fp) == NULL) {
    return 0;
  }
  return 1;
}

static u8 fl_write_line(const char *s, void *ctx) {
  void *fp = ctx;
  u32 len;
  if (fp == NULL || s == NULL) {
    return 0;
  }
  len = (u32)strlen(s);
  return (u8)(fl_fwrite((void *)s, 1, len, fp) == len);
}

void lineio_fl_bind(LineIO *io, void *fp) {
  if (io == NULL) {
    return;
  }
  io->read_line = fl_read_line;
  io->write_line = fl_write_line;
  io->ctx = fp;
}

u8 lineio_fl_close_written(void *fp) {
  if (fp == NULL) {
    return 0;
  }
  /* push the current file sector before fclose's fat_purge / next open */
  if (fl_fflush(fp) != 0) {
    fl_fclose(fp);
    return 0;
  }
  fl_fclose(fp);
  return 1;
}
