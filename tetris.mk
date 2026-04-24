EXERCISES += tetris
CLEAN_FILES += tetris

tetris: tetris.c
	$(CC) -o $@ $<