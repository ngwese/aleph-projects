#include "name_edit.h"

#include <string.h>

#include "app.h"
#include "events.h"

#include "between_limits.h"
#include "modal.h"
#include "pages.h"
#include "render.h"

/* A–O | P–Z | a–o | p–z | 0–9-_ */
static const char *const charset_rows[5] = {
    "ABCDEFGHIJKLMNO",
    "PQRSTUVWXYZ",
    "abcdefghijklmno",
    "pqrstuvwxyz",
    "0123456789-_",
};

#define CHARSET_N 64 /* 26 + 26 + 10 + 2 */
#define CHARSET_ROWS 5

static char stem_buf[BETWEEN_NAME_LEN];
static u8 cursor;
static u8 palette_idx;
static u8 select_held;
static NameEditKind edit_kind;
static name_edit_done_fn done_fn;
static void *done_ctx;

static u8 stem_len(void) {
  u8 n = 0;
  while(stem_buf[n] != '\0' && n < BETWEEN_NAME_LEN - 1) {
    ++n;
  }
  return n;
}

static char palette_char(u8 idx) {
  u8 r;
  u8 off = 0;
  u8 len;

  if(idx >= CHARSET_N) {
    return 'a';
  }
  for(r = 0; r < CHARSET_ROWS; ++r) {
    len = (u8)strlen(charset_rows[r]);
    if(idx < off + len) {
      return charset_rows[r][idx - off];
    }
    off = (u8)(off + len);
  }
  return 'a';
}

static void row_sel_for_palette(u8 idx, u8 *out_row, u8 *out_sel) {
  u8 r;
  u8 off = 0;
  u8 len;

  for(r = 0; r < CHARSET_ROWS; ++r) {
    len = (u8)strlen(charset_rows[r]);
    if(idx < off + len) {
      *out_row = r;
      *out_sel = (u8)(idx - off);
      return;
    }
    off = (u8)(off + len);
  }
  *out_row = 0;
  *out_sel = 0;
}

static void redraw(void) {
  u8 r;
  u8 prow;
  u8 psel;
  const char *title =
    (edit_kind == eNameEditSetup) ? "setup name" : "preset name";

  render_clear();
  render_header(title, 0);
  render_edit_string(0, stem_buf, cursor);

  if(select_held) {
    row_sel_for_palette(palette_idx, &prow, &psel);
    for(r = 0; r < CHARSET_ROWS; ++r) {
      render_charset_row((u8)(1 + r), charset_rows[r],
			 (r == prow) ? psel : (u8)0xff);
    }
  }

  render_footer("select", "clear", "cancel", "ok");
}

/* drop state without touching input; modal_abort() owns the registry side. */
static void name_edit_reset(void) {
  select_held = 0;
  done_fn = NULL;
  done_ctx = NULL;
}

static const Modal name_edit_modal = {
  .name = "name",
  .redraw_fn = redraw,
  .abort_fn = name_edit_reset,
};

static void close_modal(u8 ok) {
  name_edit_done_fn fn = done_fn;
  void *ctx = done_ctx;
  char stem[BETWEEN_NAME_LEN];

  name_edit_reset();
  /* restores the enc/sw bindings captured by modal_open */
  modal_close();

  if(ok && fn != NULL) {
    strncpy(stem, stem_buf, BETWEEN_NAME_LEN - 1);
    stem[BETWEEN_NAME_LEN - 1] = '\0';
    fn(stem, ctx);
  }
}

static void insert_char(char ch) {
  u8 len = stem_len();

  if(cursor < len) {
    stem_buf[cursor] = ch;
  } else if(len < BETWEEN_NAME_LEN - 1) {
    stem_buf[len] = ch;
    stem_buf[len + 1] = '\0';
    cursor = len;
  } else {
    return;
  }
  if(cursor < BETWEEN_NAME_LEN - 2) {
    ++cursor;
  } else if(cursor < BETWEEN_NAME_LEN - 1) {
    /* stay on last editable cell */
    cursor = (u8)(BETWEEN_NAME_LEN - 2);
  }
}

static void handle_enc0(s32 data) { (void)data; }
static void handle_enc1(s32 data) { (void)data; }
static void handle_enc3(s32 data) { (void)data; }

static void handle_enc2(s32 data) {
  u8 len;
  s8 dir = (data > 0) ? 1 : -1;

  if(select_held) {
    if(dir > 0) {
      if(palette_idx < CHARSET_N - 1) {
	++palette_idx;
      }
    } else {
      if(palette_idx > 0) {
	--palette_idx;
      }
    }
  } else {
    len = stem_len();
    if(dir > 0) {
      if(cursor < len) {
	++cursor;
      } else if(len < BETWEEN_NAME_LEN - 1 && cursor == len) {
	/* already at append position */
      } else if(len < BETWEEN_NAME_LEN - 1) {
	cursor = len;
      }
    } else {
      if(cursor > 0) {
	--cursor;
      }
    }
  }
  render_mark_dirty();
}

static void handle_sw0(s32 data) {
  /* select: hold shows palette; release inserts */
  if(data > 0) {
    select_held = 1;
    render_mark_dirty();
    return;
  }
  if(!select_held) {
    return;
  }
  select_held = 0;
  insert_char(palette_char(palette_idx));
  render_mark_dirty();
}

static void handle_sw1(s32 data) {
  if(data <= 0) {
    return;
  }
  stem_buf[0] = '\0';
  cursor = 0;
  render_mark_dirty();
}

static void handle_sw2(s32 data) {
  if(data <= 0) {
    return;
  }
  close_modal(0);
}

static void handle_sw3(s32 data) {
  if(data <= 0) {
    return;
  }
  if(stem_buf[0] == '\0') {
    render_log("empty name");
    return;
  }
  close_modal(1);
}

void name_edit_open(NameEditKind kind, const char *initial,
		    name_edit_done_fn on_ok, void *ctx) {
  edit_kind = kind;
  done_fn = on_ok;
  done_ctx = ctx;
  cursor = 0;
  palette_idx = 0;
  select_held = 0;
  g_alt_mode = 0;

  stem_buf[0] = '\0';
  if(initial != NULL && initial[0] != '\0') {
    strncpy(stem_buf, initial, BETWEEN_NAME_LEN - 1);
    stem_buf[BETWEEN_NAME_LEN - 1] = '\0';
  }

  modal_open(&name_edit_modal);

  app_event_handlers[kEventEncoder0] = handle_enc0;
  app_event_handlers[kEventEncoder1] = handle_enc1;
  app_event_handlers[kEventEncoder2] = handle_enc2;
  app_event_handlers[kEventEncoder3] = handle_enc3;
  app_event_handlers[kEventSwitch0] = handle_sw0;
  app_event_handlers[kEventSwitch1] = handle_sw1;
  app_event_handlers[kEventSwitch2] = handle_sw2;
  app_event_handlers[kEventSwitch3] = handle_sw3;

  render_mark_dirty();
}
