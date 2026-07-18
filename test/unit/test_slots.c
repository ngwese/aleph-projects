#include "unity.h"

#include "slots.h"

#include <string.h>

#define NPARAMS 4

static ParamValue applied[NPARAMS];
static u16 apply_count;

static void mock_set(u16 index, ParamValue value, void *ctx) {
  (void)ctx;
  if (index < NPARAMS) {
    applied[index] = value;
    apply_count++;
  }
}

void setUp(void) {
  memset(applied, 0, sizeof(applied));
  apply_count = 0;
}

void tearDown(void) {}

void test_morph_apply_corners(void) {
  ParamDesc desc[NPARAMS];
  ParamValue bank_a[NPARAMS];
  ParamValue bank_b[NPARAMS];
  ParamValue bank_c[NPARAMS];
  ParamValue bank_d[NPARAMS];
  ParamValue *banks[MORPH2D_SLOTS] = {bank_a, bank_b, bank_c, bank_d};
  Slots s;
  ModuleVersion ver = {0, 1, 0};
  u16 i;

  memset(desc, 0, sizeof(desc));
  for (i = 0; i < NPARAMS; ++i) {
    desc[i].type = eParamTypeFix;
    bank_a[i] = 100;
    bank_b[i] = 200;
    bank_c[i] = 300;
    bank_d[i] = 400;
  }

  slots_init(&s, NPARAMS, desc, banks, mock_set, NULL);
  slots_set_module(&s, "waves", &ver);
  slots_set_num_params(&s, NPARAMS);
  slots_assign_stem(&s, eMorphSlotA, "a");
  slots_assign_stem(&s, eMorphSlotB, "b");
  slots_assign_stem(&s, eMorphSlotC, "c");
  slots_assign_stem(&s, eMorphSlotD, "d");

  slots_snap_to(&s, eMorphSlotA);
  slots_apply(&s);
  TEST_ASSERT_EQUAL_INT32(100, applied[0]);

  slots_snap_to(&s, eMorphSlotC);
  slots_apply(&s);
  TEST_ASSERT_EQUAL_INT32(300, applied[0]);
}

void test_live_edit_and_discrete(void) {
  ParamDesc desc[NPARAMS];
  ParamValue bank_a[NPARAMS];
  ParamValue bank_b[NPARAMS];
  ParamValue bank_c[NPARAMS];
  ParamValue bank_d[NPARAMS];
  ParamValue *banks[MORPH2D_SLOTS] = {bank_a, bank_b, bank_c, bank_d};
  Slots s;
  u16 i;

  memset(desc, 0, sizeof(desc));
  desc[0].type = eParamTypeFix;
  desc[1].type = eParamTypeBool;
  for (i = 0; i < NPARAMS; ++i) {
    bank_a[i] = 0;
    bank_b[i] = 10;
    bank_c[i] = 0;
    bank_d[i] = 0;
  }
  bank_a[1] = 0;
  bank_b[1] = 1;

  slots_init(&s, NPARAMS, desc, banks, mock_set, NULL);
  slots_set_num_params(&s, NPARAMS);
  slots_assign_stem(&s, eMorphSlotA, "a");
  slots_assign_stem(&s, eMorphSlotB, "b");

  slots_set_morph(&s, MORPH2D_ONE, 0); /* corner B */
  slots_set_value(&s, eMorphSlotB, 0, 42);
  slots_apply(&s);
  TEST_ASSERT_EQUAL_INT32(42, applied[0]);
  TEST_ASSERT_EQUAL_INT32(1, applied[1]); /* discrete from B */
  TEST_ASSERT_TRUE(s.dirty[eMorphSlotB]);
}

void test_empty_slots_no_apply(void) {
  ParamDesc desc[NPARAMS];
  ParamValue bank_a[NPARAMS];
  ParamValue *banks[MORPH2D_SLOTS] = {bank_a, NULL, NULL, NULL};
  Slots s;
  slots_init(&s, NPARAMS, desc, banks, mock_set, NULL);
  slots_set_num_params(&s, NPARAMS);
  slots_apply(&s);
  TEST_ASSERT_EQUAL_UINT16(0, apply_count);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_morph_apply_corners);
  RUN_TEST(test_live_edit_and_discrete);
  RUN_TEST(test_empty_slots_no_apply);
  return UNITY_END();
}
