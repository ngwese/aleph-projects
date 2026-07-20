#include "preset_file.h"

#include <string.h>

#include "compiler.h"
#include "app.h"
#include "filesystem.h"

#include "between_limits.h"
#include "lineio_fl.h"
#include "render.h"

static void make_path(char *path, const char *module, const char *stem) {
  strcpy(path, BETWEEN_PRESET_PATH);
  strcat(path, module);
  strcat(path, "/");
  strcat(path, stem);
  strcat(path, ".txt");
}

PresetIoStatus preset_file_load(const char *module, const char *stem,
				PresetMeta *meta, preset_on_param_fn on_param,
				void *ctx) {
  char path[BETWEEN_PATH_MAX];
  LineIO io;
  void *fp;
  PresetIoStatus st;

  render_log("read preset...");
  make_path(path, module, stem);
  app_pause();
  fp = fl_fopen(path, "r");
  if(fp == NULL) {
    app_resume();
    return ePresetIoEof;
  }
  render_log("parse preset...");
  lineio_fl_bind(&io, fp);
  st = preset_io_read(&io, meta, on_param, ctx);
  fl_fclose(fp);
  app_resume();
  return st;
}

PresetIoStatus preset_file_save(const char *module, const char *stem,
				const PresetMeta *meta,
				preset_next_param_fn next_param, void *ctx) {
  char path[BETWEEN_PATH_MAX];
  LineIO io;
  void *fp;
  PresetIoStatus st;

  render_log("write preset...");
  make_path(path, module, stem);
  app_pause();
  fp = fl_fopen(path, "w");
  if(fp == NULL) {
    app_resume();
    return ePresetIoWriteFail;
  }
  lineio_fl_bind(&io, fp);
  st = preset_io_write(&io, meta, next_param, ctx);
  fl_fclose(fp);
  app_resume();
  return st;
}

u8 preset_file_delete(const char *module, const char *stem) {
  char path[BETWEEN_PATH_MAX];
  int rc;
  if(module == NULL || stem == NULL || stem[0] == '\0') {
    return 0;
  }
  render_log("delete preset...");
  make_path(path, module, stem);
  app_pause();
  rc = fl_remove(path);
  app_resume();
  return (u8)(rc == 0);
}

u8 preset_file_exists(const char *module, const char *stem) {
  char path[BETWEEN_PATH_MAX];
  void *fp;
  if(module == NULL || stem == NULL || stem[0] == '\0') {
    return 0;
  }
  make_path(path, module, stem);
  app_pause();
  fp = fl_fopen(path, "r");
  if(fp == NULL) {
    app_resume();
    return 0;
  }
  fl_fclose(fp);
  app_resume();
  return 1;
}
