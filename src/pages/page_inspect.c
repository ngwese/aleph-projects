#include "pages.h"

#include <stddef.h>

#include "input_roles.h"
#include "render.h"

static void redraw(void) {
  render_clear();
  render_header("inspect", 0);
  render_inspect_cv_values();
  render_inspect_vu_bars();
  render_footer("-", "-", "-", "-");
}

void redraw_inspect(void) { redraw(); }

void select_inspect(void) {
  static const InputEncBinding enc[4] = {
    {eInputRoleUnmapped, NULL},
    {eInputRoleUnmapped, NULL},
    {eInputRoleUnmapped, NULL},
    {eInputRoleUnmapped, NULL},
  };
  static const InputSwBinding sw[4] = {
    {eInputSwRoleUnmapped, NULL},
    {eInputSwRoleUnmapped, NULL},
    {eInputSwRoleUnmapped, NULL},
    {eInputSwRoleUnmapped, NULL},
  };
  input_roles_bind(enc, sw);
  render_mark_dirty();
}

void page_inspect_init(void) {}
