# ------------------------
# -- aleph_avr32_post.mk
#
# include after ASF Makefile.avr32.in. ASF sets VPATH := $(PRJ_PATH), which
# finds avr32/ and common/ sources; add the repo root so rewritten
# vendor/libavr32/... and vendor/aleph/avr32/... CSRCS resolve.

VPATH := $(PRJ_PATH):$(ALEPH_PROJECTS_ROOT)
