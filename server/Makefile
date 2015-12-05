CC ?= gcc
CFLAGS ?=
LDFLAGS ?=

EXTRA_CFLAGS =

CURRENT_DIR := $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))
COMMON_DIR = $(CURRENT_DIR)/../common

# TODO: move this def into parent Makefile
INCLUDE_PATHS = $(COMMON_DIR)/include include

OBJ_DIR = obj
SRC_DIR = src
INC_DIR = include

SOURCES = $(wildcard $(SRC_DIR)/*.c)
INCLUDES = $(wildcard $(INC_DIR)/*.h)

COMMON_INCLUDES = $(wildcard $(COMMON_DIR)/$(INC_DIR)/*.h)
COMMON_OBJS = $(wildcard $(COMMON_DIR)/$(OBJ_DIR)/*.o)

OBJECTS = $(SOURCES:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
override CFLAGS += $(INCLUDE_PATHS:%=-I%) $(EXTRA_CFLAGS)

all: _server

_server: dirs server

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c $(INCLUDES) $(COMMON_INCLUDES)
	$(CC) -c -o $@ $< $(CFLAGS)

server: $(OBJECTS) $(COMMON_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

dirs: $(OBJ_DIR)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

clean:
	rm -f $(OBJECTS) server

.PHONY: clean
