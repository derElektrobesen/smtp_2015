CC ?=
CFLAGS ?=
LDFLAGS ?=

EXTRA_FLAGS ?= -Wall -Werror -Wconversion -std=c99
EXTRA_LDFLASG ?= -flto -lconfig

CURRENT_DIR := $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))

MAKE_FLAGS = CC="$(CC)" CFLAGS="$(CFLAGS) $(EXTRA_FLAGS)" LDFLAGS="$(LDFLAGS) $(EXTRA_LDFLASG)"

all:
	cd $(CURRENT_DIR)/common ; make $(MAKE_FLAGS)
	cd $(CURRENT_DIR)/server ; make $(MAKE_FLAGS)

clean:
	cd $(CURRENT_DIR)/common ; make clean
	cd $(CURRENT_DIR)/server ; make clean
