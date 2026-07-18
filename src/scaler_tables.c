#include "scaler_tables.h"

#include <string.h>

#include "compiler.h"
#include "app.h"
#include "filesystem.h"
#include "param_scaler.h"
#include "print_funcs.h"

#define BETWEEN_SCALERS_PATH "/data/bees/scalers/"

static u8 g_scaler_bytes[PARAM_SCALER_DATA_SIZE];
static u8 g_type_ok[eParamNumTypes];
static u8 g_logged_miss[eParamNumTypes];

u8 *scaler_tables_bytes(void) {
  return g_scaler_bytes;
}

u8 scaler_tables_ok(ParamType p) {
  if(p >= eParamNumTypes) {
    return 0;
  }
  return g_type_ok[p];
}

/* load one .dat into dst[0..dstWords). file: u32 count, then count s32 LE words.
   returns 1 on success. */
static u8 load_scaler_dat(const char *name, s32 *dst, u32 dstWords) {
  char path[96];
  void *fp;
  u32 i;
  u32 size;
  union { u32 u; s32 s; u8 b[4]; } swap;

  if(name == NULL || name[0] == '\0' || dstWords == 0) {
    return 1;
  }

  strcpy(path, BETWEEN_SCALERS_PATH);
  strncat(path, name, sizeof(path) - strlen(path) - 1);

  app_pause();
  fp = fl_fopen(path, "r");
  if(fp == NULL) {
    app_resume();
    return 0;
  }

  swap.b[0] = (u8)fl_fgetc(fp);
  swap.b[1] = (u8)fl_fgetc(fp);
  swap.b[2] = (u8)fl_fgetc(fp);
  swap.b[3] = (u8)fl_fgetc(fp);
  size = swap.u;

  if(size > dstWords) {
    size = dstWords;
  }

  for(i = 0; i < size; ++i) {
    swap.b[0] = (u8)fl_fgetc(fp);
    swap.b[1] = (u8)fl_fgetc(fp);
    swap.b[2] = (u8)fl_fgetc(fp);
    swap.b[3] = (u8)fl_fgetc(fp);
    dst[i] = swap.s;
  }
  for(; i < dstWords; ++i) {
    dst[i] = 0;
  }

  fl_fclose(fp);
  app_resume();
  return 1;
}

void scaler_tables_init(void) {
  ParamType p;
  u32 words;
  s32 *dst;
  u8 ok;

  memset(g_scaler_bytes, 0, sizeof(g_scaler_bytes));
  memset(g_type_ok, 0, sizeof(g_type_ok));
  memset(g_logged_miss, 0, sizeof(g_logged_miss));

  print_dbg("\r\n between; loading scaler tables...");

  for(p = 0; p < eParamNumTypes; ++p) {
    ok = 1;
    words = scaler_get_data_bytes(p) / 4;
    if(words > 0) {
      dst = (s32 *)(g_scaler_bytes + scaler_get_data_offset(p));
      if(!load_scaler_dat(scaler_get_data_path(p), dst, words)) {
	ok = 0;
      }
    }
    words = scaler_get_rep_bytes(p) / 4;
    if(words > 0) {
      dst = (s32 *)(g_scaler_bytes + scaler_get_rep_offset(p));
      if(!load_scaler_dat(scaler_get_rep_path(p), dst, words)) {
	ok = 0;
      }
    }
    g_type_ok[p] = ok;
    if(!ok && !g_logged_miss[p]) {
      g_logged_miss[p] = 1;
      print_dbg("\r\n between; scaler table miss type=");
      print_dbg_ulong((u32)p);
      print_dbg(" path=");
      print_dbg(scaler_get_data_path(p));
    }
  }
}
