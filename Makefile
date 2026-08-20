CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2
SRC = src/lexer/lexer.c src/lexer/main.c
BIN = bin/minic_lexer

.PHONY: all clean test

all: $(BIN)

$(BIN): $(SRC) src/lexer/lexer.h
	mkdir -p bin
	$(CC) $(CFLAGS) -o $(BIN) $(SRC)

test: all
	bash tests/run_tests.sh

clean:
	rm -rf bin
