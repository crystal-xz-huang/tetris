/**
 * tetris.c (v1)
 *
 * A simple game of Tetris.
 *
 */

#include <stdio.h>
#include <stdlib.h>

/////////////////// Constants ///////////////////

#define FIELD_WIDTH  9
#define FIELD_HEIGHT 15
#define PIECE_SIZE   4
#define NUM_SHAPES   7

#define FALSE 0
#define TRUE  1

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

//////////////////// Globals ////////////////////

struct shape shapes[NUM_SHAPES] = {
    { 'I', { {-1,  0}, { 0,  0}, { 1,  0}, { 2,  0} } },
    { 'J', { {-1, -1}, {-1,  0}, { 0,  0}, { 1,  0} } },
    { 'L', { {-1,  0}, { 0,  0}, { 1,  0}, { 1, -1} } },
    { 'O', { { 0,  0}, { 0,  1}, { 1,  1}, { 1,  0} } },
    { 'S', { { 0,  0}, {-1,  0}, { 0, -1}, { 1, -1} } },
    { 'T', { { 0,  0}, { 0, -1}, {-1,  0}, { 1,  0} } },
    { 'Z', { { 0,  0}, { 1,  0}, { 0, -1}, {-1, -1} } },
};

int next_shape_index = 0;

struct coordinate shape_coordinates[PIECE_SIZE];
char piece_symbol;
int piece_x, piece_y;
int piece_rotation;
int score = 0;

int game_running = TRUE;

char field[FIELD_HEIGHT][FIELD_WIDTH];

////////////////// Prototypes ///////////////////

// Main functions
int main(void);
void rotate_left(void);
int move_piece(int dx, int dy);
int compute_points_for_line(int bonus);
void setup_field(void);
void choose_next_shape(void);
void print_field(void);
struct coordinate *piece_hit_test(struct coordinate coordinates[PIECE_SIZE], int row, int col);
int piece_intersects_field(void);
void rotate_right(void);
void place_piece(void);
void new_piece(int should_announce);
void consume_lines(void);

// Mores
void show_debug_info(void);
void game_loop(void);
char read_char(void);


/////////////////// MAIN ////////////////////

int main(void) {
    printf("Welcome to my tetris!\n");

    setup_field();
    new_piece(/* should_announce = */ FALSE); // Start the game with a new piece.
    game_loop();

    return 0;
}

// Rotates the current piece counter-clocwkise (left)
// by rotating it clockwise three times.
void rotate_left(void) {
    rotate_right();
    rotate_right();
    rotate_right();
}

// Translates the current piece, only if the new location is valid
int move_piece(int dx, int dy) {
    piece_x += dx;
    piece_y += dy;

    if (piece_intersects_field()) {
        // Reverse the move if it resulted in the piece getting into
        // an invalid position.
        piece_x -= dx;
        piece_y -= dy;

        return FALSE;
    }

    return TRUE;
}

/////////////////// SETUP ////////////////////

// Computes the score obtained from clearing a line,
// where `bonus` is the number of cleared lines up to this line after
// placing the current piece.
int compute_points_for_line(int bonus) {
    if (bonus == 4) {
        printf("\n*** TETRIS! ***\n\n");
    }

    // Reward clearing multiple lines at the same time.
    return 100 + 40 * (bonus - 1) * (bonus - 1);
}

// Initialises the field to all EMPTY
void setup_field(void) {
    for (int row = 0; row < FIELD_HEIGHT; ++row) {
        for (int col = 0; col < FIELD_WIDTH; ++col) {
            field[row][col] = EMPTY;
        }
    }
}

// Allows the user to override which shape will drop next.
void choose_next_shape(void) {
    printf("Enter new next shape: ");
    char symbol = read_char();

    int i = 0;
    while (i != NUM_SHAPES && shapes[i].symbol != symbol) {
        i++;
    }

    if (i != NUM_SHAPES) {
        next_shape_index = i;
    } else {
        printf("No shape found for %c\n", symbol);
    }
}

// Print the playing field
void print_field(void) {
    printf("\n/= Field =\\    SCORE: %d\n", score);

    for (int row = 0; row < FIELD_HEIGHT; ++row) {
        putchar('|');

        for (int col = 0; col < FIELD_WIDTH; ++col) {
            if (piece_hit_test(shape_coordinates, row, col)) {
                putchar(piece_symbol);
            } else {
                putchar(field[row][col]);
            }
        }

        putchar('|');

        if (row == 1) {
            printf("     NEXT: %c", shapes[next_shape_index].symbol);
        }

        putchar('\n');
    }
    printf("\\=========/\n");
}

// Checks if a piece with a given array of coordinates intersects with a point,
// and if so returns a pointer to that matching coordinate, otherwise NULL.
struct coordinate *piece_hit_test(struct coordinate coordinates[PIECE_SIZE], int row, int col) {
    for (int i = 0; i < PIECE_SIZE; ++i) {
        if (coordinates[i].x + piece_x == col && coordinates[i].y + piece_y == row) {
            // note that the below line involves *pointer* arithmetic
            return coordinates + i;
        }
    }

    return NULL;
}

// Check if the current piece is fully in-bounds and doesn't collide
// with any non-EMPTY part of the field.
int piece_intersects_field(void) {
    for (int i = 0; i < PIECE_SIZE; ++i) {
        int x = shape_coordinates[i].x + piece_x;
        int y = shape_coordinates[i].y + piece_y;

        if (x < 0 || x >= FIELD_WIDTH) {
            return TRUE;
        }
        if (y < 0 || y >= FIELD_HEIGHT) {
            return TRUE;
        }

        if (field[y][x] != EMPTY) {
            return TRUE;
        }
    }

    return FALSE;
}

