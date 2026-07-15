/*
  app_spray.c

  aleph/app/spray

  required app-specific implementation of avr32_lib/src/app.h
*/

// asf
#include "delay.h"
#include "gpio.h"
#include "print_funcs.h"
#include "sd_mmc_spi.h"

// avr32_lib
#include "app.h"
#include "bfin.h"
#include "encoders.h"

//--- app-specific
#include "app_timers.h"
#include "ctl.h"
#include "files.h"
#include "handler.h"
#include "render.h"

// this is called during hardware initialization.
// allocate memory here.
void app_init(void) {
  print_dbg("\r\n spray; app_init...");
  render_init();
}

// this is called from the event queue to start the app
// return >0 if there is an error doing firstrun init
u8 app_launch(eLaunchState state) {
  u8 dspOk;

  print_dbg("\r\n launching app with state: ");
  print_dbg_ulong(state);

  // wait for SD card (FAT already initialized in main)
  print_dbg("\r\n spray; waiting for SD card...");
  while (!sd_mmc_spi_mem_check()) {
    ;
  }
  print_dbg("\r\n spray; SD card ready");

  // load companion DSP module from /mod/spray.ldr
  dspOk = files_load_dsp(DEFAULT_LDR);
  if (!dspOk) {
    print_dbg("\r\n spray; failed to load /mod/spray.ldr");
  } else {
    bfin_wait_ready();

    // extra few ms...
    delay_ms(10);

    // enable audio
    bfin_enable();
  }

  // enable timers
  init_app_timers();

  // render initial screen
  render_startup();
  render_update();

  // set hardcoded default values (safe even if DSP failed to load)
  ctl_init();

  if (state == eLaunchStateFirstRun) {
    // this was the first run since firwmare was flashed.
    // do any necessary flash initialization here.
  } else {
    if (state == eLaunchStateClean) {
      // use this condition to launch in a "default" or "clean" mode,
      // with known settings
    } else {
      // go ahead and load stored state from flash...
    }
  }

  // set app event handlers
  assign_event_handlers();

  // tell the main loop that we launched successfully.
  // if this was the first run,
  // main() should now write the firstrun pattern to flash and reboot.
  return 1;
}
