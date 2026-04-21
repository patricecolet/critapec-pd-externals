# Makefile for bytes2int

lib.name = bytes2int

lib.version := $(shell sed -n 's|^\#X text [0-9][0-9]* [0-9][0-9]* VERSION \(.*\);|\1|p' $(lib.name)-meta.pd)

class.sources = bytes2int.c

datafiles = \
        LICENSE.txt \
        README.md \
        bytes2int-help.pd \
        bytes2int-meta.pd

cflags = -DVERSION='"$(lib.version)"'

# This Makefile is based on pd-lib-builder
PDLIBBUILDER_DIR=../pd-lib-builder/
include $(firstword $(wildcard $(PDLIBBUILDER_DIR)/Makefile.pdlibbuilder Makefile.pdlibbuilder))





