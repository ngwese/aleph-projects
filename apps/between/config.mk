# --- config.mk
#
# --- customized makefile for aleph-avr32 application.
# --- this is included via the ASF utility makefile.

APP = between

BAUD = 115200

include ../aleph_avr32_config.mk
include ../aleph_avr32_src.mk

# short git id (+ -dirty) for the info page (projects repo, not vendor/aleph)
GIT_REV := $(shell git -C $(ALEPH_PROJECTS_ROOT) rev-parse --short HEAD 2>/dev/null)
ifneq ($(GIT_REV),)
  GIT_DIRTY := $(shell git -C $(ALEPH_PROJECTS_ROOT) diff --quiet --ignore-submodules HEAD >/dev/null 2>&1; echo $$?)
  ifeq ($(GIT_DIRTY),0)
    GIT_HASH := $(GIT_REV)
  else
    GIT_HASH := $(GIT_REV)-dirty
  endif
else
  GIT_HASH :=
endif
CPPFLAGS += -DGIT_HASH='"$(GIT_HASH)"'
$(info GIT_HASH: $(GIT_HASH))

# app sources are cwd-relative (ASF APP_DIR is only for INC_PATH / linker)
CSRCS += \
	src/app_between.c \
	src/app_timers.c \
	src/handler.c \
	src/render.c \
	src/pages.c \
	src/modal.c \
	src/input_roles.c \
	src/pages/page_setups.c \
	src/pages/page_modules.c \
	src/pages/page_slots.c \
	src/pages/page_slot.c \
	src/pages/page_play_maps.c \
	src/pages/page_play.c \
	src/pages/page_info.c \
	src/pages/page_inspect.c \
	src/pages/name_edit.c \
	src/midi_between.c \
	src/cv_in.c \
	src/module_load.c \
	src/meters.c \
	src/xruns.c \
	src/dirlist.c \
	src/files_ensure.c \
	src/lineio_fl.c \
	src/preset_file.c \
	src/setup_file.c \
	src/state.c \
	src/scaler_tables.c \
	src/scalers/param_scaler.c \
	src/scalers/op_math.c \
	src/scalers/scaler_amp.c \
	src/scalers/scaler_bool.c \
	src/scalers/scaler_fix.c \
	src/scalers/scaler_fract.c \
	src/scalers/scaler_integrator.c \
	src/scalers/scaler_integrator_short.c \
	src/scalers/scaler_note.c \
	src/scalers/scaler_label.c \
	src/scalers/scaler_short.c \
	src/scalers/scaler_svf_fc.c \
	src/lib/kvtext.c \
	src/lib/cv_scale.c \
	src/lib/midi_nrpn.c \
	src/lib/morph2d.c \
	src/lib/play_maps.c \
	src/lib/preset_io.c \
	src/lib/setup_io.c \
	src/lib/slots.c

ASSRCS +=

INC_PATH += \
	$(APP_DIR) \
	$(APP_DIR)/src \
	$(APP_DIR)/src/lib \
	$(APP_DIR)/src/pages \
	$(APP_DIR)/src/scalers \
	$(APP_DIR)/../../../avr32-toolchain-linux/include
