# Tetris in C

`tetris.c` is an implementation of [Tetris](https://en.wikipedia.org/wiki/Tetris) that runs in a terminal.

A game of Tetris takes place on a 2D field, where the player must fit together descending shapes to make lines.

You can move a piece left (<kbd>a</kbd>) and right (<kbd>d</kbd>), drop it down (one step with (<kbd>s</kbd>) or all the way with (<kbd>S</kbd> ), and rotate it (r and R).

Once a piece hits the bottom, another piece will appear at the top of the field.

Any horizontal lines in the field that become completely filled will be cleared, and points will be awarded to the player's score based on how many lines are cleared at the same time.

To get a feel for this game, try it out in a terminal:

```bash
git clone https://github.com/crystal-xz-huang/tetris.git
```

## Load Screen

The load screen is shown once on TTY launch. Press any key to start, or `Q` to quit.

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

The clock doesn't start until you dismiss the load screen. Piped stdin skips the load screen so scripts stay deterministic.

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

## Scoring

| Event               | Points              |
| ------------------- | ------------------- |
| Soft drop           | `1 × distance`      |
| Hard drop           | `2 × distance`      |
| Single line clear   | 100                 |
| Double line clear   | 300                 |
| Triple line clear   | 500                 |
| Tetris (4 lines)    | 800                 |

The level advances every ten lines cleared, starting from whatever level you launched at. The best score this session is shown on the HUD as `BEST`.

## Settings

Four display options can be toggled at any time during play:

- **Color** (<kbd>C</kbd>) — turn ANSI colors on or off.
- **Block** (<kbd>B</kbd>) — cycle the piece glyph: `██` blocks, `[]` brackets, or `II` letters.
- **Grid** (<kbd>G</kbd>) — cycle the field background: none, dot grid, or plus grid.
- **Ghost piece** (<kbd>H</kbd>) — dim preview of where the current piece would land.

## Getting Started

```bash
git clone https://github.com/crystal-xz-huang/tetris.git
cd tetris
make
sudo make install
```

Then run from anywhere:

```bash
tetris                      # start at level 1
tetris --start-level 5      # start at level 5 (1–9)
```

To uninstall:

```bash
sudo make uninstall
```

## Testing

Run in debug mode with `./tetris --debug` (or `make run-debug`). With `--debug`, `?` dumps internal game state instead of the help panel.
