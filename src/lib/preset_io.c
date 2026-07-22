#include "preset_io.h"

#include "kvtext.h"

#include <stdlib.h>
#include <string.h>

u8 preset_io_parse_version(const char *s, ModuleVersion *out) {
  u32 maj = 0;
  u32 min = 0;
  u32 rev = 0;
  const char *p;
  if(s == NULL || out == NULL) {
    return 0;
  }
  p = s;
  while(*p >= '0' && *p <= '9') {
    maj = maj * 10 + (u32)(*p - '0');
    ++p;
  }
  if(*p != '.') {
    return 0;
  }
  ++p;
  while(*p >= '0' && *p <= '9') {
    min = min * 10 + (u32)(*p - '0');
    ++p;
  }
  if(*p != '.') {
    return 0;
  }
  ++p;
  while(*p >= '0' && *p <= '9') {
    rev = rev * 10 + (u32)(*p - '0');
    ++p;
  }
  if(*p != '\0' || maj > 255 || min > 255 || rev > 65535) {
    return 0;
  }
  out->maj = (u8)maj;
  out->min = (u8)min;
  out->rev = (u16)rev;
  return 1;
}

static void append_u(char *buf, u32 *pos, u32 buf_size, u32 v) {
  char tmp[10];
  u8 n = 0;
  u8 i;
  if(v == 0) {
    if(*pos + 1 < buf_size) {
      buf[(*pos)++] = '0';
      buf[*pos] = '\0';
    }
    return;
  }
  while(v && n < 10) {
    tmp[n++] = (char)('0' + (v % 10));
    v /= 10;
  }
  for(i = 0; i < n && *pos + 1 < buf_size; ++i) {
    buf[(*pos)++] = tmp[n - 1 - i];
  }
  buf[*pos] = '\0';
}

void preset_io_format_version(char *buf, u32 buf_size, const ModuleVersion *v) {
  u32 pos = 0;
  if(buf == NULL || buf_size == 0 || v == NULL) {
    return;
  }
  buf[0] = '\0';
  append_u(buf, &pos, buf_size, v->maj);
  if(pos + 1 < buf_size) {
    buf[pos++] = '.';
    buf[pos] = '\0';
  }
  append_u(buf, &pos, buf_size, v->min);
  if(pos + 1 < buf_size) {
    buf[pos++] = '.';
    buf[pos] = '\0';
  }
  append_u(buf, &pos, buf_size, v->rev);
}

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

PresetIoStatus preset_io_read(LineIO *io, PresetMeta *meta,
			      preset_on_param_fn on_param, void *ctx) {
  char line[PRESET_IO_LINE_MAX];
  KvPair pair;
  u8 have_format = 0;
  u8 have_module = 0;
  u8 have_version = 0;
  u8 in_params = 0;

  if(io == NULL || io->read_line == NULL || meta == NULL) {
    return ePresetIoMalformed;
  }

  memset(meta, 0, sizeof(*meta));

  while(io->read_line(line, PRESET_IO_LINE_MAX, io->ctx)) {
    KvLineKind kind = kvtext_parse_line(line, &pair);
    if(kind == eKvBlank || kind == eKvComment) {
      continue;
    }
    if(kind != eKvPair) {
      return ePresetIoMalformed;
    }

    if(!in_params) {
      if(kvtext_key_eq(pair.key, "format")) {
	s32 f = 0;
	if(!parse_s32(pair.val, &f) || f < 0 || f > 255 ||
	   f != PRESET_IO_FORMAT) {
	  return ePresetIoBadFormat;
	}
	meta->format = (u8)f;
	have_format = 1;
	continue;
      }
      if(kvtext_key_eq(pair.key, "module")) {
	strncpy(meta->module, pair.val, MODULE_NAME_LEN - 1);
	meta->module[MODULE_NAME_LEN - 1] = '\0';
	have_module = 1;
	continue;
      }
      if(kvtext_key_eq(pair.key, "version")) {
	if(!preset_io_parse_version(pair.val, &meta->version)) {
	  return ePresetIoMalformed;
	}
	have_version = 1;
	continue;
      }
      /* first non-meta key starts params; require meta first */
      if(!have_format || !have_module || !have_version) {
	return ePresetIoMissingMeta;
      }
      in_params = 1;
      /* fall through and treat this line as a param */
    }

    if(in_params) {
      s32 value = 0;
      if(!parse_s32(pair.val, &value)) {
	return ePresetIoMalformed;
      }
      if(on_param != NULL && !on_param(pair.key, value, ctx)) {
	return ePresetIoMalformed;
      }
    }
  }

  if(!have_format || !have_module || !have_version) {
    return ePresetIoMissingMeta;
  }
  return ePresetIoOk;
}

static u8 write_kv(LineIO *io, const char *key, const char *val) {
  char line[PRESET_IO_LINE_MAX];
  if(kvtext_format_line(line, PRESET_IO_LINE_MAX, key, val) < 0) {
    return 0;
  }
  return io->write_line(line, io->ctx);
}

PresetIoStatus preset_io_write(LineIO *io, const PresetMeta *meta,
			       preset_next_param_fn next_param, void *ctx) {
  char ver[32];
  char num[24];

  if(io == NULL || io->write_line == NULL || meta == NULL) {
    return ePresetIoWriteFail;
  }

  if(!io->write_line("# between preset\n", io->ctx)) {
    return ePresetIoWriteFail;
  }
  {
    u32 pos = 0;
    num[0] = '\0';
    append_u(num, &pos, sizeof(num), PRESET_IO_FORMAT);
  }
  if(!write_kv(io, "format", num)) {
    return ePresetIoWriteFail;
  }
  if(!write_kv(io, "module", meta->module)) {
    return ePresetIoWriteFail;
  }
  preset_io_format_version(ver, sizeof(ver), &meta->version);
  if(!write_kv(io, "version", ver)) {
    return ePresetIoWriteFail;
  }
  if(!io->write_line("\n", io->ctx)) {
    return ePresetIoWriteFail;
  }

  if(next_param != NULL) {
    const char *label = NULL;
    s32 value = 0;
    while(next_param(&label, &value, ctx)) {
      u32 pos = 0;
      u32 mag;
      if(label == NULL) {
	return ePresetIoWriteFail;
      }
      num[0] = '\0';
      if(value < 0) {
	num[pos++] = '-';
	num[pos] = '\0';
	/* avoid signed overflow on INT_MIN */
	mag = (u32)(-(value + 1)) + 1u;
      } else {
	mag = (u32)value;
      }
      append_u(num, &pos, sizeof(num), mag);
      if(!write_kv(io, label, num)) {
	return ePresetIoWriteFail;
      }
    }
  }
  return ePresetIoOk;
}
