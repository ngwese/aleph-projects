#include "unity.h"

#include "morph2d.h"

void setUp(void) {}
void tearDown(void) {}

static void assert_weights_sum(const u16 w[MORPH2D_SLOTS]) {
  u32 sum = (u32)w[0] + w[1] + w[2] + w[3];
  TEST_ASSERT_EQUAL_UINT32(MORPH2D_ONE, sum);
}

void test_corners(void) {
  u8 occ[MORPH2D_SLOTS] = {1, 1, 1, 1};
  u16 w[MORPH2D_SLOTS];
  u16 x, y;

  morph2d_slot_corner(eMorphSlotA, &x, &y);
  morph2d_weights(x, y, occ, w);
  TEST_ASSERT_EQUAL_UINT16(MORPH2D_ONE, w[eMorphSlotA]);
  assert_weights_sum(w);

  morph2d_slot_corner(eMorphSlotB, &x, &y);
  morph2d_weights(x, y, occ, w);
  TEST_ASSERT_EQUAL_UINT16(MORPH2D_ONE, w[eMorphSlotB]);

  morph2d_slot_corner(eMorphSlotC, &x, &y);
  morph2d_weights(x, y, occ, w);
  TEST_ASSERT_EQUAL_UINT16(MORPH2D_ONE, w[eMorphSlotC]);

  morph2d_slot_corner(eMorphSlotD, &x, &y);
  morph2d_weights(x, y, occ, w);
  TEST_ASSERT_EQUAL_UINT16(MORPH2D_ONE, w[eMorphSlotD]);
}

void test_center(void) {
  u8 occ[MORPH2D_SLOTS] = {1, 1, 1, 1};
  u16 w[MORPH2D_SLOTS];
  morph2d_weights(MORPH2D_ONE / 2, MORPH2D_ONE / 2, occ, w);
  assert_weights_sum(w);
  /* each corner roughly equal */
  TEST_ASSERT_UINT16_WITHIN(64, MORPH2D_ONE / 4, w[0]);
  TEST_ASSERT_UINT16_WITHIN(64, MORPH2D_ONE / 4, w[1]);
  TEST_ASSERT_UINT16_WITHIN(64, MORPH2D_ONE / 4, w[2]);
  TEST_ASSERT_UINT16_WITHIN(64, MORPH2D_ONE / 4, w[3]);
}

void test_empty_renormalize(void) {
  u8 occ[MORPH2D_SLOTS] = {1, 0, 0, 0};
  u16 w[MORPH2D_SLOTS];
  morph2d_weights(MORPH2D_ONE / 2, MORPH2D_ONE / 2, occ, w);
  TEST_ASSERT_EQUAL_UINT16(MORPH2D_ONE, w[eMorphSlotA]);
  TEST_ASSERT_EQUAL_UINT16(0, w[eMorphSlotB]);
}

void test_no_occupied(void) {
  u8 occ[MORPH2D_SLOTS] = {0, 0, 0, 0};
  u16 w[MORPH2D_SLOTS];
  morph2d_weights(0, 0, occ, w);
  TEST_ASSERT_EQUAL_UINT16(0, (u16)(w[0] + w[1] + w[2] + w[3]));
}

void test_blend_and_discrete(void) {
  u16 w[MORPH2D_SLOTS] = {MORPH2D_ONE, 0, 0, 0};
  s32 v[MORPH2D_SLOTS] = {100, 200, 300, 400};
  u8 occ[MORPH2D_SLOTS] = {1, 1, 1, 1};
  TEST_ASSERT_EQUAL_INT32(100, morph2d_blend_s32(w, v));

  w[0] = MORPH2D_ONE / 2;
  w[1] = MORPH2D_ONE / 2;
  w[2] = 0;
  w[3] = 0;
  TEST_ASSERT_EQUAL_INT32(150, morph2d_blend_s32(w, v));

  TEST_ASSERT_EQUAL_INT8(eMorphSlotA, morph2d_pick_discrete(w, occ));
  w[0] = 10;
  w[1] = 1000;
  TEST_ASSERT_EQUAL_INT8(eMorphSlotB, morph2d_pick_discrete(w, occ));
}

void test_origin_and_far_corner(void) {
  u8 occ[MORPH2D_SLOTS] = {1, 1, 1, 1};
  u16 w[MORPH2D_SLOTS];

  morph2d_weights(0, 0, occ, w);
  TEST_ASSERT_EQUAL_UINT16(MORPH2D_ONE, w[eMorphSlotA]);
  TEST_ASSERT_EQUAL_UINT16(0, w[eMorphSlotB]);
  TEST_ASSERT_EQUAL_UINT16(0, w[eMorphSlotC]);
  TEST_ASSERT_EQUAL_UINT16(0, w[eMorphSlotD]);

  morph2d_weights(MORPH2D_ONE, MORPH2D_ONE, occ, w);
  TEST_ASSERT_EQUAL_UINT16(0, w[eMorphSlotA]);
  TEST_ASSERT_EQUAL_UINT16(0, w[eMorphSlotB]);
  TEST_ASSERT_EQUAL_UINT16(0, w[eMorphSlotC]);
  TEST_ASSERT_EQUAL_UINT16(MORPH2D_ONE, w[eMorphSlotD]);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_corners);
  RUN_TEST(test_origin_and_far_corner);
  RUN_TEST(test_center);
  RUN_TEST(test_empty_renormalize);
  RUN_TEST(test_no_occupied);
  RUN_TEST(test_blend_and_discrete);
  return UNITY_END();
}
