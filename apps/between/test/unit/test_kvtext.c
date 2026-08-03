#include "unity.h"

#include "kvtext.h"

void setUp(void) {}
void tearDown(void) {}

void test_blank_and_comment(void) {
  KvPair p;
  TEST_ASSERT_EQUAL_INT(eKvBlank, kvtext_parse_line("", &p));
  TEST_ASSERT_EQUAL_INT(eKvBlank, kvtext_parse_line("   \n", &p));
  TEST_ASSERT_EQUAL_INT(eKvComment, kvtext_parse_line("# hello", &p));
  TEST_ASSERT_EQUAL_INT(eKvComment, kvtext_parse_line("  # x", &p));
}

void test_parse_pair(void) {
  KvPair p;
  TEST_ASSERT_EQUAL_INT(eKvPair, kvtext_parse_line("hz0: 12000\n", &p));
  TEST_ASSERT_EQUAL_STRING("hz0", p.key);
  TEST_ASSERT_EQUAL_STRING("12000", p.val);

  TEST_ASSERT_EQUAL_INT(eKvPair, kvtext_parse_line("module:waves", &p));
  TEST_ASSERT_EQUAL_STRING("module", p.key);
  TEST_ASSERT_EQUAL_STRING("waves", p.val);
}

void test_malformed(void) {
  KvPair p;
  TEST_ASSERT_EQUAL_INT(eKvMalformed, kvtext_parse_line("nocolon", &p));
  TEST_ASSERT_EQUAL_INT(eKvMalformed, kvtext_parse_line(":novaluekey", &p));
}

void test_format_line(void) {
  char buf[64];
  s32 n = kvtext_format_line(buf, sizeof(buf), "amp0", "-12");
  TEST_ASSERT_TRUE(n > 0);
  TEST_ASSERT_EQUAL_STRING("amp0:-12\n", buf);
}

void test_key_eq(void) {
  TEST_ASSERT_TRUE(kvtext_key_eq("format", "format"));
  TEST_ASSERT_FALSE(kvtext_key_eq("format", "Format"));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_blank_and_comment);
  RUN_TEST(test_parse_pair);
  RUN_TEST(test_malformed);
  RUN_TEST(test_format_line);
  RUN_TEST(test_key_eq);
  return UNITY_END();
}
