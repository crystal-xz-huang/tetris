/**
 * tetris.c
 *
 * A simple game of Tetris.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>

/////////////////// Constants ///////////////////

#define FIELD_WIDTH  9
#define FIELD_HEIGHT 15
#define PIECE_SIZE   4
#define NUM_SHAPES   7

#define EMPTY ' '

// Preview grid dimensions. The widest piece (I) spans 4 cells, and every
// piece fits in 2 rows when rendered from its bounding-box top-left.
#define PREVIEW_W 4
#define PREVIEW_H 2

///////////////////// Types /////////////////////

struct coordinate {
    int x;
    int y;
};

struct shape {
    char symbol;
    struct coordinate coordinates[PIECE_SIZE];
};

struct game_state {
    int  next_shape_index;
    struct coordinate shape_coordinates[PIECE_SIZE];
    char piece_symbol;
    int  piece_x;
    int  piece_y;
    int  piece_rotation;
    int  score;
    int  lines;
    int  level;
    bool game_running;
    bool use_color;
    char field[FIELD_HEIGHT][FIELD_WIDTH];
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

//////////////////// Glyphs /////////////////////

// A "cell" is two characters wide so filled cells look square on a
// typical terminal (glyphs are roughly twice as tall as they are wide).
// CELL_FILLED is used in color mode; in mono mode a cell is the piece
// symbol doubled (see put_cell below), which is also two characters wide.
#define CELL_FILLED "██"   // U+2588 full block, twice
#define CELL_EMPTY  "  "   // two spaces

// Box-drawing chars for the field border (U+2550..U+255D).
#define BOX_H   "═"
#define BOX_V   "║"
#define BOX_TL  "╔"
#define BOX_TR  "╗"
#define BOX_BL  "╚"
#define BOX_BR  "╝"

//////////////////// Colors /////////////////////
//
// ANSI colors are only emitted when the game is running on an interactive
// terminal AND the NO_COLOR environment variable is unset.

#define ANSI_RESET "\033[0m"

static const char *color_for_symbol(char sym) {
    switch (sym) {
        case 'I': return "\033[96m";  // bright cyan
        case 'J': return "\033[94m";  // bright blue
        case 'L': return "\033[33m";  // yellow (warm, orange-ish)
        case 'O': return "\033[93m";  // bright yellow
        case 'S': return "\033[92m";  // bright green
        case 'T': return "\033[95m";  // bright magenta
        case 'Z': return "\033[91m";  // bright red
        default:  return "";
    }
}

// Prints a filled 2-char cell.
//
// Color mode:  a solid "██" block in the piece's color.
// Mono mode:   the piece's letter doubled (e.g. "II", "JJ"). Using the
//              symbol as the glyph keeps cells the same width as the
//              color version, but lets you see where one piece ends and
//              the next begins — two adjacent pieces of different types
//              read as `...IIIIJJJJ...` instead of a single undifferentiated
//              `████████` blob.
//
// EMPTY cells are two spaces regardless of mode.
static void put_cell(bool use_color, char sym) {
    if (sym == EMPTY) {
        fputs(CELL_EMPTY, stdout);
        return;
    }
    if (use_color) {
        const char *col = color_for_symbol(sym);
        if (*col != '\0') {
            printf("%s" CELL_FILLED ANSI_RESET, col);
            return;
        }
        // Unknown symbol: fall through to the solid block.
        fputs(CELL_FILLED, stdout);
        return;
    }
    // Mono: piece letter doubled so pieces stay distinguishable.
    putchar(sym);
    putchar(sym);
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
static int  compute_points_for_line(const struct game_state *gs, int bonus);

static void rotate_right(struct game_state *gs);
static void rotate_left(struct game_state *gs);
static bool move_piece(struct game_state *gs, int dx, int dy);
static bool piece_intersects_field(const struct game_state *gs);
static const struct coordinate *piece_hit_test(
    const struct coordinate coordinates[PIECE_SIZE],
    int piece_x, int piece_y, int row, int col);

static void print_field(const struct game_state *gs);
static void print_next_preview_row(const struct game_state *gs, int preview_row);
static void show_debug_info(const struct game_state *gs);

static void choose_next_shape(struct game_state *gs);
static char read_char(void);

static void game_loop(struct game_state *gs);
static bool handle_command(struct game_state *gs, char command);

/////////////////// MAIN ////////////////////

int main(void) {
    struct game_state gs;

    printf("Welcome to my tetris!\n");

    setup_game(&gs);
    new_piece(&gs, /* should_announce = */ false);
    game_loop(&gs);

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
    gs->lines            = 0;
    gs->level            = 1;
    gs->game_running     = true;
    gs->use_color        = should_use_color();
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
        print_field(gs);
        printf("Game over :[\n");
        gs->game_running = false;
    } else if (should_announce) {
        printf("A new piece has appeared: ");
        if (gs->use_color) {
            printf("%s%c" ANSI_RESET "\n",
                   color_for_symbol(gs->piece_symbol), gs->piece_symbol);
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
        gs->level = gs->lines / 10 + 1;
        gs->score += compute_points_for_line(gs, lines_cleared);
    }
}

