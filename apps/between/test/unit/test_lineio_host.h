#ifndef TEST_LINEIO_HOST_H
#define TEST_LINEIO_HOST_H

#include "lineio.h"

#include <stdio.h>
#include <string.h>

static u8 host_read_line(char *buf, u32 n, void *ctx) {
  FILE *fp = (FILE *)ctx;
  if (fp == NULL || fgets(buf, (int)n, fp) == NULL) {
    return 0;
  }
  return 1;
}

static u8 host_write_line(const char *s, void *ctx) {
  FILE *fp = (FILE *)ctx;
  if (fp == NULL || s == NULL) {
    return 0;
  }
  return fputs(s, fp) >= 0 ? 1 : 0;
}

static void host_lineio_init(LineIO *io, FILE *fp) {
  io->read_line = host_read_line;
  io->write_line = host_write_line;
  io->ctx = fp;
}

#endif
