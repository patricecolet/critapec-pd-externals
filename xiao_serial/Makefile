# Makefile for xiao_serial

lib.name = xiao_serial

lib.version := $(shell sed -n 's|^\#X text [0-9][0-9]* [0-9][0-9]* VERSION \(.*\);|\1|p' $(lib.name)-meta.pd)

class.sources = xiao_serial.c

datafiles = \
	LICENSE.txt \
	README.md \
	$(lib.name)-help.pd \
	$(lib.name)-meta.pd

sysname := $(shell uname -s)

ifeq ($(sysname),Linux)
ldlibs += -lpthread
else ifeq ($(sysname),Darwin)
ldlibs += -lpthread
# macOS a aussi termios POSIX, pas besoin de stub
endif

PDLIBBUILDER_DIR=pd-lib-builder/
include $(firstword $(wildcard $(PDLIBBUILDER_DIR)/Makefile.pdlibbuilder Makefile.pdlibbuilder))
