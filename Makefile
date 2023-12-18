CC := gcc
CFLAGS := -Wall

BIN := entr3

DESTDIR := $(HOME)/.local/bin

$(BIN): entr3.c
	$(CC) -o $@ $(CFLAGS) $^

install: $(BIN)
	@mkdir -p $(DESTDIR)
	install $(BIN) $(DESTDIR)

.PHONY: uninstall
uninstall:
	rm $(DESTDIR)/$(BIN)

.PHONY: lint
lint: $(BIN)
	 valgrind --leak-check=full --leak-resolution=high --show-reachable=yes ./$<
