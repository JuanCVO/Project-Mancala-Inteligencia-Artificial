#pragma once
#include "board.h"
#include <atomic>
#include <limits>

// Alpha-Beta search result
struct ABResult {
    int move;        // best move (pit index), -1 if terminal
    int score;       // heuristic score from the perspective of the maximizing side
    long long nodes; // nodes explored
    long long prunes;// alpha-beta cuts performed
};

// Sequential Minimax with Alpha-Beta pruning.
// `side`         : the side to maximize (0 or 1)
// `current_side` : whose turn it is at this node
// `depth`        : plies remaining
// `alpha`, `beta`: current window
// `nodes`, `prunes` : instrumentation counters (incremented in place)
// `heuristic_alpha`  : weight for seed-count term in evaluate()
ABResult alphabeta_seq(
    const Board& b,
    int side,
    int current_side,
    int depth,
    int alpha,
    int beta,
    long long& nodes,
    long long& prunes,
    double heuristic_alpha = 0.5
);

// Parallel Alpha-Beta using root parallelism:
//   Each legal move at the root is explored by a separate OpenMP thread,
//   running a sequential Alpha-Beta inside. Results are reduced to find
//   the global best move.
//
// `threads` : number of OpenMP threads (0 = use OMP_NUM_THREADS env var)
ABResult alphabeta_par(
    const Board& b,
    int side,
    int depth,
    int threads = 0,
    double heuristic_alpha = 0.5
);

// Pure Minimax (no pruning) — used only for correctness verification in tests.
ABResult minimax_seq(
    const Board& b,
    int side,
    int current_side,
    int depth,
    long long& nodes,
    double heuristic_alpha = 0.5
);
