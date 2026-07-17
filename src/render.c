/*
  render.c
  
  aleph-spray

  definitions for rendering graphics.

*/

//--- std headers
#include <string.h>

//--- libavr32 headers
// region class
#include "region.h"

//--- aleph-specific headers
// application framework
#include "app.h"

//--- app-specific headers
#include "ctl.h"
#include "render.h"

//-------------------------------------------------
//----- -static variables

/* 
   the screen-drawing routines in avr32_lib provide the "region" object
   a simple 16-bit pixel buffer with width, height, x/y offset, and dirty flag.
   there are also methods for basic fill and text rendering into regions.
*/

// Four-character level values stacked in the upper-left corner.
static region regChan[] = {
  {.w = 24, .h = 8, .x = 0, .y = 0},
  {.w = 24, .h = 8, .x = 0, .y = 8},
  {.w = 24, .h = 8, .x = 0, .y = 16},
  {.w = 24, .h = 8, .x = 0, .y = 24},
};

// four compact xrun counters across the bottom
static region regXruns = {
  .w = 128,
  .h = 8,
  .x = 0,
  .y = 56};

// full-screen scrolling region for startup diagnostics
static region bootScrollRegion = {
  .w = 128,
  .h = 64,
  .x = 0,
  .y = 0};
static scroll bootScroll;

//-------------------------------------------------
//----- static functions

static void format_u32(char* buf, u32 val) {
  char reverse[10];
  u8 digits = 0;
  u8 i;

  if(val > 99999) {
    memcpy(buf, ">99k", 5);
    return;
  }

  do {
    reverse[digits++] = '0' + (val % 10);
    val /= 10;
  } while(val);

  for(i = 0; i < digits; i++) {
    buf[i] = reverse[digits - i - 1];
  }
  buf[digits] = '\0';
}

//-------------------------------------------------
//----- external functions

// initialze renderer
void render_init(void) {
  region_alloc(&bootScrollRegion);
  scroll_init(&bootScroll, &bootScrollRegion);

  // allocate memory for each channel region
  region_alloc(&regChan[0]);
  region_alloc(&regChan[1]);
  region_alloc(&regChan[2]);
  region_alloc(&regChan[3]);
  region_alloc(&regXruns);
}

// render text to scrolling buffer during boot (immediate screen blit)
void render_boot(const char* str) {
  int i;
  u8* p = bootScroll.reg->data;

  // dim older lines so new text stands out
  for(i = 0; i < bootScroll.reg->len; i++) {
    if(*p > 0x4) { *p = 0x4; }
    p++;
  }
  scroll_string_front(&bootScroll, (char*)str);
  region_draw(bootScroll.reg);
}

// fill with initial graphics
void render_startup(void) {
  u32 i;
  for(i = 0; i < 4; i++) {
    // fill the graphics buffer (with black)
    region_fill(&(regChan[i]), 0x0);
    // physically render the region data to the screen
    region_draw(&(regChan[i]));
  }
  region_fill(&regXruns, 0x0);
  region_draw(&regXruns);
}

// update dirty regions
// (this will be called from an application timer)
void render_update(void) {
  app_pause();

  // physically update the screen with each region's data (if changed)
  region_draw(&(regChan[0]));
  region_draw(&(regChan[1]));
  region_draw(&(regChan[2]));
  region_draw(&(regChan[3]));
  region_draw(&regXruns);

  app_resume();
}

// render amplitude
void render_chan(u8 ch) {
  // tmp decibel value
  s32 db;
  // stupid way to show channel numbers
  // static const char num[4][3] = {"0.", "1.", "2.", "3."};
  // text buffer
  static char buf[5];
  // point at the appropriate region
  region* reg = &(regChan[ch]);

  // clear the region
  region_fill(reg, 1);

  // build a compact integer decibel value string
  memset(buf, 0, sizeof(buf));
  db = ctl_get_amp_db(ch);
  // print "-inf" if very small,
  // otherwise print the value
  // make sure this is a signed comparison
  if(db < (s32)0xffbf0000) {
    memcpy(buf, "-inf\0", 5);
  } else {
    s32 dbInteger = db >> 16;
    u32 magnitude;
    u8 pos = 0;

    if(dbInteger < 0) {
      buf[pos++] = '-';
      magnitude = (u32)(-dbInteger);
    } else {
      magnitude = (u32)dbInteger;
    }
    format_u32(buf + pos, magnitude);
  }

  // use the small system font
  region_string(reg, buf, 0, 0, 0xf, 0, 0);

  // if channel is muted,
  // highlight background, limit the foreground,
  // invert the value
  if(ctl_get_mute(ch)) {
    region_hl(reg, 2, 2);
    region_max(reg, 6);
  }

  // write label in small font
  // region_string(reg, num[ch], 0, 0, 0xf, 1, 0);

  // the render functions set the region's dirty flag,
  // so there's nothing left to do now,
  // except wait for the screen refresh timer to trigger a redraw.
}

void render_xruns(u32 windowRx, u32 windowTx, u32 clashRx, u32 clashTx) {
  char buf[6];
  u32 values[4];
  u8 i;

  values[0] = windowRx;
  values[1] = windowTx;
  values[2] = clashRx;
  values[3] = clashTx;

  region_fill(&regXruns, 0x0);
  for(i = 0; i < 4; i++) {
    format_u32(buf, values[i]);
    region_string(&regXruns, buf, i * 32, 0, 0xf, 0, 0);
  }
}
