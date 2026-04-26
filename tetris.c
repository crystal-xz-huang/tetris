/**
 * tetris.c
 *
 * Tetris in a terminal.
 *
 * Gameplay:
 *   - On a TTY, the game opens with a load screen showing the TETRIS
 *     title art + "PRESS ANY KEY TO START" + the empty playing field
 *     with a READY! banner. Press any key to start (Q to quit). Level
 *     defaults to 1 (override with -l N). Gravity scales with the
 *     level. Piped stdin keeps the deterministic command-driven path
 *     (no gravity, no load screen) so scripts stay reproducible.
 *   - Frames redraw in place via cursor-home (no flicker). The cursor
 *     is hidden during play and restored on exit / signal.
 *   - The HUD column sits to the LEFT of the field; NEXT preview and
 *     the "Press '?' for help" hint sit to the right.
 *   - Arrow keys and WASD both work on a TTY.
 *
 * HUD shows: NEXT preview, LEVEL, LINES, TIME (mm:ss), SCORE, BEST,
 * and a "Press '?' for help" hint. The HELP / settings panel is hidden
 * by default and opens on '?'.
 *
 * Modes:
 *   - Default: '?' opens the HELP panel (controls + current settings)
 *     and pauses the game until any key is pressed.
 *   - --debug: '?' dumps internal state instead of the HELP panel.
 *
 * Controls:
 *   MOVE              A / D | Arrow Left / Arrow Right
 *   ROTATE            W     | Arrow Up
 *   SOFT DROP         S     | Arrow Down                (+1 per row)
 *   HARD DROP         Space                             (+2 per row)
 *
 *   PAUSE/RESUME      P
 *   HELP              ?
 *   QUIT              Q
 *
 *   TOGGLE COLOR      C
 *   TOGGLE BLOCK      B   (block / [] / II)
 *   TOGGLE GRID       G   (none / dots / plus)
 *   SHOW GHOST PIECE  H
 *
 * Scoring:
 *   Soft Drop   1 x Distance
 *   Hard Drop   2 x Distance
 *   Single      100
 *   Double      300
 *   Triple      500
 *   Tetris (4)  800
 *
 * Usage:
 *   tetris                  Play the game.
 *   tetris -d, --debug      '?' dumps debug state instead of HELP.
 *   tetris -l N             Skip the start menu; start at level N.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <ctype.h>

#include <signal.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <termios.h>

/////////////////// Constants ///////////////////

#define FIELD_WIDTH   10
#define FIELD_HEIGHT  20
#define PIECE_SIZE    4
#define NUM_SHAPES    7

#define EMPTY ' '

// NEXT preview grid (cells). 4x4 fits every tetromino in any rotation.
#define NEXT_BOX_W    4
#define NEXT_BOX_H    4

///////////////////// Types /////////////////////

struct coordinate {
    int x;
    int y;
};

struct shape {
    char symbol;
    struct coordinate coordinates[PIECE_SIZE];
};

typedef enum {
    THEME_CLASSIC = 0,
    THEME_PASTEL,
    THEME_RETRO,
    THEME_BW,
    THEME_COUNT
} theme_t;

typedef enum {
    SYMBOL_BLOCK = 0,    // ██
    SYMBOL_BRACKET,      // []
    SYMBOL_LETTER,       // II  (piece letter doubled)
    SYMBOL_COUNT
} symbol_style_t;

typedef enum {
    GRID_NONE = 0,
    GRID_DOT,
    GRID_PLUS,
    GRID_COUNT
} grid_style_t;

struct game_state {
    int    next_shape_index;
    struct coordinate shape_coordinates[PIECE_SIZE];
    char   piece_symbol;
    int    piece_x;
    int    piece_y;
    int    piece_rotation;
    int    score;
    int    best_score;
    int    lines;
    int    level;
    int    starting_level;
    bool   game_running;
    bool   use_color;
    bool   debug_mode;
    bool   show_ghost;
    bool   show_ready;      // READY! banner until first key after level pick
    bool   paused;          // PAUSED banner; TTY loop waits for any key
    bool   show_help;       // '?' opens the HELP+settings overlay
    theme_t         theme;
    symbol_style_t  symbol_style;
    grid_style_t    grid_style;
    time_t start_time;
    char   field[FIELD_HEIGHT][FIELD_WIDTH];
};

//////////////////// Shape Table ////////////////

static const struct shape shapes[NUM_SHAPES] = {
    { 'I', { {-1,  0}, { 0,  0}, { 1,  0}, { 2,  0} } },
    { 'J', { {-1, -1}, {-1,  0}, { 0,  0}, { 1,  0} } },
    { 'L', { {-1,  0}, { 0,  0}, { 1,  0}, { 1, -1} } },
    { 'O', { { 0,  0}, { 0,  1}, { 1,  1}, { 1,  0} } },
    { 'S', { { 0,  0}, {-1,  0}, { 0, -1}, { 1, -1} } },
    { 'T', { { 0,  0}, { 0, -1}, {-1,  0}, { 1,  0} } },
    { 'Z', { { 0,  0}, { 1,  0}, { 0, -1}, {-1, -1} } },
};

//////////////////// Borders ////////////////////

#define BOX_H   "═"
#define BOX_V   "║"
#define BOX_TL  "╔"
#define BOX_TR  "╗"
#define BOX_BL  "╚"
#define BOX_BR  "╝"

#define ANSI_RESET "\033[0m"
#define ANSI_DIM   "\033[2m"
#define ANSI_BOLD  "\033[1m"

//////////////////// Colors /////////////////////

static const char *color_classic(char sym) {
    switch (sym) {
        case 'I': return "\033[96m";  // bright cyan
        case 'J': return "\033[94m";  // bright blue
        case 'L': return "\033[33m";  // yellow/orange
        case 'O': return "\033[93m";  // bright yellow
        case 'S': return "\033[92m";  // bright green
        case 'T': return "\033[95m";  // bright magenta
        case 'Z': return "\033[91m";  // bright red
        default:  return "";
    }
}

static const char *color_pastel(char sym) {
    // 256-color palette, muted hues.
    switch (sym) {
        case 'I': return "\033[38;5;152m";
        case 'J': return "\033[38;5;147m";
        case 'L': return "\033[38;5;216m";
        case 'O': return "\033[38;5;229m";
        case 'S': return "\033[38;5;157m";
        case 'T': return "\033[38;5;219m";
        case 'Z': return "\033[38;5;217m";
        default:  return "";
    }
}

static const char *color_retro(char sym) {
    // Neon-green CRT arcade look; single bright tone for every piece.
    (void)sym;
    return "\033[38;5;46m";
}

static const char *color_for_symbol(const struct game_state *gs, char sym) {
    if (!gs->use_color) return "";
    switch (gs->theme) {
        case THEME_CLASSIC: return color_classic(sym);
        case THEME_PASTEL:  return color_pastel(sym);
        case THEME_RETRO:   return color_retro(sym);
        case THEME_BW:      return "";
        default:            return "";
    }
}

static const char *theme_name(theme_t t) {
    switch (t) {
        case THEME_CLASSIC: return "Classic";
        case THEME_PASTEL:  return "Pastel";
        case THEME_RETRO:   return "Retro";
        case THEME_BW:      return "Black/White";
        default:            return "?";
    }
}

static const char *symbol_name(symbol_style_t s) {
    switch (s) {
        case SYMBOL_BLOCK:   return "Block";
        case SYMBOL_BRACKET: return "[]";
        case SYMBOL_LETTER:  return "II";
        default:             return "?";
    }
}

static const char *grid_name(grid_style_t g) {
    switch (g) {
        case GRID_NONE: return "None";
        case GRID_DOT:  return "Dots";
        case GRID_PLUS: return "Plus";
        default:        return "?";
    }
}

// Filled cell, 2 chars wide. `is_ghost` dims the cell for the ghost
// piece preview. ANSI styling is only emitted when use_color is on.
static void put_cell_filled(const struct game_state *gs, char sym, bool is_ghost) {
    const char *col   = color_for_symbol(gs, sym);
    const char *dim   = (is_ghost && gs->use_color) ? ANSI_DIM : "";
    const char *reset = (*col || (is_ghost && gs->use_color)) ? ANSI_RESET : "";

    if (gs->symbol_style == SYMBOL_BRACKET) {
        const char *glyph = is_ghost ? "::" : "[]";
        printf("%s%s%s%s", dim, col, glyph, reset);
    } else if (gs->symbol_style == SYMBOL_LETTER) {
        // In mono mode letter ghosts would be indistinguishable from
        // placed pieces. Fall back to "::" so the ghost still reads as a
        // hint rather than a solid block.
        if (is_ghost && !gs->use_color) {
            fputs("::", stdout);
        } else {
            printf("%s%s%c%c%s", dim, col, sym, sym, reset);
        }
    } else {
        const char *glyph = is_ghost ? "░░" : "██";
        printf("%s%s%s%s", dim, col, glyph, reset);
    }
}

// Empty cell, 2 chars wide. Grid styles render a pattern instead of blanks.
static void put_cell_empty(const struct game_state *gs) {
    if (gs->grid_style == GRID_DOT && gs->use_color) {
        printf("%s ·%s", ANSI_DIM, ANSI_RESET);
    } else if (gs->grid_style == GRID_DOT) {
        fputs(" .", stdout);
    } else if (gs->grid_style == GRID_PLUS && gs->use_color) {
        printf("%s +%s", ANSI_DIM, ANSI_RESET);
    } else if (gs->grid_style == GRID_PLUS) {
        fputs(" +", stdout);
    } else {
        fputs("  ", stdout);
    }
}

static bool should_use_color(void) {
    if (getenv("NO_COLOR") != NULL) return false;
    return isatty(STDOUT_FILENO) != 0;
}

////////////////// Prototypes ///////////////////

static void setup_game(struct game_state *gs);
static void setup_field(struct game_state *gs);

static void new_piece(struct game_state *gs, bool should_announce);
static void place_piece(struct game_state *gs);
static void consume_lines(struct game_state *gs);
static int  compute_points_for_clear(int lines_cleared);

static void rotate_right(struct game_state *gs);
static void rotate_left(struct game_state *gs);
static bool move_piece(struct game_state *gs, int dx, int dy);
static bool piece_intersects_field(const struct game_state *gs);
static const struct coordinate *piece_hit_test(
    const struct coordinate coordinates[PIECE_SIZE],
    int piece_x, int piece_y, int row, int col);
static int  ghost_drop_distance(const struct game_state *gs);

static void print_screen(const struct game_state *gs);
static void print_title_screen(const struct game_state *gs);
static void print_help_menu(const struct game_state *gs);
static void show_debug_info(const struct game_state *gs);

static char read_char(void);

static void game_loop(struct game_state *gs);
static bool handle_command(struct game_state *gs, char command);

static void game_loop_interactive(struct game_state *gs);

/////////////////// MAIN ////////////////////

static void print_usage(const char *prog) {
    fprintf(stderr,
        "Usage: %s [-d|--debug] [-l LEVEL]\n"
        "  -d, --debug     '?' prints internal debug state instead of pausing\n"
        "  -l LEVEL        skip the start menu; start at LEVEL (1-9)\n",
        prog);
}

int main(int argc, char **argv) {
    bool debug_mode = false;
    int  cli_level  = 0; // 0 = not set

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--debug") == 0) {
            debug_mode = true;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "-l") == 0 && i + 1 < argc) {
            cli_level = atoi(argv[++i]);
            if (cli_level < 1 || cli_level > 9) {
                fprintf(stderr, "Level must be between 1 and 9\n");
                return 1;
            }
        } else {
            fprintf(stderr, "Unknown argument: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }

    struct game_state gs;
    printf("Welcome to my tetris!\n");

    setup_game(&gs);
    gs.debug_mode = debug_mode;
    if (cli_level > 0) {
        gs.starting_level = cli_level;
        gs.level          = cli_level;
    }
    new_piece(&gs, /* should_announce = */ false);

    if (isatty(STDIN_FILENO)) {
        game_loop_interactive(&gs);
    } else {
        game_loop(&gs);
    }

    return 0;
}

