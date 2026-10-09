#!/usr/bin/env bash
# Builds profiling-only harness variants with the measured binary's project flags.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"; repo="$(cd "$here/../../.." && pwd)"
flags=(-std=c++20 -O3 -DNDEBUG -march=native -flto -Wall -Wextra -I"$repo/include" -I"$repo/benchmarks")
srcs=("$repo/src/order_pool.cpp" "$repo/src/price_level.cpp" "$here/prof_harness.cpp")
/usr/bin/clang++ "${flags[@]}" "${srcs[@]}" -o "$here/prof_timing"
/usr/bin/clang++ "${flags[@]}" -DUSE_RUSAGE "${srcs[@]}" -o "$here/prof_rusage"
/usr/bin/clang++ "${flags[@]}" -DCOUNT_ALLOCS "${srcs[@]}" -o "$here/prof_alloc"
/usr/bin/clang++ "${flags[@]}" -DPC_SAMPLE "${srcs[@]}" -o "$here/prof_pcsamp"
