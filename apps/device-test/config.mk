# --- config.mk
#
# --- customized makefile for aleph-avr32 application.
# --- this is included via the ASF utility makefile.

APP = device-test

BAUD = 115200

include ../aleph_avr32_config.mk
include ../aleph_avr32_src.mk

CSRCS += \
	$(APP_DIR)/src/app_device_test.c \
	$(APP_DIR)/src/app_timers.c \
	$(APP_DIR)/src/handler.c \
	$(APP_DIR)/src/render.c

ASSRCS +=

INC_PATH += \
	$(APP_DIR) \
	$(APP_DIR)/src \
	$(APP_DIR)/../../../avr32-toolchain-linux/include