/////////////////// SETUP ////////////////////

static void setup_game(struct game_state *gs) {
    gs->next_shape_index = 0;
    gs->piece_symbol     = EMPTY;
    gs->piece_x          = 0;
    gs->piece_y          = 0;
    gs->piece_rotation   = 0;
    gs->score            = 0;
    gs->best_score       = 0;
    gs->lines            = 0;
    gs->level            = 1;
    gs->starting_level   = 1;
    gs->game_running     = true;
    gs->use_color        = should_use_color();
    gs->debug_mode       = false;
    gs->show_ghost       = true;
    gs->show_ready       = false;
    gs->paused           = false;
    gs->show_help        = false;
    gs->theme            = THEME_CLASSIC;
    gs->symbol_style     = SYMBOL_BLOCK;
    gs->grid_style       = GRID_NONE;
    gs->start_time       = time(NULL);
    for (int i = 0; i < PIECE_SIZE; ++i) {
        gs->shape_coordinates[i].x = 0;
        gs->shape_coordinates[i].y = 0;
    }
    setup_field(gs);
}

static void setup_field(struct game_state *gs) {
    for (int row = 0; row < FIELD_HEIGHT; ++row) {
        for (int col = 0; col < FIELD_WIDTH; ++col) {
            gs->field[row][col] = EMPTY;
        }
    }
}

