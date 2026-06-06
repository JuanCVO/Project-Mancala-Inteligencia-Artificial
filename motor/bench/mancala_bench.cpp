// motor/bench/mancala_bench.cpp
// Standalone benchmark binary — runs without the HTTP backend.
//
// Usage examples:
//   OMP_NUM_THREADS=1 ./mancala_bench --algo alphabeta --depth 12 --positions tests/suite.txt
//   OMP_NUM_THREADS=8 ./mancala_bench --algo alphabeta --depth 8  --positions tests/suite.txt
//   OMP_NUM_THREADS=4 ./mancala_bench --algo mcts --simulations 100000 --positions tests/suite.txt
//   OMP_NUM_THREADS=4 ./mancala_bench --algo mcts --simulations 10000 \
//                      --compare-depth 8 --positions tests/suite.txt
//
// Output (one JSON per line, then a summary).
// --compare-depth N: also run Alpha-Beta at depth N and report coincidence %.

#include "../src/board.h"
#include "../src/alphabeta.h"
#include "../src/mcts.h"

#include <omp.h>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

static bool parse_position(const std::string& line, Board& b, int& side) {
    std::istringstream ss(line);
    for (int i = 0; i < BOARD_SIZE; ++i)
        if (!(ss >> b[i])) return false;
    side = 0;
    std::string token;
    while (ss >> token)
        if (token.rfind("side=", 0) == 0)
            side = std::stoi(token.substr(5));
    return true;
}

static std::string arg_str(int argc, char** argv, const char* flag,
                            const std::string& def = "") {
    for (int i = 1; i < argc - 1; ++i)
        if (std::strcmp(argv[i], flag) == 0) return argv[i + 1];
    return def;
}

static int arg_int(int argc, char** argv, const char* flag, int def = 0) {
    std::string v = arg_str(argc, argv, flag);
    return v.empty() ? def : std::stoi(v);
}

int main(int argc, char** argv) {
    std::string algo           = arg_str(argc, argv, "--algo",         "alphabeta");
    std::string positions_file = arg_str(argc, argv, "--positions",    "tests/suite.txt");
    int depth                  = arg_int(argc, argv, "--depth",         8);
    int simulations            = arg_int(argc, argv, "--simulations",   10000);
    int threads                = arg_int(argc, argv, "--threads",       0);
    int compare_depth          = arg_int(argc, argv, "--compare-depth", 0);

    if (threads > 0) omp_set_num_threads(threads);
    int actual_threads = omp_get_max_threads();

    std::ifstream f(positions_file);
    if (!f.is_open()) {
        std::fprintf(stderr, "Cannot open positions file: %s\n", positions_file.c_str());
        return 1;
    }

    std::vector<std::pair<Board, int>> positions;
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        Board b{}; int side = 0;
        if (parse_position(line, b, side))
            positions.emplace_back(b, side);
    }
    if (positions.empty()) {
        std::fprintf(stderr, "No positions loaded from %s\n", positions_file.c_str());
        return 1;
    }

    // Accumulators
    long long total_elapsed    = 0;
    long long total_nodes      = 0;   // nodes (AB) or rollouts (MCTS)
    long long total_prunes     = 0;
    double    sum_tree_depth   = 0.0;
    int       matching_moves   = 0;

    for (int idx = 0; idx < (int)positions.size(); ++idx) {
        const Board& b = positions[idx].first;
        int           side = positions[idx].second;

        auto t0 = std::chrono::steady_clock::now();

        if (algo == "alphabeta") {
            ABResult r = alphabeta_par(b, side, depth, threads);

            auto t1 = std::chrono::steady_clock::now();
            long long elapsed = std::chrono::duration_cast<
                std::chrono::milliseconds>(t1 - t0).count();

            total_elapsed += elapsed;
            total_nodes   += r.nodes;
            total_prunes  += r.prunes;

            std::printf(
                "{\"position\":%d,\"move\":%d,\"score\":%d,"
                "\"nodes\":%lld,\"prunes\":%lld,\"elapsed_ms\":%lld,\"threads\":%d}\n",
                idx, r.move, r.score, r.nodes, r.prunes, elapsed, actual_threads);

        } else {
            MCTSResult r = mcts_search(b, side, simulations, threads);

            auto t1 = std::chrono::steady_clock::now();
            long long elapsed = std::chrono::duration_cast<
                std::chrono::milliseconds>(t1 - t0).count();

            total_elapsed  += elapsed;
            total_nodes    += r.rollouts;
            sum_tree_depth += r.tree_depth_avg;

            // Optional coincidence check (does NOT count toward elapsed time)
            int ab_move = -2;
            if (compare_depth > 0) {
                ABResult ab = alphabeta_par(b, side, compare_depth, 0);
                ab_move = ab.move;
                if (ab.move == r.move) matching_moves++;
            }

            if (compare_depth > 0) {
                std::printf(
                    "{\"position\":%d,\"move\":%d,\"ab_move\":%d,\"match\":%s,"
                    "\"win_rate\":%.4f,\"rollouts\":%lld,"
                    "\"tree_depth_avg\":%.2f,\"elapsed_ms\":%lld,\"threads\":%d}\n",
                    idx, r.move, ab_move,
                    (ab_move == r.move ? "true" : "false"),
                    r.win_rate, r.rollouts,
                    r.tree_depth_avg, elapsed, actual_threads);
            } else {
                std::printf(
                    "{\"position\":%d,\"move\":%d,\"win_rate\":%.4f,"
                    "\"rollouts\":%lld,\"tree_depth_avg\":%.2f,"
                    "\"elapsed_ms\":%lld,\"threads\":%d}\n",
                    idx, r.move, r.win_rate, r.rollouts,
                    r.tree_depth_avg, elapsed, actual_threads);
            }
        }
    }

    int n = (int)positions.size();

    if (algo == "alphabeta") {
        std::printf(
            "{\"summary\":{\"algo\":\"alphabeta\",\"depth\":%d,"
            "\"threads\":%d,\"total_positions\":%d,\"total_elapsed_ms\":%lld,"
            "\"avg_nodes\":%lld,\"avg_prunes\":%lld}}\n",
            depth, actual_threads, n, total_elapsed,
            n > 0 ? total_nodes  / n : 0LL,
            n > 0 ? total_prunes / n : 0LL);
    } else {
        double avg_tree_depth = n > 0 ? sum_tree_depth / n : 0.0;
        if (compare_depth > 0) {
            double coincidence = n > 0 ? 100.0 * matching_moves / n : 0.0;
            std::printf(
                "{\"summary\":{\"algo\":\"mcts\",\"simulations\":%d,"
                "\"threads\":%d,\"total_positions\":%d,\"total_elapsed_ms\":%lld,"
                "\"avg_rollouts\":%lld,\"avg_tree_depth\":%.2f,"
                "\"compare_depth\":%d,\"coincidence_pct\":%.1f}}\n",
                simulations, actual_threads, n, total_elapsed,
                n > 0 ? total_nodes / n : 0LL, avg_tree_depth,
                compare_depth, coincidence);
        } else {
            std::printf(
                "{\"summary\":{\"algo\":\"mcts\",\"simulations\":%d,"
                "\"threads\":%d,\"total_positions\":%d,\"total_elapsed_ms\":%lld,"
                "\"avg_rollouts\":%lld,\"avg_tree_depth\":%.2f}}\n",
                simulations, actual_threads, n, total_elapsed,
                n > 0 ? total_nodes / n : 0LL, avg_tree_depth);
        }
    }

    return 0;
}
