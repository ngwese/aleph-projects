#include "unity.h"

#include "midi_nrpn.h"
#include "param_common.h"

void setUp(void) {}
void tearDown(void) {}

void test_param_index(void) {
  TEST_ASSERT_EQUAL_UINT16(0, midi_nrpn_param_index(0, 0));
  TEST_ASSERT_EQUAL_UINT16(1, midi_nrpn_param_index(0, 1));
  TEST_ASSERT_EQUAL_UINT16(127, midi_nrpn_param_index(0, 127));
  TEST_ASSERT_EQUAL_UINT16(128, midi_nrpn_param_index(1, 0));
  TEST_ASSERT_EQUAL_UINT16(1023, midi_nrpn_param_index(7, 127));
}

void test_v14(void) {
  TEST_ASSERT_EQUAL_UINT16(0, midi_nrpn_v14(0, 0));
  TEST_ASSERT_EQUAL_UINT16(8192, midi_nrpn_v14(64, 0));
  TEST_ASSERT_EQUAL_UINT16(16383, midi_nrpn_v14(127, 127));
}

void test_map_fix_ends(void) {
  ParamDesc d;
  d.type = eParamTypeFix;
  d.min = -1000;
  d.max = 1000;
  d.radix = 16;

  TEST_ASSERT_EQUAL_INT32(-1000, midi_nrpn_map_v14(&d, 0));
  TEST_ASSERT_EQUAL_INT32(1000, midi_nrpn_map_v14(&d, 16383));
  TEST_ASSERT_EQUAL_INT32(0, midi_nrpn_map_v14(&d, 8192) / 1);
  /* mid-ish: 8191.5 would be ideal; integer map of 8192 * 2000 / 16383 */
  TEST_ASSERT_INT32_WITHIN(2, 0, midi_nrpn_map_v14(&d, 8192));
}

void test_map_unsigned_range(void) {
  ParamDesc d;
  d.type = eParamTypeAmp;
  d.min = 0;
  d.max = 0x7fffffff;
  d.radix = 0;

  TEST_ASSERT_EQUAL_INT32(0, midi_nrpn_map_v14(&d, 0));
  TEST_ASSERT_EQUAL_INT32(0x7fffffff, midi_nrpn_map_v14(&d, 16383));
}

void test_map_bool(void) {
  ParamDesc d;
  d.type = eParamTypeBool;
  d.min = 0;
  d.max = 1;
  d.radix = 0;

  TEST_ASSERT_EQUAL_INT32(0, midi_nrpn_map_v14(&d, 0));
  TEST_ASSERT_EQUAL_INT32(0, midi_nrpn_map_v14(&d, 8191));
  TEST_ASSERT_EQUAL_INT32(1, midi_nrpn_map_v14(&d, 8192));
  TEST_ASSERT_EQUAL_INT32(1, midi_nrpn_map_v14(&d, 16383));
}

void test_map_label_round(void) {
  ParamDesc d;
  d.type = eParamTypeLabel;
  d.min = 0;
  d.max = 4;
  d.radix = 0;

  TEST_ASSERT_EQUAL_INT32(0, midi_nrpn_map_v14(&d, 0));
  TEST_ASSERT_EQUAL_INT32(4, midi_nrpn_map_v14(&d, 16383));
  TEST_ASSERT_EQUAL_INT32(2, midi_nrpn_map_v14(&d, 8192));
}

void test_fmt_msb_lsb(void) {
  char buf[8];

  TEST_ASSERT_EQUAL_UINT8(3, midi_nrpn_fmt_msb_lsb(buf, sizeof(buf), 0));
  TEST_ASSERT_EQUAL_STRING("0:0", buf);

  TEST_ASSERT_EQUAL_UINT8(3, midi_nrpn_fmt_msb_lsb(buf, sizeof(buf), 3));
  TEST_ASSERT_EQUAL_STRING("0:3", buf);

  TEST_ASSERT_EQUAL_UINT8(3, midi_nrpn_fmt_msb_lsb(buf, sizeof(buf), 128));
  TEST_ASSERT_EQUAL_STRING("1:0", buf);

  TEST_ASSERT_EQUAL_UINT8(5, midi_nrpn_fmt_msb_lsb(buf, sizeof(buf), 1023));
  TEST_ASSERT_EQUAL_STRING("7:127", buf);

  TEST_ASSERT_EQUAL_UINT8(7, midi_nrpn_fmt_msb_lsb(buf, sizeof(buf), 16383));
  TEST_ASSERT_EQUAL_STRING("127:127", buf);
}

void test_raw_to_v14_roundtrip_ends(void) {
  ParamDesc d;
  d.type = eParamTypeFix;
  d.min = -1000;
  d.max = 1000;
  d.radix = 16;

  TEST_ASSERT_EQUAL_UINT16(0, midi_nrpn_raw_to_v14(&d, -1000));
  TEST_ASSERT_EQUAL_UINT16(16383, midi_nrpn_raw_to_v14(&d, 1000));
  TEST_ASSERT_UINT16_WITHIN(2, 8192, midi_nrpn_raw_to_v14(&d, 0));
}

void test_raw_to_v14_bool(void) {
  ParamDesc d;
  d.type = eParamTypeBool;
  d.min = 0;
  d.max = 1;
  d.radix = 0;

  TEST_ASSERT_EQUAL_UINT16(0, midi_nrpn_raw_to_v14(&d, 0));
  TEST_ASSERT_EQUAL_UINT16(16383, midi_nrpn_raw_to_v14(&d, 1));
}

void test_io_range_like_amp(void) {
  /* amp-style scaler io: 0 .. (1023 << 5) */
  const s32 in_min = 0;
  const s32 in_max = 1023 * 32;

  TEST_ASSERT_EQUAL_INT32(0, midi_nrpn_v14_to_range(in_min, in_max, 0));
  TEST_ASSERT_EQUAL_INT32(in_max,
                          midi_nrpn_v14_to_range(in_min, in_max, 16383));
  TEST_ASSERT_EQUAL_UINT16(0, midi_nrpn_range_to_v14(in_min, in_max, 0));
  TEST_ASSERT_EQUAL_UINT16(16383,
                           midi_nrpn_range_to_v14(in_min, in_max, in_max));
  /* mid io should land near mid v14 — not stuck at 0/1 */
  {
    s32 mid_io = midi_nrpn_v14_to_range(in_min, in_max, 8192);
    u16 back = midi_nrpn_range_to_v14(in_min, in_max, mid_io);
    TEST_ASSERT_UINT16_WITHIN(2, 8192, back);
    TEST_ASSERT_TRUE(mid_io > 1000);
  }
  /* small-but-nonzero io must not collapse to 0 like raw-vs-0x7fffffff did */
  TEST_ASSERT_TRUE(midi_nrpn_range_to_v14(in_min, in_max, 32) > 0);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_param_index);
  RUN_TEST(test_v14);
  RUN_TEST(test_map_fix_ends);
  RUN_TEST(test_map_unsigned_range);
  RUN_TEST(test_map_bool);
  RUN_TEST(test_map_label_round);
  RUN_TEST(test_fmt_msb_lsb);
  RUN_TEST(test_raw_to_v14_roundtrip_ends);
  RUN_TEST(test_raw_to_v14_bool);
  RUN_TEST(test_io_range_like_amp);
  return UNITY_END();
}
