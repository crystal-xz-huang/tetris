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