/////////////////// PIECES ////////////////////

static void new_piece(struct game_state *gs, bool should_announce) {
    gs->piece_x = 4;
    gs->piece_y = 1;
    gs->piece_rotation = 0;

    gs->piece_symbol = shapes[gs->next_shape_index].symbol;

    if (gs->piece_symbol == 'O') {
        gs->piece_x -= 1;
        gs->piece_y -= 1;
    } else if (gs->piece_symbol == 'I') {
        gs->piece_y -= 1;
    }

    for (int i = 0; i < PIECE_SIZE; ++i) {
        gs->shape_coordinates[i] = shapes[gs->next_shape_index].coordinates[i];
    }

    gs->next_shape_index += 1;
    gs->next_shape_index %= NUM_SHAPES;

    if (piece_intersects_field(gs)) {
        print_screen(gs);
        printf("Game over :[\n");
        if (gs->score > gs->best_score) gs->best_score = gs->score;
        printf("Final score: %d   Best: %d\n", gs->score, gs->best_score);
        gs->game_running = false;
    } else if (should_announce) {
        printf("A new piece has appeared: ");
        const char *col = color_for_symbol(gs, gs->piece_symbol);
        if (*col) {
            printf("%s%c" ANSI_RESET "\n", col, gs->piece_symbol);
        } else {
            printf("%c\n", gs->piece_symbol);
        }
    }
}

static void place_piece(struct game_state *gs) {
    for (int i = 0; i < PIECE_SIZE; ++i) {
        int row = gs->shape_coordinates[i].y + gs->piece_y;
        int col = gs->shape_coordinates[i].x + gs->piece_x;
        gs->field[row][col] = gs->piece_symbol;
    }

    consume_lines(gs);
    new_piece(gs, /* should_announce = */ true);
}

static void consume_lines(struct game_state *gs) {
    int lines_cleared = 0;

    for (int row = FIELD_HEIGHT - 1; row >= 0; --row) {
        bool line_is_full = true;
        for (int col = 0; col < FIELD_WIDTH; ++col) {
            if (gs->field[row][col] == EMPTY) {
                line_is_full = false;
                break;
            }
        }

        if (!line_is_full) {
            continue;
        }

        for (int row_to_copy = row; row_to_copy >= 0; --row_to_copy) {
            for (int col = 0; col < FIELD_WIDTH; ++col) {
                if (row_to_copy != 0) {
                    gs->field[row_to_copy][col] = gs->field[row_to_copy - 1][col];
                } else {
                    gs->field[row_to_copy][col] = EMPTY;
                }
            }
        }

        row++;
        lines_cleared++;
        gs->lines++;
        gs->level = gs->starting_level + gs->lines / 10;
    }

    if (lines_cleared > 0) {
        int points = compute_points_for_clear(lines_cleared);
        gs->score += points;
        if (gs->score > gs->best_score) gs->best_score = gs->score;

        if (lines_cleared == 4) {
            if (gs->use_color) {
                printf("\n\033[1;93m*** TETRIS! ***" ANSI_RESET "\n\n");
            } else {
                printf("\n*** TETRIS! ***\n\n");
            }
        }
    }
}

// Modern Tetris scoring for line clears (before level multiplier):
//   Single = 100, Double = 300, Triple = 500, Tetris = 800.
static int compute_points_for_clear(int lines_cleared) {
    switch (lines_cleared) {
        case 1: return 100;
        case 2: return 300;
        case 3: return 500;
        case 4: return 800;
        default: return 0;
    }
}

/////////////////// MOVEMENT ////////////////////

static void rotate_right(struct game_state *gs) {
    gs->piece_rotation++;
    for (int i = 0; i < PIECE_SIZE; ++i) {
        int temp = gs->shape_coordinates[i].x;
        gs->shape_coordinates[i].x = -gs->shape_coordinates[i].y;
        gs->shape_coordinates[i].y = temp;
    }
    if (gs->piece_symbol == 'I' || gs->piece_symbol == 'O') {
        for (int i = 0; i < PIECE_SIZE; ++i) {
            gs->shape_coordinates[i].x += 1;
        }
    }
}

static void rotate_left(struct game_state *gs) {
    rotate_right(gs);
    rotate_right(gs);
    rotate_right(gs);
}

