#ifndef BETWEEN_MIDI_NRPN_H
#define BETWEEN_MIDI_NRPN_H

#include "param_common.h"
#include "types.h"

#define MIDI_NRPN_PARAM_MAX 1023
#define MIDI_NRPN_V14_MAX 16383

u16 midi_nrpn_param_index(u8 msb, u8 lsb);
u16 midi_nrpn_v14(u8 data_msb, u8 data_lsb);

/* map absolute 14-bit data entry into ParamDesc.min…max (raw bank domain). */
ParamValue midi_nrpn_map_v14(const ParamDesc *d, u16 v14);

/* write "msb:lsb" (no padding) into dst. returns string length. */
u8 midi_nrpn_fmt_addr(char *dst, u8 dst_len, u16 param_index);

#endif
