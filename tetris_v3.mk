EXERCISES   += tetris_v3
CLEAN_FILES += tetris_v3

tetris_v3: tetris_v3.c
	$(CC) -o $@ $<
