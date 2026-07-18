#include "handler.h"

#include "gpio.h"
#include "delay.h"
#include "print_funcs.h"

#include "conf_board.h"
#include "app.h"
#include "events.h"

#include "pages.h"

static void handle_Switch4(s32 data) {
  if(data > 0) {
    if(pages_toggle_play()) {
      gpio_set_gpio_pin(LED_MODE_PIN);
    } else {
      gpio_clr_gpio_pin(LED_MODE_PIN);
    }
  }
}

static void handle_Switch5(s32 data) {
  (void)data;
  delay_ms(100);
  gpio_clr_gpio_pin(POWER_CTL_PIN);
}

void assign_event_handlers(void) {
  /* page select installs enc/sw0-3; keep mode + power global */
  app_event_handlers[kEventSwitch4] = handle_Switch4;
  app_event_handlers[kEventSwitch5] = handle_Switch5;
}
