#include "unity.h"

#include "setup_io.h"
#include "test_lineio_host.h"

#include <stdio.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

void test_setup_roundtrip(void) {
  const char *path = "build/tmp_setup.txt";
  SetupData in;
  SetupData out;
  LineIO io;
  FILE *fp;

  memset(&in, 0, sizeof(in));
  in.format = SETUP_IO_FORMAT;
  strcpy(in.module, "waves");
  in.version.maj = 0;
  in.version.min = 4;
  in.version.rev = 5;
  strcpy(in.slot_stem[0], "soft-pad");
  in.slot_occupied[0] = 1;
  strcpy(in.slot_stem[1], "bright-pad");
  in.slot_occupied[1] = 1;
  in.slot_occupied[2] = 0;
  strcpy(in.slot_stem[3], "soft-pad");
  in.slot_occupied[3] = 1;
  in.x = (u16)((0.35 * MORPH2D_ONE) + 0.5);
  in.y = (u16)((0.60 * MORPH2D_ONE) + 0.5);

  fp = fopen(path, "w");
  TEST_ASSERT_NOT_NULL(fp);
  host_lineio_init(&io, fp);
  TEST_ASSERT_EQUAL_INT(eSetupIoOk, setup_io_write(&io, &in));
  fclose(fp);

  fp = fopen(path, "r");
  host_lineio_init(&io, fp);
  TEST_ASSERT_EQUAL_INT(eSetupIoOk, setup_io_read(&io, &out));
  fclose(fp);

  TEST_ASSERT_EQUAL_STRING("waves", out.module);
  TEST_ASSERT_EQUAL_STRING("soft-pad", out.slot_stem[0]);
  TEST_ASSERT_EQUAL_STRING("bright-pad", out.slot_stem[1]);
  TEST_ASSERT_FALSE(out.slot_occupied[2]);
  TEST_ASSERT_TRUE(out.slot_occupied[3]);
  TEST_ASSERT_EQUAL_UINT16(in.x, out.x);
  TEST_ASSERT_EQUAL_UINT16(in.y, out.y);
}

void test_unknown_keys_ignored(void) {
  const char *path = "build/tmp_setup_extra.txt";
  SetupData out;
  LineIO io;
  FILE *fp = fopen(path, "w");
  TEST_ASSERT_NOT_NULL(fp);
  fputs("format: 1\nmodule: waves\nversion: 0.1.0\n"
        "midi.x: 1\nslot.a: a\nx: 0\ny: 0\n",
        fp);
  fclose(fp);

  fp = fopen(path, "r");
  host_lineio_init(&io, fp);
  TEST_ASSERT_EQUAL_INT(eSetupIoOk, setup_io_read(&io, &out));
  fclose(fp);
  TEST_ASSERT_EQUAL_STRING("a", out.slot_stem[0]);
  /* missing play.* → defaults */
  TEST_ASSERT_EQUAL_INT(ePlayEncMorphX, out.maps.enc[2].kind);
  TEST_ASSERT_EQUAL_INT(ePlaySwSnapA, out.maps.sw[0].kind);
  TEST_ASSERT_EQUAL_INT(ePlaySwNone, out.maps.fs[0].kind);
  TEST_ASSERT_EQUAL_INT(ePlaySwNone, out.maps.fs[1].kind);
}

void test_play_maps_roundtrip(void) {
  const char *path = "build/tmp_setup_play.txt";
  SetupData in;
  SetupData out;
  LineIO io;
  FILE *fp;

  memset(&in, 0, sizeof(in));
  in.format = SETUP_IO_FORMAT;
  strcpy(in.module, "waves");
  in.version.maj = 0;
  in.version.min = 1;
  in.version.rev = 0;
  in.slot_occupied[0] = 1;
  strcpy(in.slot_stem[0], "a");
  play_maps_set_defaults(&in.maps);
  in.maps.enc[0].kind = ePlayEncParamSlot;
  in.maps.enc[0].slot = eMorphSlotB;
  strcpy(in.maps.enc[0].label, "amp");
  in.maps.sw[1].kind = ePlaySwSetAll;
  strcpy(in.maps.sw[1].label, "gate");
  in.maps.sw[1].value = 1;
  in.maps.fs[0].kind = ePlaySwSnapB;
  in.maps.fs[1].kind = ePlaySwMomAll;
  strcpy(in.maps.fs[1].label, "amp");
  in.maps.fs[1].value = 42;

  fp = fopen(path, "w");
  TEST_ASSERT_NOT_NULL(fp);
  host_lineio_init(&io, fp);
  TEST_ASSERT_EQUAL_INT(eSetupIoOk, setup_io_write(&io, &in));
  fclose(fp);

  fp = fopen(path, "r");
  host_lineio_init(&io, fp);
  TEST_ASSERT_EQUAL_INT(eSetupIoOk, setup_io_read(&io, &out));
  fclose(fp);

  TEST_ASSERT_EQUAL_INT(ePlayEncParamSlot, out.maps.enc[0].kind);
  TEST_ASSERT_EQUAL_INT(eMorphSlotB, out.maps.enc[0].slot);
  TEST_ASSERT_EQUAL_STRING("amp", out.maps.enc[0].label);
  TEST_ASSERT_EQUAL_INT(ePlayEncMorphX, out.maps.enc[2].kind);
  TEST_ASSERT_EQUAL_INT(ePlaySwSetAll, out.maps.sw[1].kind);
  TEST_ASSERT_EQUAL_STRING("gate", out.maps.sw[1].label);
  TEST_ASSERT_EQUAL_INT(1, out.maps.sw[1].value);
  TEST_ASSERT_EQUAL_INT(ePlaySwSnapA, out.maps.sw[0].kind);
  TEST_ASSERT_EQUAL_INT(ePlaySwSnapB, out.maps.fs[0].kind);
  TEST_ASSERT_EQUAL_INT(ePlaySwMomAll, out.maps.fs[1].kind);
  TEST_ASSERT_EQUAL_STRING("amp", out.maps.fs[1].label);
  TEST_ASSERT_EQUAL_INT(42, out.maps.fs[1].value);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_setup_roundtrip);
  RUN_TEST(test_unknown_keys_ignored);
  RUN_TEST(test_play_maps_roundtrip);
  return UNITY_END();
}
