#include "kvtext.h"

#include <string.h>

static const char *skip_ws(const char *s) {
  while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') {
    ++s;
  }
  return s;
}

static void rtrim(char *s) {
  s32 n = (s32)strlen(s);
  while (n > 0) {
    char c = s[n - 1];
    if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
      s[--n] = '\0';
    } else {
      break;
    }
  }
}

static u8 copy_bounded(char *dst, u32 dst_size, const char *src, u32 src_len) {
  u32 n;
  if (dst_size == 0) {
    return 0;
  }
  n = src_len;
  if (n >= dst_size) {
    n = dst_size - 1;
  }
  memcpy(dst, src, n);
  dst[n] = '\0';
  return 1;
}

KvLineKind kvtext_parse_line(const char *line, KvPair *out) {
  const char *p;
  const char *colon;
  u32 key_len;
  u32 val_len;

  if (out == NULL) {
    return eKvMalformed;
  }
  out->kind = eKvBlank;
  out->key[0] = '\0';
  out->val[0] = '\0';

  if (line == NULL) {
    return eKvMalformed;
  }

  p = skip_ws(line);
  if (*p == '\0') {
    out->kind = eKvBlank;
    return eKvBlank;
  }
  if (*p == '#') {
    out->kind = eKvComment;
    return eKvComment;
  }

  colon = strchr(p, ':');
  if (colon == NULL || colon == p) {
    out->kind = eKvMalformed;
    return eKvMalformed;
  }

  key_len = (u32)(colon - p);
  while (key_len > 0 && (p[key_len - 1] == ' ' || p[key_len - 1] == '\t')) {
    --key_len;
  }
  if (key_len == 0 || key_len >= KVTEXT_KEY_MAX) {
    out->kind = eKvMalformed;
    return eKvMalformed;
  }
  if (!copy_bounded(out->key, KVTEXT_KEY_MAX, p, key_len)) {
    out->kind = eKvMalformed;
    return eKvMalformed;
  }

  p = skip_ws(colon + 1);
  val_len = (u32)strlen(p);
  if (!copy_bounded(out->val, KVTEXT_VAL_MAX, p, val_len)) {
    out->kind = eKvMalformed;
    return eKvMalformed;
  }
  rtrim(out->val);

  out->kind = eKvPair;
  return eKvPair;
}

s32 kvtext_format_line(char *buf, u32 buf_size, const char *key,
                       const char *val) {
  u32 key_len;
  u32 val_len;
  u32 need;

  if (buf == NULL || key == NULL || val == NULL || buf_size == 0) {
    return -1;
  }
  key_len = (u32)strlen(key);
  val_len = (u32)strlen(val);
  need = key_len + 1 + val_len + 1 + 1; /* key : value \n \0 */
  if (need > buf_size) {
    return -1;
  }
  memcpy(buf, key, key_len);
  buf[key_len] = ':';
  memcpy(buf + key_len + 1, val, val_len);
  buf[key_len + 1 + val_len] = '\n';
  buf[key_len + 1 + val_len + 1] = '\0';
  return (s32)(key_len + 1 + val_len + 1);
}

u8 kvtext_key_eq(const char *a, const char *b) {
  if (a == NULL || b == NULL) {
    return 0;
  }
  return (u8)(strcmp(a, b) == 0);
}