static bool move_piece(struct game_state *gs, int dx, int dy) {
    gs->piece_x += dx;
    gs->piece_y += dy;
    if (piece_intersects_field(gs)) {
        gs->piece_x -= dx;
        gs->piece_y -= dy;
        return false;
    }
    return true;
}

static bool piece_intersects_field(const struct game_state *gs) {
    for (int i = 0; i < PIECE_SIZE; ++i) {
        int x = gs->shape_coordinates[i].x + gs->piece_x;
        int y = gs->shape_coordinates[i].y + gs->piece_y;
        if (x < 0 || x >= FIELD_WIDTH) return true;
        if (y < 0 || y >= FIELD_HEIGHT) return true;
        if (gs->field[y][x] != EMPTY) return true;
    }
    return false;
}

static const struct coordinate *piece_hit_test(
    const struct coordinate coordinates[PIECE_SIZE],
    int piece_x, int piece_y, int row, int col)
{
    for (int i = 0; i < PIECE_SIZE; ++i) {
        if (coordinates[i].x + piece_x == col &&
            coordinates[i].y + piece_y == row) {
            return coordinates + i;
        }
    }
    return NULL;
}

// How many rows the current piece can fall before locking. Used to
// render the ghost overlay and (in effect) to answer "where will this
// piece land?".
static int ghost_drop_distance(const struct game_state *gs) {
    struct game_state copy = *gs;
    int dist = 0;
    while (move_piece(&copy, 0, 1)) dist++;
    return dist;
}

/////////////////// RENDERING ////////////////////

// Draw a single cell of the NEXT preview (4x4 cells, piece centred).
static void print_next_preview_cell(const struct game_state *gs, int row, int col) {
    const struct shape *s = &shapes[gs->next_shape_index];

    int min_x = s->coordinates[0].x, max_x = s->coordinates[0].x;
    int min_y = s->coordinates[0].y, max_y = s->coordinates[0].y;
    for (int i = 1; i < PIECE_SIZE; ++i) {
        if (s->coordinates[i].x < min_x) min_x = s->coordinates[i].x;
        if (s->coordinates[i].x > max_x) max_x = s->coordinates[i].x;
        if (s->coordinates[i].y < min_y) min_y = s->coordinates[i].y;
        if (s->coordinates[i].y > max_y) max_y = s->coordinates[i].y;
    }

    int w = max_x - min_x + 1;
    int h = max_y - min_y + 1;
    int off_x = (NEXT_BOX_W - w) / 2;
    int off_y = (NEXT_BOX_H - h) / 2;

    for (int i = 0; i < PIECE_SIZE; ++i) {
        int cx = s->coordinates[i].x - min_x + off_x;
        int cy = s->coordinates[i].y - min_y + off_y;
        if (cx == col && cy == row) {
            put_cell_filled(gs, s->symbol, false);
            return;
        }
    }
    fputs("  ", stdout);
}

// Print a ruler of N copies of BOX_H (used to draw a titled top border).
static void print_hline(int n) {
    for (int i = 0; i < n; ++i) fputs(BOX_H, stdout);
}

// Print N spaces.
static void print_spaces(int n) {
    for (int i = 0; i < n; ++i) putchar(' ');
}

// Visual (column) width of a UTF-8 string. The characters used in this
// program (ASCII, box-drawing ╔╗╚╝═║, full block █) are all 1 column
// wide, so we can just count Unicode codepoints — i.e. every byte that
// isn't a UTF-8 continuation byte (0b10xxxxxx).
static int visual_width(const char *s) {
    int w = 0;
    while (*s) {
        if (((unsigned char)*s++ & 0xC0) != 0x80) ++w;
    }
    return w;
}

// Current terminal width in columns; falls back to 0 if unknown (e.g.
// stdout is not a TTY). Callers should treat 0 as "don't center".
static int term_cols(void) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) {
        return ws.ws_col;
    }
    return 0;
}

// Left padding to horizontally center `content_w` columns inside the
// current terminal. Returns 0 if the terminal isn't wide enough or its
// width can't be detected (so output stays flush-left for tests / pipes).
static int center_pad(int content_w) {
    int t = term_cols();
    if (t <= content_w) return 0;
    return (t - content_w) / 2;
}

// Width budget for one rendered "game block" line:
//   HUD column (LEFT)          : 14 chars  (e.g. "TIME : 00:00  ")
//   gap                        :  4 chars
//   field (border + 20 + border):22 chars
//   gap                        :  4 chars
//   right column (NEXT or hint):  e.g. "Press '?' for help" = 18, NEXT box = 14
//                              -> we reserve 18 for the right column
// Total = 14 + 4 + 22 + 4 + 18 = 62 chars
#define HUD_COL_W       14
#define GAP_W            4
#define FIELD_BLOCK_W   (1 + FIELD_WIDTH * 2 + 1)         // 22
#define RIGHT_COL_W     18
#define GAME_BLOCK_W    (HUD_COL_W + GAP_W + FIELD_BLOCK_W + GAP_W + RIGHT_COL_W)

// Print one HUD label, left-justified, padded to HUD_COL_W visual cols.
// `text == NULL` prints an empty HUD cell.
static void print_hud_cell(const char *text) {
    if (!text) {
        print_spaces(HUD_COL_W);
        return;
    }
    int len = (int)strlen(text);
    if (len > HUD_COL_W) len = HUD_COL_W;
    fwrite(text, 1, (size_t)len, stdout);
    print_spaces(HUD_COL_W - len);
}

