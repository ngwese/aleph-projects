# --- config.mk
#
# --- customized makefile for aleph-avr32 application.
# --- this is included via the ASF utility makefile.

# app name
APP = spray

# baudrate! can override in make invocation
BAUD = 115200

# avr32 configuration
include ../aleph_avr32_config.mk
# avr32 sources
include ../aleph_avr32_src.mk

# app sources are cwd-relative (ASF APP_DIR is only for INC_PATH / linker)
CSRCS += \
	src/app_spray.c \
	src/app_timers.c \
	src/ctl.c \
	src/files.c \
	src/handler.c \
	src/render.c \
	src/scaler.c

# List of assembler source files.
ASSRCS +=

# List of include paths.
INC_PATH += \
	$(APP_DIR) \
	$(APP_DIR)/src \
	$(APP_DIR)/../../../avr32-toolchain-linux/include
