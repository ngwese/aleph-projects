#include "files_ensure.h"

#include <string.h>

#include "compiler.h"
#include "print_funcs.h"

#include "app.h"
#include "filesystem.h"

#include "between_limits.h"
#include "render.h"

/* return 1 if path opens as a directory. */
static u8 dir_exists(const char *path) {
  FL_DIR dirstat;
  if (path == NULL) {
    return 0;
  }
  if (fl_opendir(path, &dirstat) != NULL) {
    return 1;
  }
  return 0;
}

/* create leaf directory if missing. parents must already exist.
 * existing dirs are left untouched. */
static u8 ensure_dir(const char *path) {
  if (dir_exists(path)) {
    return 1;
  }
  print_dbg("\r\n between; mkdir ");
  print_dbg(path);
  if (!fl_createdirectory(path)) {
    print_dbg(" failed");
    return 0;
  }
  return dir_exists(path);
}

u8 files_ensure_data_dirs(void) {
  u8 ok = 1;
  render_log("check dirs...");
  app_pause();
  /* create in order — fl_createdirectory only makes the leaf */
  if (!ensure_dir("/data")) {
    ok = 0;
  }
  if (ok && !ensure_dir("/data/between")) {
    ok = 0;
  }
  if (ok && !ensure_dir("/data/between/presets")) {
    ok = 0;
  }
  if (ok && !ensure_dir("/data/between/setups")) {
    ok = 0;
  }
  app_resume();
  if (!ok) {
    render_log("dirs fail");
  }
  return ok;
}

u8 files_ensure_preset_module_dir(const char *module) {
  char path[BETWEEN_PATH_MAX];
  u8 ok;

  if (module == NULL || module[0] == '\0') {
    return 0;
  }
  /* base tree first (no-op if already present) */
  if (!files_ensure_data_dirs()) {
    return 0;
  }
  strcpy(path, BETWEEN_PRESET_PATH);
  strcat(path, module);

  app_pause();
  ok = ensure_dir(path);
  app_resume();
  return ok;
}
