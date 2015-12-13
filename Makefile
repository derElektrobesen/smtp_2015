CC ?=
CFLAGS ?=
LDFLAGS ?=

VERSION = '\"0.1\"'
BUILD_YEAR = '\"$(shell date +"%Y")\"'

PROJECT = '\"SMTP 2015\"'
DEVELOPERS = '\"Pavel Berezhnoy <pberejnoy2005@gmail.com>\"'

DEBUG ?= 0

ifeq ($(DEBUG), 0)
	CFLAGS += -O3 -flto
else
	CFLAGS += -O0 -ggdb3 -DDEBUG
endif

EXTRA_FLAGS ?= -Wall -Werror -Wconversion -std=c99 \
	-DVERSION=$(VERSION) \
	-DBUILD_YEAR=$(BUILD_YEAR) \
	-DDEVELOPERS=$(DEVELOPERS) \
	-DPROJECT=$(PROJECT) \
	-DMESSAGE_MAX_SIZE=$(shell echo '10*1024*1024' | bc)lu \
	-DLOG_PATH

EXTRA_LDFLASG ?= -flto -lconfig -lc

CURRENT_DIR := $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))

MAKE_FLAGS = CC="$(CC)" CFLAGS="$(CFLAGS) $(EXTRA_FLAGS)" LDFLAGS="$(LDFLAGS) $(EXTRA_LDFLASG)"

all:
	cd $(CURRENT_DIR)/common ; make $(MAKE_FLAGS)
	cd $(CURRENT_DIR)/server ; make $(MAKE_FLAGS)

clean:
	cd $(CURRENT_DIR)/common ; make clean
	cd $(CURRENT_DIR)/server ; make clean
