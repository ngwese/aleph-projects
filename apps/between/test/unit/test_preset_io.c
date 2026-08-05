#include "unity.h"

#include "preset_io.h"
#include "test_lineio_host.h"

#include <stdio.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

typedef struct {
  char labels[8][16];
  s32 values[8];
  u32 count;
} ParamSink;

static u8 on_param(const char *label, s32 value, void *ctx) {
  ParamSink *s = (ParamSink *)ctx;
  if (s->count >= 8) {
    return 0;
  }
  strncpy(s->labels[s->count], label, 15);
  s->labels[s->count][15] = '\0';
  s->values[s->count] = value;
  s->count++;
  return 1;
}

typedef struct {
  const char *labels[8];
  s32 values[8];
  u32 count;
  u32 idx;
} ParamSrc;

static u8 next_param(const char **label, s32 *value, void *ctx) {
  ParamSrc *s = (ParamSrc *)ctx;
  if (s->idx >= s->count) {
    return 0;
  }
  *label = s->labels[s->idx];
  *value = s->values[s->idx];
  s->idx++;
  return 1;
}

void test_roundtrip(void) {
  const char *path = "build/tmp_preset.txt";
  LineIO io;
  PresetMeta meta;
  PresetMeta meta2;
  ParamSink sink;
  ParamSrc src;
  FILE *fp;

  memset(&meta, 0, sizeof(meta));
  meta.format = PRESET_IO_FORMAT;
  strcpy(meta.module, "waves");
  meta.version.maj = 0;
  meta.version.min = 4;
  meta.version.rev = 5;

  src.labels[0] = "hz0";
  src.values[0] = 12000;
  src.labels[1] = "amp0";
  src.values[1] = -12;
  src.count = 2;
  src.idx = 0;

  fp = fopen(path, "w");
  TEST_ASSERT_NOT_NULL(fp);
  host_lineio_init(&io, fp);
  TEST_ASSERT_EQUAL_INT(ePresetIoOk,
                        preset_io_write(&io, &meta, next_param, &src));
  fclose(fp);

  memset(&sink, 0, sizeof(sink));
  fp = fopen(path, "r");
  TEST_ASSERT_NOT_NULL(fp);
  host_lineio_init(&io, fp);
  TEST_ASSERT_EQUAL_INT(ePresetIoOk,
                        preset_io_read(&io, &meta2, on_param, &sink));
  fclose(fp);

  TEST_ASSERT_EQUAL_STRING("waves", meta2.module);
  TEST_ASSERT_EQUAL_UINT8(0, meta2.version.maj);
  TEST_ASSERT_EQUAL_UINT8(4, meta2.version.min);
  TEST_ASSERT_EQUAL_UINT16(5, meta2.version.rev);
  TEST_ASSERT_EQUAL_UINT32(2, sink.count);
  TEST_ASSERT_EQUAL_STRING("hz0", sink.labels[0]);
  TEST_ASSERT_EQUAL_INT32(12000, sink.values[0]);
  TEST_ASSERT_EQUAL_STRING("amp0", sink.labels[1]);
  TEST_ASSERT_EQUAL_INT32(-12, sink.values[1]);
}

void test_bad_format(void) {
  const char *path = "build/tmp_preset_bad.txt";
  LineIO io;
  PresetMeta meta;
  FILE *fp = fopen(path, "w");
  TEST_ASSERT_NOT_NULL(fp);
  fputs("format: 99\nmodule: waves\nversion: 0.1.0\n", fp);
  fclose(fp);

  fp = fopen(path, "r");
  host_lineio_init(&io, fp);
  TEST_ASSERT_EQUAL_INT(ePresetIoBadFormat,
                        preset_io_read(&io, &meta, NULL, NULL));
  fclose(fp);
}

void test_missing_meta(void) {
  const char *path = "build/tmp_preset_miss.txt";
  LineIO io;
  PresetMeta meta;
  FILE *fp = fopen(path, "w");
  TEST_ASSERT_NOT_NULL(fp);
  fputs("hz0: 1\n", fp);
  fclose(fp);

  fp = fopen(path, "r");
  host_lineio_init(&io, fp);
  TEST_ASSERT_EQUAL_INT(ePresetIoMissingMeta,
                        preset_io_read(&io, &meta, NULL, NULL));
  fclose(fp);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_roundtrip);
  RUN_TEST(test_bad_format);
  RUN_TEST(test_missing_meta);
  return UNITY_END();
}
