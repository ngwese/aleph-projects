#
# host unit tests for apps/between/src/lib
# uses vendored Unity from libavr32/test/vendor/unity
#

BETWEEN_ROOT = ..
ALEPH_ROOT = ../../..
LIB_DIR = $(BETWEEN_ROOT)/src/lib
COMMON_DIR = $(ALEPH_ROOT)/common
LIBAVR32_SRC = $(ALEPH_ROOT)/libavr32/src
UNITY_DIR = $(ALEPH_ROOT)/libavr32/test/vendor/unity/src

UNIT_DIR = unit
BUILD_DIR = build

CC = gcc
CFLAGS = -g -Wall -Wextra -Wstrict-prototypes -std=c99
CPPFLAGS = -DTEST \
	-I$(BETWEEN_ROOT)/src \
	-I$(LIB_DIR) \
	-I$(COMMON_DIR) \
	-I$(LIBAVR32_SRC) \
	-I$(UNITY_DIR) \
	-I$(UNIT_DIR)

UNIT_TESTS = $(shell find $(UNIT_DIR) -type f -name 'test_*.c')
TEST_EXECS = $(addprefix $(BUILD_DIR)/,$(addsuffix .out,$(basename $(UNIT_TESTS))))
TEST_RESULTS = $(addprefix $(BUILD_DIR)/,$(addsuffix .txt,$(basename $(UNIT_TESTS))))

LIB_SRCS = \
	$(LIB_DIR)/kvtext.c \
	$(LIB_DIR)/midi_nrpn.c \
	$(LIB_DIR)/morph2d.c \
	$(LIB_DIR)/play_maps.c \
	$(LIB_DIR)/preset_io.c \
	$(LIB_DIR)/setup_io.c \
	$(LIB_DIR)/slots.c

LIB_OBJS = $(addprefix $(BUILD_DIR)/lib/,$(notdir $(LIB_SRCS:.c=.o)))
UNITY_OBJ = $(BUILD_DIR)/unity.o

.PHONY: test clean rebuild

test: $(TEST_RESULTS)
	@echo "-----------------------\nFAILURES:\n-----------------------"
	@grep --no-filename -s FAIL $(TEST_RESULTS) || true
	@echo "-----------------------\nSUMMARY:\n-----------------------"
	@grep --no-filename -s 'Tests\|Failures\|Ignored' $(TEST_RESULTS) || true
	@echo "\nDONE"
	@! grep -q FAIL $(TEST_RESULTS)

$(BUILD_DIR)/lib/%.o: $(LIB_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

$(UNITY_OBJ): $(UNITY_DIR)/unity.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

$(BUILD_DIR)/unit/%.out: $(UNIT_DIR)/%.c $(LIB_OBJS) $(UNITY_OBJ)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(CPPFLAGS) $< $(LIB_OBJS) $(UNITY_OBJ) -o $@

$(BUILD_DIR)/unit/%.txt: $(BUILD_DIR)/unit/%.out
	@mkdir -p $(dir $@)
	./$< > $@ 2>&1

clean:
	$(RM) -rf $(BUILD_DIR)

rebuild: clean test
