#include "midi_nrpn.h"

#include <stddef.h>

u16 midi_nrpn_param_index(u8 msb, u8 lsb) {
  return (u16)(((u16)(msb & 0x7f) << 7) | (u16)(lsb & 0x7f));
}

u16 midi_nrpn_v14(u8 data_msb, u8 data_lsb) {
  return (u16)(((u16)(data_msb & 0x7f) << 7) | (u16)(data_lsb & 0x7f));
}

s32 midi_nrpn_v14_to_range(s32 min, s32 max, u16 v14) {
  s32 span;
  s32 out;

  if(v14 > MIDI_NRPN_V14_MAX) {
    v14 = MIDI_NRPN_V14_MAX;
  }
  if(v14 == 0) {
    return min;
  }
  if(v14 == MIDI_NRPN_V14_MAX) {
    return max;
  }

  span = max - min;
  if(span >= 0) {
    out = min + (s32)(((u32)v14 * (u32)span) / (u32)MIDI_NRPN_V14_MAX);
  } else {
    out = min - (s32)(((u32)v14 * (u32)(-span)) / (u32)MIDI_NRPN_V14_MAX);
  }
  if(out < min) {
    out = min;
  }
  if(out > max) {
    out = max;
  }
  return out;
}

u16 midi_nrpn_range_to_v14(s32 min, s32 max, s32 v) {
  s32 span;
  u32 d;
  u32 s;

  if(v <= min) {
    return 0;
  }
  if(v >= max) {
    return MIDI_NRPN_V14_MAX;
  }
  span = max - min;
  if(span == 0) {
    return 0;
  }
  if(span > 0) {
    d = (u32)(v - min);
    s = (u32)span;
  } else {
    d = (u32)(min - v);
    s = (u32)(-span);
  }
  /* (d * 16383) / s without overflowing u32 */
  return (u16)((d / s) * (u32)MIDI_NRPN_V14_MAX +
	       ((d % s) * (u32)MIDI_NRPN_V14_MAX) / s);
}

static s32 map_round(s32 min, s32 max, u16 v14) {
  s32 span;
  s32 raw;

  if(v14 > MIDI_NRPN_V14_MAX) {
    v14 = MIDI_NRPN_V14_MAX;
  }
  if(v14 == 0) {
    return min;
  }
  if(v14 == MIDI_NRPN_V14_MAX) {
    return max;
  }

  span = max - min;
  if(span >= 0) {
    raw = min + (s32)(((u32)v14 * (u32)span + 8191u) / (u32)MIDI_NRPN_V14_MAX);
  } else {
    raw = min - (s32)(((u32)v14 * (u32)(-span) + 8191u) / (u32)MIDI_NRPN_V14_MAX);
  }
  if(raw < min) {
    raw = min;
  }
  if(raw > max) {
    raw = max;
  }
  return raw;
}

ParamValue midi_nrpn_map_v14(const ParamDesc *d, u16 v14) {
  s32 min;
  s32 max;

  if(d == NULL) {
    return 0;
  }
  min = d->min;
  max = d->max;
  if(v14 > MIDI_NRPN_V14_MAX) {
    v14 = MIDI_NRPN_V14_MAX;
  }

  if(d->type == eParamTypeBool) {
    return (ParamValue)((v14 < 8192) ? min : max);
  }
  if(d->type == eParamTypeLabel) {
    return (ParamValue)map_round(min, max, v14);
  }
  return (ParamValue)midi_nrpn_v14_to_range(min, max, v14);
}

u16 midi_nrpn_raw_to_v14(const ParamDesc *d, ParamValue raw) {
  s32 min;
  s32 max;
  s32 v;

  if(d == NULL) {
    return 0;
  }
  min = d->min;
  max = d->max;
  v = (s32)raw;

  if(d->type == eParamTypeBool) {
    return (u16)((v == min) ? 0 : MIDI_NRPN_V14_MAX);
  }
  return midi_nrpn_range_to_v14(min, max, v);
}

static u8 fmt_u_small(char *dst, u8 v) {
  u8 n = 0;
  char tmp[3];
  u8 i;

  if(v == 0) {
    dst[0] = '0';
    return 1;
  }
  while(v && n < 3) {
    tmp[n++] = (char)('0' + (v % 10));
    v = (u8)(v / 10);
  }
  for(i = 0; i < n; ++i) {
    dst[i] = tmp[n - 1 - i];
  }
  return n;
}

u8 midi_nrpn_fmt_msb_lsb(char *dst, u8 dst_len, u16 v14) {
  u8 msb;
  u8 lsb;
  u8 n;

  if(dst == NULL || dst_len < 2) {
    return 0;
  }
  if(v14 > MIDI_NRPN_V14_MAX) {
    v14 = MIDI_NRPN_V14_MAX;
  }
  msb = (u8)((v14 >> 7) & 0x7f);
  lsb = (u8)(v14 & 0x7f);
  n = fmt_u_small(dst, msb);
  if((u8)(n + 1) >= dst_len) {
    dst[dst_len - 1] = '\0';
    return (u8)(dst_len - 1);
  }
  dst[n++] = ':';
  n = (u8)(n + fmt_u_small(dst + n, lsb));
  if(n >= dst_len) {
    dst[dst_len - 1] = '\0';
    return (u8)(dst_len - 1);
  }
  dst[n] = '\0';
  return n;
}
