EXERCISES += tetris
CLEAN_FILES += tetris tests/test_tetris

tetris: tetris.c
	$(CC) -o $@ $<

tests/test_tetris: tests/test_tetris.c tetris.c
	$(CC) -Wall -Wextra -Werror -o $@ $<

.PHONY: test
test: tests/test_tetris tetris
	./tests/test_tetris
	./tests/e2e.sh

# Build and run with a single command. `make run` compiles if needed,
# then launches ./tetris. `make run-debug` passes --debug so '?' dumps
# internal state instead of printing the help menu.
.PHONY: run run-debug
run: tetris
	./tetris

run-debug: tetris
	./tetris --debug

PREFIX ?= /usr/local

.PHONY: install uninstall
install: tetris
	install -d $(PREFIX)/bin
	install -m 755 tetris $(PREFIX)/bin/tetris

uninstall:
	rm -f $(PREFIX)/bin/tetris

DOCS_DIR := docs

# Record a gameplay session to docs/gameplay.cast, then convert to docs/gameplay.svg.
# Run `make record` first (play, then exit with Q), then `make svg`.
.PHONY: record svg
record: tetris
	@mkdir -p $(DOCS_DIR)
	asciinema rec --overwrite $(DOCS_DIR)/gameplay.cast -c './tetris'

svg: $(DOCS_DIR)/gameplay.cast
	svg-term --in $(DOCS_DIR)/gameplay.cast --out $(DOCS_DIR)/gameplay.svg \
	    --window --width 80 --height 26
