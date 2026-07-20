#ifndef BETWEEN_MIDI_NRPN_H
#define BETWEEN_MIDI_NRPN_H

#include "param_common.h"
#include "types.h"

#define MIDI_NRPN_PARAM_MAX 1023
#define MIDI_NRPN_V14_MAX 16383

u16 midi_nrpn_param_index(u8 msb, u8 lsb);
u16 midi_nrpn_v14(u8 data_msb, u8 data_lsb);

/* linear map/unmap on an arbitrary [min, max] axis (e.g. scaler io). */
s32 midi_nrpn_v14_to_range(s32 min, s32 max, u16 v14);
u16 midi_nrpn_range_to_v14(s32 min, s32 max, s32 v);

/* map via ParamDesc.min…max (bool / label / unscaled fallback). */
ParamValue midi_nrpn_map_v14(const ParamDesc *d, u16 v14);
u16 midi_nrpn_raw_to_v14(const ParamDesc *d, ParamValue raw);

/* write "msb:lsb" (no padding) for a 14-bit quantity. returns length. */
u8 midi_nrpn_fmt_msb_lsb(char *dst, u8 dst_len, u16 v14);

#endif