static int compute_points_for_line(const struct game_state *gs, int bonus) {
    if (bonus == 4) {
        if (gs->use_color) {
            printf("\n\033[1;93m*** TETRIS! ***" ANSI_RESET "\n\n");
        } else {
            printf("\n*** TETRIS! ***\n\n");
        }
    }
    return 100 + 40 * (bonus - 1) * (bonus - 1);
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

/////////////////// RENDERING ////////////////////

// Renders one row (0 or 1) of the NEXT piece preview. The piece is
// aligned to its bounding-box top-left so every piece shows up in the
// same "cell" of the preview, regardless of where its coordinates sit
// around the origin.
static void print_next_preview_row(const struct game_state *gs, int preview_row) {
    const struct shape *s = &shapes[gs->next_shape_index];

    int min_x = s->coordinates[0].x;
    int min_y = s->coordinates[0].y;
    for (int i = 1; i < PIECE_SIZE; ++i) {
        if (s->coordinates[i].x < min_x) min_x = s->coordinates[i].x;
        if (s->coordinates[i].y < min_y) min_y = s->coordinates[i].y;
    }

    for (int col = 0; col < PREVIEW_W; ++col) {
        bool filled = false;
        for (int i = 0; i < PIECE_SIZE; ++i) {
            if (s->coordinates[i].x - min_x == col &&
                s->coordinates[i].y - min_y == preview_row) {
                filled = true;
                break;
            }
        }
        put_cell(gs->use_color, filled ? s->symbol : EMPTY);
    }
}

// Prints the playing field. Layout per row:
//
//   ║<FIELD_WIDTH * CELL>║    [optional NEXT-panel content]
//
// The NEXT panel lives on rows 1–3 of the field output:
//   row 1 → "NEXT:"
//   row 2 → first preview row
//   row 3 → second preview row
static void print_field(const struct game_state *gs) {
    // Top border
    putchar('\n');
    fputs(BOX_TL, stdout);
    for (int col = 0; col < FIELD_WIDTH; ++col) {
        fputs(BOX_H BOX_H, stdout);
    }
    printf("%s    SCORE: %d\n", BOX_TR, gs->score);

    // Field rows
    for (int row = 0; row < FIELD_HEIGHT; ++row) {
        fputs(BOX_V, stdout);

        for (int col = 0; col < FIELD_WIDTH; ++col) {
            char c;
            if (piece_hit_test(gs->shape_coordinates,
                               gs->piece_x, gs->piece_y, row, col)) {
                c = gs->piece_symbol;
            } else {
                c = gs->field[row][col];
            }
            put_cell(gs->use_color, c);
        }

        fputs(BOX_V, stdout);

        // NEXT panel, positioned alongside specific rows.
        if (row == 1) {
            printf("     NEXT: %c", shapes[gs->next_shape_index].symbol);
        } else if (row == 3) {
            fputs("      ", stdout);
            print_next_preview_row(gs, 0);
        } else if (row == 4) {
            fputs("      ", stdout);
            print_next_preview_row(gs, 1);
        }

        putchar('\n');
    }

    // Bottom border
    fputs(BOX_BL, stdout);
    for (int col = 0; col < FIELD_WIDTH; ++col) {
        fputs(BOX_H BOX_H, stdout);
    }
    fputs(BOX_BR "\n", stdout);
}

// The debug view still uses letters so `?` output stays easy to grep.
static void show_debug_info(const struct game_state *gs) {
    printf("next_shape_index = %d\n", gs->next_shape_index);
    printf("piece_symbol     = %d\n", gs->piece_symbol);
    printf("piece_x          = %d\n", gs->piece_x);
    printf("piece_y          = %d\n", gs->piece_y);
    printf("game_running     = %d\n", gs->game_running);
    printf("piece_rotation   = %d\n", gs->piece_rotation);

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

static void choose_next_shape(struct game_state *gs) {
    printf("Enter new next shape: ");
    char symbol = read_char();

    int i = 0;
    while (i != NUM_SHAPES && shapes[i].symbol != symbol) {
        i++;
    }

    if (i != NUM_SHAPES) {
        gs->next_shape_index = i;
    } else {
        printf("No shape found for %c\n", symbol);
    }
}

static char read_char(void) {
    char command;
    if (scanf(" %c", &command) == 1) {
        return command;
    }
    exit(1);
}

/////////////////// GAME LOOP ////////////////////

static bool handle_command(struct game_state *gs, char command) {
    if (command == 'r') {
        rotate_right(gs);
        if (piece_intersects_field(gs)) rotate_left(gs);
    } else if (command == 'R') {
        rotate_left(gs);
        if (piece_intersects_field(gs)) rotate_right(gs);
    } else if (command == 'n') {
        new_piece(gs, /* should_announce = */ false);
    } else if (command == 's') {
        if (!move_piece(gs, 0, 1)) place_piece(gs);
    } else if (command == 'S') {
        while (move_piece(gs, 0, 1)) {}
        place_piece(gs);
    } else if (command == 'a') {
        move_piece(gs, -1, 0);
    } else if (command == 'd') {
        move_piece(gs, 1, 0);
    } else if (command == 'p') {
        place_piece(gs);
    } else if (command == 'c') {
        choose_next_shape(gs);
    } else if (command == 't') {
        gs->use_color = !gs->use_color;
        printf("Color is now %s.\n", gs->use_color ? "ON" : "OFF");
    } else if (command == '?') {
        show_debug_info(gs);
    } else if (command == 'q') {
        printf("Quitting...\n");
        return false;
    } else {
        printf("Unknown command!\n");
    }
    return true;
}

static void game_loop(struct game_state *gs) {
    while (gs->game_running) {
        print_field(gs);
        printf("  > ");
        char command = read_char();
        if (!handle_command(gs, command)) break;
    }
    printf("\nGoodbye!\n");
}
