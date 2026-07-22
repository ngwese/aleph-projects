#include "input_roles.h"

#include "app.h"
#include "encoders.h"
#include "events.h"
#include "pages.h"

static input_role_fn g_role_fn[4];
static InputRole g_role[4];

static u8 role_thresh(InputRole role) {
  switch(role) {
  case eInputRolePageSelect:
    return 32;
  case eInputRoleListSelect:
    return 4;
  case eInputRoleParamFine:
    return 0;
  case eInputRoleParamCoarse:
    return 4;
  case eInputRoleUnmapped:
  default:
    return 4;
  }
}

static void role_dispatch(u8 idx, s32 data) {
  InputRole role;
  input_role_fn fn;

  if(idx >= 4 || data == 0) {
    return;
  }
  role = g_role[idx];
  fn = g_role_fn[idx];
  switch(role) {
  case eInputRoleUnmapped:
    return;
  case eInputRolePageSelect:
    if(fn != NULL) {
      fn(data);
    } else {
      pages_next(data > 0 ? 1 : -1);
    }
    return;
  case eInputRoleListSelect:
  case eInputRoleParamFine:
  case eInputRoleParamCoarse:
    if(fn != NULL) {
      fn(data);
    }
    return;
  default:
    return;
  }
}

static void handle_role_enc0(s32 data) { role_dispatch(0, data); }
static void handle_role_enc1(s32 data) { role_dispatch(1, data); }
static void handle_role_enc2(s32 data) { role_dispatch(2, data); }
static void handle_role_enc3(s32 data) { role_dispatch(3, data); }

void input_roles_bind(const InputEncBinding enc[4]) {
  u8 i;
  static void (*const wrappers[4])(s32 data) = {
    handle_role_enc0, handle_role_enc1, handle_role_enc2, handle_role_enc3};

  if(enc == NULL) {
    return;
  }
  for(i = 0; i < 4; ++i) {
    g_role[i] = enc[i].role;
    g_role_fn[i] = enc[i].fn;
    set_enc_thresh(i, role_thresh(enc[i].role));
    app_event_handlers[kEventEncoder0 + i] = wrappers[i];
  }
}
