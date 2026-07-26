#include "unity.h"

#include "play_maps.h"

#include <string.h>

void setUp(void) {}
void tearDown(void) {}

void test_defaults(void) {
  PlayMaps m;
  play_maps_set_defaults(&m);
  TEST_ASSERT_EQUAL_INT(ePlayEncNone, m.enc[0].kind);
  TEST_ASSERT_EQUAL_INT(ePlayEncNone, m.enc[1].kind);
  TEST_ASSERT_EQUAL_INT(ePlayEncMorphX, m.enc[2].kind);
  TEST_ASSERT_EQUAL_INT(ePlayEncMorphY, m.enc[3].kind);
  TEST_ASSERT_EQUAL_INT(ePlaySwSnapA, m.sw[0].kind);
  TEST_ASSERT_EQUAL_INT(ePlaySwSnapB, m.sw[1].kind);
  TEST_ASSERT_EQUAL_INT(ePlaySwSnapC, m.sw[2].kind);
  TEST_ASSERT_EQUAL_INT(ePlaySwSnapD, m.sw[3].kind);
  TEST_ASSERT_EQUAL_INT(ePlaySwNone, m.fs[0].kind);
  TEST_ASSERT_EQUAL_INT(ePlaySwNone, m.fs[1].kind);
  TEST_ASSERT_EQUAL_INT(ePlayEncNone, m.cv[0].kind);
  TEST_ASSERT_EQUAL_INT(ePlayEncNone, m.cv[3].kind);
  TEST_ASSERT_EQUAL_INT(ePlayCcNone, m.cc[0].kind);
  TEST_ASSERT_EQUAL_INT(ePlayCcNone, m.cc[11].kind);
  TEST_ASSERT_FALSE(play_maps_cv_any_bound(&m));
}

void test_enc_roundtrip(void) {
  PlayEncMap m;
  char buf[64];

  TEST_ASSERT_TRUE(play_maps_parse_enc("-", &m));
  TEST_ASSERT_EQUAL_INT(ePlayEncNone, m.kind);
  TEST_ASSERT_TRUE(play_maps_format_enc(buf, sizeof(buf), &m));
  TEST_ASSERT_EQUAL_STRING("-", buf);

  TEST_ASSERT_TRUE(play_maps_parse_enc("morph.x", &m));
  TEST_ASSERT_EQUAL_INT(ePlayEncMorphX, m.kind);
  TEST_ASSERT_TRUE(play_maps_format_enc(buf, sizeof(buf), &m));
  TEST_ASSERT_EQUAL_STRING("morph.x", buf);

  TEST_ASSERT_TRUE(play_maps_parse_enc("param.b.amp", &m));
  TEST_ASSERT_EQUAL_INT(ePlayEncParamSlot, m.kind);
  TEST_ASSERT_EQUAL_INT(eMorphSlotB, m.slot);
  TEST_ASSERT_EQUAL_STRING("amp", m.label);
  TEST_ASSERT_TRUE(play_maps_format_enc(buf, sizeof(buf), &m));
  TEST_ASSERT_EQUAL_STRING("param.b.amp", buf);

  TEST_ASSERT_TRUE(play_maps_parse_enc("param.all.gate", &m));
  TEST_ASSERT_EQUAL_INT(ePlayEncParamAll, m.kind);
  TEST_ASSERT_EQUAL_STRING("gate", m.label);
  TEST_ASSERT_TRUE(play_maps_format_enc(buf, sizeof(buf), &m));
  TEST_ASSERT_EQUAL_STRING("param.all.gate", buf);
}

