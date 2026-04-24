#define main tetris_program_main
#include "../tetris.c"
#undef main

#include <stdio.h>

static int tests_run = 0;
static int tests_failed = 0;

#define EXPECT_TRUE(expr) do { \
    tests_run++; \
    if (!(expr)) { \
        tests_failed++; \
        printf("FAIL %s:%d: expected %s\n", __FILE__, __LINE__, #expr); \
    } \
} while (0)

#define EXPECT_EQ_INT(actual, expected) do { \
    int actual_value = (actual); \
    int expected_value = (expected); \
    tests_run++; \
    if (actual_value != expected_value) { \
        tests_failed++; \
        printf("FAIL %s:%d: expected %s == %d, got %d\n", \
               __FILE__, __LINE__, #actual, expected_value, actual_value); \
    } \
} while (0)

static void test_first_piece_starts_in_expected_position(void) {
    struct game_state gs;

    setup_game(&gs);
    new_piece(&gs, false);

    EXPECT_EQ_INT(gs.piece_symbol, 'I');
    EXPECT_EQ_INT(gs.next_shape_index, 1);
    EXPECT_EQ_INT(gs.piece_x, 4);
    EXPECT_EQ_INT(gs.piece_y, 0);
    EXPECT_TRUE(gs.game_running);
    EXPECT_TRUE(!piece_intersects_field(&gs));
}

static void test_piece_cannot_move_past_left_wall(void) {
    struct game_state gs;

    setup_game(&gs);
    new_piece(&gs, false);

    EXPECT_TRUE(move_piece(&gs, -1, 0));
    EXPECT_TRUE(move_piece(&gs, -1, 0));
    EXPECT_TRUE(move_piece(&gs, -1, 0));
    EXPECT_TRUE(!move_piece(&gs, -1, 0));
    EXPECT_EQ_INT(gs.piece_x, 1);
}

static void test_soft_drop_places_piece_at_bottom(void) {
    struct game_state gs;

    setup_game(&gs);
    new_piece(&gs, false);

    int moves = 0;
    while (move_piece(&gs, 0, 1)) {
        moves++;
    }
    place_piece(&gs);

    EXPECT_EQ_INT(moves, FIELD_HEIGHT - 1);
    EXPECT_EQ_INT(gs.field[FIELD_HEIGHT - 1][3], 'I');
    EXPECT_EQ_INT(gs.field[FIELD_HEIGHT - 1][4], 'I');
    EXPECT_EQ_INT(gs.field[FIELD_HEIGHT - 1][5], 'I');
    EXPECT_EQ_INT(gs.field[FIELD_HEIGHT - 1][6], 'I');
    EXPECT_EQ_INT(gs.piece_symbol, 'J');
}

static void test_line_clear_scores_and_compacts_rows(void) {
    struct game_state gs;

    setup_game(&gs);
    for (int col = 0; col < FIELD_WIDTH; col++) {
        gs.field[FIELD_HEIGHT - 1][col] = 'Z';
    }
    gs.field[FIELD_HEIGHT - 2][0] = 'T';

    consume_lines(&gs);

    EXPECT_EQ_INT(gs.score, 100);
    EXPECT_EQ_INT(gs.field[FIELD_HEIGHT - 1][0], 'T');
    for (int col = 1; col < FIELD_WIDTH; col++) {
        EXPECT_EQ_INT(gs.field[FIELD_HEIGHT - 1][col], EMPTY);
    }
    for (int col = 0; col < FIELD_WIDTH; col++) {
        EXPECT_EQ_INT(gs.field[0][col], EMPTY);
    }
}

static void test_next_shape_index_controls_new_piece(void) {
    struct game_state gs;

    setup_game(&gs);
    gs.next_shape_index = 4;
    new_piece(&gs, false);

    EXPECT_EQ_INT(gs.piece_symbol, 'S');
    EXPECT_EQ_INT(gs.next_shape_index, 5);
}

int main(void) {
    test_first_piece_starts_in_expected_position();
    test_piece_cannot_move_past_left_wall();
    test_soft_drop_places_piece_at_bottom();
    test_line_clear_scores_and_compacts_rows();
    test_next_shape_index_controls_new_piece();

    if (tests_failed != 0) {
        printf("%d/%d tests failed\n", tests_failed, tests_run);
        return 1;
    }

    printf("%d tests passed\n", tests_run);
    return 0;
}
