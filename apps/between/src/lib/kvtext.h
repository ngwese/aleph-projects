/* kvtext — parse/write a single key:value text line */

#ifndef BETWEEN_KVTEXT_H
#define BETWEEN_KVTEXT_H

#include "types.h"

#define KVTEXT_KEY_MAX 32
#define KVTEXT_VAL_MAX 96

typedef enum { eKvBlank = 0, eKvComment, eKvPair, eKvMalformed } KvLineKind;

typedef struct {
  KvLineKind kind;
  char key[KVTEXT_KEY_MAX];
  char val[KVTEXT_VAL_MAX];
} KvPair;

/* classify and parse one null-terminated line (may include trailing \n/\r). */
KvLineKind kvtext_parse_line(const char *line, KvPair *out);

/* write "key:value\n" into buf. returns bytes written (excluding NUL), or -1.
 */
s32 kvtext_format_line(char *buf, u32 buf_size, const char *key,
                       const char *val);

/* true if keys match (case-sensitive). */
u8 kvtext_key_eq(const char *a, const char *b);

#endif
