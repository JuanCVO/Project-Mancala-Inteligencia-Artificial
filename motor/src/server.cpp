// motor/src/server.cpp
// Minimal HTTP/1.1 server exposing the Alpha-Beta and MCTS engines.
// Listens on 0.0.0.0:8001 (internal cluster port, not exposed to users).
//
// Endpoints:
//   POST /move   — run alphabeta or mcts and return best move + stats
//   GET  /healthz — liveness probe (always 200 if process is alive)
//   GET  /readyz  — readiness probe (200)
//   GET  /metrics — plaintext aggregate metrics
//
// JSON is parsed/serialised manually (no external library) to keep the
// container image small. The backend (Python) is the public-facing API;
// this server only speaks to the backend over the cluster's internal network.

#include "board.h"
#include "alphabeta.h"

#include <omp.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <sstream>
#include <chrono>
#include <atomic>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

// ---------------------------------------------------------------------------
// Global aggregate metrics (thread-safe via atomics)
// ---------------------------------------------------------------------------
static std::atomic<long long> g_ab_calls{0};
static std::atomic<long long> g_ab_nodes{0};
static std::atomic<long long> g_ab_prunes{0};
static std::atomic<long long> g_mcts_calls{0};
static std::atomic<long long> g_mcts_rollouts{0};

// ---------------------------------------------------------------------------
// Tiny JSON helpers
// ---------------------------------------------------------------------------
static int json_int(const std::string& body, const char* key, int def = 0) {
    std::string k = std::string("\"") + key + "\"";
    auto pos = body.find(k);
    if (pos == std::string::npos) return def;
    pos = body.find(':', pos);
    if (pos == std::string::npos) return def;
    return std::stoi(body.c_str() + pos + 1);
}

static std::string json_str(const std::string& body, const char* key,
                             const std::string& def = "") {
    std::string k = std::string("\"") + key + "\"";
    auto pos = body.find(k);
    if (pos == std::string::npos) return def;
    pos = body.find('"', pos + k.size());
    if (pos == std::string::npos) return def;
    auto end = body.find('"', pos + 1);
    if (end == std::string::npos) return def;
    return body.substr(pos + 1, end - pos - 1);
}

// Parse board array [n0,n1,...,n13] from JSON body into Board.
static bool parse_board(const std::string& body, Board& out) {
    auto pos = body.find("\"board\"");
    if (pos == std::string::npos) return false;
    pos = body.find('[', pos);
    if (pos == std::string::npos) return false;
    for (int i = 0; i < BOARD_SIZE; ++i) {
        pos = body.find_first_of("0123456789-", pos + 1);
        if (pos == std::string::npos) return false;
        out[i] = std::stoi(body.c_str() + pos);
    }
    return true;
}

// ---------------------------------------------------------------------------
// Request handler
// ---------------------------------------------------------------------------
static std::string handle_move(const std::string& body) {
    Board b{};
    if (!parse_board(body, b))
        return "HTTP/1.1 400 Bad Request\r\nContent-Type: application/json\r\n\r\n"
               "{\"error\":\"missing or invalid board\"}";

    int side    = json_int(body, "side", 0);
    int threads = json_int(body, "threads", 1);
    std::string algo = json_str(body, "algo", "alphabeta");

    auto t0 = std::chrono::steady_clock::now();

    std::string stats_json;
    int best_move   = -1;
    double eval_out = 0.0;

    if (algo == "alphabeta") {
        int depth = json_int(body, "depth", 8);
        ABResult r = alphabeta_par(b, side, depth, threads);
        best_move  = r.move;
        eval_out   = static_cast<double>(r.score);

        g_ab_calls++;
        g_ab_nodes  += r.nodes;
        g_ab_prunes += r.prunes;

        char buf[256];
        std::snprintf(buf, sizeof(buf),
            "\"algo\":\"alphabeta\",\"nodes\":%lld,\"prunes\":%lld",
            r.nodes, r.prunes);
        stats_json = buf;

    } else {
        // MCTS stub — will be filled in by Andres Felipe's implementation.
        // Returns a random legal move so the server stays functional.
        auto moves = legal_moves(b, side);
        best_move  = moves.empty() ? -1 : moves[0];
        eval_out   = 0.5;
        g_mcts_calls++;
        stats_json = "\"algo\":\"mcts\",\"rollouts\":0,\"tree_depth_avg\":0,\"win_rate\":0.5";
    }

    auto t1 = std::chrono::steady_clock::now();
    long long elapsed_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

    char response_body[1024];
    std::snprintf(response_body, sizeof(response_body),
        "{\"move\":%d,\"evaluation\":%.4f,\"elapsed_ms\":%lld,"
        "\"stats\":{%s},\"threads_used\":%d}",
        best_move, eval_out, elapsed_ms, stats_json.c_str(), threads);

    std::string resp = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n";
    resp += response_body;
    return resp;
}