// Build the HUD label text for a given visible row (row 0 = title row,
// rows 1..FIELD_HEIGHT = field rows, row FIELD_HEIGHT+1 = bottom border).
// Returns NULL if no HUD content sits on that row.
static const char *hud_label_for_row(const struct game_state *gs,
                                     int visible_row, char *buf, size_t bufsz) {
    time_t now = time(NULL);
    long elapsed = (gs->start_time != 0) ? (long)(now - gs->start_time) : 0;
    if (elapsed < 0) elapsed = 0;

    switch (visible_row) {
        case 0:  snprintf(buf, bufsz, "SCORE: %d", gs->score);     return buf;
        case 1:  snprintf(buf, bufsz, "BEST : %d", gs->best_score); return buf;
        case 3:  snprintf(buf, bufsz, "LEVEL: %d", gs->level);      return buf;
        case 4:  snprintf(buf, bufsz, "LINES: %d", gs->lines);      return buf;
        case 5:  snprintf(buf, bufsz, "TIME : %02ld:%02ld",
                          elapsed / 60, elapsed % 60);              return buf;
        default: return NULL;
    }
}

// Render the right-column content for a given visible row. Emits exactly
// RIGHT_COL_W visual columns. The NEXT box occupies the top of the
// column; the help hint sits near the bottom.
static void print_right_col(const struct game_state *gs, int visible_row) {
    // NEXT box top border lives on row 0 alongside the field title row.
    if (visible_row == 0) {
        // "╔═══ NEXT ═══╗" -> 1 + 3 + 6 + 3 + 1 = 14 visual cols
        fputs(BOX_TL, stdout);
        print_hline(3); fputs(" NEXT ", stdout); print_hline(3);
        fputs(BOX_TR, stdout);
        print_spaces(RIGHT_COL_W - 14);
        return;
    }
    // Rows 1..NEXT_BOX_H render the 4 preview rows; row NEXT_BOX_H+1 is
    // the NEXT box bottom border.
    if (visible_row >= 1 && visible_row <= NEXT_BOX_H) {
        fputs(BOX_V, stdout);
        print_spaces(2);
        for (int c = 0; c < NEXT_BOX_W; ++c) {
            print_next_preview_cell(gs, visible_row - 1, c);
        }
        print_spaces(2);
        fputs(BOX_V, stdout);
        print_spaces(RIGHT_COL_W - 14);
        return;
    }
    if (visible_row == NEXT_BOX_H + 1) {
        fputs(BOX_BL, stdout);
        print_hline(12);
        fputs(BOX_BR, stdout);
        print_spaces(RIGHT_COL_W - 14);
        return;
    }
    // "Press '?' for help" sits two rows above the field bottom (the
    // bottom border is on visible_row == FIELD_HEIGHT + 1, so the hint
    // row is FIELD_HEIGHT - 1).
    if (visible_row == FIELD_HEIGHT - 1) {
        const char *hint = "Press '?' for help";
        fputs(hint, stdout);
        int w = visual_width(hint);
        if (w < RIGHT_COL_W) print_spaces(RIGHT_COL_W - w);
        return;
    }
    print_spaces(RIGHT_COL_W);
}

// Render the whole screen: HUD column (LEFT) + titled field (CENTER) +
// NEXT preview box and help hint (RIGHT). The HELP/settings panel
// itself is rendered separately when '?' is pressed.
static void print_screen(const struct game_state *gs) {
    int ghost_dy = 0;
    if (gs->show_ghost && gs->game_running && !gs->show_ready) {
        ghost_dy = ghost_drop_distance(gs);
    }

    int pad = center_pad(GAME_BLOCK_W);
    char hud_buf[32];

    // ---- Title row (visible_row 0) ----
    // HUD: SCORE | field top border "╔══════ TETRIS ══════╗" | NEXT top
    putchar('\n');
    print_spaces(pad);
    print_hud_cell(hud_label_for_row(gs, 0, hud_buf, sizeof hud_buf));
    print_spaces(GAP_W);
    fputs(BOX_TL, stdout);
    print_hline(6); fputs(" TETRIS ", stdout); print_hline(6);
    fputs(BOX_TR, stdout);
    print_spaces(GAP_W);
    print_right_col(gs, 0);
    putchar('\n');

    // ---- Field rows (visible_row 1..FIELD_HEIGHT) ----
    for (int row = 0; row < FIELD_HEIGHT; ++row) {
        int visible_row = row + 1;
        print_spaces(pad);
        print_hud_cell(hud_label_for_row(gs, visible_row, hud_buf, sizeof hud_buf));
        print_spaces(GAP_W);

        // Field interior.
        fputs(BOX_V, stdout);
        if ((gs->paused || gs->show_ready) && row == 11) {
            const char *banner = gs->paused ? "PAUSED" : "READY!";
            // "       BANNER       " -> 7 + 6 + 7 = 20 chars
            print_spaces(7);
            if (gs->use_color) fputs(ANSI_BOLD, stdout);
            fputs(banner, stdout);
            if (gs->use_color) fputs(ANSI_RESET, stdout);
            print_spaces(7);
        } else {
            for (int col = 0; col < FIELD_WIDTH; ++col) {
                bool is_current = piece_hit_test(gs->shape_coordinates,
                                                 gs->piece_x, gs->piece_y,
                                                 row, col) != NULL;
                bool is_ghost_cell = false;
                if (!is_current && ghost_dy > 0) {
                    is_ghost_cell = piece_hit_test(gs->shape_coordinates,
                                                   gs->piece_x,
                                                   gs->piece_y + ghost_dy,
                                                   row, col) != NULL;
                }

                if (is_current) {
                    put_cell_filled(gs, gs->piece_symbol, false);
                } else if (is_ghost_cell) {
                    put_cell_filled(gs, gs->piece_symbol, true);
                } else {
                    char c = gs->field[row][col];
                    if (c == EMPTY) {
                        put_cell_empty(gs);
                    } else {
                        put_cell_filled(gs, c, false);
                    }
                }
            }
        }
        fputs(BOX_V, stdout);

        print_spaces(GAP_W);
        print_right_col(gs, visible_row);
        putchar('\n');
    }

    // ---- Bottom border row (visible_row FIELD_HEIGHT + 1) ----
    print_spaces(pad);
    print_hud_cell(NULL);
    print_spaces(GAP_W);
    fputs(BOX_BL, stdout);
    for (int c = 0; c < FIELD_WIDTH; ++c) fputs(BOX_H BOX_H, stdout);
    fputs(BOX_BR, stdout);
    print_spaces(GAP_W);
    print_right_col(gs, FIELD_HEIGHT + 1);
    putchar('\n');
}

