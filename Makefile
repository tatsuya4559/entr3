CC := gcc
CFLAGS := -Wall

BIN := entr3

$(BIN): entr3.c
	$(CC) -o $@ $(CFLAGS) $^

.PHONY: lint
lint: $(BIN)
	 valgrind --leak-check=full --leak-resolution=high --show-reachable=yes ./$<
