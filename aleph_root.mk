# shared paths for out-of-tree apps and modules.
# include from apps/<name> or modules[_block]/<name> (cwd two levels below
# repo root).

ALEPH_PROJECTS_ROOT := $(abspath ../..)
ALEPH_ROOT ?= $(ALEPH_PROJECTS_ROOT)/vendor/aleph
LIB_AVR32_ROOT ?= $(ALEPH_PROJECTS_ROOT)/vendor/libavr32
BUILD_ROOT ?= $(ALEPH_PROJECTS_ROOT)/build
