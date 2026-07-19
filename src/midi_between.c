#include "midi_between.h"

#include "midi_common.h"
#include "morph2d.h"
#include "pages.h"
#include "render.h"
#include "state.h"

/* MIDI channel 16 (1-based) → ch index 15.
 * CC14 / CC15 → morph x / y (SPEC).
 * channels 1–4 (ch 0–3) reserved for slots A–D (message set TBD). */

#define MIDI_CH_SETUP 15
#define MIDI_CC_MORPH_X 14
#define MIDI_CC_MORPH_Y 15

static u16 cc_to_morph(u8 val) {
  if(val >= 127) {
    return MORPH2D_ONE;
  }
  return (u16)(((u32)val * MORPH2D_ONE) / 127u);
}

static void on_control_change(u8 ch, u8 num, u8 val) {
  u16 x;
  u16 y;

  if(ch != MIDI_CH_SETUP) {
    /* ch 0–3: slot A–D — not implemented in base MIDI */
    return;
  }

  x = g_slots.x;
  y = g_slots.y;
  if(num == MIDI_CC_MORPH_X) {
    x = cc_to_morph(val);
  } else if(num == MIDI_CC_MORPH_Y) {
    y = cc_to_morph(val);
  } else {
    return;
  }

  slots_set_morph(&g_slots, x, y);
  state_apply();
  /* refresh header morph cursor and play-mode morph / param readouts */
  pages_redraw();
  render_update();
}

static midi_behavior_t g_midi_beh = {
    .note_on = NULL,
    .note_off = NULL,
    .channel_pressure = NULL,
    .pitch_bend = NULL,
    .control_change = on_control_change,
    .program_change = NULL,
    .clock_tick = NULL,
    .seq_start = NULL,
    .seq_stop = NULL,
    .seq_continue = NULL,
    .panic = NULL,
    .aftertouch = NULL,
};

void between_midi_handle_packet(u32 data) {
  midi_packet_parse(&g_midi_beh, data);
}
