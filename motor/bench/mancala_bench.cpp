// motor/bench/mancala_bench.cpp
// Standalone benchmark binary — runs without the HTTP backend.
//
// Usage examples:
//   OMP_NUM_THREADS=1 ./mancala_bench --algo alphabeta --depth 12 --positions tests/suite.txt
//   OMP_NUM_THREADS=8 ./mancala_bench --algo alphabeta --depth 8  --positions tests/suite.txt
//   OMP_NUM_THREADS=4 ./mancala_bench --algo mcts --simulations 100000 --positions tests/suite.txt
//
// Output (one JSON object per line, then a summary):
//   {"position":0,"move":3,"score":7,"nodes":184521,"prunes":31208,"elapsed_ms":124,"threads":8}
//   ...
//   {"summary":{"algo":"alphabeta","depth":12,"threads":8,
//               "total_positions":10,"total_elapsed_ms":1240,
//               "avg_nodes":184521,"avg_prunes":31208,
//               "speedup_vs_seq":null}}

#include "../src/board.h"
#include "../src/alphabeta.h"

#include <omp.h>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Parse a position line from tests/suite.txt
// Format: 14 integers separated by spaces, optionally followed by "side=N"
//   4 4 4 4 4 4 0 4 4 4 4 4 4 0 side=0
// ---------------------------------------------------------------------------
static bool parse_position(const std::string& line, Board& b, int& side) {
    std::istringstream ss(line);
    for (int i = 0; i < BOARD_SIZE; ++i) {
        if (!(ss >> b[i])) return false;
    }
    side = 0;
    std::string token;
    while (ss >> token) {
        if (token.rfind("side=", 0) == 0)
            side = std::stoi(token.substr(5));
    }
    return true;
}

// ---------------------------------------------------------------------------
// CLI argument helpers
// ---------------------------------------------------------------------------
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

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main(int argc, char** argv) {
    std::string algo       = arg_str(argc, argv, "--algo", "alphabeta");
    std::string positions_file = arg_str(argc, argv, "--positions", "tests/suite.txt");
    int depth              = arg_int(argc, argv, "--depth", 8);
    int simulations        = arg_int(argc, argv, "--simulations", 10000);
    int threads            = arg_int(argc, argv, "--threads", 0); // 0 = env var

    if (threads > 0) omp_set_num_threads(threads);
    int actual_threads = omp_get_max_threads();

    // Load positions
    std::ifstream f(positions_file);
    if (!f.is_open()) {
        std::fprintf(stderr, "Cannot open positions file: %s\n", positions_file.c_str());
        return 1;
    }

    std::vector<std::pair<Board, int>> positions;
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        Board b{};
        int side = 0;
        if (parse_position(line, b, side))
            positions.emplace_back(b, side);
    }

    if (positions.empty()) {
        std::fprintf(stderr, "No positions loaded from %s\n", positions_file.c_str());
        return 1;
    }

    long long total_elapsed = 0;
    long long total_nodes   = 0;
    long long total_prunes  = 0;

    for (int idx = 0; idx < (int)positions.size(); ++idx) {
        const Board& b = positions[idx].first;
        int side       = positions[idx].second;

        auto t0 = std::chrono::steady_clock::now();

        int move = -1, score = 0;
        long long nodes = 0, prunes = 0;

        if (algo == "alphabeta") {
            ABResult r = alphabeta_par(b, side, depth, threads);
            move   = r.move;
            score  = r.score;
            nodes  = r.nodes;
            prunes = r.prunes;
        } else {
            // MCTS placeholder — Andres Felipe fills this in
            auto mv = legal_moves(b, side);
            move    = mv.empty() ? -1 : mv[0];
            score   = 0;
            nodes   = simulations;
        }

        auto t1 = std::chrono::steady_clock::now();
        long long elapsed =
            std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

        total_elapsed += elapsed;
        total_nodes   += nodes;
        total_prunes  += prunes;

        std::printf(
            "{\"position\":%d,\"move\":%d,\"score\":%d,"
            "\"nodes\":%lld,\"prunes\":%lld,\"elapsed_ms\":%lld,\"threads\":%d}\n",
            idx, move, score, nodes, prunes, elapsed, actual_threads);
    }

    int n = (int)positions.size();
    std::printf(
        "{\"summary\":{\"algo\":\"%s\",\"depth\":%d,\"simulations\":%d,"
        "\"threads\":%d,\"total_positions\":%d,\"total_elapsed_ms\":%lld,"
        "\"avg_nodes\":%lld,\"avg_prunes\":%lld}}\n",
        algo.c_str(), depth, simulations, actual_threads, n,
        total_elapsed,
        n > 0 ? total_nodes / n : 0LL,
        n > 0 ? total_prunes / n : 0LL);

    return 0;
}
