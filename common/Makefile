CC ?= gcc
CFLAGS ?=
LDFLAGS ?=

EXTRA_CFLAGS =

# TODO: move this def into parent Makefile
INCLUDE_PATHS = include

OBJ_DIR = obj
SRC_DIR = src
INC_DIR = include

SOURCES = $(wildcard $(SRC_DIR)/*.c)
INCLUDES = $(wildcard $(INC_DIR)/*.h)

OBJECTS = $(SOURCES:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
override CFLAGS += $(INCLUDE_PATHS:%=-I%) $(EXTRA_CFLAGS)

all: _common

_common: dirs common

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c $(INCLUDES)
	$(CC) -c -o $@ $< $(CFLAGS)

# this rule will create a list of objects. Use them in static linkage
# (can't build static library on fucking MacOS)
common: $(OBJECTS)

dirs: $(OBJ_DIR)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

clean:
	rm -f $(OBJECTS)

.PHONY: clean
