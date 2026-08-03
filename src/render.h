#ifndef _ALEPH_APP_SPRAY_RENDER_H_
#define _ALEPH_APP_SPRAY_RENDER_H_

#include "types.h"

// init
extern void render_init(void);

// startup diagnostic line on OLED (immediate blit)
extern void render_boot(const char* str);

// startup state
extern void render_startup(void);

// render a channel
extern void render_chan(u8 ch);

// render the four xrun counters across the bottom (u16 SPI counters)
extern void render_xruns(u16 windowRx, u16 windowTx, u16 clashRx, u16 clashTx);

// update
extern void render_update(void);

#endif  // h guard
