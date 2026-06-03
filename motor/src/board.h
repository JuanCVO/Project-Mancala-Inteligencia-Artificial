#pragma once
#include <array>
#include <vector>
#include <cstdint>

// Kalah(6,4) canonical board layout (14 integers):
//
//   Index  0..5  : Player 0 pits (left to right from Player 0's perspective)
//   Index  6     : Player 0 kalaha (store)
//   Index  7..12 : Player 1 pits (left to right from Player 1's perspective)
//   Index  13    : Player 1 kalaha (store)
//
// Seeds travel counter-clockwise:
//   Player 0 sows: 0,1,2,3,4,5, 6(own store), 7,8,9,10,11,12 — skips 13
//   Player 1 sows: 7,8,9,10,11,12, 13(own store), 0,1,2,3,4,5 — skips 6
//
// PITS = 6, INITIAL_SEEDS = 4, matching Kalah(6,4) standard.

static constexpr int PITS        = 6;
static constexpr int BOARD_SIZE  = 14;  // 12 pits + 2 stores
static constexpr int INIT_SEEDS  = 4;

static constexpr int STORE_0 = 6;
static constexpr int STORE_1 = 13;

using Board = std::array<int, BOARD_SIZE>;

// Returns a fresh starting board.
inline Board initial_board() {
    Board b{};
    for (int i = 0; i < PITS;       ++i) b[i]        = INIT_SEEDS;
    b[STORE_0] = 0;
    for (int i = PITS + 1; i < STORE_1; ++i) b[i]   = INIT_SEEDS;
    b[STORE_1] = 0;
    return b;
}

// Returns the store index for the given side (0 or 1).
inline int store_of(int side) { return side == 0 ? STORE_0 : STORE_1; }

// Returns the first pit index for the given side.
inline int pit_start(int side) { return side == 0 ? 0 : PITS + 1; }

// Returns true if index i is a pit belonging to `side`.
inline bool is_own_pit(int i, int side) {
    return side == 0 ? (i >= 0 && i < PITS)
                     : (i >= PITS + 1 && i < STORE_1);
}

// Returns the pit directly opposite to pit i.
// Opposite of pit p0 (0..5) is pit p1 (12..7) — mirror: 12 - p0 + 7 = STORE_1 - 1 - p0
inline int opposite_pit(int i) {
    if (i >= 0 && i < PITS)         return STORE_1 - 1 - i;   // 0->12, 1->11, ...
    if (i >= PITS + 1 && i < STORE_1) return STORE_1 - 1 - i; // 7->6? no — symmetric
    return -1;
}
// Correct formula: pit p of player0 is index p (0-5), opposite is (STORE_1-1-p) = 12-p
// pit p of player1 is index p+PITS+1 (7-12), opposite is (PITS - (p - (PITS+1))) = hmm
// Simpler: for index i, opposite = STORE_0 + STORE_1 - 1 - i = 19 - i
inline int opposite(int i) { return STORE_0 + STORE_1 - 1 - i; }  // 19 - i

// Returns legal moves (pit indices) for `side`. A move is legal if the pit
// belongs to `side` and has at least one seed.
inline std::vector<int> legal_moves(const Board& b, int side) {
    std::vector<int> moves;
    moves.reserve(PITS);
    int start = pit_start(side);
    for (int i = start; i < start + PITS; ++i) {
        if (b[i] > 0) moves.push_back(i);
    }
    return moves;
}

// Returns true if the game is over (either side has all pits empty).
inline bool is_terminal(const Board& b) {
    bool side0_empty = true, side1_empty = true;
    for (int i = 0; i < PITS;    ++i) if (b[i]      > 0) { side0_empty = false; break; }
    for (int i = PITS+1; i < STORE_1; ++i) if (b[i] > 0) { side1_empty = false; break; }
    return side0_empty || side1_empty;
}

// Applies move (pit index) for `side` on board b.
// Returns the side that moves next (handles extra-turn rule).
// Mutates b in place.
inline int apply_move(Board& b, int pit, int side) {
    int seeds = b[pit];
    b[pit]    = 0;
    int idx   = pit;
    int opp_store = store_of(1 - side);

    while (seeds > 0) {
        idx = (idx + 1) % BOARD_SIZE;
        if (idx == opp_store) continue;  // skip opponent's store
        ++b[idx];
        --seeds;
    }

    // Extra turn: last seed landed in own store
    if (idx == store_of(side)) {
        return side;  // same player moves again
    }

    // Capture: last seed landed in own empty pit and opposite has seeds
    if (is_own_pit(idx, side) && b[idx] == 1) {
        int opp = opposite(idx);
        if (b[opp] > 0) {
            b[store_of(side)] += b[opp] + 1;  // capture opposite + the landing seed
            b[opp] = 0;
            b[idx] = 0;
        }
    }

    // End-of-game: sweep remaining seeds to their owner's store
    if (is_terminal(b)) {
        for (int i = 0; i < PITS;    ++i) { b[STORE_0] += b[i];      b[i]      = 0; }
        for (int i = PITS+1; i < STORE_1; ++i) { b[STORE_1] += b[i]; b[i]      = 0; }
    }

    return 1 - side;
}

// Heuristic value of the board from the perspective of `side`.
// h = (own_store - opp_store) + alpha * (own_seeds - opp_seeds)
// alpha is configurable; the project uses alpha = 0.5 by default.
inline int evaluate(const Board& b, int side, double alpha = 0.5) {
    int own_store = b[store_of(side)];
    int opp_store = b[store_of(1 - side)];
    int own_seeds = 0, opp_seeds = 0;
    int os  = pit_start(side);
    int ops = pit_start(1 - side);
    for (int i = os;  i < os  + PITS; ++i) own_seeds += b[i];
    for (int i = ops; i < ops + PITS; ++i) opp_seeds += b[i];
    return (own_store - opp_store) + static_cast<int>(alpha * (own_seeds - opp_seeds));
}
