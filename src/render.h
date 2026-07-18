#ifndef BETWEEN_RENDER_H
#define BETWEEN_RENDER_H

#include "morph2d.h"
#include "types.h"

/* content rows below the page header; row above footer is the diagnostic log. */
#define RENDER_CONTENT_ROWS 5
#define RENDER_LOG_CLEAR_MS 2000
#define RENDER_TICK_MS 50

void render_init(void);
void render_boot(const char *str);
void render_update(void);

/* clear content area only (does not clear header, log, or footer). */
void render_clear(void);
void render_line(u8 row, const char *str);
void render_line_inv(u8 row, const char *str);
/* draw str at pixel x within a content row (fg white on black). */
void render_line_at(u8 row, u8 x, const char *str);
/* status row: 2px mid-grey bar, 1px gap, then name (or "none"). */
void render_status_line(u8 row, const char *name);

/* name entry: draw str on content row with inverse glyph at cursor. */
void render_edit_string(u8 row, const char *str, u8 cursor);
/* charset row: draw chars with inverse glyph at sel (0xff = none). */
void render_charset_row(u8 row, const char *chars, u8 sel);

/* edit-mode page header: mid-grey bar, title box(es), right indicator. */
void render_header(const char *title, u8 dirty);
/* slot page: capital letter box, optional preset-name box, indicator. */
void render_header_slot(char slot_letter, const char *preset, u8 dirty);
void render_header_clear(void);

void render_footer(const char *a, const char *b, const char *c, const char *d);

/* diagnostic log on the line above the footer; redrawn immediately. */
void render_log(const char *str);
void render_log_clear(void);
/* call from the screen timer; clears log after RENDER_LOG_CLEAR_MS idle. */
void render_log_tick(void);

/* footer triangle for single-slot param targets (morph corner). */
void render_footer_slot_tri(u8 cell, MorphSlot slot);

/* play morph square: light-gray frame + 3×3 white cursor in content area. */
void render_play_morph(u16 x, u16 y);

#endif
