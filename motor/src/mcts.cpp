// motor/src/mcts.cpp
// MCTS with UCT policy — OpenMP leaf parallelization.
//
// Parallelization strategy: LEAF PARALLELIZATION
//   Each tree traversal (select + expand) is single-threaded so the shared
//   tree needs no locks.  Once a leaf is reached, `nthreads` independent
//   random rollouts are launched with `#pragma omp parallel for reduction`.
//   Results are averaged and backpropagated as a batch update.
//
//   Property: identical statistical distribution to sequential MCTS because
//   rollouts are independent given a fixed leaf; only variance per leaf is
//   reduced by a factor of nthreads (law of large numbers applies per node).

#include "mcts.h"
#include <omp.h>
#include <cmath>
#include <random>
#include <memory>
#include <vector>
#include <algorithm>

static constexpr double UCT_C = 1.41421356237;  // sqrt(2), standard exploration constant

// ---------------------------------------------------------------------------
// Tree node
// ---------------------------------------------------------------------------
struct MCTSNode {
    Board  board;
    int    side;        // whose turn to move AT this node
    int    mover;       // side that moved INTO this node (-1 for root)
    int    move_made;   // pit index that led here  (-1 for root)
    MCTSNode* parent;
    std::vector<std::unique_ptr<MCTSNode>> children;
    std::vector<int> untried;  // moves not yet expanded
    double w = 0.0;   // accumulated wins for `mover`
    int    N = 0;     // total visits (summed over batch rollouts)

    MCTSNode(const Board& b, int s, int mv, int mr, MCTSNode* p)
        : board(b), side(s), mover(mr), move_made(mv), parent(p)
    {
        if (!is_terminal(b))
            untried = legal_moves(b, s);
    }
};

// ---------------------------------------------------------------------------
// UCT child selection
// w/N + C * sqrt(ln(N_parent) / N_child)
// Unvisited children receive infinite priority so they are explored first.
// ---------------------------------------------------------------------------
static MCTSNode* uct_child(MCTSNode* node) {
    MCTSNode* best = nullptr;
    double    best_val = -1e18;
    for (auto& ch : node->children) {
        double val = (ch->N == 0 || node->N == 0)
            ? 1e18
            : ch->w / ch->N + UCT_C * std::sqrt(std::log((double)node->N) / (double)ch->N);
        if (val > best_val) { best_val = val; best = ch.get(); }
    }
    return best;
}

// ---------------------------------------------------------------------------
// Phase 1 — Selection
// Descend using UCT until we reach a node with untried moves or a terminal.
// ---------------------------------------------------------------------------
static MCTSNode* selection(MCTSNode* root) {
    MCTSNode* n = root;
    while (!is_terminal(n->board) && n->untried.empty()) {
        MCTSNode* next = uct_child(n);
        if (!next) break;
        n = next;
    }
    return n;
}

// ---------------------------------------------------------------------------
// Phase 2 — Expansion
// Add one child chosen uniformly at random from untried moves.
// Returns the new child (or `node` itself if it is terminal / fully expanded).
// ---------------------------------------------------------------------------
static MCTSNode* expansion(MCTSNode* node, std::mt19937& rng) {
    if (node->untried.empty() || is_terminal(node->board))
        return node;

    std::uniform_int_distribution<int> d(0, (int)node->untried.size() - 1);
    int idx = d(rng);
    int mv  = node->untried[idx];
    node->untried.erase(node->untried.begin() + idx);

    Board nb      = node->board;
    int next_side = apply_move(nb, mv, node->side);

    auto child = std::make_unique<MCTSNode>(nb, next_side, mv, node->side, node);
    MCTSNode* ptr = child.get();
    node->children.push_back(std::move(child));
    return ptr;
}

