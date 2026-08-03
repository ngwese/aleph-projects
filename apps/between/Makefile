# List of available make goals:
#
# all                     Default target, builds the project
# clean                   Clean up the project
# rebuild                 Rebuild the project
# format                  Run clang-format on src/*.c and src/*.h
#
# isp                     Use BatchISP instead of avr32program when interfacing
#                         the target device.
#
# chiperase               Perform a JTAG chip erase command
# cpuinfo                 Get CPU information
# debug                   Open a debug connection with the MCU
# erase                   Perform a flash erase
# halt                    Stop CPU execution
# program                 Program MCU memory from ELF output file
# reset                   Reset MCU
# readregs                Read CPU registers
# run                     Start CPU execution
# secureflash             Protect chip by setting security bit
#
# doc                     Build the documentation
# cleandoc                Clean up the documentation
# rebuilddoc              Rebuild the documentation


# Path to top level ASF directory under the vendored aleph tree.
include ../../aleph_root.mk
PRJ_PATH = $(ALEPH_ROOT)/libavr32/asf

# Include the common Makefile, which will also include the project specific
# config.mk file.

MAKEFILE_PATH = $(PRJ_PATH)/avr32/utils/make/Makefile.avr32.in
include $(MAKEFILE_PATH)

# Prefix with aleph-builder when available (toolchain wrapper).
ALEPH_BUILDER := $(shell command -v aleph-builder 2>/dev/null)
CLANG_FORMAT := $(if $(ALEPH_BUILDER),aleph-builder clang-format,clang-format)

.PHONY: format
format:
	$(CLANG_FORMAT) -i src/*.h src/*.c src/lib/*.h src/lib/*.c src/pages/*.c
