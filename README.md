# Tetris in C

`tetris.c` is an implementation of [Tetris](https://en.wikipedia.org/wiki/Tetris) that runs in a terminal.

## Table of Contents <!-- omit in toc -->
- [Tetris in C](#tetris-in-c)
  - [Overview](#overview)
  - [Load Screen](#load-screen)
  - [Controls](#controls)
  - [Scoring](#scoring)
  - [Settings](#settings)
  - [Launching](#launching)
  - [Getting Started](#getting-started)
  - [Testing](#testing)
  - [Example Gameplay](#example-gameplay)

## Overview

A command-line game of _Tetris_ takes place on a 2D field, where players
must fit together descending shapes to make lines. Fill a horizontal line
and it clears; clear enough lines and you level up.

When launched on a TTY the game opens with a load screen showing the
TETRIS title art, a `PRESS ANY KEY TO START` prompt, and the empty
playing field with a `READY!` banner. Press any key to start (Q to
quit). The HUD then sits to the **left** of the field and shows
`SCORE`, `BEST`, `LEVEL`, `LINES`, and elapsed `TIME`; the `NEXT`
preview and the `Press '?' for help` hint sit to the **right**. The
controls + settings panel opens on demand with <kbd>?</kbd>. An
optional ghost piece shows where the current piece would land. Frames
redraw in place each frame so gameplay stays smooth and flicker-free,
and the whole game block is horizontally centered to the terminal
width.

## Load Screen

The load screen is shown once on TTY launch:

```
              █████████╗ ████████╗ █████████╗ ████████╗   ███╗  ████████╗
              ╚══███╔══╝ ███╔════╝ ╚══███╔══╝ ███╔══███╗  ███║  ███╔════╝
                 ███║    ███████╗     ███║    ████████╔╝  ███║  ████████╗
                 ███║    ███╔═══╝     ███║    ███╔══███╗  ███║  ╚════███║
                 ███║    ████████╗    ███║    ███║  ███║  ███║  ████████║
                 ╚══╝    ╚═══════╝    ╚══╝    ╚══╝  ╚══╝  ╚══╝  ╚═══════╝

                              PRESS ANY KEY TO START

      SCORE: 0          ╔══════ TETRIS ══════╗    ╔═══ NEXT ═══╗
      BEST : 0          ║                    ║    ║            ║
                        ║                    ║    ║  ██        ║
      LEVEL: 1          ║                    ║    ║  ██████    ║
      LINES: 0          ║                    ║    ║            ║
      TIME : 00:00      ║                    ║    ╚════════════╝
                        ║                    ║
                        ║                    ║
                        ║                    ║
                        ║                    ║
                        ║                    ║
                        ║                    ║
                        ║       READY!       ║
                        ║                    ║
                        ║                    ║
                        ║                    ║
                        ║                    ║
                        ║                    ║
                        ║                    ║
                        ║                    ║    Press '?' for help
                        ║                    ║
                        ╚════════════════════╝
```

The clock doesn't start until you dismiss the load screen. Pressing
<kbd>Q</kbd> at the load screen quits without playing. Piped stdin
skips the load screen so scripts stay deterministic.

## Controls

| Action | Key |
| --- | --- |
| Move | <kbd>A</kbd> / <kbd>D</kbd> or <kbd>&larr;</kbd> / <kbd>&rarr;</kbd> |
| Rotate | <kbd>W</kbd> or <kbd>&uarr;</kbd> |
| Soft drop | <kbd>S</kbd> or <kbd>&darr;</kbd> (+1 per row) |
| Hard drop | <kbd>Space</kbd> (+2 per row) |
| Pause / resume | <kbd>P</kbd> |
| Help | <kbd>?</kbd> (with `--debug`, dumps internal state instead) |
| Quit | <kbd>Q</kbd> |
| Toggle color | <kbd>C</kbd> |
| Toggle block | <kbd>B</kbd> (`██` &rarr; `[]` &rarr; `II`) |
| Toggle grid | <kbd>G</kbd> (none &rarr; dots &rarr; plus) |
| Show ghost piece | <kbd>H</kbd> |

On an interactive terminal the current piece falls automatically. The
gravity tick scales with the current level (slow at level 1, fast at
level 9). A piped stdin stays command-driven with no gravity so scripts
remain deterministic.

## Scoring

| Event               | Points              |
| ------------------- | ------------------- |
| Soft drop           | `1 × distance`      |
| Hard drop           | `2 × distance`      |
| Single line clear   | 100                 |
| Double line clear   | 300                 |
| Triple line clear   | 500                 |
| Tetris (4 lines)    | 800                 |

The level advances every ten lines cleared, starting from whatever
level you launched at. The best score achieved this session is shown on
the HUD as `BEST`.

## Settings

Four display options can be toggled at any time during play:

- **Color** (<kbd>C</kbd>) — turn ANSI colors on or off.
- **Block** (<kbd>B</kbd>) — cycle the piece glyph: `██` blocks, `[]` brackets, or `II` letters.
- **Grid** (<kbd>G</kbd>) — cycle the field background: none, dot grid, or plus grid.
- **Ghost piece** (<kbd>H</kbd>) — dim preview of where the current piece would land.

The current value of each setting is shown inside the HELP overlay
(opened with <kbd>?</kbd>), so you can confirm what's active at a glance
without cluttering the field.

## Launching

Run `./tetris` (or `make run`) to drop straight into the game on a TTY.
Level defaults to `1`. To start at a higher level (harder levels raise
the gravity tick rate), pass `-l N`:

```bash
./tetris -l 5        # start directly at level 5
```

## Getting Started

To get a feel for this game, try it out in a terminal:

```bash
# Open a terminal (Command Prompt or PowerShell for Windows, Terminal for macOS or Linux)

# Ensure Git is installed
# Visit https://git-scm.com to download and install console Git if not already installed

# Clone the repository
git clone https://github.com/crystal-xz-huang/tetris.git

# Navigate to the project directory
cd tetris

# Compile and run the game
make run
```

> [!NOTE]
>
> The `make run` command compiles the source code and runs the game in one step. If you prefer to compile and run separately, you can use the following commands:
>
> ```bash
> make
> ./tetris
> ```

To clean up the compiled files and start fresh, use the following command:

```bash
make clean
```

This will remove the `tetris` executable and any object files generated during compilation.

## Testing

Run the game in debug mode using the following command:

```bash
make run-debug
# or
./tetris -d
./tetris --debug
```

Now the <kbd>?</kbd> command will print the internal state of the game instead of the help menu.

<!-- TODO: Add example output from debug mode here. -->

## Example Gameplay