// Title art shown on the load screen above the playing field.
// All glyphs (█, box-drawing chars, spaces) are 1 visual column wide.
static const char *const TETRIS_TITLE_ART[] = {
    "█████████╗ ████████╗ █████████╗ ████████╗   ███╗  ████████╗",
    "╚══███╔══╝ ███╔════╝ ╚══███╔══╝ ███╔══███╗  ███║  ███╔════╝",
    "   ███║    ███████╗     ███║    ████████╔╝  ███║  ████████╗",
    "   ███║    ███╔═══╝     ███║    ███╔══███╗  ███║  ╚════███║",
    "   ███║    ████████╗    ███║    ███║  ███║  ███║  ████████║",
    "   ╚══╝    ╚═══════╝    ╚══╝    ╚══╝  ╚══╝  ╚══╝  ╚═══════╝",
};
#define TETRIS_TITLE_ROWS ((int)(sizeof(TETRIS_TITLE_ART) / sizeof(TETRIS_TITLE_ART[0])))

// Render the load screen: TETRIS title art + "PRESS ANY KEY TO START"
// + the empty playing field with the READY! banner. Both blocks are
// horizontally centered to the current terminal width independently
// (the title art is wider than the game block on most terminals).
static void print_title_screen(const struct game_state *gs) {
    // Find the widest title row so the whole block centers as a unit.
    int title_w = 0;
    for (int i = 0; i < TETRIS_TITLE_ROWS; ++i) {
        int w = visual_width(TETRIS_TITLE_ART[i]);
        if (w > title_w) title_w = w;
    }
    int title_pad = center_pad(title_w);

    putchar('\n');
    for (int i = 0; i < TETRIS_TITLE_ROWS; ++i) {
        print_spaces(title_pad);
        if (gs->use_color) fputs(ANSI_BOLD, stdout);
        fputs(TETRIS_TITLE_ART[i], stdout);
        if (gs->use_color) fputs(ANSI_RESET, stdout);
        putchar('\n');
    }
    putchar('\n');

    const char *prompt = "PRESS ANY KEY TO START";
    int prompt_pad = center_pad((int)strlen(prompt));
    print_spaces(prompt_pad);
    if (gs->use_color) fputs(ANSI_BOLD, stdout);
    fputs(prompt, stdout);
    if (gs->use_color) fputs(ANSI_RESET, stdout);
    putchar('\n');

    // print_screen handles its own centering for the game block.
    print_screen(gs);
}

// HELP overlay (controls + current settings). Shown when '?' is pressed
// in non-debug mode. Keeps the "HELP", "Controls:", and "QUIT" markers
// intact so scripted consumers (and tests) can still grep for them.
static void print_help_menu(const struct game_state *gs) {
    putchar('\n');
    fputs(BOX_TL, stdout);
    print_hline(13); fputs(" HELP ", stdout); print_hline(13);
    fputs(BOX_TR, stdout);
    putchar('\n');

    // Each inner line is exactly 32 visual columns wide. The arrow
    // glyphs (←, →, ↑, ↓) are 1 column each but 3 bytes in UTF-8 — the
    // spacing below is hand-tuned, do not run snprintf padding over it.
    static const char *const CONTROL_LINES[] = {
        " Controls:                      ",
        "                                ",
        "   MOVE              A/D, ← →   ",
        "   ROTATE            W, ↑       ",
        "   SOFT DROP         S, ↓       ",
        "   HARD DROP         SPACE      ",
        "                                ",
        "   PAUSE/RESUME      P          ",
        "   HELP              ?          ",
        "   QUIT              Q          ",
        "                                ",
        "   TOGGLE COLOR      C          ",
        "   TOGGLE BLOCK      B          ",
        "   TOGGLE GRID       G          ",
        "   SHOW GHOST PIECE  H          ",
        "                                ",
    };
    for (size_t i = 0; i < sizeof CONTROL_LINES / sizeof CONTROL_LINES[0]; ++i) {
        fputs(BOX_V, stdout);
        fputs(CONTROL_LINES[i], stdout);
        fputs(BOX_V "\n", stdout);
    }

    // Settings block: ASCII only, so %-32s padding is correct.
    char buf[64];
    snprintf(buf, sizeof buf, " Settings:");
    printf(BOX_V "%-32s" BOX_V "\n", buf);
    snprintf(buf, sizeof buf, "   Color  (c)   %s", gs->use_color ? "ON" : "OFF");
    printf(BOX_V "%-32s" BOX_V "\n", buf);
    snprintf(buf, sizeof buf, "   Block  (b)   %s", symbol_name(gs->symbol_style));
    printf(BOX_V "%-32s" BOX_V "\n", buf);
    snprintf(buf, sizeof buf, "   Grid   (g)   %s", grid_name(gs->grid_style));
    printf(BOX_V "%-32s" BOX_V "\n", buf);
    snprintf(buf, sizeof buf, "   Ghost  (h)   %s", gs->show_ghost ? "ON" : "OFF");
    printf(BOX_V "%-32s" BOX_V "\n", buf);

    fputs(BOX_BL, stdout);
    print_hline(32);
    fputs(BOX_BR, stdout);
    putchar('\n');
}

