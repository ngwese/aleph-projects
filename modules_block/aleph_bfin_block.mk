# out-of-tree Blackfin block-module build support.
# include from modules_block/<name>/Makefile after setting module_name / version.mk.
# keeps core and DSP objects under ./obj so vendor/aleph stays clean.

include ../../aleph_root.mk

# modules define `all` after including this file; keep it as the default goal
# so the directory mkdir rules below are not chosen instead.
.DEFAULT_GOAL := all

audio = $(ALEPH_ROOT)/dsp
audio_block = $(ALEPH_ROOT)/dsp_block

bfin_lib_dir = $(ALEPH_ROOT)/bfin_lib_block/
bfin_lib_srcdir = $(bfin_lib_dir)src/
bfin_lib_objdir = obj/bfin_lib/
common_dir = $(ALEPH_ROOT)/common
audio_dir = $(ALEPH_ROOT)/dsp
dsp_block_dir = $(ALEPH_ROOT)/dsp_block
module_dir = ./

dsp_objdir = obj/dsp/
dsp_block_objdir = obj/dsp_block/

bfin_lib_src = audio.c \
	clock_ebiu.c \
	control.c \
	cv.c \
	gpio.c \
	dma.c \
	isr.c \
	main.c \
	meters.c \
	serial.c \
	spi.c

bfin_lib_obj = $(patsubst %.c, %.o, $(bfin_lib_src))

bfin_lib_hdr = $(wildcard $(bfin_lib_srcdir)*.h) $(module_dir)module_custom.h

INC += -I$(bfin_lib_srcdir) \
	-I$(bfin_lib_srcdir)/libfixmath \
	-I$(common_dir) \
	-I$(audio_dir) \
	-I$(dsp_block_dir) \
	-I$(module_dir)

CROSS_COMPILE = bfin-elf-
CC = $(CROSS_COMPILE)gcc
LDR = $(CROSS_COMPILE)ldr
CPU = bf533
CFLAGS += -Wall -mcpu=$(CPU) $(INC)
CFLAGS += -O3

LDFLAGS += -mcpu=$(CPU)
LDRFLAGS += --bits 16 --dma 8
LDRFLAGS += --bmode spi_slave --port F --gpio 2
LDRFLAGS += --verbose

$(bfin_lib_objdir) $(dsp_objdir) $(dsp_block_objdir):
	mkdir -p $@

bfin_lib_target: $(patsubst %.o, $(bfin_lib_objdir)%.o, $(bfin_lib_obj))
	@echo bfin_lib objects are complete in $(bfin_lib_objdir)

$(bfin_lib_objdir)%.o : $(bfin_lib_srcdir)%.c $(bfin_lib_hdr) | $(bfin_lib_objdir)
	$(CC) $(CFLAGS) $(INC) -c \
	-D MAJ=$(maj) -D MIN=$(min) -D REV=$(rev) \
	$< -o $@

%.ldr: %
	$(LDR) -T $(CPU) -c $(LDRFLAGS) $@ $<

bfin_lib_clean:
	rm -rf obj

$(dsp_objdir)%.o: $(audio)/%.c | $(dsp_objdir)
	$(CC) $(CFLAGS) -c -o $@ $<

$(dsp_block_objdir)%.o: $(audio_block)/%.c | $(dsp_block_objdir)
	$(CC) $(CFLAGS) -c -o $@ $<

.PHONY: bfin_lib_target bfin_lib_clean
