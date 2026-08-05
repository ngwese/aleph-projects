#include "setup_file.h"

#include <string.h>

#include "app.h"
#include "compiler.h"
#include "filesystem.h"
#include "print_funcs.h"

#include "between_limits.h"
#include "kvtext.h"
#include "lineio_fl.h"
#include "render.h"

static void strip_txt_ext(char *str) {
  u32 nlen;
  u32 elen;
  const char *ext = ".txt";
  if (str == NULL) {
    return;
  }
  nlen = (u32)strlen(str);
  elen = (u32)strlen(ext);
  if (nlen > elen && strcmp(str + nlen - elen, ext) == 0) {
    str[nlen - elen] = '\0';
  }
}

static void make_path(char *path, const char *stem) {
  char s[BETWEEN_NAME_LEN];
  strncpy(s, stem != NULL ? stem : "", BETWEEN_NAME_LEN - 1);
  s[BETWEEN_NAME_LEN - 1] = '\0';
  strip_txt_ext(s);
  strcpy(path, BETWEEN_SETUP_PATH);
  strcat(path, s);
  strcat(path, ".txt");
}

SetupIoStatus setup_file_load(const char *stem, SetupData *out) {
  char path[BETWEEN_PATH_MAX];
  LineIO io;
  void *fp;
  SetupIoStatus st;

  make_path(path, stem);
  print_dbg("\r\n between; setup load ");
  print_dbg(path);
  app_pause();
  fp = fl_fopen(path, "r");
  if (fp == NULL) {
    app_resume();
    print_dbg(" open fail");
    return eSetupIoMalformed;
  }
  lineio_fl_bind(&io, fp);
  st = setup_io_read(&io, out);
  fl_fclose(fp);
  app_resume();
  print_dbg(" status=");
  print_dbg_ulong((u32)st);
  return st;
}

SetupIoStatus setup_file_save(const char *stem, const SetupData *data) {
  char path[BETWEEN_PATH_MAX];
  LineIO io;
  void *fp;
  SetupIoStatus st;

  render_log("write setup...");
  make_path(path, stem);
  app_pause();
  fp = fl_fopen(path, "w");
  if (fp == NULL) {
    app_resume();
    return eSetupIoWriteFail;
  }
  lineio_fl_bind(&io, fp);
  st = setup_io_write(&io, data);
  if (st == eSetupIoOk) {
    if (!lineio_fl_close_written(fp)) {
      st = eSetupIoWriteFail;
    }
  } else {
    fl_fclose(fp);
  }
  app_resume();
  return st;
}

u8 setup_file_delete(const char *stem) {
  char path[BETWEEN_PATH_MAX];
  int rc;
  if (stem == NULL || stem[0] == '\0') {
    return 0;
  }
  render_log("delete setup...");
  make_path(path, stem);
  app_pause();
  rc = fl_remove(path);
  app_resume();
  return (u8)(rc == 0);
}

u8 setup_file_exists(const char *stem) {
  char path[BETWEEN_PATH_MAX];
  void *fp;
  if (stem == NULL || stem[0] == '\0') {
    return 0;
  }
  make_path(path, stem);
  app_pause();
  fp = fl_fopen(path, "r");
  if (fp == NULL) {
    app_resume();
    return 0;
  }
  fl_fclose(fp);
  app_resume();
  return 1;
}

u8 setup_file_write_state(const char *stem) {
  void *fp;
  LineIO io;
  char line[BETWEEN_LINE_MAX];

  if (stem == NULL) {
    return 0;
  }
  render_log("write state...");
  app_pause();
  fp = fl_fopen(BETWEEN_STATE_PATH, "w");
  if (fp == NULL) {
    app_resume();
    return 0;
  }
  lineio_fl_bind(&io, fp);
  strcpy(line, "setup:");
  strcat(line, stem);
  strcat(line, "\n");
  if (!io.write_line(line, io.ctx)) {
    fl_fclose(fp);
    app_resume();
    return 0;
  }
  if (!lineio_fl_close_written(fp)) {
    app_resume();
    return 0;
  }
  app_resume();
  return 1;
}

u8 setup_file_read_state(char *stem, u32 stem_size) {
  void *fp;
  LineIO io;
  char line[BETWEEN_LINE_MAX];
  KvPair pair;

  if (stem == NULL || stem_size == 0) {
    return 0;
  }
  render_log("read state...");
  app_pause();
  fp = fl_fopen(BETWEEN_STATE_PATH, "r");
  if (fp == NULL) {
    app_resume();
    return 0;
  }
  lineio_fl_bind(&io, fp);
  if (!io.read_line(line, BETWEEN_LINE_MAX, io.ctx)) {
    fl_fclose(fp);
    app_resume();
    return 0;
  }
  fl_fclose(fp);
  app_resume();

  if (kvtext_parse_line(line, &pair) != eKvPair) {
    return 0;
  }
  if (!kvtext_key_eq(pair.key, "setup")) {
    return 0;
  }
  strncpy(stem, pair.val, stem_size - 1);
  stem[stem_size - 1] = '\0';
  return 1;
}
