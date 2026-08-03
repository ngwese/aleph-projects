# aleph-projects

out-of-tree apps, modules, and dsp objects for [aleph](https://github.com/monome/aleph),
built against a vendored copy of the firmware tree.

## layout

```text
apps/            AVR32 applications
modules/         classic Blackfin modules
modules_block/   block-processing Blackfin modules
dsp/             out-of-tree classic DSP sources
dsp_block/       out-of-tree block DSP sources
vendor/aleph/    aleph firmware submodule (ngwese fork, branch `dev`)
vendor/libavr32/ ASF + monome AVR32 lib (used by apps; not aleph's nested copy)
```

## clone

```sh
git clone --recurse-submodules git@github.com:ngwese/aleph-projects.git
cd aleph-projects
# if you already cloned without submodules:
git submodule update --init --recursive
```

AVR32 apps build against `vendor/libavr32`. after a recursive update, deinit
aleph's nested copy so it is not checked out twice:

```sh
git -C vendor/aleph submodule deinit -f libavr32
```

## build

from an app or module directory, with [aleph-builder](https://github.com/ngwese/aleph-builder)
on `PATH` when available:

```sh
cd apps/between
aleph-builder make

cd modules_block/mx44
aleph-builder make
```

override the firmware tree or libavr32 if needed:

```sh
make ALEPH_ROOT=/path/to/aleph
make LIB_AVR32_ROOT=/path/to/libavr32
```
