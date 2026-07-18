#include "setup_io.h"

#include "kvtext.h"
#include "preset_io.h"

#include <stdlib.h>
#include <string.h>

static u8 parse_s32(const char *s, s32 *out) {
  long v;
  char *end = NULL;
  if(s == NULL || out == NULL || *s == '\0') {
    return 0;
  }
  v = strtol(s, &end, 10);
  if(end == s || (end != NULL && *end != '\0')) {
    return 0;
  }
  *out = (s32)v;
  return 1;
}

/* parse morph coordinate as decimal integer 0..MORPH2D_ONE */
static u8 parse_u16_morph(const char *s, u16 *out) {
  s32 v = 0;
  if(!parse_s32(s, &v) || v < 0) {
    return 0;
  }
  if(v > (s32)MORPH2D_ONE) {
    v = (s32)MORPH2D_ONE;
  }
  *out = (u16)v;
  return 1;
}

static void format_u16(char *buf, u32 buf_size, u16 v) {
  char tmp[6];
  u8 n = 0;
  u8 i;
  if(buf == NULL || buf_size == 0) {
    return;
  }
  if(v == 0) {
    buf[0] = '0';
    if(buf_size > 1) {
      buf[1] = '\0';
    }
    return;
  }
  while(v && n < 5) {
    tmp[n++] = (char)('0' + (v % 10));
    v /= 10;
  }
  if((u32)n + 1 > buf_size) {
    n = (u8)(buf_size - 1);
  }
  for(i = 0; i < n; ++i) {
    buf[i] = tmp[n - 1 - i];
  }
  buf[n] = '\0';
}

static s8 slot_index_from_key(const char *key) {
  if(strcmp(key, "slot.a") == 0 || strcmp(key, "slot.A") == 0) {
    return eMorphSlotA;
  }
  if(strcmp(key, "slot.b") == 0 || strcmp(key, "slot.B") == 0) {
    return eMorphSlotB;
  }
  if(strcmp(key, "slot.c") == 0 || strcmp(key, "slot.C") == 0) {
    return eMorphSlotC;
  }
  if(strcmp(key, "slot.d") == 0 || strcmp(key, "slot.D") == 0) {
    return eMorphSlotD;
  }
  return -1;
}

static void set_slot_stem(SetupData *out, s8 idx, const char *val) {
  if(idx < 0 || idx >= MORPH2D_SLOTS) {
    return;
  }
  if(val == NULL || val[0] == '\0' || strcmp(val, "-") == 0) {
    out->slot_stem[idx][0] = '\0';
    out->slot_occupied[idx] = 0;
    return;
  }
  strncpy(out->slot_stem[idx], val, SETUP_STEM_MAX - 1);
  out->slot_stem[idx][SETUP_STEM_MAX - 1] = '\0';
  out->slot_occupied[idx] = 1;
}

SetupIoStatus setup_io_read(LineIO *io, SetupData *out) {
  char line[SETUP_IO_LINE_MAX];
  KvPair pair;
  u8 have_format = 0;
  u8 have_module = 0;
  u8 have_version = 0;

  if(io == NULL || io->read_line == NULL || out == NULL) {
    return eSetupIoMalformed;
  }
  memset(out, 0, sizeof(*out));

  while(io->read_line(line, SETUP_IO_LINE_MAX, io->ctx)) {
    KvLineKind kind = kvtext_parse_line(line, &pair);
    s8 slot_i;
    if(kind == eKvBlank || kind == eKvComment) {
      continue;
    }
    if(kind != eKvPair) {
      return eSetupIoMalformed;
    }

    if(kvtext_key_eq(pair.key, "format")) {
      s32 f = 0;
      if(!parse_s32(pair.val, &f) || f < 0 || f > 255 ||
	 f != SETUP_IO_FORMAT) {
	return eSetupIoBadFormat;
      }
      out->format = (u8)f;
      have_format = 1;
      continue;
    }
    if(kvtext_key_eq(pair.key, "module")) {
      strncpy(out->module, pair.val, MODULE_NAME_LEN - 1);
      out->module[MODULE_NAME_LEN - 1] = '\0';
      have_module = 1;
      continue;
    }
    if(kvtext_key_eq(pair.key, "version")) {
      if(!preset_io_parse_version(pair.val, &out->version)) {
	return eSetupIoMalformed;
      }
      have_version = 1;
      continue;
    }
    slot_i = slot_index_from_key(pair.key);
    if(slot_i >= 0) {
      set_slot_stem(out, slot_i, pair.val);
      continue;
    }
    if(kvtext_key_eq(pair.key, "x")) {
      if(!parse_u16_morph(pair.val, &out->x)) {
	return eSetupIoMalformed;
      }
      continue;
    }
    if(kvtext_key_eq(pair.key, "y")) {
      if(!parse_u16_morph(pair.val, &out->y)) {
	return eSetupIoMalformed;
      }
      continue;
    }
    /* unknown keys ignored for forward compatibility */
  }

  if(!have_format || !have_module || !have_version) {
    return eSetupIoMissingMeta;
  }
  return eSetupIoOk;
}

static u8 write_kv(LineIO *io, const char *key, const char *val) {
  char line[SETUP_IO_LINE_MAX];
  if(kvtext_format_line(line, SETUP_IO_LINE_MAX, key, val) < 0) {
    return 0;
  }
  return io->write_line(line, io->ctx);
}

SetupIoStatus setup_io_write(LineIO *io, const SetupData *data) {
  char ver[32];
  char num[24];
  static const char *slot_keys[MORPH2D_SLOTS] = {
    "slot.a", "slot.b", "slot.c", "slot.d"};
  u32 i;

  if(io == NULL || io->write_line == NULL || data == NULL) {
    return eSetupIoWriteFail;
  }

  if(!io->write_line("# between setup\n", io->ctx)) {
    return eSetupIoWriteFail;
  }
  format_u16(num, sizeof(num), (u16)SETUP_IO_FORMAT);
  if(!write_kv(io, "format", num)) {
    return eSetupIoWriteFail;
  }
  if(!write_kv(io, "module", data->module)) {
    return eSetupIoWriteFail;
  }
  preset_io_format_version(ver, sizeof(ver), &data->version);
  if(!write_kv(io, "version", ver)) {
    return eSetupIoWriteFail;
  }
  if(!io->write_line("\n", io->ctx)) {
    return eSetupIoWriteFail;
  }

  for(i = 0; i < MORPH2D_SLOTS; ++i) {
    const char *stem =
      data->slot_occupied[i] ? data->slot_stem[i] : "-";
    if(!write_kv(io, slot_keys[i], stem)) {
      return eSetupIoWriteFail;
    }
  }
  if(!io->write_line("\n", io->ctx)) {
    return eSetupIoWriteFail;
  }
  format_u16(num, sizeof(num), data->x);
  if(!write_kv(io, "x", num)) {
    return eSetupIoWriteFail;
  }
  format_u16(num, sizeof(num), data->y);
  if(!write_kv(io, "y", num)) {
    return eSetupIoWriteFail;
  }
  return eSetupIoOk;
}
