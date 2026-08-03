/*
  app_device_test.c

  aleph/app/device-test
*/

#include "print_funcs.h"

#include "app.h"

#include "app_timers.h"
#include "handler.h"
#include "render.h"

#ifndef VERSIONSTRING
#define VERSIONSTRING "0.0.1"
#endif

void app_init(void) {
  print_dbg("\r\n device-test; app_init...");
  render_init();
}

u8 app_launch(eLaunchState state) {
  print_dbg("\r\n launching device-test; state=");
  print_dbg_ulong(state);

  render_boot("DEVICE-TEST " VERSIONSTRING);

  init_app_timers();
  assign_event_handlers();

  render_set_focus(FOCUS_NONE);
  render_mark_dirty();
  render_frame_service();

  (void)state;
  return 1;
}
