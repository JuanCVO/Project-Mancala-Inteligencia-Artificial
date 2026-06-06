// motor/tests/test_mcts.cpp
// Unit tests for the MCTS engine.
// Run via: cd build && ctest -R MCTSTests --output-on-failure

#include "../src/board.h"
#include "../src/mcts.h"
#include "../src/alphabeta.h"

#include <cstdio>
#include <vector>
#include <algorithm>

static int tests_run    = 0;
static int tests_failed = 0;

#define ASSERT(cond, msg) do {                                                \
    tests_run++;                                                              \
    if (!(cond)) {                                                            \
        tests_failed++;                                                       \
        std::fprintf(stderr, "FAIL [%s:%d] %s\n", __FILE__, __LINE__, (msg));\
    }                                                                         \
} while(0)

static bool is_legal(int move, const Board& b, int side) {
    auto mv = legal_moves(b, side);
    return std::find(mv.begin(), mv.end(), move) != mv.end();
}

// ---------------------------------------------------------------------------
// Test 1: legal move on the initial board (sequential)
// ---------------------------------------------------------------------------
static void test_initial_legal_seq() {
    Board b = initial_board();
    MCTSResult r = mcts_search(b, 0, 200, 1);
    ASSERT(is_legal(r.move, b, 0),   "initial board: must return a legal move");
    ASSERT(r.rollouts == 200,         "initial board: rollout count must equal budget");
    ASSERT(r.win_rate >= 0.0 && r.win_rate <= 1.0,
                                      "initial board: win_rate must be in [0,1]");
    ASSERT(r.tree_depth_avg > 0.0,   "initial board: tree must have non-zero depth");
}

// ---------------------------------------------------------------------------
// Test 2: legal move for player 1
// ---------------------------------------------------------------------------
static void test_player1_legal() {
    Board b = initial_board();
    MCTSResult r = mcts_search(b, 1, 200, 1);
    ASSERT(is_legal(r.move, b, 1), "player 1: must return a legal move");
}

// ---------------------------------------------------------------------------
// Test 3: terminal board returns -1
// ---------------------------------------------------------------------------
static void test_terminal_returns_minus1() {
    Board b{};
    b[STORE_0] = 25;
    b[STORE_1] = 23;  // all pits empty → terminal
    MCTSResult r = mcts_search(b, 0, 100, 1);
    ASSERT(r.move == -1, "terminal board: must return move = -1");
    ASSERT(r.rollouts == 0, "terminal board: must return 0 rollouts");
}

// ---------------------------------------------------------------------------
// Test 4: rollout count equals simulations budget exactly
// ---------------------------------------------------------------------------
static void test_rollout_count_exact() {
    Board b = initial_board();
    for (int sims : {1, 7, 100, 333, 1000}) {
        MCTSResult r = mcts_search(b, 0, sims, 1);
        ASSERT(r.rollouts == sims, "rollout count must equal simulations");
    }
}

// ---------------------------------------------------------------------------
// Test 5: more simulations ⇒ equal or greater average tree depth
// ---------------------------------------------------------------------------
static void test_depth_grows_with_sims() {
    Board b = initial_board();
    MCTSResult r_small = mcts_search(b, 0, 100, 1);
    MCTSResult r_large = mcts_search(b, 0, 2000, 1);
    ASSERT(r_large.tree_depth_avg >= r_small.tree_depth_avg,
           "more simulations should produce equal or deeper tree");
}

// ---------------------------------------------------------------------------
// Test 6: forced single legal move (pit 2 only) on a sparse position
// Position: 0 0 4 0 0 0 5 4 4 4 4 4 4 5 side=0
// Only pit 2 has seeds for player 0 — MCTS must pick it.
// ---------------------------------------------------------------------------
static void test_forced_move() {
    Board b = {0, 0, 4, 0, 0, 0, 5, 4, 4, 4, 4, 4, 4, 5};
    MCTSResult r = mcts_search(b, 0, 300, 1);
    ASSERT(r.move == 2, "forced single legal move must be pit 2");
}

// ---------------------------------------------------------------------------
// Test 7: leaf parallelization produces a legal move
// ---------------------------------------------------------------------------
static void test_parallel_legal() {
    Board b = initial_board();
    for (int t : {2, 4}) {
        MCTSResult r = mcts_search(b, 0, 800, t);
        ASSERT(is_legal(r.move, b, 0), "parallel MCTS must return a legal move");
        ASSERT(r.rollouts == 800,       "parallel MCTS must complete full rollout budget");
    }
}

// ---------------------------------------------------------------------------
// Test 8: seed conservation — all seeds stay on the board during MCTS search
// (MCTS must not mutate the input board)
// ---------------------------------------------------------------------------
static void test_no_board_mutation() {
    Board b = initial_board();
    Board before = b;
    mcts_search(b, 0, 500, 1);
    bool unchanged = (b == before);
    ASSERT(unchanged, "mcts_search must not mutate the input board");
}

// ---------------------------------------------------------------------------
// Test 9: MCTS on a near-terminal position picks a legal move
// Position 4 from suite: 1 0 0 2 0 0 20 4 5 3 2 1 3 7 side=0
// ---------------------------------------------------------------------------
static void test_near_terminal() {
    Board b = {1, 0, 0, 2, 0, 0, 20, 4, 5, 3, 2, 1, 3, 7};
    MCTSResult r = mcts_search(b, 0, 500, 1);
    ASSERT(is_legal(r.move, b, 0), "near-terminal: must return a legal move");
}

// ---------------------------------------------------------------------------
// Test 10: coincidence with Alpha-Beta on an obvious capture position
// Position 7: 0 1 0 0 0 0 10 0 5 0 0 0 0 10 side=0
// Only pit 1 is non-empty for player 0.
// ---------------------------------------------------------------------------
static void test_forced_capture() {
    Board b = {0, 1, 0, 0, 0, 0, 10, 0, 5, 0, 0, 0, 0, 10};
    MCTSResult r = mcts_search(b, 0, 300, 1);
    ASSERT(r.move == 1, "forced single legal move (capture) must be pit 1");
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main() {
    test_initial_legal_seq();
    test_player1_legal();
    test_terminal_returns_minus1();
    test_rollout_count_exact();
    test_depth_grows_with_sims();
    test_forced_move();
    test_parallel_legal();
    test_no_board_mutation();
    test_near_terminal();
    test_forced_capture();

    std::printf("MCTS tests: %d passed, %d failed\n",
                tests_run - tests_failed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
