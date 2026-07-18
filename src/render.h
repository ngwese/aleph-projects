#ifndef BETWEEN_RENDER_H
#define BETWEEN_RENDER_H

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

#endif
