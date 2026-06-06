#pragma once
#include "board.h"

struct MCTSResult {
    int       move;           // best move (pit index), -1 if terminal/no moves
    double    win_rate;       // estimated win probability for `side` in [0,1]
    long long rollouts;       // actual rollouts executed
    double    tree_depth_avg; // mean depth of leaves reached during search
};

// Run MCTS with UCT policy and OpenMP leaf parallelization.
//   simulations : total rollout budget
//   threads     : OpenMP thread count (0 = honour OMP_NUM_THREADS env var)
MCTSResult mcts_search(const Board& b, int side, int simulations, int threads = 0);
