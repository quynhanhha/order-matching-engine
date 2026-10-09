# Synthetic batch API throughput

The engine sustains **40.895 million resting additions/sec** and **50.791 million incoming full fills/sec** in the 10,000-order workloads below. These are single-threaded, amortized batch rates with an empty callback. Setup and teardown are excluded; allocations and deallocations inside API calls are included. They do not establish production capacity or individual-order latency.

## Environment and source

| Property | Measurement setting |
|---|---|
| Platform | macOS 26.5.2, Darwin 25.5.0; Apple M3 Pro, Mac15,6; 6 performance + 6 efficiency cores |
| Memory | 36 GB RAM |
| Compiler / build | Apple Clang 21.0.0; CMake 4.2.0; C++20 |
| Release flags | `-O3 -DNDEBUG -march=native -flto -std=c++20` |
| Dependencies | Google Benchmark v1.8.3; Google Test v1.14.0 |
| Power / CPU controls | AC power; affinity, core placement, and frequency uncontrolled |

The [evidence index](evidence/README.md#current-document-support) identifies source hashes, effective compiler commands, dependency commits, executable hash, environment records, every repetition, and reproduction tools. Thermal queries are restricted; the benchmark's estimated CPU frequency is not a measured clock rate. Google Benchmark uses C++11 internally; project sources use C++20.

## Workloads

| | Resting additions | One-to-one full fills |
|---|---|---|
| N | 100, 1,000, or 10,000 incoming calls | 100, 1,000, or 10,000 initial resting sells |
| Calls per batch | N | N/2 incoming buys |
| Capacity | N + 100 | N + N/2 + 100 |
| Prices / quantities | Alternating buys at 90–99 and sells at 110–119; quantities 1–100 | All orders at price 100, quantity one |
| Participants / IDs | Participants 1–100; unique sequential IDs | Resting participant 1, incoming participant 2; disjoint sequential IDs |
| Depth | Empty to N; up to 20 price levels | N to N/2; one populated level throughout |
| Expected outcomes | All orders rest; no trades or SMP | Each incoming order fills the next FIFO order; no SMP |

Resting inputs use `std::mt19937_64`, seed 42, and uniform price, quantity, and participant distributions. Every submission checks the resting-ID index before allocation or matching. Resting additions allocate one index node; full fills erase and free a resting node without inserting an incoming node.

The full-fill case avoids level removal, partial fills, SMP, and multi-level sweeps. Its N = 10,000 registration measures **5,000 incoming calls**, not 10,000.

## Measurement and validation

Inputs are prepared outside timing and replayed with a recording callback. Validation checks trade IDs, prices, quantities, counts, final contents, FIFO links, aggregate quantities, cancellation/index reachability, and absence of SMP. Timed matching replays also check final best prices and aggregates outside timing; outcome counters describe the validated deterministic trace.

Each iteration uses a fresh book. `PauseTiming()` / `ResumeTiming()` exclude construction, prepopulation, validation, and destruction. API memory costs, input traversal, optimization barriers, and residual framework timing overhead remain included. In particular, resting-book teardown frees N index nodes outside measurement, so these rates do not describe an add-plus-destruction workload.

Both cases use `UseRealTime()` and count incoming calls with `SetItemsProcessed`:

`calls/sec = calls per batch / measured elapsed seconds per batch`

Two process sessions each retain five repetitions per case, with one-second warmup, at least one second of measured time, adaptive iteration counts, and randomized repetition interleaving. The sessions are not consecutive; no repetitions are discarded. The checker requires all six cases, unique repetition indices, consistent call/time accounting, and expected outcome counters.

## Results

Rates are millions of incoming calls per measured second. Each median and range includes all ten repetitions. Sample CV is standard deviation divided by mean; session gap is the absolute difference between session medians divided by their mean.

| Workload | N | Calls/batch | Median M/sec | Range M/sec | Sample CV | Session gap |
|---|---:|---:|---:|---:|---:|---:|
| Resting additions | 100 | 100 | 30.618 | 29.457–30.916 | 1.68% | 0.53% |
| Resting additions | 1,000 | 1,000 | 38.696 | 37.696–38.964 | 1.00% | 0.23% |
| Resting additions | 10,000 | 10,000 | 40.895 | 40.209–41.474 | 0.97% | 1.13% |
| One-to-one full fills | 100 | 50 | 26.657 | 26.339–26.919 | 0.75% | 1.05% |
| One-to-one full fills | 1,000 | 500 | 46.829 | 46.261–47.563 | 0.95% | 0.61% |
| One-to-one full fills | 10,000 | 5,000 | 50.791 | 44.123–51.369 | 4.38% | 0.02% |

[Session 1](evidence/2026-10-09-current/session-1.json), [session 2](evidence/2026-10-09-current/session-2.json), and the [summary](evidence/2026-10-09-current/summary.json) retain every rate and statistic. Spread describes observed variability, not confidence intervals or future guarantees; the low session gap for large full fills does not eliminate the spread within sessions.

## Limits

- Full fills use a predictable single-level book; resting additions grow an empty book. Neither covers realistic mixed traffic, fixed-depth steady state, cancellation, partial fills, SMP, or changing level counts.
- Smaller batches amortize framework costs differently and also change footprint, cache/allocator behavior, and population trajectory. Their rate differences cannot be assigned solely to timer overhead.
- The empty callback excludes logging, serialization, queues, persistence, and downstream trade processing. Network ingestion, concurrency, setup, and cleanup are also excluded.
- Results depend on this machine, OS, standard library, allocator, toolchain, native code generation, and LTO. Core placement, frequency, thermal conditions, and background activity are uncontrolled; other platforms are unvalidated.
- Throughput reciprocals are amortized costs, not individual-order median or p99 latency. `mach_absolute_time()` advances in 125/3 ≈ 41.7 ns ticks, coarser than the roughly 24.5 ns amortized resting-addition cost, so that timer cannot resolve single calls at this rate.

## Reproduction

Provide a C++20 toolchain, CMake 3.16+, Git, Bash, and Python 3.9+ (standard library only). The measured environment is macOS/Apple Silicon with Apple Clang 21.

From the repository root:

```bash
bash docs/evidence/2026-10-09-throughput/reproduce.sh
```

The helper prepares clean, pinned dependency checkouts in `.cache/throughput-deps/` and invokes [the runner](../scripts/run_throughput.sh). Initial downloads require network access; CMake builds use the prepared sources without downloads. `THROUGHPUT_DEPS_DIR` selects a cache, and `CXX` selects a compiler. To use prepared checkouts directly:

```bash
BENCHMARK_SOURCE_DIR="$PWD/.cache/throughput-deps/benchmark" \
GOOGLETEST_SOURCE_DIR="$PWD/.cache/throughput-deps/googletest" \
CXX=/usr/bin/clang++ bash scripts/run_throughput.sh repaired
```

The runner gates measurements on the Debug ASan/UBSan suite and Python accounting tests, verifies Release flags and absence of sanitizers, then records two consecutive sessions with the settings above. It creates `benchmark_results/<UTC timestamp>-repaired/`, refuses existing destinations, and stops on build, test, or accounting failure. Output includes source snapshots, hashes, build/test logs, environment, raw JSON, and summaries. Use AC power; the runner does not change power settings or pin cores.

Correctness evidence contains **93 passing Debug C++ cases**, **72 relevant Release cases**, and **six Python accounting cases**. Release checks exclude assertion-dependent pool death tests. Verify source identity, artifact hashes, and result calculations without benchmarking:

```bash
PYTHONDONTWRITEBYTECODE=1 python3 docs/evidence/2026-10-09-current/verify_evidence.py
```
