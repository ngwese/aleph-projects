#include "unity.h"

#include "cv_scale.h"

void setUp(void) {}
void tearDown(void) {}

void test_adc_to_fr32_endpoints(void) {
  TEST_ASSERT_EQUAL_INT32(0, cv_in_adc_to_fr32(0));
  TEST_ASSERT_EQUAL_INT32(0x7fffffff, cv_in_adc_to_fr32(4095));
  TEST_ASSERT_EQUAL_INT32(0x7fffffff, cv_in_adc_to_fr32(0xffff));
}

void test_adc_to_fr32_mid(void) {
  fract32 mid = cv_in_adc_to_fr32(2048);
  fract32 expect = (fract32)(((long long)2048 * 0x7fffffffLL) / 4095LL);
  TEST_ASSERT_EQUAL_INT32(expect, mid);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_adc_to_fr32_endpoints);
  RUN_TEST(test_adc_to_fr32_mid);
  return UNITY_END();
}
