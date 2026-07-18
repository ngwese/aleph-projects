#include "setup_file.h"

#include <string.h>

#include "compiler.h"
#include "app.h"
#include "filesystem.h"

#include "between_limits.h"
#include "kvtext.h"
#include "lineio_fl.h"
#include "render.h"

static void make_path(char *path, const char *stem) {
  strcpy(path, BETWEEN_SETUP_PATH);
  strcat(path, stem);
  strcat(path, ".txt");
}

SetupIoStatus setup_file_load(const char *stem, SetupData *out) {
  char path[BETWEEN_PATH_MAX];
  LineIO io;
  void *fp;
  SetupIoStatus st;

  render_log("read setup...");
  make_path(path, stem);
  app_pause();
  fp = fl_fopen(path, "r");
  if(fp == NULL) {
    app_resume();
    return eSetupIoMalformed;
  }
  render_log("parse setup...");
  lineio_fl_bind(&io, fp);
  st = setup_io_read(&io, out);
  fl_fclose(fp);
  app_resume();
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
  if(fp == NULL) {
    app_resume();
    return eSetupIoWriteFail;
  }
  lineio_fl_bind(&io, fp);
  st = setup_io_write(&io, data);
  fl_fclose(fp);
  app_resume();
  return st;
}

u8 setup_file_delete(const char *stem) {
  char path[BETWEEN_PATH_MAX];
  void *fp;
  render_log("delete setup...");
  make_path(path, stem);
  app_pause();
  fp = fl_fopen(path, "w");
  if(fp == NULL) {
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

  if(stem == NULL) {
    return 0;
  }
  render_log("write state...");
  app_pause();
  fp = fl_fopen(BETWEEN_STATE_PATH, "w");
  if(fp == NULL) {
    app_resume();
    return 0;
  }
  lineio_fl_bind(&io, fp);
  strcpy(line, "setup:");
  strcat(line, stem);
  strcat(line, "\n");
  io.write_line(line, io.ctx);
  fl_fclose(fp);
  app_resume();
  return 1;
}

u8 setup_file_read_state(char *stem, u32 stem_size) {
  void *fp;
  LineIO io;
  char line[BETWEEN_LINE_MAX];
  KvPair pair;

  if(stem == NULL || stem_size == 0) {
    return 0;
  }
  render_log("read state...");
  app_pause();
  fp = fl_fopen(BETWEEN_STATE_PATH, "r");
  if(fp == NULL) {
    app_resume();
    return 0;
  }
  lineio_fl_bind(&io, fp);
  if(!io.read_line(line, BETWEEN_LINE_MAX, io.ctx)) {
    fl_fclose(fp);
    app_resume();
    return 0;
  }
  fl_fclose(fp);
  app_resume();

  if(kvtext_parse_line(line, &pair) != eKvPair) {
    return 0;
  }
  if(!kvtext_key_eq(pair.key, "setup")) {
    return 0;
  }
  strncpy(stem, pair.val, stem_size - 1);
  stem[stem_size - 1] = '\0';
  return 1;
}
