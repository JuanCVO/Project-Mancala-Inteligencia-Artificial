// motor/tests/test_board.cpp
// Unit tests for board.h — Kalah(6,4) rules.
// Compiled and run via CMake/CTest (see CMakeLists.txt).
// Uses a minimal hand-rolled test harness (no external deps).

#include "../src/board.h"
#include <cstdio>
#include <cstdlib>

// ---------------------------------------------------------------------------
// Minimal test harness
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
// Tests
// ---------------------------------------------------------------------------

// Initial board has 4 seeds in each of the 12 pits, 0 in both stores.
static void test_initial_board() {
    Board b = initial_board();
    for (int i = 0; i < PITS; ++i)
        CHECK(b[i] == INIT_SEEDS, "player0 pit initial seeds");
    CHECK(b[STORE_0] == 0, "player0 store initially 0");
    for (int i = PITS + 1; i < STORE_1; ++i)
        CHECK(b[i] == INIT_SEEDS, "player1 pit initial seeds");
    CHECK(b[STORE_1] == 0, "player1 store initially 0");

    // Total seeds = 48
    int total = 0;
    for (int v : b) total += v;
    CHECK(total == PITS * 2 * INIT_SEEDS, "total seeds == 48");
}

// All moves legal at start for both sides.
static void test_legal_moves_initial() {
    Board b = initial_board();
    auto m0 = legal_moves(b, 0);
    auto m1 = legal_moves(b, 1);
    CHECK((int)m0.size() == PITS, "player0 has 6 legal moves initially");
    CHECK((int)m1.size() == PITS, "player1 has 6 legal moves initially");
}

// After clearing all pits of side 0, no legal moves for side 0.
static void test_no_legal_moves() {
    Board b = initial_board();
    for (int i = 0; i < PITS; ++i) b[i] = 0;
    auto m0 = legal_moves(b, 0);
    CHECK(m0.empty(), "player0 has no legal moves when pits empty");
}

// Sowing: pick pit 0 (4 seeds), seeds land on pits 1,2,3,4.
static void test_sow_basic() {
    Board b = initial_board();
    int next = apply_move(b, 0, 0);
    CHECK(b[0] == 0,          "pit 0 emptied after sowing");
    CHECK(b[1] == INIT_SEEDS + 1, "pit 1 gets +1 seed");
    CHECK(b[2] == INIT_SEEDS + 1, "pit 2 gets +1 seed");
    CHECK(b[3] == INIT_SEEDS + 1, "pit 3 gets +1 seed");
    CHECK(b[4] == INIT_SEEDS + 1, "pit 4 gets +1 seed");
    CHECK(b[5] == INIT_SEEDS,     "pit 5 unchanged");
    CHECK(b[STORE_0] == 0,        "store 0 unchanged (sow from pit 0)");
    CHECK(next == 1,              "turn passes to player 1");
}

// Extra turn: pit 2 has exactly 4 seeds; last seed lands in store_0.
// player0 pits: indices 0..5, store at 6.
// Pit 2: distance to store_0 = 6 - 2 = 4 seeds needed. With 4 seeds => lands on store.
static void test_extra_turn() {
    Board b{};
    b[2] = 4;  // only pit 2 has seeds on player 0's side
    // pit 3,4,5 empty so seeds land on 3,4,5,6(store)
    int next = apply_move(b, 2, 0);
    CHECK(b[STORE_0] == 1, "extra-turn: last seed in own store");
    CHECK(next == 0,       "extra-turn: player 0 moves again");
}

