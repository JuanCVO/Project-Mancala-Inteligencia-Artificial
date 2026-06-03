// motor/tests/test_alphabeta.cpp
// Unit tests for alphabeta.h/.cpp
// Key invariant: Alfa-Beta must produce the SAME optimal move as pure Minimax
// at equal depth. This is the correctness criterion stated in the project spec.

#include "../src/board.h"
#include "../src/alphabeta.h"
#include <cstdio>
#include <vector>

// ---------------------------------------------------------------------------
// Minimal test harness (same style as test_board.cpp)
// ---------------------------------------------------------------------------
static int g_passed = 0;
static int g_failed = 0;

#define CHECK(cond, msg) \
    do { \
        if (cond) { ++g_passed; } \
        else { \
            ++g_failed; \
            std::fprintf(stderr, "FAIL [%s:%d] %s\n", __FILE__, __LINE__, msg); \
        } \
    } while (0)

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Verify AB == Minimax on the given board/side/depth.
static bool ab_equals_minimax(const Board& b, int side, int depth) {
    long long mn = 0, an = 0, ap = 0;
    ABResult mm = minimax_seq(b, side, side, depth, mn);
    ABResult ab = alphabeta_seq(b, side, side, depth, -1000000, 1000000, an, ap);
    return mm.move == ab.move && mm.score == ab.score;
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

// On a fresh board at depth 1, both algorithms must agree.
static void test_ab_vs_minimax_initial_d1() {
    Board b = initial_board();
    CHECK(ab_equals_minimax(b, 0, 1), "AB == Minimax at initial board, depth=1");
    CHECK(ab_equals_minimax(b, 1, 1), "AB == Minimax at initial board, depth=1, side=1");
}

// At depth 2.
static void test_ab_vs_minimax_initial_d2() {
    Board b = initial_board();
    CHECK(ab_equals_minimax(b, 0, 2), "AB == Minimax at initial board, depth=2");
}

// At depth 4 (heavier but still feasible in a test suite).
static void test_ab_vs_minimax_initial_d4() {
    Board b = initial_board();
    CHECK(ab_equals_minimax(b, 0, 4), "AB == Minimax at initial board, depth=4");
}

// Near-terminal position: player0 has one pit left.
static void test_ab_vs_minimax_near_terminal() {
    Board b{};
    b[3]      = 6;    // player0: only pit 3 has seeds
    b[7]      = 4;
    b[8]      = 4;
    b[9]      = 4;
    b[STORE_0] = 10;
    b[STORE_1] = 8;
    CHECK(ab_equals_minimax(b, 0, 3), "AB == Minimax near terminal, depth=3");
    CHECK(ab_equals_minimax(b, 1, 3), "AB == Minimax near terminal, depth=3, side=1");
}

// Parallel AB must agree with sequential AB on move choice.
static void test_par_vs_seq() {
    Board b = initial_board();
    long long n = 0, p = 0;
    ABResult seq = alphabeta_seq(b, 0, 0, 4, -1000000, 1000000, n, p);
    ABResult par = alphabeta_par(b, 0, 4, 2);  // 2 threads
    CHECK(seq.move == par.move, "parallel AB move == sequential AB move, depth=4, 2 threads");
    CHECK(seq.score == par.score, "parallel AB score == sequential AB score, depth=4, 2 threads");
}

// Parallel AB with 4 threads.
static void test_par_vs_seq_4threads() {
    Board b = initial_board();
    long long n = 0, p = 0;
    ABResult seq = alphabeta_seq(b, 0, 0, 4, -1000000, 1000000, n, p);
    ABResult par = alphabeta_par(b, 0, 4, 4);
    CHECK(seq.move == par.move, "parallel AB move == sequential AB move, depth=4, 4 threads");
}

// AB prunes: in a non-trivial tree, prune count > 0.
static void test_pruning_occurs() {
    Board b = initial_board();
    long long n = 0, p = 0;
    alphabeta_seq(b, 0, 0, 5, -1000000, 1000000, n, p);
    CHECK(p > 0, "alpha-beta performs at least one prune at depth=5");
}

// Minimax and AB node counts: AB explores strictly fewer nodes.
static void test_ab_fewer_nodes() {
    Board b = initial_board();
    long long mm_n = 0, ab_n = 0, ab_p = 0;
    minimax_seq(b, 0, 0, 4, mm_n);
    alphabeta_seq(b, 0, 0, 4, -1000000, 1000000, ab_n, ab_p);
    CHECK(ab_n < mm_n, "alpha-beta explores fewer nodes than minimax at depth=4");
}

// Seed conservation through a full AB-driven game.
static void test_game_seed_conservation() {
    Board b    = initial_board();
    int side   = 0;
    int steps  = 0;
    while (!is_terminal(b) && steps < 200) {
        auto mv = legal_moves(b, side);
        if (mv.empty()) { side = 1 - side; continue; }
        long long n = 0, p = 0;
        ABResult r = alphabeta_seq(b, side, side, 3, -1000000, 1000000, n, p);
        side = apply_move(b, r.move, side);
        ++steps;
    }
    int total = 0;
    for (int v : b) total += v;
    CHECK(total == PITS * 2 * INIT_SEEDS, "seed conservation through AB-driven game");
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main() {
    test_ab_vs_minimax_initial_d1();
    test_ab_vs_minimax_initial_d2();
    test_ab_vs_minimax_initial_d4();
    test_ab_vs_minimax_near_terminal();
    test_par_vs_seq();
    test_par_vs_seq_4threads();
    test_pruning_occurs();
    test_ab_fewer_nodes();
    test_game_seed_conservation();

    std::printf("Alpha-Beta tests: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed > 0 ? 1 : 0;
}
