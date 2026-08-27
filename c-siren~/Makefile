# Makefile for c-siren~

lib.name = c-siren~

lib.version := $(shell sed -n 's|^\#X text [0-9][0-9]* [0-9][0-9]* VERSION \(.*\);|\1|p' $(lib.name)-meta.pd)

class.sources = src/c-siren~.cpp
common.sources = src/CSirenEngine.cpp src/Sirene.cpp

datafiles = \
        README.md \
        c-siren~-help.pd \
        c-siren~-meta.pd

cflags += -Isrc -DVERSION='"$(lib.version)"'
cxxflags += -std=c++17

# This Makefile is based on pd-lib-builder
PDLIBBUILDER_DIR=../pd-lib-builder/
include $(firstword $(wildcard $(PDLIBBUILDER_DIR)/Makefile.pdlibbuilder Makefile.pdlibbuilder))