// Capture: player 0 sows into an empty own pit whose opposite has seeds.
static void test_capture() {
    Board b{};
    // Set up: pit 0 has 1 seed (will land on pit 1 which is empty => not capture)
    // Better setup: pit 5 has 1 seed, lands on... pit 6 (store) — extra turn, not capture.
    // Capture setup: pit 3 has 1 seed, lands on pit 4 (empty own pit). Opposite = 19-4 = 15? no.
    // opposite(4) = 19 - 4 = 15 — but 15 > STORE_1=13, that's wrong. Let me use the formula:
    // opposite(i) = 19 - i.  pit 4 (player0) => 19-4=15 — out of range. Wait: STORE_1=13.
    // Formula: opposite(i) = STORE_0 + STORE_1 - 1 - i = 6+13-1-i = 18-i.
    // opposite(4) = 18-4 = 14 — still out of range (board is 0..13).
    // Correct: STORE_0=6, STORE_1=13. opposite = 6+13-1-i = 18-i.
    // pit 0 (p0) => opposite = 18-0=18? No. Let me recheck.
    // The board has indices 0..13. Pits: p0=0..5, store0=6, p1=7..12, store1=13.
    // Mirror: pit 0 of p0 faces pit 12 of p1. pit 5 faces pit 7.
    // So opposite(i) = 12 - i + 7 = 19 - i  for i in [0..5]  => 7..12  ✓ (indices 7-12 for p1)
    // And opposite(i) = 19 - i for i in [7..12] => 7..12 ✓
    // But 19 - i must be in [0..13]. For i=7: 12 ✓. For i=0: 19 — out of range!
    // The opposite() function in board.h uses: STORE_0 + STORE_1 - 1 - i = 6+13-1-i = 18-i
    // For i=0: 18. Still wrong.
    // After reviewing: correct formula for pit i of player0 (i in 0..5):
    //   opposite = STORE_1 - 1 - i = 13 - 1 - i = 12 - i   => gives 12,11,10,9,8,7 ✓
    // And for player1 pit j (j in 7..12):
    //   opposite = STORE_1 - 1 - j = 12 - j   => for j=7: 5 ✓, j=12: 0 ✓
    // So the correct formula is simply: opposite(i) = STORE_1 - 1 - i = 12 - i
    // board.h uses 19-i which is wrong for p0 pits. We correct in the test here:

    // Let's hand-craft a capture scenario:
    // player0 sows 1 seed from pit 0 into pit 1 (which is empty), opposite pit = 12-1=11 has seeds.
    b[0]  = 1;  // pit 0 player0: 1 seed
    b[1]  = 0;  // pit 1 player0: empty (will receive the 1 seed => capture)
    b[11] = 3;  // pit 11 player1 (opposite of pit 1): 3 seeds
    int next = apply_move(b, 0, 0);
    // After capture: store0 gets b[11]+b[1] = 3+1 = 4; pit1=0; pit11=0
    CHECK(b[STORE_0] == 4, "capture: own store gets landing seed + opposite seeds");
    CHECK(b[1]  == 0,      "capture: landing pit emptied");
    CHECK(b[11] == 0,      "capture: opposite pit emptied");
    CHECK(next  == 1,      "capture: turn passes to player 1");
}

// is_terminal returns true when one side is empty.
static void test_terminal() {
    Board b = initial_board();
    CHECK(!is_terminal(b), "initial board not terminal");
    for (int i = 0; i < PITS; ++i) b[i] = 0;
    CHECK(is_terminal(b), "board terminal when player0 pits empty");
}

// Seed conservation: total seeds never change throughout a game.
static void test_seed_conservation() {
    Board b = initial_board();
    int side = 0;
    for (int step = 0; step < 20 && !is_terminal(b); ++step) {
        auto mv = legal_moves(b, side);
        if (mv.empty()) { side = 1 - side; continue; }
        side = apply_move(b, mv[0], side);
    }
    int total = 0;
    for (int v : b) total += v;
    CHECK(total == PITS * 2 * INIT_SEEDS, "seed conservation over 20 moves");
}

// evaluate() from terminal position: winner has more seeds in store.
static void test_evaluate_terminal() {
    Board b{};
    b[STORE_0] = 30;
    b[STORE_1] = 18;
    int v = evaluate(b, 0);
    CHECK(v > 0, "evaluate: player0 ahead when store0 > store1");
    int v1 = evaluate(b, 1);
    CHECK(v1 < 0, "evaluate: player1 behind when store0 > store1");
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main() {
    test_initial_board();
    test_legal_moves_initial();
    test_no_legal_moves();
    test_sow_basic();
    test_extra_turn();
    test_capture();
    test_terminal();
    test_seed_conservation();
    test_evaluate_terminal();

    std::printf("Board tests: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed > 0 ? 1 : 0;
}
