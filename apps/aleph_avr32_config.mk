# ------------------------
# -- aleph_avr32_config.mk (out-of-tree)
#
# path variables below are relative to PRJ_PATH (vendor/libavr32/asf).
# app sources in each app's config.mk should use cwd-relative paths (src/...),
# because ASF prefixes INC_PATH / LINKER_SCRIPT with $(PRJ_PATH)/ but resolves
# many CSRCS via VPATH or the app working directory.

include ../../aleph_root.mk

# objects / .d under repo-root build (ASF BUILD_DIR); final .elf stays in cwd
BUILD_DIR ?= $(BUILD_ROOT)/apps/$(APP)

# configure paths for libavr32 (vendor/libavr32) and aleph cores (vendor/aleph)
LIB_AVR32 = ../
ALEPH_COMMON = ../../aleph/common
ALEPH_AVR32 = ../../aleph/avr32
# application directory relative to ASF -> aleph-projects/apps/$(APP)
APP_DIR = ../../../apps/$(APP)

# Target CPU architecture: ap, ucr1, ucr2 or ucr3
ARCH = ucr2

# Target part: none, ap7xxx or uc3xxxxx
PART = uc3a0512

# Target device flash memory details (used by the avr32program programming
# tool: [cfi|internal]@address
FLASH = internal@0x80000000

# Clock source to use when programming; xtal, extclk or int
PROG_CLOCK = int

# Application target name. Given with suffix .a for library and .elf for a
# standalone application.
TARGET = aleph-$(APP).elf

# Path relative to top level directory pointing to a linker script.
LINKER_SCRIPT = $(APP_DIR)/aleph-$(APP).lds

# AVR32 options for debugging. By default the common Makefile.in will
# add -g3.
DBGFLAGS =

# optimization level
OPTIMIZATION = -O3

# release/debug build
ifdef R
  ifeq ("$(origin R)", "command line")
    RELEASE = $(R)
  endif
endif
ifndef RELEASE
  RELEASE = 0
endif

$(info RELEASE: $(RELEASE))
$(info ALEPH_ROOT: $(ALEPH_ROOT))

# version define
include version.mk

# preprocessor definitions
CPPFLAGS = \
       -D BOARD=USER_BOARD -D ARCH_AVR32=1 -D UHD_ENABLE -DDEV_USART_BAUDRATE=$(BAUD) -D VERSIONSTRING='"$(version)"' -D MAJ=$(maj) -D MIN=$(min) -D REV=$(rev) -D RELEASEBUILD=$(RELEASE)

# Extra flags to use when linking
#####
## NOTE:
# use this line instead if you want to use without the bootloader!
# linking with the trampoline on a bootloaded app is bad news.
# LDFLAGS = -nostartfiles -Wl,-e,_trampoline
LDFLAGS = -nostartfiles
