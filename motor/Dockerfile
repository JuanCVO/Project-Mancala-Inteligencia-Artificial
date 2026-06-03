# motor/Dockerfile
# Multi-stage build:
#   Stage 1 (builder): compiles the C++/OpenMP server and benchmark binary.
#   Stage 2 (runtime): minimal image — only the compiled binaries.

# ---- Stage 1: build ----
FROM gcc:13 AS builder

RUN apt-get update && apt-get install -y --no-install-recommends \
    cmake \
    libgomp1 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY CMakeLists.txt .
COPY src/            src/
COPY bench/          bench/
COPY tests/          tests/

RUN cmake -B build -DCMAKE_BUILD_TYPE=Release && \
    cmake --build build --parallel "$(nproc)"

# ---- Stage 2: runtime ----
FROM debian:bookworm-slim AS runtime

RUN apt-get update && apt-get install -y --no-install-recommends \
    libgomp1 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY --from=builder /app/build/mancala_server  ./mancala_server
COPY --from=builder /app/build/mancala_bench   ./mancala_bench
COPY tests/suite.txt                            ./tests/suite.txt

# Internal cluster port (not exposed to end users — only backend talks here)
EXPOSE 8001

# OMP_NUM_THREADS is injected via ConfigMap / environment variable
ENV OMP_NUM_THREADS=4

CMD ["./mancala_server"]
