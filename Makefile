
module_name = mx44

# paths to aleph repository sources
audio = ../../dsp
bfin = ../../bfin_lib_block/src

# define version ids
include version.mk
version = $(maj).$(min).$(rev)
ldr_name = $(module_name)-$(version).ldr

# add sources from here/audio library.
module_obj = module.o \
	$(audio)/filter_1p.o

# -----  below here, probably dont need to customize.

all: $(module_name).ldr $(module_name).dsc

# this gets the core configuration and sources
include ../../bfin_lib_block/bfin_lib_block.mk

CFLAGS += -D ARCH_BFIN=1
# diagnose gcc errors
# CFLAGS += --verbose

desc_src = \
	$(bfin_lib_srcdir)desc.c \
	$(bfin_lib_srcdir)pickle.c \
	params.c

# this target generates the descriptor helper program
$(module_name)_desc_build: $(desc_src) params.h module_custom.h
	gcc $(desc_src) \
	$(INC) \
	-D NAME=\"$(module_name)\" \
	-o $(module_name)_desc_build

desc: $(module_name)_desc_build

$(module_name).dsc: $(module_name)_desc_build
	./$(module_name)_desc_build

$(module_obj): %.o : %.c
	$(CC) $(CFLAGS) -c -o $@ $< $(LDFLAGS)

$(module_name): bfin_lib_target $(module_obj)
	$(CC) $(LDFLAGS) -T $(module_name).lds \
	$(patsubst %.o, $(bfin_lib_objdir)%.o, $(bfin_lib_obj)) \
	$(module_obj) \
	-o $(module_name) \
	-lm -lbfdsp -lbffastfp

clean: bfin_lib_clean
	rm -f $(module_obj)
	rm -f $(module_name).ldr
	rm -f $(module_name)
	rm -f $(module_name).dsc
	rm -f $(module_name)_desc_build

# this generates the module and descriptor helper app,
# and makes copies with version number strings.
# best used after "make clean"
deploy: $(module_name).ldr $(module_name).dsc
	cp $(module_name).ldr $(module_name)-$(maj).$(min).$(rev).ldr
	cp $(module_name).dsc $(module_name)-$(maj).$(min).$(rev).dsc

.PHONY: all clean deploy desc
