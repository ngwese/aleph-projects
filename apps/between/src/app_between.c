/*
  app_between.c

  aleph/app/between
*/

#include "delay.h"
#include "gpio.h"
#include "print_funcs.h"
#include "sd_mmc_spi.h"

#include "app.h"
#include "bfin.h"
#include "conf_board.h"

#include "app_timers.h"
#include "between_limits.h"
#include "cv_in.h"
#include "files_ensure.h"
#include "handler.h"
#include "meters.h"
#include "pages.h"
#include "render.h"
#include "scaler_tables.h"
#include "state.h"

#ifndef VERSIONSTRING
#define VERSIONSTRING "0.0.1"
#endif

void app_init(void) {
  print_dbg("\r\n between; app_init...");
  render_init();
  state_init();
}

u8 app_launch(eLaunchState state) {
  char last[BETWEEN_NAME_LEN];
  u8 have_setup = 0;

  print_dbg("\r\n launching between; state=");
  print_dbg_ulong(state);

  render_boot("BETWEEN " VERSIONSTRING);

  print_dbg("\r\n between; waiting for SD...");
  while (!sd_mmc_spi_mem_check()) {
    render_boot("waiting for SD...");
  }

  render_boot("data dirs...");
  if (!files_ensure_data_dirs()) {
    render_log("dirs fail");
    print_dbg("\r\n between; failed to ensure data dirs");
  }

  render_boot("scalers...");
  scaler_tables_init();

  init_app_timers();
  meters_init();
  cv_in_init();
  pages_init();
  assign_event_handlers();

  have_setup = state_read_last_setup(last, sizeof(last));
  if (have_setup && state_load_setup(last)) {
    render_boot("setup loaded");
    pages_enter_play();
  } else {
    render_boot("edit mode");
    pages_enter_edit();
  }
  cv_in_sync_poll();

  (void)state;
  return 1;
}
