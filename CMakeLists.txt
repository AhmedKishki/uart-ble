# CMakeLists.txt (top-level, next to prj.conf)
cmake_minimum_required(VERSION 3.20.0)

# Find Zephyr
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(app)

# App sources
target_sources(app PRIVATE
  src/main.c
  src/ipc.c
  src/serial/serial_io.c
  src/proto/ab_payload.c
  src/proto/parser_worker.c
  src/workers/tx_worker.c
  src/producers/producer_a.c
  src/producers/producer_b.c
)

# Public headers live in these folders
target_include_directories(app PRIVATE
  src
  src/serial
  src/proto
  src/workers
  src/producers
)