void test_sw_roundtrip(void) {
  PlaySwMap m;
  char buf[96];

  TEST_ASSERT_TRUE(play_maps_parse_sw("snap.c", &m));
  TEST_ASSERT_EQUAL_INT(ePlaySwSnapC, m.kind);
  TEST_ASSERT_TRUE(play_maps_format_sw(buf, sizeof(buf), &m));
  TEST_ASSERT_EQUAL_STRING("snap.c", buf);

  TEST_ASSERT_TRUE(play_maps_parse_sw("set.a.amp:123456", &m));
  TEST_ASSERT_EQUAL_INT(ePlaySwSetSlot, m.kind);
  TEST_ASSERT_EQUAL_INT(eMorphSlotA, m.slot);
  TEST_ASSERT_EQUAL_STRING("amp", m.label);
  TEST_ASSERT_EQUAL_INT(123456, m.value);
  TEST_ASSERT_TRUE(play_maps_format_sw(buf, sizeof(buf), &m));
  TEST_ASSERT_EQUAL_STRING("set.a.amp:123456", buf);

  TEST_ASSERT_TRUE(play_maps_parse_sw("mom.all.gate:1", &m));
  TEST_ASSERT_EQUAL_INT(ePlaySwMomAll, m.kind);
  TEST_ASSERT_EQUAL_STRING("gate", m.label);
  TEST_ASSERT_EQUAL_INT(1, m.value);
  TEST_ASSERT_TRUE(play_maps_format_sw(buf, sizeof(buf), &m));
  TEST_ASSERT_EQUAL_STRING("mom.all.gate:1", buf);

  TEST_ASSERT_TRUE(play_maps_parse_sw("set.d.hz:-42", &m));
  TEST_ASSERT_EQUAL_INT(-42, m.value);
  TEST_ASSERT_TRUE(play_maps_format_sw(buf, sizeof(buf), &m));
  TEST_ASSERT_EQUAL_STRING("set.d.hz:-42", buf);
}

void test_cc_roundtrip(void) {
  PlayCcMap m;
  char buf[64];

  TEST_ASSERT_TRUE(play_maps_parse_cc("-", &m));
  TEST_ASSERT_EQUAL_INT(ePlayCcNone, m.kind);
  TEST_ASSERT_TRUE(play_maps_format_cc(buf, sizeof(buf), &m));
  TEST_ASSERT_EQUAL_STRING("-", buf);

  TEST_ASSERT_TRUE(play_maps_parse_cc("param.amp", &m));
  TEST_ASSERT_EQUAL_INT(ePlayCcParam, m.kind);
  TEST_ASSERT_EQUAL_STRING("amp", m.label);
  TEST_ASSERT_TRUE(play_maps_format_cc(buf, sizeof(buf), &m));
  TEST_ASSERT_EQUAL_STRING("param.amp", buf);

  play_maps_summary_cc(buf, sizeof(buf), &m);
  TEST_ASSERT_EQUAL_STRING("amp", buf);
}

void test_clear_invalid(void) {
  PlayMaps m;
  ParamDesc desc[2];

  memset(&desc, 0, sizeof(desc));
  strcpy(desc[0].label, "amp");
  strcpy(desc[1].label, "gate");

  play_maps_set_defaults(&m);
  m.enc[0].kind = ePlayEncParamSlot;
  m.enc[0].slot = eMorphSlotA;
  strcpy(m.enc[0].label, "amp");
  m.enc[1].kind = ePlayEncParamAll;
  strcpy(m.enc[1].label, "missing");
  m.sw[0].kind = ePlaySwSetSlot;
  m.sw[0].slot = eMorphSlotB;
  strcpy(m.sw[0].label, "gate");
  m.sw[1].kind = ePlaySwMomAll;
  strcpy(m.sw[1].label, "gone");
  m.fs[0].kind = ePlaySwSetAll;
  strcpy(m.fs[0].label, "amp");
  m.fs[1].kind = ePlaySwMomSlot;
  m.fs[1].slot = eMorphSlotA;
  strcpy(m.fs[1].label, "missing");
  m.cc[0].kind = ePlayCcParam;
  strcpy(m.cc[0].label, "amp");
  m.cc[1].kind = ePlayCcParam;
  strcpy(m.cc[1].label, "nope");
  m.cv[0].kind = ePlayEncParamAll;
  strcpy(m.cv[0].label, "amp");
  m.cv[1].kind = ePlayEncParamSlot;
  m.cv[1].slot = eMorphSlotA;
  strcpy(m.cv[1].label, "missing");

  play_maps_clear_invalid(&m, desc, 2);
  TEST_ASSERT_EQUAL_INT(ePlayEncParamSlot, m.enc[0].kind);
  TEST_ASSERT_EQUAL_INT(ePlayEncNone, m.enc[1].kind);
  TEST_ASSERT_EQUAL_INT(ePlaySwSetSlot, m.sw[0].kind);
  TEST_ASSERT_EQUAL_INT(ePlaySwNone, m.sw[1].kind);
  TEST_ASSERT_EQUAL_INT(ePlaySwSetAll, m.fs[0].kind);
  TEST_ASSERT_EQUAL_INT(ePlaySwNone, m.fs[1].kind);
  TEST_ASSERT_EQUAL_INT(ePlayCcParam, m.cc[0].kind);
  TEST_ASSERT_EQUAL_INT(ePlayCcNone, m.cc[1].kind);
  TEST_ASSERT_EQUAL_INT(ePlayEncParamAll, m.cv[0].kind);
  TEST_ASSERT_EQUAL_INT(ePlayEncNone, m.cv[1].kind);
  /* snaps and morph defaults untouched */
  TEST_ASSERT_EQUAL_INT(ePlayEncMorphX, m.enc[2].kind);
  TEST_ASSERT_EQUAL_INT(ePlaySwSnapC, m.sw[2].kind);
  TEST_ASSERT_EQUAL_PTR(&m.sw[0], play_maps_sw_total_at(&m, 0));
  TEST_ASSERT_EQUAL_PTR(&m.fs[0], play_maps_sw_total_at(&m, 4));
  TEST_ASSERT_EQUAL_PTR(&m.fs[1], play_maps_sw_total_at(&m, 5));
  TEST_ASSERT_TRUE(play_maps_cv_any_bound(&m));
}

