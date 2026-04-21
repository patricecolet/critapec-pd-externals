# Makefile for rtpmidi

lib.name = rtpmidi

lib.version := $(shell sed -n 's|^\#X text [0-9][0-9]* [0-9][0-9]* VERSION \(.*\);|\1|p' $(lib.name)-meta.pd)

class.sources = rtpmidi.c

datafiles = \
	LICENSE.txt \
	README.md \
	$(lib.name)-help.pd \
	$(lib.name)-meta.pd

cflags = -DVERSION='"$(lib.version)"'

sysname := $(shell uname -s)

ifeq ($(sysname),Darwin)
# Core MIDI Network Session lives in the CoreMIDI framework; Foundation is
# needed once we drive MIDINetworkSession from Objective-C code.
ldlibs += -framework CoreMIDI -framework Foundation
endif

PDLIBBUILDER_DIR=../pd-lib-builder/
include $(firstword $(wildcard $(PDLIBBUILDER_DIR)/Makefile.pdlibbuilder Makefile.pdlibbuilder))
