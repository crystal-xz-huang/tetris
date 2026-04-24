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
