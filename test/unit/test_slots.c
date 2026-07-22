#include "unity.h"

#include "slots.h"

#include <string.h>

#define NPARAMS 4

static ParamValue applied[NPARAMS];
static u16 apply_count;
static u16 apply_order_log[NPARAMS];

static void mock_set(u16 index, ParamValue value, void *ctx) {
  (void)ctx;
  if (index < NPARAMS) {
    applied[index] = value;
    if (apply_count < NPARAMS) {
      apply_order_log[apply_count] = index;
    }
    apply_count++;
  }
}

void setUp(void) {
  memset(applied, 0, sizeof(applied));
  memset(apply_order_log, 0, sizeof(apply_order_log));
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

/* Descriptor order: amp, integrator, amp, integrator-short.
 * Apply send order must put integrators first (stable within groups). */
void test_apply_order_integrators_first(void) {
  ParamDesc desc[NPARAMS];
  ParamValue bank_a[NPARAMS];
  ParamValue *banks[MORPH2D_SLOTS] = {bank_a, NULL, NULL, NULL};
  Slots s;
  u16 i;

  memset(desc, 0, sizeof(desc));
  desc[0].type = eParamTypeAmp;
  desc[1].type = eParamTypeIntegrator;
  desc[2].type = eParamTypeAmp;
  desc[3].type = eParamTypeIntegratorShort;
  for (i = 0; i < NPARAMS; ++i) {
    bank_a[i] = (ParamValue)(i + 1);
  }

  slots_init(&s, NPARAMS, desc, banks, mock_set, NULL);
  slots_set_num_params(&s, NPARAMS);
  TEST_ASSERT_EQUAL_UINT16(NPARAMS, s.apply_order_len);
  TEST_ASSERT_EQUAL_UINT16(1, s.apply_order[0]); /* integrator */
  TEST_ASSERT_EQUAL_UINT16(3, s.apply_order[1]); /* integrator short */
  TEST_ASSERT_EQUAL_UINT16(0, s.apply_order[2]); /* amp */
  TEST_ASSERT_EQUAL_UINT16(2, s.apply_order[3]); /* amp */

  slots_assign_stem(&s, eMorphSlotA, "a");
  slots_snap_to(&s, eMorphSlotA);
  slots_apply(&s);

  TEST_ASSERT_EQUAL_UINT16(NPARAMS, apply_count);
  TEST_ASSERT_EQUAL_UINT16(1, apply_order_log[0]);
  TEST_ASSERT_EQUAL_UINT16(3, apply_order_log[1]);
  TEST_ASSERT_EQUAL_UINT16(0, apply_order_log[2]);
  TEST_ASSERT_EQUAL_UINT16(2, apply_order_log[3]);
  TEST_ASSERT_EQUAL_INT32(1, applied[0]);
  TEST_ASSERT_EQUAL_INT32(2, applied[1]);
  TEST_ASSERT_EQUAL_INT32(3, applied[2]);
  TEST_ASSERT_EQUAL_INT32(4, applied[3]);
}

void test_exclude_skips_apply(void) {
  ParamDesc desc[NPARAMS];
  ParamValue bank_a[NPARAMS];
  ParamValue bank_b[NPARAMS];
  ParamValue bank_c[NPARAMS];
  ParamValue bank_d[NPARAMS];
  ParamValue *banks[MORPH2D_SLOTS] = {bank_a, bank_b, bank_c, bank_d};
  Slots s;
  u16 i;

  memset(desc, 0, sizeof(desc));
  for(i = 0; i < NPARAMS; ++i) {
    desc[i].type = eParamTypeFix;
    bank_a[i] = 10;
  }
  slots_init(&s, NPARAMS, desc, banks, mock_set, NULL);
  slots_set_num_params(&s, NPARAMS);
  slots_assign_stem(&s, eMorphSlotA, "a");
  s.exclude[1] = 1;
  s.exclude[2] = 1;
  slots_snap_to(&s, eMorphSlotA);
  slots_apply(&s);
  TEST_ASSERT_EQUAL_UINT16(2, apply_count);
  TEST_ASSERT_EQUAL_INT32(10, applied[0]);
  TEST_ASSERT_EQUAL_INT32(0, applied[1]);
  TEST_ASSERT_EQUAL_INT32(0, applied[2]);
  TEST_ASSERT_EQUAL_INT32(10, applied[3]);

  slots_send_param(&s, 1, 99);
  TEST_ASSERT_EQUAL_INT32(99, applied[1]);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_morph_apply_corners);
  RUN_TEST(test_live_edit_and_discrete);
  RUN_TEST(test_empty_slots_no_apply);
  RUN_TEST(test_apply_order_integrators_first);
  RUN_TEST(test_exclude_skips_apply);
  return UNITY_END();
}
