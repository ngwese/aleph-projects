#include "input_roles.h"

#include "app.h"
#include "encoders.h"
#include "events.h"
#include "pages.h"

static input_role_fn g_enc_fn[4];
static InputRole g_enc_role[4];
static input_role_fn g_sw_fn[4];
static InputSwRole g_sw_role[4];

static u8 role_thresh(InputRole role) {
  switch (role) {
  case eInputRolePageSelect:
    return 24;
  case eInputRoleListSelect:
    return 4;
  case eInputRoleParamFine:
    return 0;
  case eInputRoleParamCoarse:
    /* same thresh as fine so posted tick counts reach the handler; step
     * size is applied in the page (0x100 * data), matching live play. */
    return 0;
  case eInputRoleUnmapped:
  default:
    return 4;
  }
}

static void enc_dispatch(u8 idx, s32 data) {
  InputRole role;
  input_role_fn fn;

  if (idx >= 4 || data == 0) {
    return;
  }
  role = g_enc_role[idx];
  fn = g_enc_fn[idx];
  switch (role) {
  case eInputRoleUnmapped:
    return;
  case eInputRolePageSelect:
    if (fn != NULL) {
      fn(data);
    } else {
      pages_next(data > 0 ? 1 : -1);
    }
    return;
  case eInputRoleListSelect:
  case eInputRoleParamFine:
  case eInputRoleParamCoarse:
    if (fn != NULL) {
      fn(data);
    }
    return;
  default:
    return;
  }
}

static void sw_dispatch(u8 idx, s32 data) {
  InputSwRole role;
  input_role_fn fn;

  if (idx >= 4) {
    return;
  }
  role = g_sw_role[idx];
  fn = g_sw_fn[idx];
  switch (role) {
  case eInputSwRoleUnmapped:
    return;
  case eInputSwRoleAction:
    if (fn != NULL) {
      fn(data);
    }
    return;
  case eInputSwRoleAlt:
    g_alt_mode = data > 0 ? 1 : 0;
    if (fn != NULL) {
      fn(data);
    }
    return;
  default:
    return;
  }
}

static void handle_role_enc0(s32 data) { enc_dispatch(0, data); }
static void handle_role_enc1(s32 data) { enc_dispatch(1, data); }
static void handle_role_enc2(s32 data) { enc_dispatch(2, data); }
static void handle_role_enc3(s32 data) { enc_dispatch(3, data); }

static void handle_role_sw0(s32 data) { sw_dispatch(0, data); }
static void handle_role_sw1(s32 data) { sw_dispatch(1, data); }
static void handle_role_sw2(s32 data) { sw_dispatch(2, data); }
static void handle_role_sw3(s32 data) { sw_dispatch(3, data); }

void input_roles_bind(const InputEncBinding enc[4],
                      const InputSwBinding sw[4]) {
  u8 i;
  static void (*const enc_wrappers[4])(s32 data) = {
      handle_role_enc0, handle_role_enc1, handle_role_enc2, handle_role_enc3};
  static void (*const sw_wrappers[4])(s32 data) = {
      handle_role_sw0, handle_role_sw1, handle_role_sw2, handle_role_sw3};

  if (enc != NULL) {
    for (i = 0; i < 4; ++i) {
      g_enc_role[i] = enc[i].role;
      g_enc_fn[i] = enc[i].fn;
      set_enc_thresh(i, role_thresh(enc[i].role));
      app_event_handlers[kEventEncoder0 + i] = enc_wrappers[i];
    }
  }
  if (sw != NULL) {
    for (i = 0; i < 4; ++i) {
      g_sw_role[i] = sw[i].role;
      g_sw_fn[i] = sw[i].fn;
      app_event_handlers[kEventSwitch0 + i] = sw_wrappers[i];
    }
  }
}
