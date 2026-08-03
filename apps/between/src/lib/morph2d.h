/* morph2d — bilinear morph weights and parameter blending */

#ifndef BETWEEN_MORPH2D_H
#define BETWEEN_MORPH2D_H

#include "types.h"

#define MORPH2D_SLOTS 4
#define MORPH2D_ONE 65535u

/* slot corners (origin at A upper-left; y increases downward):
 * A (0,0), B (1,0), C (0,1), D (1,1)
 */
typedef enum {
  eMorphSlotA = 0,
  eMorphSlotB = 1,
  eMorphSlotC = 2,
  eMorphSlotD = 3
} MorphSlot;

/* x,y in [0, MORPH2D_ONE]. occupied[i] non-zero if slot i has a preset.
 * out_w[i] normalized to sum MORPH2D_ONE among occupied slots (0 if none).
 */
void morph2d_weights(u16 x, u16 y, const u8 occupied[MORPH2D_SLOTS],
                     u16 out_w[MORPH2D_SLOTS]);

/* clamp x,y into [0, MORPH2D_ONE]. */
void morph2d_clamp(u16 *x, u16 *y);

/* continuous bilinear blend of four s32 values using weights. */
s32 morph2d_blend_s32(const u16 w[MORPH2D_SLOTS], const s32 v[MORPH2D_SLOTS]);

/* discrete pick: index of highest-weight occupied slot; -1 if none. */
s8 morph2d_pick_discrete(const u16 w[MORPH2D_SLOTS],
                         const u8 occupied[MORPH2D_SLOTS]);

/* corner position for a slot. */
void morph2d_slot_corner(MorphSlot slot, u16 *x, u16 *y);

#endif
