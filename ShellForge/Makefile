.PHONY: all clean run test

CC = gcc
CFLAGS = -Wall -Wextra -Wpedantic -std=c11 -g -Iinclude

TARGET = shellforge

SRC = src/main.c src/input.c src/history.c src/tokenizer.c src/parser.c src/shell.c src/builtins.c src/jobs.c src/executor.c src/signals.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET)

clean:
	rm -f $(TARGET)

run: $(TARGET)
	./$(TARGET)

test:
	chmod +x tests/basic_shell_tests.sh
	./tests/basic_shell_tests.sh
	python3 tests/interactive_shell_tests.py
