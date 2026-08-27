# Makefile for rtpmidi

lib.name = rtpmidi

lib.version := $(shell sed -n 's|^\#X text [0-9][0-9]* [0-9][0-9]* VERSION \(.*\);|\1|p' $(lib.name)-meta.pd)

datafiles = \
	LICENSE.txt \
	README.md \
	$(lib.name)-help.pd \
	$(lib.name)-meta.pd

cflags = -DVERSION='"$(lib.version)"'

sysname := $(shell uname -s)

# Platform split: the macOS backend is Objective-C (Core MIDI's
# MIDINetworkSession has no plain-C API); Linux/Windows load the stub.
ifeq ($(sysname),Darwin)
class.sources = rtpmidi.m
ldlibs += -framework CoreMIDI -framework Foundation
# MIDINetworkSession is annotated as macOS 10.15+, so raise the deployment
# target. pd-lib-builder reads -mmacosx-version-min from cflags if present,
# otherwise defaults to 10.6 (too low for this API).
cflags += -fno-objc-arc -mmacosx-version-min=10.15
else
class.sources = rtpmidi.c
endif

PDLIBBUILDER_DIR=../pd-lib-builder/
include $(firstword $(wildcard $(PDLIBBUILDER_DIR)/Makefile.pdlibbuilder Makefile.pdlibbuilder))

# pd-lib-builder ships pattern rules for .c/.cc/.cpp only. On Darwin we keep
# both rtpmidi.c (the non-Apple stub) and rtpmidi.m (the real backend) in the
# tree, so its %.o:: %.c pattern matches first. Declare an explicit target to
# force the .m source - explicit targets beat pattern rules.
ifeq ($(sysname),Darwin)
rtpmidi.$(object.extension): rtpmidi.m
	$(info ++++ info: making $@ in lib $(lib.name))
	$(compile-c) $(c.flags) -o $@ -c $<
endif
