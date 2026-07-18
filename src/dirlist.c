#include "dirlist.h"

#include <string.h>

#include "compiler.h"
#include "app.h"
#include "filesystem.h"
#include "render.h"

static void strip_ext(char *str) {
  int i = (int)strlen(str);
  while(i > 0) {
    --i;
    if(str[i] == '.') {
      str[i] = '\0';
      return;
    }
  }
}

static u8 has_ext(const char *name, const char *ext) {
  u32 nlen;
  u32 elen;
  if(name == NULL || ext == NULL) {
    return 0;
  }
  nlen = (u32)strlen(name);
  elen = (u32)strlen(ext);
  if(nlen <= elen) {
    return 0;
  }
  return (u8)(strcmp(name + nlen - elen, ext) == 0);
}

u16 dirlist_scan(DirList *list, const char *path, const char *ext) {
  FL_DIR dirstat;
  struct fs_dir_ent dirent;

  if(list == NULL) {
    return 0;
  }
  list->count = 0;

  render_log("scan dir...");
  app_pause();
  if(!fl_opendir((char *)path, &dirstat)) {
    app_resume();
    return 0;
  }
  while(fl_readdir(&dirstat, &dirent) == 0) {
    char full[64];
    char stem[BETWEEN_NAME_LEN];
    if(dirent.is_dir) {
      continue;
    }
    if(!has_ext(dirent.filename, ext)) {
      continue;
    }
    if(list->count >= BETWEEN_DIR_MAX) {
      break;
    }
    /* strip extension before truncating to BETWEEN_NAME_LEN */
    strncpy(full, dirent.filename, sizeof(full) - 1);
    full[sizeof(full) - 1] = '\0';
    strip_ext(full);
    strncpy(stem, full, BETWEEN_NAME_LEN - 1);
    stem[BETWEEN_NAME_LEN - 1] = '\0';
    strncpy(list->names[list->count], stem, BETWEEN_NAME_LEN - 1);
    list->names[list->count][BETWEEN_NAME_LEN - 1] = '\0';
    list->count++;
  }
  app_resume();
  return list->count;
}
