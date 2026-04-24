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

//////////////////// Colors /////////////////////
//
// ANSI colors are only emitted when the game is running on an interactive
// terminal AND the NO_COLOR environment variable is unset. In pipe mode
// (redirected stdout, regression tests) colors stay OFF, so the byte-level
// output is identical to the original tetris.c.

#define ANSI_RESET "\033[0m"

// ANSI foreground colour code for each piece, roughly matching the
// "guideline" Tetris palette. Returns "" for unknown symbols / EMPTY.
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

// Prints a single character, optionally wrapped in its piece color escape.
// When `use_color` is false, behaves exactly like putchar(c).
static void put_piece_char(bool use_color, char c) {
    if (use_color && c != EMPTY) {
        const char *col = color_for_symbol(c);
        if (*col != '\0') {
            printf("%s%c%s", col, c, ANSI_RESET);
            return;
        }
    }
    putchar(c);
}

// Should colors be emitted in this run? Yes iff stdout is a terminal
// AND the NO_COLOR env var is unset (https://no-color.org).
static bool should_use_color(void) {
    if (getenv("NO_COLOR") != NULL) {
        return false;
    }
    return isatty(STDOUT_FILENO) != 0;
}

////////////////// Prototypes ///////////////////

// Setup
static void setup_game(struct game_state *gs);
static void setup_field(struct game_state *gs);

// Piece lifecycle
static void new_piece(struct game_state *gs, bool should_announce);
static void place_piece(struct game_state *gs);
static void consume_lines(struct game_state *gs);
static int  compute_points_for_line(const struct game_state *gs, int bonus);

// Piece movement / rotation
static void rotate_right(struct game_state *gs);
static void rotate_left(struct game_state *gs);
static bool move_piece(struct game_state *gs, int dx, int dy);
static bool piece_intersects_field(const struct game_state *gs);
static const struct coordinate *piece_hit_test(
    const struct coordinate coordinates[PIECE_SIZE],
    int piece_x, int piece_y, int row, int col);

// Rendering
static void print_field(const struct game_state *gs);
static void show_debug_info(const struct game_state *gs);

// Input
static void choose_next_shape(struct game_state *gs);
static char read_char(void);

// Game loop / dispatch
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

// Initialises the game state for a fresh game.
static void setup_game(struct game_state *gs) {
    gs->next_shape_index = 0;
    gs->piece_symbol     = EMPTY;
    gs->piece_x          = 0;
    gs->piece_y          = 0;
    gs->piece_rotation   = 0;
    gs->score            = 0;
    gs->game_running     = true;
    gs->use_color        = should_use_color();
    for (int i = 0; i < PIECE_SIZE; ++i) {
        gs->shape_coordinates[i].x = 0;
        gs->shape_coordinates[i].y = 0;
    }
    setup_field(gs);
}

// Initialises the field to all EMPTY.
static void setup_field(struct game_state *gs) {
    for (int row = 0; row < FIELD_HEIGHT; ++row) {
        for (int col = 0; col < FIELD_WIDTH; ++col) {
            gs->field[row][col] = EMPTY;
        }
    }
}

/////////////////// PIECES ////////////////////

// Sets the current piece from the upcoming shape, advances the queue,
// and ends the game if the new piece immediately collides with the field.
static void new_piece(struct game_state *gs, bool should_announce) {
    // Put the piece (roughly) in the top middle.
    gs->piece_x = 4;
    gs->piece_y = 1;
    gs->piece_rotation = 0;

    gs->piece_symbol = shapes[gs->next_shape_index].symbol;

    // The `O` and `I` pieces need a bit of nudging.
    if (gs->piece_symbol == 'O') {
        gs->piece_x -= 1;
        gs->piece_y -= 1;
    } else if (gs->piece_symbol == 'I') {
        gs->piece_y -= 1;
    }

    for (int i = 0; i < PIECE_SIZE; ++i) {
        gs->shape_coordinates[i] = shapes[gs->next_shape_index].coordinates[i];
    }

    // Just cycle through the shapes in order.
    gs->next_shape_index += 1;
    gs->next_shape_index %= NUM_SHAPES;

    if (piece_intersects_field(gs)) {
        print_field(gs);
        printf("Game over :[\n");
        gs->game_running = false;
    } else if (should_announce) {
        printf("A new piece has appeared: ");
        put_piece_char(gs->use_color, gs->piece_symbol);
        putchar('\n');
    }
}

// Fixes the current piece to the field, clears full lines, spawns next piece.
static void place_piece(struct game_state *gs) {
    for (int i = 0; i < PIECE_SIZE; ++i) {
        int row = gs->shape_coordinates[i].y + gs->piece_y;
        int col = gs->shape_coordinates[i].x + gs->piece_x;
        gs->field[row][col] = gs->piece_symbol;
    }

    consume_lines(gs);
    new_piece(gs, /* should_announce = */ true);
}

// Clears any full lines and awards the appropriate points.
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
        gs->score += compute_points_for_line(gs, lines_cleared);
    }
}

// Computes the score obtained from clearing a line, where `bonus` is
// the number of cleared lines up to this line after placing the
// current piece.
static int compute_points_for_line(const struct game_state *gs, int bonus) {
    if (bonus == 4) {
        if (gs->use_color) {
            printf("\n\033[1;93m*** TETRIS! ***" ANSI_RESET "\n\n");
        } else {
            printf("\n*** TETRIS! ***\n\n");
        }
    }

    // Reward clearing multiple lines at the same time.
    return 100 + 40 * (bonus - 1) * (bonus - 1);
}

