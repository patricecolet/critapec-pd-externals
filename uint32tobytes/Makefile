# Makefile for uint32tobytes

lib.name = uint32tobytes

lib.version := $(shell sed -n 's|^\#X text [0-9][0-9]* [0-9][0-9]* VERSION \(.*\);|\1|p' $(lib.name)-meta.pd)

class.sources = uint32tobytes.c

datafiles = \
        LICENSE.txt \
        README.md \
        uint32tobytes-help.pd \
        uint32tobytes-meta.pd

cflags = -DVERSION='"$(lib.version)"'

# This Makefile is based on the Makefile from pd-lib-builder written by
# Katja Vetter. You can get it from:
# https://github.com/pure-data/pd-lib-builder

PDLIBBUILDER_DIR=pd-lib-builder/
include $(firstword $(wildcard $(PDLIBBUILDER_DIR)/Makefile.pdlibbuilder Makefile.pdlibbuilder))




