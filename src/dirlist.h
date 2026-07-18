#ifndef BETWEEN_DIRLIST_H
#define BETWEEN_DIRLIST_H

#include "between_limits.h"
#include "types.h"

typedef struct {
  char names[BETWEEN_DIR_MAX][BETWEEN_NAME_LEN];
  u16 count;
} DirList;

/* list files under path with given extension (e.g. ".txt" or ".ldr").
 * stores stems (no extension). return count. */
u16 dirlist_scan(DirList *list, const char *path, const char *ext);

#endif