void test_footer_and_snap(void) {
  PlaySwMap m;
  char buf[16];
  MorphSlot slot;

  memset(&m, 0, sizeof(m));
  m.kind = ePlaySwSnapB;
  play_maps_footer_sw(buf, sizeof(buf), &m);
  TEST_ASSERT_EQUAL_STRING("B", buf);
  TEST_ASSERT_TRUE(play_maps_sw_snap_slot(m.kind, &slot));
  TEST_ASSERT_EQUAL_INT(eMorphSlotB, slot);

  m.kind = ePlaySwSetSlot;
  m.slot = eMorphSlotC;
  strcpy(m.label, "amp");
  play_maps_footer_sw(buf, sizeof(buf), &m);
  TEST_ASSERT_EQUAL_STRING("amp", buf);
  TEST_ASSERT_TRUE(play_maps_sw_single_slot(&m, &slot));
  TEST_ASSERT_EQUAL_INT(eMorphSlotC, slot);
}

void test_fill_bound(void) {
  PlayMaps m;
  ParamDesc desc[3];
  u8 bound[3];

  memset(&m, 0, sizeof(m));
  memset(desc, 0, sizeof(desc));
  strcpy(desc[0].label, "amp");
  strcpy(desc[1].label, "cut");
  strcpy(desc[2].label, "res");
  play_maps_set_defaults(&m);
  m.enc[0].kind = ePlayEncParamSlot;
  strcpy(m.enc[0].label, "amp");
  m.sw[0].kind = ePlaySwSetAll;
  strcpy(m.sw[0].label, "cut");
  m.cc[0].kind = ePlayCcParam;
  strcpy(m.cc[0].label, "res");
  m.cv[2].kind = ePlayEncParamAll;
  strcpy(m.cv[2].label, "amp");
  /* morph / snap must not mark */
  m.enc[2].kind = ePlayEncMorphX;
  m.sw[1].kind = ePlaySwSnapA;

  play_maps_fill_bound(&m, desc, 3, bound);
  TEST_ASSERT_EQUAL_UINT8(1, bound[0]);
  TEST_ASSERT_EQUAL_UINT8(1, bound[1]);
  TEST_ASSERT_EQUAL_UINT8(1, bound[2]);
}

void test_cv_reset(void) {
  PlayMaps m;
  play_maps_set_defaults(&m);
  m.cv[1].kind = ePlayEncMorphY;
  play_maps_reset_cv(&m, 1);
  TEST_ASSERT_EQUAL_INT(ePlayEncNone, m.cv[1].kind);
  TEST_ASSERT_FALSE(play_maps_cv_any_bound(&m));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_defaults);
  RUN_TEST(test_enc_roundtrip);
  RUN_TEST(test_sw_roundtrip);
  RUN_TEST(test_cc_roundtrip);
  RUN_TEST(test_clear_invalid);
  RUN_TEST(test_footer_and_snap);
  RUN_TEST(test_fill_bound);
  RUN_TEST(test_cv_reset);
  return UNITY_END();
}
