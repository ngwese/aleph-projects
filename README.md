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
```

## clone

```sh
git clone --recurse-submodules git@github.com:ngwese/aleph-projects.git
cd aleph-projects
# if you already cloned without submodules:
git submodule update --init --recursive
```

`vendor/aleph` itself vendors `libavr32`; the recursive update is required.

## build

from an app or module directory, with [aleph-builder](https://github.com/ngwese/aleph-builder)
on `PATH` when available:

```sh
cd apps/between
aleph-builder make

cd modules_block/mx44
aleph-builder make
```

override the firmware tree if needed:

```sh
make ALEPH_ROOT=/path/to/aleph
```
