CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -O2 -Iinclude
LDLIBS = -lm

CLI_SRC = src/soil_springs.c src/beam_fe.c src/linalg.c src/solver.c src/main.c
LIB_SRC = src/soil_springs.c src/beam_fe.c src/linalg.c src/solver.c src/api.c

CLI_BIN = bin/sheetpile

# The web backend (server/backend.py) loads this shared library via
# ctypes. The extension and the exact flags needed to build a shared
# library differ by platform, so detect it here rather than hardcoding
# .so. Override manually if this guess is ever wrong, e.g.
#   make lib OS=Windows_NT
ifeq ($(OS),Windows_NT)
    LIB_NAME = bin/libsheetpile.dll
    LIB_FLAGS = -shared
else
    UNAME_S := $(shell uname -s)
    ifeq ($(UNAME_S),Darwin)
        LIB_NAME = bin/libsheetpile.dylib
        LIB_FLAGS = -fPIC -shared
    else
        LIB_NAME = bin/libsheetpile.so
        LIB_FLAGS = -fPIC -shared
    endif
endif

all: $(CLI_BIN) lib

$(CLI_BIN): $(CLI_SRC)
	mkdir -p bin
	$(CC) $(CFLAGS) -o $(CLI_BIN) $(CLI_SRC) $(LDLIBS)

lib: $(LIB_SRC)
	mkdir -p bin
	$(CC) $(CFLAGS) $(LIB_FLAGS) -o $(LIB_NAME) $(LIB_SRC) $(LDLIBS)

run: $(CLI_BIN)
	./$(CLI_BIN)

clean:
	rm -rf bin

.PHONY: all run clean lib
