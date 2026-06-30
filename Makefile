CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -O2 -Iinclude
LDLIBS = -lm
SRC = src/soil_springs.c src/beam_fe.c src/linalg.c src/solver.c src/main.c
BIN = bin/sheetpile

all: $(BIN)

$(BIN): $(SRC)
	mkdir -p bin
	$(CC) $(CFLAGS) -o $(BIN) $(SRC) $(LDLIBS)

run: $(BIN)
	./$(BIN)

clean:
	rm -rf bin

.PHONY: all run clean