/////////////////// MOVEMENT ////////////////////

// Rotates the current piece clockwise (right).
static void rotate_right(struct game_state *gs) {
    gs->piece_rotation++;

    for (int i = 0; i < PIECE_SIZE; ++i) {
        // This negate-y-and-swap operation rotates 90 degrees clockwise.
        int temp = gs->shape_coordinates[i].x;
        gs->shape_coordinates[i].x = -gs->shape_coordinates[i].y;
        gs->shape_coordinates[i].y = temp;
    }

    // The `I` and `O` pieces aren't centered on the middle of a cell,
    // and so need a nudge after being rotated.
    if (gs->piece_symbol == 'I' || gs->piece_symbol == 'O') {
        for (int i = 0; i < PIECE_SIZE; ++i) {
            gs->shape_coordinates[i].x += 1;
        }
    }
}

// Rotates the current piece counter-clockwise (left) by rotating it
// clockwise three times. The original implementation does this too —
// kept identical so piece_rotation / coordinate drift match exactly.
static void rotate_left(struct game_state *gs) {
    rotate_right(gs);
    rotate_right(gs);
    rotate_right(gs);
}

// Translates the current piece, only if the new location is valid.
static bool move_piece(struct game_state *gs, int dx, int dy) {
    gs->piece_x += dx;
    gs->piece_y += dy;

    if (piece_intersects_field(gs)) {
        // Reverse the move if it resulted in an invalid position.
        gs->piece_x -= dx;
        gs->piece_y -= dy;
        return false;
    }

    return true;
}

// Checks if the current piece is fully in-bounds and doesn't collide
// with any non-EMPTY part of the field.
static bool piece_intersects_field(const struct game_state *gs) {
    for (int i = 0; i < PIECE_SIZE; ++i) {
        int x = gs->shape_coordinates[i].x + gs->piece_x;
        int y = gs->shape_coordinates[i].y + gs->piece_y;

        if (x < 0 || x >= FIELD_WIDTH) {
            return true;
        }
        if (y < 0 || y >= FIELD_HEIGHT) {
            return true;
        }

        if (gs->field[y][x] != EMPTY) {
            return true;
        }
    }

    return false;
}

// Checks if a piece with a given array of coordinates intersects a point;
// if so returns a pointer to that coordinate, otherwise NULL.
static const struct coordinate *piece_hit_test(
    const struct coordinate coordinates[PIECE_SIZE],
    int piece_x, int piece_y, int row, int col)
{
    for (int i = 0; i < PIECE_SIZE; ++i) {
        if (coordinates[i].x + piece_x == col &&
            coordinates[i].y + piece_y == row) {
            // note that the below line involves *pointer* arithmetic
            return coordinates + i;
        }
    }

    return NULL;
}

/////////////////// RENDERING ////////////////////

// Prints the playing field.
static void print_field(const struct game_state *gs) {
    printf("\n/= Field =\\    SCORE: %d\n", gs->score);

    for (int row = 0; row < FIELD_HEIGHT; ++row) {
        putchar('|');

        for (int col = 0; col < FIELD_WIDTH; ++col) {
            if (piece_hit_test(gs->shape_coordinates,
                               gs->piece_x, gs->piece_y, row, col)) {
                put_piece_char(gs->use_color, gs->piece_symbol);
            } else {
                put_piece_char(gs->use_color, gs->field[row][col]);
            }
        }

        putchar('|');

        if (row == 1) {
            printf("     NEXT: ");
            put_piece_char(gs->use_color, shapes[gs->next_shape_index].symbol);
        }

        putchar('\n');
    }
    printf("\\=========/\n");
}

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
        if (row < 10) {
            putchar(' ');
        }

        printf("%d:  ", row);
        for (int col = 0; col < FIELD_WIDTH; ++col) {
            printf("%d %c ", gs->field[row][col], gs->field[row][col]);
        }
        putchar('\n');
    }

    putchar('\n');
}

/////////////////// INPUT ////////////////////

// Allows the user to override which shape will drop next.
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

// Reads and returns a single character.
static char read_char(void) {
    char command;
    if (scanf(" %c", &command) == 1) {
        return command;
    }
    exit(1);
}

/////////////////// GAME LOOP ////////////////////

// Handles a single command. Returns `false` iff the user has asked to
// quit with 'q'; in every other case returns `true` so the loop
// continues (game-over is signalled separately via `gs->game_running`).
static bool handle_command(struct game_state *gs, char command) {
    if (command == 'r') {
        rotate_right(gs);
        if (piece_intersects_field(gs)) {
            rotate_left(gs);
        }
    } else if (command == 'R') {
        rotate_left(gs);
        if (piece_intersects_field(gs)) {
            rotate_right(gs);
        }
    } else if (command == 'n') {
        new_piece(gs, /* should_announce = */ false);
    } else if (command == 's') {
        if (!move_piece(gs, 0, 1)) {
            place_piece(gs);
        }
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

// The main loop — read commands and dispatch them.
static void game_loop(struct game_state *gs) {
    while (gs->game_running) {
        print_field(gs);

        printf("  > ");
        char command = read_char();

        if (!handle_command(gs, command)) {
            break;
        }
    }

    printf("\nGoodbye!\n");
}