// The debug view still uses letters so `?` output stays easy to grep.
static void show_debug_info(const struct game_state *gs) {
    printf("\n");
    printf("next_shape_index = %d\n", gs->next_shape_index);
    printf("piece_symbol     = %d\n", gs->piece_symbol);
    printf("piece_x          = %d\n", gs->piece_x);
    printf("piece_y          = %d\n", gs->piece_y);
    printf("game_running     = %d\n", gs->game_running);
    printf("piece_rotation   = %d\n", gs->piece_rotation);
    printf("level            = %d\n", gs->level);
    printf("starting_level   = %d\n", gs->starting_level);
    printf("score            = %d\n", gs->score);
    printf("best_score       = %d\n", gs->best_score);
    printf("lines            = %d\n", gs->lines);
    printf("theme            = %s\n", theme_name(gs->theme));
    printf("symbol_style     = %s\n", symbol_name(gs->symbol_style));
    printf("grid_style       = %s\n", grid_name(gs->grid_style));
    printf("show_ghost       = %d\n", gs->show_ghost);

    for (int i = 0; i < PIECE_SIZE; ++i) {
        printf("coordinates[%d]   = { %d, %d }\n", i,
               gs->shape_coordinates[i].x, gs->shape_coordinates[i].y);
    }

    printf("\nField:\n");
    for (int row = 0; row < FIELD_HEIGHT; ++row) {
        if (row < 10) putchar(' ');
        printf("%d:  ", row);
        for (int col = 0; col < FIELD_WIDTH; ++col) {
            printf("%d %c ", gs->field[row][col], gs->field[row][col]);
        }
        putchar('\n');
    }
    putchar('\n');
}

/////////////////// INPUT ////////////////////

static char read_char(void) {
    char command;
    if (scanf(" %c", &command) == 1) {
        return command;
    }
    exit(1);
}

/////////////////// GAME LOOP ////////////////////

// Apply one command. Returns false if the game should quit.
static bool handle_command(struct game_state *gs, char command) {
    // Rotate. W is the documented key; arrow-up translates to 'w'. The
    // legacy aliases (r, R, Z, z, W) are kept so older fixtures still
    // drive the game even though only one rotate direction exists now.
    if (command == 'w' || command == 'W' || command == 'r' ||
        command == 'R' || command == 'z' || command == 'Z') {
        rotate_right(gs);
        if (piece_intersects_field(gs)) rotate_left(gs);  // undo on collision
    }
    else if (command == 'n') {
        new_piece(gs, /* should_announce = */ false);
    }
    // Soft drop: s (and arrow down translates to 's').
    else if (command == 's') {
        if (move_piece(gs, 0, 1)) {
            gs->score += 1;           // +1 per row dropped
        } else {
            place_piece(gs);
        }
    }
    // Hard drop: SPACE (documented). 'S' kept as a legacy alias.
    else if (command == ' ' || command == 'S') {
        int dist = 0;
        while (move_piece(gs, 0, 1)) dist++;
        gs->score += 2 * dist;        // +2 per row dropped
        place_piece(gs);
    }
    else if (command == 'a' || command == 'A') {
        move_piece(gs, -1, 0);
    }
    else if (command == 'd' || command == 'D') {
        move_piece(gs, 1, 0);
    }
    else if (command == 'p' || command == 'P') {
        // Pause: the TTY loop draws the PAUSED banner and waits for any
        // key. In scripted mode there's no concept of "wait", so just
        // toggle the flag (it's harmless on the next redraw) and note it.
        gs->paused = true;
        printf("Paused.\n");
    }
    // Settings: toggle color / cycle block style / cycle grid / toggle ghost.
    else if (command == 'c' || command == 'C') {
        gs->use_color = !gs->use_color;
        printf("Color: %s\n", gs->use_color ? "ON" : "OFF");
    }
    else if (command == 'b' || command == 'B') {
        gs->symbol_style = (gs->symbol_style + 1) % SYMBOL_COUNT;
        printf("Block: %s\n", symbol_name(gs->symbol_style));
    }
    else if (command == 'g' || command == 'G') {
        gs->grid_style = (gs->grid_style + 1) % GRID_COUNT;
        printf("Grid: %s\n", grid_name(gs->grid_style));
    }
    else if (command == 'h' || command == 'H') {
        gs->show_ghost = !gs->show_ghost;
        printf("Ghost: %s\n", gs->show_ghost ? "ON" : "OFF");
    }
    else if (command == '?') {
        if (gs->debug_mode) {
            show_debug_info(gs);
        } else {
            // Non-debug mode: render the HELP / settings overlay. The
            // TTY loop also flips show_help so it can wait for any key
            // before resuming the field render.
            gs->show_help = true;
            print_help_menu(gs);
        }
    }
    else if (command == 'q' || command == 'Q') {
        printf("Quitting...\n");
        return false;
    }
    else {
        printf("Unknown command!\n");
    }
    return true;
}

// Scripted loop: blocking scanf, no gravity, no start menu. This is the
// deterministic path that tests and fixtures drive.
static void game_loop(struct game_state *gs) {
    while (gs->game_running) {
        print_screen(gs);
        printf("  > ");
        char command = read_char();
        if (!handle_command(gs, command)) break;
    }
    printf("\nGoodbye!\n");
}

//////////// INTERACTIVE (TTY) LOOP ////////////

// Gravity interval (ms) for a given level. Caps at level 9 so gravity
// never gets pathologically fast to test.
static int gravity_ms_for_level(int level) {
    // Step down from 1000ms at L1 to ~100ms at L9.
    static const int table[] = {
        1000, 850, 720, 600, 500, 420, 340, 260, 180, 120
    };
    if (level < 1) level = 1;
    if (level > 9) level = 9;
    return table[level];  // level index 1..9 maps into 1..9
}

static struct termios saved_termios;
static bool           termios_saved = false;

static void restore_termios(void) {
    if (termios_saved) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved_termios);
        termios_saved = false;
    }
}

static void on_fatal_signal(int sig) {
    fputs("\033[?25h", stdout);  // show cursor
    fflush(stdout);
    restore_termios();
    signal(sig, SIG_DFL);
    raise(sig);
}

static void enable_raw_input(void) {
    if (tcgetattr(STDIN_FILENO, &saved_termios) == -1) return;
    struct termios raw = saved_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    raw.c_cc[VMIN]  = 1;
    raw.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) return;
    termios_saved = true;
    atexit(restore_termios);
    signal(SIGINT,  on_fatal_signal);
    signal(SIGTERM, on_fatal_signal);
}

