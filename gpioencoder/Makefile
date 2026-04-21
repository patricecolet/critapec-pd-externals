# Makefile for rpi_encoder_step

lib.name = rpi_encoder_step

lib.version := $(shell sed -n 's|^\#X text [0-9][0-9]* [0-9][0-9]* VERSION \(.*\);|\1|p' $(lib.name)-meta.pd)

class.sources = rpi_encoder_step.c

datafiles = \
	LICENSE.txt \
	README.md \
	$(lib.name)-help.pd \
	$(lib.name)-meta.pd

sysname := $(shell uname -s)

ifeq ($(sysname),Linux)
ldlibs += -lgpiod -lpthread
else
cflags += -DRPI_ENCODER_FORCE_STUB
endif

PDLIBBUILDER_DIR=../pd-lib-builder/
include $(firstword $(wildcard $(PDLIBBUILDER_DIR)/Makefile.pdlibbuilder Makefile.pdlibbuilder))


