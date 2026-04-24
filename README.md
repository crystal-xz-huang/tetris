# Tetris in C

`tetris.c` is an implementation of [Tetris](https://en.wikipedia.org/wiki/Tetris) that runs in a terminal.

## Overview

This simple game of _Tetris_ takes place on a 2D field, where players must fit together descending shapes to make lines.

You can move a piece left (<kbd>a</kbd>) and right (<kbd>d</kbd>), drop it down (one step with <kbd>s</kbd> or all the way with <kbd>S</kbd>), and rotate it (<kbd>r</kbd> and <kbd>R</kbd>).

Once a piece hits the bottom, another piece will appear at the top of the field.

Any horizontal lines in the field that become completely filled will be cleared, and points will be awarded to the player's score based on how many lines are cleared at the same time.

## Controls

| Key | Action |
| --- | --- |
| <kbd>a</kbd> | Move left |
| <kbd>d</kbd> | Move right |
| <kbd>s</kbd> | Move down one row |
| <kbd>S</kbd> | Hard drop to the bottom |
| <kbd>r</kbd> | Rotate clockwise |
| <kbd>R</kbd> | Rotate counter-clockwise |
| <kbd>p</kbd> | Place the current piece into the field |
| <kbd>n</kbd> | Skip to a new piece |
| <kbd>c</kbd> | Choose the next piece by symbol (<kbd>I</kbd>, <kbd>J</kbd>, <kbd>L</kbd>, <kbd>O</kbd>, <kbd>S</kbd>, <kbd>T</kbd>) |
| <kbd>t</kbd> | Toggle color on/off (mono-color mode shows each piece as its letter doubled, e.g. <kbd>II</kbd>, <kbd>JJ</kbd>) |
| <kbd>?</kbd> | Print the current state of the game |
| <kbd>q</kbd> | Quit |

## Getting Started

To get a feel for this game, try it out in a terminal:

1. **Clone the project to your local machine:**

```bash
git clone https://github.com/crystal-xz-huang/tetris.git
```

This will add the following files into the directory:

- `tetris.c`: an implementation of Tetris in C.
- `tetris.mk`: a [make](https://manpages.debian.org/jump?q=make.1) fragment for compiling `tetris.c`.
- `Makefile`: a makefile that includes `tetris.mk`.

2. **Compile and run the game in the terminal:**

```bash
make
./tetris
```

3. **To clean up the compiled files, run:**

```bash
make clean
```

## Example Gameplay
