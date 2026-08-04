
module_name = mx44

include version.mk
version = $(maj).$(min).$(rev)
ldr_name = $(module_name)-$(version).ldr

# vendored paths + local obj dirs (see aleph_bfin_block.mk)
include ../aleph_bfin_block.mk

module_obj = $(module_objdir)mx44_module.o \
	$(dsp_block_objdir)filter_1p_blk.o \
	$(dsp_block_objdir)filter_bp_blk.o \
	$(dsp_block_objdir)filter_bp_alpha_tab.o \
	$(dsp_block_objdir)peak_meter_blk.o

all: $(module_name).ldr $(module_name).dsc

CFLAGS += -D ARCH_BFIN=1
CFLAGS += -I$(audio_block)

desc_src = \
	$(bfin_lib_srcdir)desc.c \
	$(bfin_lib_srcdir)pickle.c \
	mx44_params.c

$(module_name)_desc_build: $(desc_src) mx44_params.h module_custom.h
	gcc $(desc_src) \
	$(INC) \
	-D NAME=\"$(module_name)\" \
	-o $(module_name)_desc_build

desc: $(module_name)_desc_build

$(module_name).dsc: $(module_name)_desc_build
	./$(module_name)_desc_build

$(module_objdir)mx44_module.o: mx44_module.c | $(module_objdir)
	$(CC) $(CFLAGS) -c -o $@ $< $(LDFLAGS)

$(module_name): bfin_lib_target $(module_obj)
	$(CC) $(LDFLAGS) -T $(module_name).lds \
	$(patsubst %.o, $(bfin_lib_objdir)%.o, $(bfin_lib_obj)) \
	$(module_obj) \
	-o $(module_name) \
	-lm -lbfdsp -lbffastfp

clean: bfin_lib_clean
	rm -f $(module_name).ldr
	rm -f $(module_name)
	rm -f $(module_name).dsc
	rm -f $(module_name)_desc_build

deploy: $(module_name).ldr $(module_name).dsc
	cp $(module_name).ldr $(module_name)-$(maj).$(min).$(rev).ldr
	cp $(module_name).dsc $(module_name)-$(maj).$(min).$(rev).dsc

.PHONY: all clean deploy desc