static std::string handle_healthz() {
    return "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n{\"status\":\"ok\"}";
}

static std::string handle_readyz() {
    return "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n{\"status\":\"ready\"}";
}

static std::string handle_metrics() {
    char buf[512];
    std::snprintf(buf, sizeof(buf),
        "# alphabeta\nab_calls %lld\nab_nodes %lld\nab_prunes %lld\n"
        "# mcts\nmcts_calls %lld\nmcts_rollouts %lld\n",
        g_ab_calls.load(), g_ab_nodes.load(), g_ab_prunes.load(),
        g_mcts_calls.load(), g_mcts_rollouts.load());
    return std::string("HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\n") + buf;
}

// ---------------------------------------------------------------------------
// Main loop
// ---------------------------------------------------------------------------
int main() {
    const int PORT = 8001;
    int server_fd  = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { perror("socket"); return 1; }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(PORT);

    if (bind(server_fd, (sockaddr*)&addr, sizeof(addr)) < 0) { perror("bind"); return 1; }
    if (listen(server_fd, 64) < 0) { perror("listen"); return 1; }

    std::printf("Motor listening on :%d\n", PORT);
    std::fflush(stdout);

    while (true) {
        int client = accept(server_fd, nullptr, nullptr);
        if (client < 0) continue;

        std::string request;
        {
            char buf[4096];
            while (true) {
                ssize_t n = recv(client, buf, sizeof(buf), 0);
                if (n <= 0) break;
                request.append(buf, n);
                auto hdr_end = request.find("\r\n\r\n");
                if (hdr_end == std::string::npos) continue;
                auto cl_pos = request.find("Content-Length:");
                if (cl_pos != std::string::npos) {
                    long cl = 0;
                    try { cl = std::stol(request.c_str() + cl_pos + 15); } catch (...) { break; }
                    if ((long)(request.size() - hdr_end - 4) >= cl) break;
                    continue;
                }
                break;
            }
        }
        if (request.empty()) { close(client); continue; }

        // Decode chunked body if needed
        {
            auto hdr_end = request.find("\r\n\r\n");
            if (hdr_end != std::string::npos &&
                request.find("Transfer-Encoding: chunked") != std::string::npos) {
                std::string raw = request.substr(hdr_end + 4);
                std::string decoded;
                size_t pos = 0;
                while (pos < raw.size()) {
                    auto crlf = raw.find("\r\n", pos);
                    if (crlf == std::string::npos) break;
                    long csz = 0;
                    try { csz = std::stol(raw.substr(pos, crlf - pos), nullptr, 16); } catch (...) { break; }
                    if (csz == 0) break;
                    pos = crlf + 2;
                    if (pos + csz > raw.size()) break;
                    decoded += raw.substr(pos, csz);
                    pos += csz + 2;
                }
                request = request.substr(0, hdr_end + 4) + decoded;
            }
        }
        std::string response;

        if (request.find("POST /move") != std::string::npos) {
            // Extract JSON body (after \r\n\r\n)
            auto body_start = request.find("\r\n\r\n");
            std::string body = (body_start != std::string::npos)
                               ? request.substr(body_start + 4) : "";
            response = handle_move(body);
        } else if (request.find("GET /healthz") != std::string::npos) {
            response = handle_healthz();
        } else if (request.find("GET /readyz") != std::string::npos) {
            response = handle_readyz();
        } else if (request.find("GET /metrics") != std::string::npos) {
            response = handle_metrics();
        } else {
            response = "HTTP/1.1 404 Not Found\r\n\r\n";
        }

        send(client, response.c_str(), response.size(), 0);
        close(client);
    }

    close(server_fd);
    return 0;
}
