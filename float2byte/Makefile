# Makefile for float2byte

lib.name = float2byte

lib.version := $(shell sed -n 's|^\#X text [0-9][0-9]* [0-9][0-9]* VERSION \(.*\);|\1|p' $(lib.name)-meta.pd)

class.sources = float2byte.c

datafiles = \
        LICENSE.txt \
        README.md \
        float2byte-help.pd \
        float2byte-meta.pd

cflags = -DVERSION='"$(lib.version)"'

# This Makefile is based on the Makefile from pd-lib-builder written by
# Katja Vetter. You can get it from:
# https://github.com/pure-data/pd-lib-builder

PDLIBBUILDER_DIR=pd-lib-builder/
include $(firstword $(wildcard $(PDLIBBUILDER_DIR)/Makefile.pdlibbuilder Makefile.pdlibbuilder))

