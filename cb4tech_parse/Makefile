# Makefile for cb4tech_parse

lib.name = cb4tech_parse

lib.version := $(shell sed -n 's|^\#X text [0-9][0-9]* [0-9][0-9]* VERSION \(.*\);|\1|p' $(lib.name)-meta.pd)

class.sources = cb4tech_parse.c

datafiles = \
        LICENSE.txt \
        README.md \
        cb4tech_parse-help.pd \
        cb4tech_parse-meta.pd

cflags = -DVERSION='"$(lib.version)"'

# This Makefile is based on the Makefile from pd-lib-builder written by
# Katja Vetter. You can get it from:
# https://github.com/pure-data/pd-lib-builder

PDLIBBUILDER_DIR=pd-lib-builder/
include $(firstword $(wildcard $(PDLIBBUILDER_DIR)/Makefile.pdlibbuilder Makefile.pdlibbuilder))