// Rotate the current piece clockwise (right).
void rotate_right(void) {
    // The following line is provided in the starter code.
    piece_rotation++;

    for (int i = 0; i < PIECE_SIZE; ++i) {
        // This negate-y-and-swap operation rotates 90 degrees clockwise.
        int temp = shape_coordinates[i].x;
        shape_coordinates[i].x = -shape_coordinates[i].y;
        shape_coordinates[i].y = temp;
    }

    // The `I` and `O` pieces aren't centered on the middle
    // of a cell, and so need a nudge after being rotated.
    if (piece_symbol == 'I' || piece_symbol == 'O') {
        for (int i = 0; i < PIECE_SIZE; ++i) {
            shape_coordinates[i].x += 1;
        }
    }
}

// Handles a block hitting the bottom.
void place_piece(void) {
    for (int i = 0; i < PIECE_SIZE; ++i) {
        int row = shape_coordinates[i].y + piece_y;
        int col = shape_coordinates[i].x + piece_x;
        field[row][col] = piece_symbol;
    }

    consume_lines();
    new_piece(/* should_announce = */ TRUE);
}

// Sets the current piece.
void new_piece(int should_announce) {
    // Put the piece (roughly) in the top middle.
    piece_x = 4;
    piece_y = 1;
    piece_rotation = 0;

    piece_symbol = shapes[next_shape_index].symbol;

    // The `O` and `I` pieces need a bit of nudging.
    if (piece_symbol == 'O') {
        piece_x -= 1;
        piece_y -= 1;
    } else if (piece_symbol == 'I') {
        piece_y -= 1;
    }

    for (int i = 0; i < PIECE_SIZE; ++i) {
        shape_coordinates[i] = shapes[next_shape_index].coordinates[i];
    }

    // Just cycle through the shapes in order.
    next_shape_index += 1;
    next_shape_index %= NUM_SHAPES;

    if (piece_intersects_field()) {
        print_field();
        printf("Game over :[\n");
        game_running = FALSE;
    } else if (should_announce) {
        printf("A new piece has appeared: %c\n", piece_symbol);
    }
}

// Clear any full lines and award the appropriate points.
void consume_lines(void) {
    int lines_cleared = 0;

    for (int row = FIELD_HEIGHT - 1; row >= 0; --row) {
        int line_is_full = TRUE;
        for (int col = 0; col < FIELD_WIDTH; ++col) {
            if (field[row][col] == EMPTY) {
                line_is_full = FALSE;
            }
        }

        if (!line_is_full) {
            continue;
        }

        for (int row_to_copy = row; row_to_copy >= 0; --row_to_copy) {
            for (int col = 0; col < FIELD_WIDTH; ++col) {
                if (row_to_copy != 0) {
                    field[row_to_copy][col] = field[row_to_copy - 1][col];
                } else {
                    field[row_to_copy][col] = EMPTY;
                }
            }
        }

        row++;
        lines_cleared++;
        score += compute_points_for_line(lines_cleared);
    }
}


/////////////////// MAIN LOOP ////////////////////

// A big loop which accepts commands from the user and runs those commands.
void game_loop(void) {
    while (game_running) {
        print_field();

        printf("  > ");
        char command = read_char();

        if (command == 'r') {
            rotate_right();
            if (piece_intersects_field()) {
                rotate_left();
            }
        } else if (command == 'R') {
            rotate_left();
            if (piece_intersects_field()) {
                rotate_right();
            }
        } else if (command == 'n') {
            new_piece(/* should_announce = */ FALSE);
        } else if (command == 's') {
            if (!move_piece(0, 1)) {
                place_piece();
            }
        } else if (command == 'S') {
            while (move_piece(0, 1)) {}
            place_piece();
        } else if (command == 'a') {
            move_piece(-1, 0);
        } else if (command == 'd') {
            move_piece(1, 0);
        } else if (command == 'p') {
            place_piece();
        } else if (command == 'c') {
            choose_next_shape();
        } else if (command == '?') {
            show_debug_info();
        } else if (command == 'q') {
            printf("Quitting...\n");
            break;
        } else {
            printf("Unknown command!\n");
        }
    }

    printf("\nGoodbye!\n");
}

// Reads and returns a single character
char read_char(void) {
    char command;
    if (scanf(" %c", &command) == 1) {
        return command;
    }
    exit(1);
}

/////////////////// DEBUG ////////////////////

void show_debug_info(void) {
    printf("next_shape_index = %d\n", next_shape_index);
    printf("piece_symbol     = %d\n", piece_symbol);
    printf("piece_x          = %d\n", piece_x);
    printf("piece_y          = %d\n", piece_y);
    printf("game_running     = %d\n", game_running);
    printf("piece_rotation   = %d\n", piece_rotation);

    for (int i = 0; i < PIECE_SIZE; ++i) {
        printf("coordinates[%d]   = { %d, %d }\n", i, shape_coordinates[i].x, shape_coordinates[i].y);
    }

    printf("\nField:\n");
    for (int row = 0; row < FIELD_HEIGHT; ++row) {
        if (row < 10) {
            putchar(' ');
        }

        printf("%d:  ", row);
        for (int col = 0; col < FIELD_WIDTH; ++col) {
            printf("%d %c ", field[row][col], field[row][col]);
        }
        putchar('\n');
    }

    putchar('\n');
}