// Wait up to timeout_ms for one byte on stdin. -1 -> wait forever.
// Returns 1 / 0 (timeout) / -1 (EOF/error).
static int read_byte_timeout(char *out, int timeout_ms) {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    struct timeval tv = { timeout_ms / 1000, (timeout_ms % 1000) * 1000 };
    int r = select(STDIN_FILENO + 1, &fds, NULL, NULL, timeout_ms < 0 ? NULL : &tv);
    if (r < 0)  return -1;
    if (r == 0) return 0;
    ssize_t n = read(STDIN_FILENO, out, 1);
    return (n == 1) ? 1 : -1;
}

static long now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

// Full clear+home — used when the layout changes (HELP overlay opening
// or closing) so leftover lines from the larger frame don't linger.
static void clear_screen(void) {
    fputs("\033[H\033[2J", stdout);
}

// Move cursor to home without clearing. Used for normal frame-to-frame
// redraws so the previous frame is overwritten in place — no flicker.
static void cursor_home(void) {
    fputs("\033[H", stdout);
}

static void hide_cursor(void) { fputs("\033[?25l", stdout); }
static void show_cursor(void) { fputs("\033[?25h", stdout); }

// Translate a key into a normalized command character. Handles the
// ESC [ X arrow-key sequences: up/down/left/right become w/s/d/a.
// Returns 0 if the sequence was consumed but produced no command
// (e.g. partial escape that didn't complete).
static char translate_key(char c) {
    if (c != '\033') return c;
    // Try to read [ + X within a short window so a plain ESC doesn't
    // block the game forever.
    char second;
    if (read_byte_timeout(&second, 50) <= 0) return 0;
    if (second != '[' && second != 'O') return 0;
    char third;
    if (read_byte_timeout(&third, 50) <= 0) return 0;
    switch (third) {
        case 'A': return 'w'; // up    -> rotate right
        case 'B': return 's'; // down  -> soft drop
        case 'C': return 'd'; // right
        case 'D': return 'a'; // left
        default:  return 0;
    }
}

// TTY loop: raw input + level-scaled gravity + smooth in-place redraw.
// No title screen — the game starts immediately on the player's screen.
static void game_loop_interactive(struct game_state *gs) {
    enable_raw_input();
    hide_cursor();

    // ---- Load screen ----
    // Show the TETRIS title art + "PRESS ANY KEY TO START" + the empty
    // playing field with a READY! banner. Block on any key. The clock
    // doesn't start until the player dismisses this screen.
    gs->start_time = 0;
    gs->show_ready = true;
    clear_screen();
    print_title_screen(gs);
    fflush(stdout);
    {
        char c;
        int r = read_byte_timeout(&c, -1);
        if (r <= 0) goto done;
        if (c == 'q' || c == 'Q') goto done;
    }

    // Start the gameplay clock; clear the READY! banner; do a one-time
    // full clear so the title art doesn't leak into the play frame.
    gs->start_time = time(NULL);
    gs->show_ready = false;
    clear_screen();

    long last_tick = now_ms();

    while (gs->game_running) {
        // Overlays change the frame size, so do a full clear; otherwise
        // just home the cursor and overwrite the previous frame in
        // place. This is what removes the flicker.
        if (gs->show_help || gs->paused) {
            clear_screen();
        } else {
            cursor_home();
        }
        print_screen(gs);
        if (gs->show_help) {
            print_help_menu(gs);
            fputs("\n(press any key to resume)\n", stdout);
        } else if (gs->paused) {
            fputs("\n-- PAUSED -- (press any key to resume)\n", stdout);
        }
        fflush(stdout);

        // While the HELP overlay or PAUSED banner is up, block forever
        // until the next key, then dismiss the overlay without consuming
        // the key as a command.
        if (gs->show_help || gs->paused) {
            char c;
            int r = read_byte_timeout(&c, -1);
            if (r <= 0) goto done;
            gs->show_help = false;
            gs->paused    = false;
            if (c == 'q' || c == 'Q') goto done;
            // Overlay dismissed: full clear so the smaller game frame
            // doesn't leave overlay text behind.
            clear_screen();
            last_tick = now_ms();
            continue;
        }

        while (gs->game_running) {
            int gravity_ms = gravity_ms_for_level(gs->level);
            long remaining = (last_tick + gravity_ms) - now_ms();
            if (remaining < 0) remaining = 0;

            char c;
            int r = read_byte_timeout(&c, (int)remaining);
            if (r < 0) {
                goto done;
            } else if (r == 0) {
                // Gravity tick.
                if (!move_piece(gs, 0, 1)) {
                    place_piece(gs);
                }
                last_tick = now_ms();
                break;
            } else {
                char cmd = translate_key(c);
                if (cmd == 0) continue;
                if (cmd == '?') {
                    if (gs->debug_mode) {
                        clear_screen();
                        show_debug_info(gs);
                        fputs("\n(press any key)\n", stdout);
                        fflush(stdout);
                        char dummy;
                        if (read(STDIN_FILENO, &dummy, 1) != 1) goto done;
                        last_tick = now_ms();
                        break;
                    }
                    // Flip the HELP overlay flag and let the outer loop
                    // redraw + block for the dismiss key.
                    gs->show_help = true;
                    last_tick = now_ms();
                    break;
                }
                if (cmd == 'p' || cmd == 'P') {
                    // Same pattern as '?': set the flag, outer loop
                    // draws the PAUSED banner and waits for any key.
                    gs->paused = true;
                    last_tick = now_ms();
                    break;
                }
                if (!handle_command(gs, cmd)) goto done;
                // Manual drop resets the gravity timer so the piece
                // doesn't instantly double-fall.
                if (cmd == 's' || cmd == 'S' || cmd == ' ') {
                    last_tick = now_ms();
                }
                break;
            }
        }
    }

done:
    show_cursor();
    restore_termios();
    printf("\nGoodbye!\n");
}
