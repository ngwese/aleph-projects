#ifndef BETWEEN_MIDI_BETWEEN_H
#define BETWEEN_MIDI_BETWEEN_H

#include "types.h"

/* handle a kEventMidiPacket payload (USB MIDI word). */
void between_midi_handle_packet(u32 data);

#endif
