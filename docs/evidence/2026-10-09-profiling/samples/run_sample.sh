#!/usr/bin/env bash
# usage: run_sample.sh <outdir> <case-regex-name> <tag>
set -euo pipefail
out="$1"; name="$2"; tag="$3"
bin=/Users/quynhanhha/projects/c++/order-matching-engine/benchmark_results/20261009T035923Z-repaired/build-release/order_book_bench
"$bin" --benchmark_filter="^${name}\$" --benchmark_min_time=40s --benchmark_min_warmup_time=1 \
  --benchmark_out="$out/samples/$tag.bench.json" --benchmark_out_format=json > "$out/samples/$tag.bench.txt" 2>&1 &
pid=$!
sleep 4
/usr/bin/sample "$pid" 25 1 -mayDie -file "$out/samples/$tag.sample.txt" > /dev/null 2>&1 || echo "sample failed"
wait "$pid"
