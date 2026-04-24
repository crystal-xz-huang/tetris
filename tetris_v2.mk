EXERCISES   += tetris_v2
CLEAN_FILES += tetris_v2 tests/*.out.v1 tests/*.out.v2

tetris_v2: tetris_v2.c
	$(CC) -o $@ $<

.PHONY: test

# Regression tests: diffs tetris_v2 against tetris across tests/*.in,
# then benchmarks the two binaries.
test: tetris tetris_v2
	./run_tests.sh
