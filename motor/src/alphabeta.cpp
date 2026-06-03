#include "alphabeta.h"
#include <omp.h>
#include <algorithm>
#include <limits>

static constexpr int INF = std::numeric_limits<int>::max() / 2;

// ---------------------------------------------------------------------------
// Sequential Minimax with Alpha-Beta pruning
// ---------------------------------------------------------------------------
ABResult alphabeta_seq(
    const Board& b,
    int side,
    int current_side,
    int depth,
    int alpha,
    int beta,
    long long& nodes,
    long long& prunes,
    double heuristic_alpha)
{
    ++nodes;

    if (depth == 0 || is_terminal(b)) {
        return {-1, evaluate(b, side, heuristic_alpha), nodes, prunes};
    }

    auto moves = legal_moves(b, current_side);
    if (moves.empty()) {
        // No legal moves — pass turn
        Board nb = b;
        if (is_terminal(nb)) {
            return {-1, evaluate(nb, side, heuristic_alpha), nodes, prunes};
        }
        return alphabeta_seq(nb, side, 1 - current_side, depth - 1,
                             alpha, beta, nodes, prunes, heuristic_alpha);
    }

    bool maximizing = (current_side == side);
    int best_score  = maximizing ? -INF : INF;
    int best_move   = moves[0];

    for (int pit : moves) {
        Board nb       = b;
        int next_side  = apply_move(nb, pit, current_side);

        // If apply_move grants an extra turn, depth is not decremented.
        int next_depth = (next_side == current_side) ? depth : depth - 1;

        ABResult child = alphabeta_seq(nb, side, next_side, next_depth,
                                       alpha, beta, nodes, prunes, heuristic_alpha);

        if (maximizing) {
            if (child.score > best_score) {
                best_score = child.score;
                best_move  = pit;
            }
            alpha = std::max(alpha, best_score);
        } else {
            if (child.score < best_score) {
                best_score = child.score;
                best_move  = pit;
            }
            beta = std::min(beta, best_score);
        }

        if (beta <= alpha) {
            ++prunes;
            break;
        }
    }

    return {best_move, best_score, nodes, prunes};
}

// ---------------------------------------------------------------------------
// Root-parallel Alpha-Beta
// ---------------------------------------------------------------------------
// Strategy: distribute the root's legal moves across threads. Each thread
// runs a full sequential Alpha-Beta on its assigned sub-tree with an
// initial window of [-INF, INF] (no shared alpha/beta across threads to
// avoid losing pruning quality in a complex way). After all threads finish,
// we reduce to find the globally best move.
//
// Trade-off (documented per assignment):
//   - Each thread searches its sub-tree independently, so pruning derived
//     from sibling sub-trees' results is not propagated. This means we
//     perform strictly more work than the sequential search when the optimal
//     move is visited last in thread order.
//   - Synchronization cost is minimal: one #pragma omp parallel for with
//     a reduction at the end; no shared mutable state during search.
//   - Despite the pruning loss, wall-clock time decreases nearly linearly
//     with thread count for depth >= 8 because the tree is large enough
//     that each thread has substantial independent work.
// ---------------------------------------------------------------------------
ABResult alphabeta_par(
    const Board& b,
    int side,
    int depth,
    int threads,
    double heuristic_alpha)
{
    if (threads > 0) omp_set_num_threads(threads);

    auto moves = legal_moves(b, side);
    if (moves.empty() || is_terminal(b)) {
        long long n = 0, p = 0;
        return {-1, evaluate(b, side, heuristic_alpha), n, p};
    }

    int n_moves = static_cast<int>(moves.size());

    // Per-thread results
    std::vector<int>       scores(n_moves, -INF);
    std::vector<long long> thread_nodes(n_moves, 0);
    std::vector<long long> thread_prunes(n_moves, 0);

    #pragma omp parallel for schedule(dynamic, 1) default(none) \
        shared(b, moves, scores, thread_nodes, thread_prunes, side, depth, heuristic_alpha, n_moves)
    for (int i = 0; i < n_moves; ++i) {
        Board nb      = b;
        int next_side = apply_move(nb, moves[i], side);
        int nd        = (next_side == side) ? depth : depth - 1;

        long long ln = 0, lp = 0;
        ABResult r = alphabeta_seq(nb, side, next_side, nd,
                                   -INF, INF, ln, lp, heuristic_alpha);
        scores[i]        = r.score;
        thread_nodes[i]  = ln;
        thread_prunes[i] = lp;
    }

    // Reduce
    int best_idx   = 0;
    long long total_nodes  = 0;
    long long total_prunes = 0;
    for (int i = 0; i < n_moves; ++i) {
        if (scores[i] > scores[best_idx]) best_idx = i;
        total_nodes  += thread_nodes[i];
        total_prunes += thread_prunes[i];
    }

    return {moves[best_idx], scores[best_idx], total_nodes, total_prunes};
}

// ---------------------------------------------------------------------------
// Pure Minimax (no pruning) — correctness reference only
// ---------------------------------------------------------------------------
ABResult minimax_seq(
    const Board& b,
    int side,
    int current_side,
    int depth,
    long long& nodes,
    double heuristic_alpha)
{
    ++nodes;

    if (depth == 0 || is_terminal(b)) {
        return {-1, evaluate(b, side, heuristic_alpha), nodes, 0};
    }

    auto moves = legal_moves(b, current_side);
    if (moves.empty()) {
        Board nb = b;
        return minimax_seq(nb, side, 1 - current_side, depth - 1, nodes, heuristic_alpha);
    }

    bool maximizing = (current_side == side);
    int best_score  = maximizing ? -INF : INF;
    int best_move   = moves[0];

    for (int pit : moves) {
        Board nb      = b;
        int next_side = apply_move(nb, pit, current_side);
        int nd        = (next_side == current_side) ? depth : depth - 1;

        ABResult child = minimax_seq(nb, side, next_side, nd, nodes, heuristic_alpha);

        if (maximizing && child.score > best_score) {
            best_score = child.score; best_move = pit;
        } else if (!maximizing && child.score < best_score) {
            best_score = child.score; best_move = pit;
        }
    }

    return {best_move, best_score, nodes, 0};
}