// ---------------------------------------------------------------------------
// Phase 3 — Simulation (random rollout)
// Returns 1.0 if root_side wins, 0.0 if loses, 0.5 for draw.
// ---------------------------------------------------------------------------
static double rollout_once(Board b, int side, int root_side, std::mt19937& rng) {
    while (!is_terminal(b)) {
        auto moves = legal_moves(b, side);
        if (moves.empty()) break;
        std::uniform_int_distribution<int> d(0, (int)moves.size() - 1);
        side = apply_move(b, moves[d(rng)], side);
    }
    int s0 = b[STORE_0], s1 = b[STORE_1];
    if (s0 == s1) return 0.5;
    return (((s0 > s1) ? 0 : 1) == root_side) ? 1.0 : 0.0;
}

// ---------------------------------------------------------------------------
// Phase 4 — Backpropagation
// Batch update: N += visits, w += wins-for-mover.
// `total_wins` is the sum of rollout results from root_side's perspective.
// ---------------------------------------------------------------------------
static void backprop(MCTSNode* node, double total_wins, int visits, int root_side) {
    for (MCTSNode* cur = node; cur; cur = cur->parent) {
        cur->N += visits;
        if (cur->mover >= 0) {
            // w counts wins for cur->mover
            cur->w += (cur->mover == root_side)
                      ? total_wins
                      : (double)(visits) - total_wins;
        }
    }
}

// ---------------------------------------------------------------------------
// Main search function
// ---------------------------------------------------------------------------
MCTSResult mcts_search(const Board& b, int side, int simulations, int threads) {
    if (threads > 0) omp_set_num_threads(threads);
    int nt = omp_get_max_threads();
    if (nt < 1) nt = 1;

    if (is_terminal(b) || legal_moves(b, side).empty())
        return {-1, 0.0, 0, 0.0};

    auto root = std::make_unique<MCTSNode>(b, side, -1, -1, nullptr);

    // Tree traversal is single-threaded — one RNG suffices
    std::mt19937 tree_rng(42u);

    // Each normal iteration runs exactly `nt` parallel rollouts.
    // The remainder iteration handles leftover simulations.
    int iters     = simulations / nt;
    int remainder = simulations % nt;

    long long actual     = 0;
    long long depth_sum  = 0;
    int       depth_cnt  = 0;

    // Lambda for one select-expand-simulate-backprop cycle with `batch` rollouts.
    auto run_iter = [&](int batch, unsigned seed_base) {
        // Phase 1: Selection
        MCTSNode* node = selection(root.get());
        // Phase 2: Expansion
        MCTSNode* leaf = expansion(node, tree_rng);

        // Track leaf depth (tree-depth metric)
        int d = 0;
        for (MCTSNode* c = leaf; c; c = c->parent) d++;
        depth_sum += d;
        depth_cnt++;

        // Phase 3: Parallel rollouts (leaf parallelization)
        double wins = 0.0;
        Board  lb   = leaf->board;  // copied to avoid sharing
        int    ls   = leaf->side;

        #pragma omp parallel for reduction(+:wins) schedule(static) num_threads(batch)
        for (int r = 0; r < batch; ++r) {
            // Each thread uses a distinct deterministic seed
            std::mt19937 local_rng(seed_base + (unsigned)r * 31337u);
            wins += rollout_once(lb, ls, side, local_rng);
        }

        // Phase 4: Backpropagation
        backprop(leaf, wins, batch, side);
        actual += batch;
    };

    for (int it = 0; it < iters; ++it)
        run_iter(nt, (unsigned)(it * 997u + 1u));

    if (remainder > 0)
        run_iter(remainder, (unsigned)(iters * 997u + 1u));

    // Best move = most visited direct child of root
    int    best_move = -1;
    int    best_n    = -1;
    double best_wr   = 0.0;
    for (const auto& ch : root->children) {
        if (ch->N > best_n) {
            best_n    = ch->N;
            best_move = ch->move_made;
            best_wr   = (ch->N > 0) ? ch->w / ch->N : 0.0;
        }
    }
    // Fallback: if no children were created (extremely small budget), pick first legal
    if (best_move < 0) {
        auto mv   = legal_moves(b, side);
        best_move = mv.empty() ? -1 : mv[0];
    }

    double avg_depth = depth_cnt > 0 ? (double)depth_sum / depth_cnt : 0.0;
    return {best_move, best_wr, actual, avg_depth};
}
