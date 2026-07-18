# --- config.mk
#
# --- customized makefile for aleph-avr32 application.
# --- this is included via the ASF utility makefile.

APP = between

BAUD = 115200

include ../aleph_avr32_config.mk
include ../aleph_avr32_src.mk

CSRCS += \
	$(APP_DIR)/src/app_between.c \
	$(APP_DIR)/src/app_timers.c \
	$(APP_DIR)/src/handler.c \
	$(APP_DIR)/src/render.c \
	$(APP_DIR)/src/pages.c \
	$(APP_DIR)/src/pages/page_setups.c \
	$(APP_DIR)/src/pages/page_modules.c \
	$(APP_DIR)/src/pages/page_slots.c \
	$(APP_DIR)/src/pages/page_slot.c \
	$(APP_DIR)/src/pages/page_play_maps.c \
	$(APP_DIR)/src/pages/page_play.c \
	$(APP_DIR)/src/pages/name_edit.c \
	$(APP_DIR)/src/module_load.c \
	$(APP_DIR)/src/dirlist.c \
	$(APP_DIR)/src/files_ensure.c \
	$(APP_DIR)/src/lineio_fl.c \
	$(APP_DIR)/src/preset_file.c \
	$(APP_DIR)/src/setup_file.c \
	$(APP_DIR)/src/state.c \
	$(APP_DIR)/src/scaler_tables.c \
	$(APP_DIR)/src/scalers/param_scaler.c \
	$(APP_DIR)/src/scalers/op_math.c \
	$(APP_DIR)/src/scalers/scaler_amp.c \
	$(APP_DIR)/src/scalers/scaler_bool.c \
	$(APP_DIR)/src/scalers/scaler_fix.c \
	$(APP_DIR)/src/scalers/scaler_fract.c \
	$(APP_DIR)/src/scalers/scaler_integrator.c \
	$(APP_DIR)/src/scalers/scaler_integrator_short.c \
	$(APP_DIR)/src/scalers/scaler_note.c \
	$(APP_DIR)/src/scalers/scaler_label.c \
	$(APP_DIR)/src/scalers/scaler_short.c \
	$(APP_DIR)/src/scalers/scaler_svf_fc.c \
	$(APP_DIR)/src/lib/kvtext.c \
	$(APP_DIR)/src/lib/morph2d.c \
	$(APP_DIR)/src/lib/play_maps.c \
	$(APP_DIR)/src/lib/preset_io.c \
	$(APP_DIR)/src/lib/setup_io.c \
	$(APP_DIR)/src/lib/slots.c

ASSRCS +=

INC_PATH += \
	$(APP_DIR) \
	$(APP_DIR)/src \
	$(APP_DIR)/src/lib \
	$(APP_DIR)/src/pages \
	$(APP_DIR)/src/scalers \
	$(APP_DIR)/../../../avr32-toolchain-linux/include
