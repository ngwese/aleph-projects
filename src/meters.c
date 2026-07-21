#include "meters.h"

#include "render.h"

static bfin_meter_bank_t g_meters_in = {{0, 0, 0, 0}};
static bfin_meter_bank_t g_meters_out = {{0, 0, 0, 0}};

void meters_init(void) {
  u8 i;
  for(i = 0; i < BFIN_METER_CH; i++) {
    g_meters_in.ch[i] = 0;
    g_meters_out.ch[i] = 0;
  }
}

const bfin_meter_bank_t *meters_in(void) {
  return &g_meters_in;
}

const bfin_meter_bank_t *meters_out(void) {
  return &g_meters_out;
}

static u8 bank_changed(const bfin_meter_bank_t *a, const bfin_meter_bank_t *b) {
  u8 i;
  for(i = 0; i < BFIN_METER_CH; i++) {
    if(a->ch[i] != b->ch[i]) {
      return 1;
    }
  }
  return 0;
}

u8 meters_poll(void) {
  bfin_meter_bank_t in;
  bfin_meter_bank_t out;
  u8 changed;

  bfin_get_meter_bank(BFIN_METER_BANK_IN, &in);
  bfin_get_meter_bank(BFIN_METER_BANK_OUT, &out);
  changed = bank_changed(&in, &g_meters_in) || bank_changed(&out, &g_meters_out);
  g_meters_in = in;
  g_meters_out = out;
  if(changed) {
    /* header VU only; avoid full page redraw */
    render_header_midi_refresh();
  }
  return changed;
}
