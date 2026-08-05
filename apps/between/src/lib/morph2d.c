#include "morph2d.h"

#include <stddef.h>

void morph2d_clamp(u16 *x, u16 *y) {
  /* MORPH2D_ONE == UINT16_MAX; compare via u32 so the check stays valid if
   * MORPH2D_ONE is ever lowered. */
  if (x != NULL && (u32)*x > MORPH2D_ONE) {
    *x = (u16)MORPH2D_ONE;
  }
  if (y != NULL && (u32)*y > MORPH2D_ONE) {
    *y = (u16)MORPH2D_ONE;
  }
}

void morph2d_slot_corner(MorphSlot slot, u16 *x, u16 *y) {
  if (x == NULL || y == NULL) {
    return;
  }
  switch (slot) {
  case eMorphSlotA:
    *x = 0;
    *y = 0;
    break;
  case eMorphSlotB:
    *x = MORPH2D_ONE;
    *y = 0;
    break;
  case eMorphSlotC:
    *x = 0;
    *y = MORPH2D_ONE;
    break;
  case eMorphSlotD:
  default:
    *x = MORPH2D_ONE;
    *y = MORPH2D_ONE;
    break;
  }
}

void morph2d_weights(u16 x, u16 y, const u8 occupied[MORPH2D_SLOTS],
                     u16 out_w[MORPH2D_SLOTS]) {
  u16 inv_x;
  u16 inv_y;
  u32 raw[MORPH2D_SLOTS];
  u32 sum = 0;
  u32 i;

  morph2d_clamp(&x, &y);
  inv_x = (u16)(MORPH2D_ONE - x);
  inv_y = (u16)(MORPH2D_ONE - y);

  /* wa = (1-x)*(1-y) ; wb = x*(1-y) ; wc = (1-x)*y ; wd = x*y */
  raw[eMorphSlotA] = ((u32)inv_x * inv_y) / MORPH2D_ONE;
  raw[eMorphSlotB] = ((u32)x * inv_y) / MORPH2D_ONE;
  raw[eMorphSlotC] = ((u32)inv_x * y) / MORPH2D_ONE;
  raw[eMorphSlotD] = ((u32)x * y) / MORPH2D_ONE;

  for (i = 0; i < MORPH2D_SLOTS; ++i) {
    if (occupied == NULL || !occupied[i]) {
      raw[i] = 0;
    }
    sum += raw[i];
  }

  if (sum == 0) {
    for (i = 0; i < MORPH2D_SLOTS; ++i) {
      out_w[i] = 0;
    }
    return;
  }

  if (sum == MORPH2D_ONE) {
    for (i = 0; i < MORPH2D_SLOTS; ++i) {
      out_w[i] = (u16)raw[i];
    }
    return;
  }

  /* renormalize occupied weights to sum MORPH2D_ONE */
  {
    u32 acc = 0;
    u32 last = 0;
    for (i = 0; i < MORPH2D_SLOTS; ++i) {
      if (raw[i] == 0) {
        out_w[i] = 0;
      } else {
        out_w[i] = (u16)(((u32)raw[i] * MORPH2D_ONE) / sum);
        acc += out_w[i];
        last = i;
      }
    }
    if (acc != MORPH2D_ONE && out_w[last] != 0) {
      if (acc < MORPH2D_ONE) {
        out_w[last] = (u16)(out_w[last] + (MORPH2D_ONE - acc));
      } else if (out_w[last] >= (u16)(acc - MORPH2D_ONE)) {
        out_w[last] = (u16)(out_w[last] - (acc - MORPH2D_ONE));
      }
    }
  }
}

s32 morph2d_blend_s32(const u16 w[MORPH2D_SLOTS], const s32 v[MORPH2D_SLOTS]) {
  s64 acc = 0;
  u32 i;
  for (i = 0; i < MORPH2D_SLOTS; ++i) {
    acc += (s64)w[i] * (s64)v[i];
  }
  if (acc >= 0) {
    return (s32)((acc + (MORPH2D_ONE / 2)) / MORPH2D_ONE);
  }
  return (s32)(-(((-acc) + (MORPH2D_ONE / 2)) / MORPH2D_ONE));
}

s8 morph2d_pick_discrete(const u16 w[MORPH2D_SLOTS],
                         const u8 occupied[MORPH2D_SLOTS]) {
  s8 best = -1;
  u16 best_w = 0;
  u32 i;
  for (i = 0; i < MORPH2D_SLOTS; ++i) {
    if (occupied != NULL && occupied[i] && w[i] >= best_w) {
      if (best < 0 || w[i] > best_w) {
        best_w = w[i];
        best = (s8)i;
      }
    }
  }
  return best;
}
