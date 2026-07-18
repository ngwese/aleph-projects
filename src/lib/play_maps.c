#include "play_maps.h"

#include <stdlib.h>
#include <string.h>

static void copy_label(char *dst, const char *src) {
  if(dst == NULL) {
    return;
  }
  if(src == NULL) {
    dst[0] = '\0';
    return;
  }
  strncpy(dst, src, PARAM_LABEL_LEN - 1);
  dst[PARAM_LABEL_LEN - 1] = '\0';
}

static u8 slot_from_char(char c, MorphSlot *out) {
  if(c == 'a' || c == 'A') {
    *out = eMorphSlotA;
    return 1;
  }
  if(c == 'b' || c == 'B') {
    *out = eMorphSlotB;
    return 1;
  }
  if(c == 'c' || c == 'C') {
    *out = eMorphSlotC;
    return 1;
  }
  if(c == 'd' || c == 'D') {
    *out = eMorphSlotD;
    return 1;
  }
  return 0;
}

static char slot_to_char(MorphSlot s) {
  static const char letters[MORPH2D_SLOTS] = {'a', 'b', 'c', 'd'};
  if(s >= MORPH2D_SLOTS) {
    return '?';
  }
  return letters[s];
}

static u8 label_known(const char *label, const ParamDesc *desc, u16 n) {
  u16 i;
  if(label == NULL || label[0] == '\0' || desc == NULL) {
    return 0;
  }
  for(i = 0; i < n; ++i) {
    if(strncmp(desc[i].label, label, PARAM_LABEL_LEN) == 0) {
      return 1;
    }
  }
  return 0;
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

static void format_s32(char *buf, u32 buf_size, s32 v) {
  char tmp[12];
  u8 n = 0;
  u8 neg = 0;
  u8 i;
  u32 x;
  if(buf == NULL || buf_size == 0) {
    return;
  }
  if(v < 0) {
    neg = 1;
    x = (u32)(-v);
  } else {
    x = (u32)v;
  }
  if(x == 0) {
    buf[0] = '0';
    if(buf_size > 1) {
      buf[1] = '\0';
    }
    return;
  }
  while(x && n < 11) {
    tmp[n++] = (char)('0' + (x % 10));
    x /= 10;
  }
  i = 0;
  if(neg && (u32)i + 1 < buf_size) {
    buf[i++] = '-';
  }
  while(n && (u32)i + 1 < buf_size) {
    buf[i++] = tmp[--n];
  }
  buf[i] = '\0';
}

void play_maps_set_defaults(PlayMaps *m) {
  if(m == NULL) {
    return;
  }
  memset(m, 0, sizeof(*m));
  m->enc[2].kind = ePlayEncMorphX;
  m->enc[3].kind = ePlayEncMorphY;
  m->sw[0].kind = ePlaySwSnapA;
  m->sw[1].kind = ePlaySwSnapB;
  m->sw[2].kind = ePlaySwSnapC;
  m->sw[3].kind = ePlaySwSnapD;
}

void play_maps_clear_invalid(PlayMaps *m, const ParamDesc *desc,
			     u16 num_params) {
  u8 i;
  if(m == NULL) {
    return;
  }
  for(i = 0; i < PLAY_MAPS_ENC_COUNT; ++i) {
    if(m->enc[i].kind == ePlayEncParamSlot ||
       m->enc[i].kind == ePlayEncParamAll) {
      if(!label_known(m->enc[i].label, desc, num_params)) {
	memset(&m->enc[i], 0, sizeof(m->enc[i]));
	m->enc[i].kind = ePlayEncNone;
      }
    }
  }
  for(i = 0; i < PLAY_MAPS_SW_COUNT; ++i) {
    if(m->sw[i].kind == ePlaySwSetSlot || m->sw[i].kind == ePlaySwMomSlot ||
       m->sw[i].kind == ePlaySwSetAll || m->sw[i].kind == ePlaySwMomAll) {
      if(!label_known(m->sw[i].label, desc, num_params)) {
	memset(&m->sw[i], 0, sizeof(m->sw[i]));
	m->sw[i].kind = ePlaySwNone;
      }
    }
  }
}

u8 play_maps_parse_enc(const char *val, PlayEncMap *out) {
  MorphSlot slot;
  const char *p;
  if(out == NULL) {
    return 0;
  }
  memset(out, 0, sizeof(*out));
  if(val == NULL || val[0] == '\0' || strcmp(val, "-") == 0) {
    out->kind = ePlayEncNone;
    return 1;
  }
  if(strcmp(val, "morph.x") == 0) {
    out->kind = ePlayEncMorphX;
    return 1;
  }
  if(strcmp(val, "morph.y") == 0) {
    out->kind = ePlayEncMorphY;
    return 1;
  }
  /* param.<slot|all>.<label> */
  if(strncmp(val, "param.", 6) != 0) {
    return 0;
  }
  p = val + 6;
  if(strncmp(p, "all.", 4) == 0) {
    out->kind = ePlayEncParamAll;
    copy_label(out->label, p + 4);
    return out->label[0] != '\0';
  }
  if(!slot_from_char(p[0], &slot) || p[1] != '.') {
    return 0;
  }
  out->kind = ePlayEncParamSlot;
  out->slot = slot;
  copy_label(out->label, p + 2);
  return out->label[0] != '\0';
}

u8 play_maps_parse_sw(const char *val, PlaySwMap *out) {
  MorphSlot slot;
  const char *p;
  const char *colon;
  char scope;
  u8 mom = 0;
  s32 raw = 0;

  if(out == NULL) {
    return 0;
  }
  memset(out, 0, sizeof(*out));
  if(val == NULL || val[0] == '\0' || strcmp(val, "-") == 0) {
    out->kind = ePlaySwNone;
    return 1;
  }
  if(strncmp(val, "snap.", 5) == 0) {
    if(!slot_from_char(val[5], &slot) || val[6] != '\0') {
      return 0;
    }
    out->kind = (PlaySwKind)(ePlaySwSnapA + slot);
    return 1;
  }
  if(strncmp(val, "set.", 4) == 0) {
    mom = 0;
    p = val + 4;
  } else if(strncmp(val, "mom.", 4) == 0) {
    mom = 1;
    p = val + 4;
  } else {
    return 0;
  }
  colon = strrchr(p, ':');
  if(colon == NULL || colon == p) {
    return 0;
  }
  if(!parse_s32(colon + 1, &raw)) {
    return 0;
  }
  out->value = (ParamValue)raw;

  if(strncmp(p, "all.", 4) == 0) {
    out->kind = mom ? ePlaySwMomAll : ePlaySwSetAll;
    {
      char lab[PARAM_LABEL_LEN];
      u32 len = (u32)(colon - (p + 4));
      if(len == 0 || len >= PARAM_LABEL_LEN) {
	return 0;
      }
      memcpy(lab, p + 4, len);
      lab[len] = '\0';
      copy_label(out->label, lab);
    }
    return out->label[0] != '\0';
  }
  scope = p[0];
  if(!slot_from_char(scope, &slot) || p[1] != '.') {
    return 0;
  }
  out->kind = mom ? ePlaySwMomSlot : ePlaySwSetSlot;
  out->slot = slot;
  {
    char lab[PARAM_LABEL_LEN];
    u32 len = (u32)(colon - (p + 2));
    if(len == 0 || len >= PARAM_LABEL_LEN) {
      return 0;
    }
    memcpy(lab, p + 2, len);
    lab[len] = '\0';
    copy_label(out->label, lab);
  }
  return out->label[0] != '\0';
}

u8 play_maps_format_enc(char *buf, u32 buf_size, const PlayEncMap *m) {
  if(buf == NULL || buf_size == 0 || m == NULL) {
    return 0;
  }
  switch(m->kind) {
  case ePlayEncNone:
    if(buf_size < 2) {
      return 0;
    }
    strcpy(buf, "-");
    return 1;
  case ePlayEncMorphX:
    if(buf_size < 8) {
      return 0;
    }
    strcpy(buf, "morph.x");
    return 1;
  case ePlayEncMorphY:
    if(buf_size < 8) {
      return 0;
    }
    strcpy(buf, "morph.y");
    return 1;
  case ePlayEncParamAll:
    if(buf_size < 11 + strlen(m->label)) {
      return 0;
    }
    strcpy(buf, "param.all.");
    strcat(buf, m->label);
    return 1;
  case ePlayEncParamSlot:
    if(buf_size < 9 + strlen(m->label)) {
      return 0;
    }
    strcpy(buf, "param.");
    {
      char s[2] = {slot_to_char(m->slot), '\0'};
      strcat(buf, s);
    }
    strcat(buf, ".");
    strcat(buf, m->label);
    return 1;
  default:
    return 0;
  }
}

u8 play_maps_format_sw(char *buf, u32 buf_size, const PlaySwMap *m) {
  char num[16];
  char tmp[96];
  if(buf == NULL || buf_size == 0 || m == NULL) {
    return 0;
  }
  switch(m->kind) {
  case ePlaySwNone:
    if(buf_size < 2) {
      return 0;
    }
    strcpy(buf, "-");
    return 1;
  case ePlaySwSnapA:
  case ePlaySwSnapB:
  case ePlaySwSnapC:
  case ePlaySwSnapD:
    if(buf_size < 7) {
      return 0;
    }
    strcpy(buf, "snap.");
    {
      char s[2] = {slot_to_char((MorphSlot)(m->kind - ePlaySwSnapA)), '\0'};
      strcat(buf, s);
    }
    return 1;
  case ePlaySwSetSlot:
  case ePlaySwMomSlot:
    format_s32(num, sizeof(num), m->value);
    strcpy(tmp, (m->kind == ePlaySwMomSlot) ? "mom." : "set.");
    {
      char s[2] = {slot_to_char(m->slot), '\0'};
      strcat(tmp, s);
    }
    strcat(tmp, ".");
    strcat(tmp, m->label);
    strcat(tmp, ":");
    strcat(tmp, num);
    if(strlen(tmp) + 1 > buf_size) {
      return 0;
    }
    strcpy(buf, tmp);
    return 1;
  case ePlaySwSetAll:
  case ePlaySwMomAll:
    format_s32(num, sizeof(num), m->value);
    strcpy(tmp, (m->kind == ePlaySwMomAll) ? "mom.all." : "set.all.");
    strcat(tmp, m->label);
    strcat(tmp, ":");
    strcat(tmp, num);
    if(strlen(tmp) + 1 > buf_size) {
      return 0;
    }
    strcpy(buf, tmp);
    return 1;
  default:
    return 0;
  }
}

void play_maps_summary_enc(char *buf, u32 buf_size, const PlayEncMap *m) {
  if(buf == NULL || buf_size == 0) {
    return;
  }
  buf[0] = '\0';
  if(m == NULL) {
    return;
  }
  switch(m->kind) {
  case ePlayEncNone:
    strncpy(buf, "-", buf_size - 1);
    break;
  case ePlayEncMorphX:
    strncpy(buf, "morph x", buf_size - 1);
    break;
  case ePlayEncMorphY:
    strncpy(buf, "morph y", buf_size - 1);
    break;
  case ePlayEncParamAll:
    strncpy(buf, "all/", buf_size - 1);
    strncat(buf, m->label, buf_size - strlen(buf) - 1);
    break;
  case ePlayEncParamSlot:
    {
      char s[8];
      s[0] = 's';
      s[1] = 'l';
      s[2] = 'o';
      s[3] = 't';
      s[4] = '.';
      s[5] = slot_to_char(m->slot);
      s[6] = '/';
      s[7] = '\0';
      strncpy(buf, s, buf_size - 1);
      strncat(buf, m->label, buf_size - strlen(buf) - 1);
    }
    break;
  default:
    strncpy(buf, "?", buf_size - 1);
    break;
  }
  buf[buf_size - 1] = '\0';
}

void play_maps_summary_sw(char *buf, u32 buf_size, const PlaySwMap *m) {
  if(buf == NULL || buf_size == 0) {
    return;
  }
  buf[0] = '\0';
  if(m == NULL) {
    return;
  }
  switch(m->kind) {
  case ePlaySwNone:
    strncpy(buf, "-", buf_size - 1);
    break;
  case ePlaySwSnapA:
  case ePlaySwSnapB:
  case ePlaySwSnapC:
  case ePlaySwSnapD:
    {
      char s[8] = "snap ?";
      s[5] = (char)('a' + (m->kind - ePlaySwSnapA));
      strncpy(buf, s, buf_size - 1);
    }
    break;
  case ePlaySwSetSlot:
  case ePlaySwMomSlot:
    strncpy(buf, (m->kind == ePlaySwMomSlot) ? "mom." : "set.", buf_size - 1);
    {
      char s[3] = {slot_to_char(m->slot), '/', '\0'};
      strncat(buf, s, buf_size - strlen(buf) - 1);
    }
    strncat(buf, m->label, buf_size - strlen(buf) - 1);
    break;
  case ePlaySwSetAll:
  case ePlaySwMomAll:
    strncpy(buf, (m->kind == ePlaySwMomAll) ? "mom.all/" : "set.all/",
	    buf_size - 1);
    strncat(buf, m->label, buf_size - strlen(buf) - 1);
    break;
  default:
    strncpy(buf, "?", buf_size - 1);
    break;
  }
  buf[buf_size - 1] = '\0';
}

void play_maps_footer_sw(char *buf, u32 buf_size, const PlaySwMap *m) {
  if(buf == NULL || buf_size == 0) {
    return;
  }
  buf[0] = '\0';
  if(m == NULL) {
    strncpy(buf, "-", buf_size - 1);
    buf[buf_size - 1] = '\0';
    return;
  }
  switch(m->kind) {
  case ePlaySwSnapA:
  case ePlaySwSnapB:
  case ePlaySwSnapC:
  case ePlaySwSnapD:
    buf[0] = (char)('A' + (m->kind - ePlaySwSnapA));
    if(buf_size > 1) {
      buf[1] = '\0';
    }
    break;
  case ePlaySwSetSlot:
  case ePlaySwMomSlot:
  case ePlaySwSetAll:
  case ePlaySwMomAll:
    strncpy(buf, m->label, buf_size - 1);
    buf[buf_size - 1] = '\0';
    break;
  default:
    strncpy(buf, "-", buf_size - 1);
    buf[buf_size - 1] = '\0';
    break;
  }
}

u8 play_maps_sw_single_slot(const PlaySwMap *m, MorphSlot *out_slot) {
  if(m == NULL) {
    return 0;
  }
  if(m->kind == ePlaySwSetSlot || m->kind == ePlaySwMomSlot) {
    if(out_slot != NULL) {
      *out_slot = m->slot;
    }
    return 1;
  }
  return 0;
}

u8 play_maps_sw_snap_slot(PlaySwKind kind, MorphSlot *out) {
  if(kind < ePlaySwSnapA || kind > ePlaySwSnapD) {
    return 0;
  }
  if(out != NULL) {
    *out = (MorphSlot)(kind - ePlaySwSnapA);
  }
  return 1;
}

void play_maps_reset_enc(PlayMaps *m, u8 idx) {
  PlayMaps d;
  if(m == NULL || idx >= PLAY_MAPS_ENC_COUNT) {
    return;
  }
  play_maps_set_defaults(&d);
  m->enc[idx] = d.enc[idx];
}

void play_maps_reset_sw(PlayMaps *m, u8 idx) {
  PlayMaps d;
  if(m == NULL || idx >= PLAY_MAPS_SW_COUNT) {
    return;
  }
  play_maps_set_defaults(&d);
  m->sw[idx] = d.sw[idx];
}
