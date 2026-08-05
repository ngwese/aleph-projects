#include "module_load.h"

#include <string.h>

#include "compiler.h"
#include "delay.h"
#include "print_funcs.h"

#include "app.h"
#include "bfin.h"
#include "filesystem.h"
#include "memory.h"
#include "render.h"
#include "xruns.h"

#include "fat_string.h"

ModuleState g_module;
ParamScaler g_scalers[BETWEEN_PARAMS_MAX];

#define PARAM_DESC_PICKLE_BYTES (PARAM_LABEL_LEN + 4 + 4 + 4 + 4)

static void fake_fread(volatile u8 *dst, u32 len, void *fp) {
  u32 n = 0;
  while (n < len) {
    *dst = (u8)fl_fgetc(fp);
    n++;
    dst++;
  }
}

/* strip a trailing suffix only when it matches (e.g. ".ldr").
 * do not strip version dots in names like "spray-0.1.0". */
static void strip_suffix(char *str, const char *ext) {
  u32 nlen;
  u32 elen;
  if (str == NULL || ext == NULL) {
    return;
  }
  nlen = (u32)strlen(str);
  elen = (u32)strlen(ext);
  if (nlen > elen && strcmp(str + nlen - elen, ext) == 0) {
    str[nlen - elen] = '\0';
  }
}

static void strip_mod_ext(char *str) {
  strip_suffix(str, ".ldr");
  strip_suffix(str, ".dsc");
  strip_suffix(str, ".lab");
}

static const u8 *unpickle_32(const u8 *src, u32 *dst) {
  *dst = 0;
  *dst |= *src;
  *dst |= ((u32)(*(src + 1)) << 8);
  *dst |= ((u32)(*(src + 2)) << 16);
  *dst |= ((u32)(*(src + 3)) << 24);
  return src + 4;
}

static const u8 *pdesc_unpickle(ParamDesc *pdesc, const u8 *src) {
  u32 val;
  u32 i;
  for (i = 0; i < PARAM_LABEL_LEN; ++i) {
    pdesc->label[i] = (char)(*src++);
  }
  src = unpickle_32(src, &val);
  pdesc->type = (ParamType)val;
  src = unpickle_32(src, &val);
  pdesc->min = (s32)val;
  src = unpickle_32(src, &val);
  pdesc->max = (s32)val;
  src = unpickle_32(src, &val);
  pdesc->radix = (u8)val;
  return src;
}

static void *open_mod_file(const char *name, const char *ext, u32 *size) {
  FL_DIR dirstat;
  struct fs_dir_ent dirent;
  char path[64];
  char nameTry[64];
  void *fp = NULL;

  *size = 0;
  strncpy(nameTry, name, sizeof(nameTry) - 1);
  nameTry[sizeof(nameTry) - 1] = '\0';
  strip_mod_ext(nameTry);
  strncat(nameTry, ext, sizeof(nameTry) - strlen(nameTry) - 1);

  strcpy(path, BETWEEN_MOD_PATH);
  if (!fl_opendir(path, &dirstat)) {
    return NULL;
  }
  while (fl_readdir(&dirstat, &dirent) == 0) {
    if (fatfs_compare_names(dirent.filename, nameTry)) {
      strncat(path, dirent.filename, sizeof(path) - strlen(path) - 1);
      fp = fl_fopen(path, "r");
      *size = dirent.size;
      break;
    }
  }
  return fp;
}

static u8 load_ldr(const char *name) {
  void *fp;
  u32 size = 0;
  volatile u8 *buf = NULL;
  u8 ret = 0;

  render_log("load ldr...");
  fp = open_mod_file(name, ".ldr", &size);
  if (fp == NULL || size == 0) {
    if (fp != NULL) {
      fl_fclose(fp);
    }
    return 0;
  }
  if (size > BFIN_LDR_MAX_BYTES) {
    fl_fclose(fp);
    return 0;
  }
  buf = alloc_mem(size);
  if (buf == NULL) {
    fl_fclose(fp);
    return 0;
  }
  fake_fread(buf, size, fp);
  fl_fclose(fp);
  bfin_load_buf((const u8 *)buf, size);
  free_mem(buf);
  ret = 1;
  return ret;
}

static u8 load_dsc(const char *name, u8 *out_truncated) {
  void *fp;
  u32 size = 0;
  u8 nbuf[4];
  u8 dbuf[PARAM_DESC_PICKLE_BYTES];
  ParamDesc desc;
  s32 nparams = -1;
  int i;

  if (out_truncated != NULL) {
    *out_truncated = 0;
  }
  render_log("load dsc...");
  fp = open_mod_file(name, ".dsc", &size);
  if (fp == NULL) {
    return 0;
  }

  fake_fread(nbuf, 4, fp);
  unpickle_32(nbuf, (u32 *)&nparams);
  if (nparams <= 0) {
    fl_fclose(fp);
    return 0;
  }
  if (nparams > BETWEEN_PARAMS_MAX) {
    if (out_truncated != NULL) {
      *out_truncated = 1;
    }
    nparams = BETWEEN_PARAMS_MAX;
  }

  bfin_wait_ready();

  g_module.num_params = (u16)nparams;
  for (i = 0; i < nparams; i++) {
    fake_fread(dbuf, PARAM_DESC_PICKLE_BYTES, fp);
    pdesc_unpickle(&desc, dbuf);
    g_module.desc[i] = desc;
    g_module.defaults[i] = (ParamValue)bfin_get_param((u8)i);
    scaler_init(&g_scalers[i], &g_module.desc[i]);
  }
  fl_fclose(fp);
  return 1;
}

u8 module_load(const char *name) {
  char stem[MODULE_NAME_LEN];
  u8 truncated = 0;

  if (name == NULL) {
    return 0;
  }
  strncpy(stem, name, MODULE_NAME_LEN - 1);
  stem[MODULE_NAME_LEN - 1] = '\0';
  strip_mod_ext(stem);

  delay_ms(10);
  app_pause();

  render_log("load module...");
  memset(&g_module, 0, sizeof(g_module));
  if (!load_ldr(stem)) {
    render_log("ldr fail");
    app_resume();
    return 0;
  }

  bfin_wait_ready();
  delay_ms(10);

  if (!load_dsc(stem, &truncated)) {
    render_log("dsc fail");
    app_resume();
    return 0;
  }

  strncpy(g_module.name, stem, MODULE_NAME_LEN - 1);
  g_module.name[MODULE_NAME_LEN - 1] = '\0';
  bfin_get_module_version(&g_module.version);
  g_module.loaded = 1;
  bfin_enable();
  xruns_clear_local();
  if (truncated) {
    render_log("too many params");
  } else {
    render_log("module ok");
  }
  app_resume();
  return 1;
}

s16 module_find_param(const char *label) {
  u16 i;
  if (label == NULL || !g_module.loaded) {
    return -1;
  }
  for (i = 0; i < g_module.num_params; ++i) {
    if (strncmp(g_module.desc[i].label, label, PARAM_LABEL_LEN) == 0) {
      return (s16)i;
    }
  }
  return -1;
}
