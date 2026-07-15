/*
  files.c

  aleph/app/spray

  Minimal SD loader for the companion Blackfin LDR under /mod/.
*/

#include <string.h>

#include "delay.h"
#include "print_funcs.h"

#include "app.h"
#include "bfin.h"
#include "filesystem.h"
#include "memory.h"

#include "files.h"

#define NAME_LEN 64

// byte-at-a-time read (fl_fread is unreliable for large files here)
static void fake_fread(volatile u8* dst, u32 len, void* fp) {
  u32 n = 0;
  while (n < len) {
    *dst = fl_fgetc(fp);
    n++;
    dst++;
  }
}

static void strip_ext(char* str) {
  int i = strlen(str);
  while (i > 0) {
    --i;
    if (str[i] == '.') {
      str[i] = '\0';
      return;
    }
  }
}

// Open /mod/<name>.ldr; set *size from the directory entry. Caller must fclose.
static void* open_dsp_file(const char* name, u32* size) {
  FL_DIR dirstat;
  struct fs_dir_ent dirent;
  char path[64];
  char nameTry[NAME_LEN];
  void* fp = NULL;

  *size = 0;

  strncpy(nameTry, name, NAME_LEN - 1);
  nameTry[NAME_LEN - 1] = '\0';
  strip_ext(nameTry);
  strncat(nameTry, ".ldr", NAME_LEN - strlen(nameTry) - 1);

  strcpy(path, DSP_PATH);
  if (!fl_opendir(path, &dirstat)) {
    print_dbg("\r\n spray; cannot open ");
    print_dbg(DSP_PATH);
    return NULL;
  }

  while (fl_readdir(&dirstat, &dirent) == 0) {
    if (strcmp(dirent.filename, nameTry) == 0) {
      strncat(path, dirent.filename, sizeof(path) - strlen(path) - 1);
      fp = fl_fopen(path, "r");
      *size = dirent.size;
      break;
    }
  }

  if (fp == NULL) {
    print_dbg("\r\n spray; LDR not found: ");
    print_dbg(DSP_PATH);
    print_dbg(nameTry);
  }

  return fp;
}

u8 files_load_dsp(const char* name) {
  void* fp;
  u32 size = 0;
  volatile u8* bfinLdrData = NULL;
  u8 ret = 0;

  delay_ms(10);
  app_pause();

  fp = open_dsp_file(name, &size);

  if (fp != NULL && size > 0) {
    if (size > BFIN_LDR_MAX_BYTES) {
      print_dbg("\r\n spray; LDR too large: 0x");
      print_dbg_hex(size);
      fl_fclose(fp);
    } else {
      print_dbg("\r\n spray; loading LDR size: 0x");
      print_dbg_hex(size);

      bfinLdrData = alloc_mem(size);
      if (bfinLdrData == NULL) {
        print_dbg("\r\n spray; alloc_mem failed for LDR");
        fl_fclose(fp);
      } else {
        fake_fread(bfinLdrData, size, fp);
        fl_fclose(fp);
        bfin_load_buf((const u8*)bfinLdrData, size);
        free_mem(bfinLdrData);
        ret = 1;
      }
    }
  } else if (fp != NULL) {
    print_dbg("\r\n spray; LDR empty");
    fl_fclose(fp);
  }

  app_resume();
  return ret;
}
