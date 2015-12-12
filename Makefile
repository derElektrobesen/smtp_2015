CC ?=
CFLAGS ?=
LDFLAGS ?=

VERSION = '\"0.1\"'
BUILD_YEAR = '\"$(shell date +"%Y")\"'

PROJECT = '\"SMTP 2015\"'
DEVELOPERS = '\"Pavel Berezhnoy <pberejnoy2005@gmail.com>\"'

EXTRA_FLAGS ?= -Wall -Werror -Wconversion -std=c99 \
	-DVERSION=$(VERSION) \
	-DBUILD_YEAR=$(BUILD_YEAR) \
	-DDEVELOPERS=$(DEVELOPERS) \
	-DPROJECT=$(PROJECT)

EXTRA_LDFLASG ?= -flto -lconfig

CURRENT_DIR := $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))

MAKE_FLAGS = CC="$(CC)" CFLAGS="$(CFLAGS) $(EXTRA_FLAGS)" LDFLAGS="$(LDFLAGS) $(EXTRA_LDFLASG)"

all:
	cd $(CURRENT_DIR)/common ; make $(MAKE_FLAGS)
	cd $(CURRENT_DIR)/server ; make $(MAKE_FLAGS)

clean:
	cd $(CURRENT_DIR)/common ; make clean
	cd $(CURRENT_DIR)/server ; make clean
