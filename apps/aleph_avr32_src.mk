# ------------------------
# -- aleph_avr32_src.mk (out-of-tree)
#
# reuse the vendored source list; LIB_AVR32 / ALEPH_* are set in
# aleph_avr32_config.mk relative to PRJ_PATH under vendor/libavr32/asf.
# rewrite climbing CSRCS/ASSRCS paths so ASF BUILD_DIR does not escape
# via `..` into apps/src/ or repo-root aleph/.

ifndef ALEPH_ROOT
  include ../../aleph_root.mk
endif

include $(ALEPH_ROOT)/apps/aleph_avr32_src.mk

# map PRJ_PATH-relative climbing paths to repo-root-relative paths.
# leave ASF (avr32/, common/) and app (src/) entries unchanged.
# INC_PATH still uses LIB_AVR32 / ALEPH_* as PRJ_PATH-relative.
# rewrite ALEPH_AVR32 before LIB_AVR32 so `../%` does not swallow `../../aleph/...`.
CSRCS := $(patsubst $(ALEPH_AVR32)/%,vendor/aleph/avr32/%,$(CSRCS))
CSRCS := $(patsubst $(LIB_AVR32)%,vendor/libavr32/%,$(CSRCS))
ASSRCS := $(patsubst $(ALEPH_AVR32)/%,vendor/aleph/avr32/%,$(ASSRCS))
ASSRCS := $(patsubst $(LIB_AVR32)%,vendor/libavr32/%,$(ASSRCS))
