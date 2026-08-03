# ------------------------
# -- aleph_avr32_src.mk (out-of-tree)
#
# reuse the vendored source list; LIB_AVR32 / ALEPH_* are set in
# aleph_avr32_config.mk relative to PRJ_PATH under vendor/aleph.

ifndef ALEPH_ROOT
  include ../../aleph_root.mk
endif

include $(ALEPH_ROOT)/apps/aleph_avr32_src.mk
