#ifndef BETWEEN_MIDI_BETWEEN_H
#define BETWEEN_MIDI_BETWEEN_H

#include "param_common.h"
#include "types.h"

/* handle a kEventMidiPacket payload (USB MIDI word). */
void between_midi_handle_packet(u32 data);

/* NRPN ↔ slot raw: uses scaler io range when a usable scaler exists. */
ParamValue between_midi_v14_to_raw(u16 param_idx, u16 v14);
u16 between_midi_raw_to_v14(u16 param_idx, ParamValue raw);

#endif
