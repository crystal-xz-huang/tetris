# A general makefile for C exercises.
# Looks for .mk files in the current directory and includes them.
# Each .mk file should define the EXERCISES variable with the list of exercises to build, and optionally the CLEAN_FILES variable with the list of files to remove when "make clean" is run.

ifneq (, $(shell which dcc))
CC	= dcc
else ifneq (, $(shell which clang) )
CC	= clang
else
CC  = gcc
endif

EXERCISES	?=
CLEAN_FILES	?=

.DEFAULT_GOAL	= all
.PHONY: all clean

-include *.mk

all:	${EXERCISES}

clean:
	-rm -f ${CLEAN_FILES}

