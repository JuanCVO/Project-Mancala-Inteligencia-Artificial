cmake_minimum_required(VERSION 3.16)
project(mancala_motor CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Release build by default for benchmark accuracy
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

# OpenMP is required
find_package(OpenMP REQUIRED)

# ---------------------------------------------------------------------------
# Motor HTTP server
# ---------------------------------------------------------------------------
add_executable(mancala_server
    src/server.cpp
    src/alphabeta.cpp
)
target_include_directories(mancala_server PRIVATE src)
target_link_libraries(mancala_server PRIVATE OpenMP::OpenMP_CXX)

# ---------------------------------------------------------------------------
# Benchmark binary
# ---------------------------------------------------------------------------
add_executable(mancala_bench
    bench/mancala_bench.cpp
    src/alphabeta.cpp
)
target_include_directories(mancala_bench PRIVATE src bench)
target_link_libraries(mancala_bench PRIVATE OpenMP::OpenMP_CXX)

# ---------------------------------------------------------------------------
# Unit tests
# ---------------------------------------------------------------------------
enable_testing()

add_executable(test_board
    tests/test_board.cpp
)
target_include_directories(test_board PRIVATE src)
# test_board.cpp only includes board.h (header-only logic), no alphabeta.cpp needed

add_executable(test_alphabeta
    tests/test_alphabeta.cpp
    src/alphabeta.cpp
)
target_include_directories(test_alphabeta PRIVATE src)
target_link_libraries(test_alphabeta PRIVATE OpenMP::OpenMP_CXX)

add_test(NAME BoardTests     COMMAND test_board)
add_test(NAME AlphaBetaTests COMMAND test_alphabeta)
