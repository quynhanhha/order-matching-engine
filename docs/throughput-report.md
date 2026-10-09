# Synthetic batch API throughput

## Scope and summary

Measured on 2026-10-09: **41.478 million resting additions/sec** for batches of 10,000 non-crossing additions, and **53.245 million incoming one-to-one full fills/sec** for batches of 5,000 against 10,000 resting orders. Each is the median of ten rates from two consecutive process runs, with five repetitions per run.

The experiment measures single-threaded, synthetic, amortized batch API throughput with an empty trade callback. It excludes setup and teardown and preserves allocation/deallocation performed inside API calls. It does not measure production exchange capacity or individual-order latency percentiles.

## Source version and environment

The [measured source archive](evidence/2026-10-09-throughput/measured-source.tar.gz) contains the 21 build, engine, benchmark, test, and runner files used in the experiment. Every file matches existing commit **`a8edeb48184757bfab19abc042a66788a5deb5d5`** byte-for-byte. This commit identifies the measured code; the measurement's original recorded base was `3c3541a913269f44a10dc783d69078fb68541e62` plus the captured benchmark changes. The [source manifest](evidence/2026-10-09-throughput/source-sha256.txt) and [provenance record](evidence/2026-10-09-throughput/provenance.json) are the precise identities.

| Property | Recorded value |
|---|---|
| CPU | Apple M3 Pro, 12 physical/logical cores; model Mac15,6 |
| Memory | 36 GiB (38,654,705,664 bytes) |
| OS | macOS 26.5.2; Darwin 25.5.0; arm64 |
| Compiler | Apple Clang 21.0.0, `clang-2100.1.1.101` |
| CMake | 4.2.0 |
| Project standard | C++20 |
| Release project flags | `-O3 -DNDEBUG -march=native -flto -std=c++20 -arch arm64` |
| Google Benchmark | v1.8.3, `344117638c8ff7e239044fd0fa7085839fc03021` |
| Google Test | v1.14.0, `f8d7d77c06936315286eb55f8de22cd23c188571` |
| Power | AC power; low-power mode disabled |
| CPU controls | Affinity, core placement, and frequency not controlled |

The [compiler commands](evidence/2026-10-09-throughput/compile_commands.json) retain effective flags, including repeated target optimization flags and strict core-library warnings. The [link command](evidence/2026-10-09-throughput/link-command.txt) records LTO at linkage. Google Benchmark itself is compiled as C++11; engine and benchmark project sources use C++20. Environment records [before](evidence/2026-10-09-throughput/environment-before.txt) and [after](evidence/2026-10-09-throughput/environment-after.txt) include power/thermal queries. “No recorded warning” does not establish a fixed temperature, frequency, or absence of interference.

## Workload definitions

### Non-crossing resting additions

- N = 100, 1,000, or 10,000 API calls per batch; each iteration starts with an empty book of capacity N + 100.
- `std::mt19937_64`, seed 42; alternating buy/sell sides; uniform bids 90–99, asks 110–119, quantities 1–100, and participant IDs 1–100. IDs are unique and sequential.
- All N orders rest across up to 20 price levels. Depth grows from zero to N; zero trades and zero SMP cancellations are expected.
- Resting index-node allocation remains inside the measured `addLimitOrder` calls. Pre-reserved buckets do not eliminate node allocations.

### Single-price-level, one-to-one full fills

- N = 100, 1,000, or 10,000 initially resting sells at price 100, quantity one, participant 1; capacity N + N/2 + 100.
- N/2 incoming buys at price 100, quantity one, participant 2, with disjoint sequential IDs. Each fully fills the next resting FIFO order and produces one trade.
- Depth falls from N to N/2. The sole price level remains present: this is a **favorable case that avoids price-level removal, partial fills, SMP, and multi-level lookup/sweeping**.
- Pool operations and index-node erase/deallocation are timed. Incoming full fills do not insert an incoming index node. The empty callback receives trades but performs no useful trade-output processing.

The matching registration argument denotes initial population N, not incoming-call count. Its 10,000 case times **5,000 incoming calls and 5,000 full fills**, not 10,000 incoming orders.

## Measurement methodology

Each benchmark precomputes input outside timing, then validates it in an untimed replay with a recording callback. Validation checks trade counts, IDs, prices and quantities; complete final book contents and FIFO links; order indexing through cancellation; and absence of SMP cancellations. The actual timed matching replay also checks final best prices and aggregate quantity outside measurement. Reported outcome counters describe the validated deterministic trace, rather than timed callback bookkeeping.

Each iteration constructs a fresh book. Construction, matching-book prepopulation, validation, and destruction are excluded with `PauseTiming()`/`ResumeTiming()`. **API-internal memory costs are retained**, including resting hash-node allocation and filled-order hash-node deallocation. Input traversal, observation barriers, and residual framework timing-transition overhead remain part of the measurement. The book address escapes to optimization barriers; calls to `addLimitOrder` and its allocation/deallocation paths were retained in optimized output.

Setup and teardown exclusion is material to interpretation. In particular, teardown frees all N index nodes after a resting-add batch; including it adds per-order work to an add-plus-destruction measurement and materially affects the rate being reported.

Both primary cases use Google Benchmark's `UseRealTime()`. For each repetition:

`incoming API calls/sec = calls per batch / measured elapsed seconds per batch`

`SetItemsProcessed` counts all incoming calls across iterations. CPU time is retained as a separate field. Paused work is excluded from both primary timing intervals, so the rate is not whole-process sustained throughput including setup and cleanup.

Two **consecutive process runs on the same machine** each retain five repetitions per case. They are not independently controlled experiments. Each case requests one second of warmup and at least one second of measured time, with adaptive iteration counts and randomized repetition interleaving. No repetitions are discarded.

The result checker requires all six cases and five unique repetition indices in each run, reconciles rates with elapsed time and call counts, and validates outcome counters. The summary uses the median of all ten rates, their full range, sample coefficient of variation (standard deviation divided by mean), and absolute difference between the two run medians divided by their mean.

## Results and variability

Rates below are millions of incoming API calls per measured elapsed second. For resting additions, N is batch size and initial depth is zero; for full fills, N is initial depth and batch size is N/2.

| Workload | N | Calls/batch | Median M/sec | Full range M/sec | Sample CV | Run-median difference |
|---|---:|---:|---:|---:|---:|---:|
| Resting additions | 100 | 100 | 31.293 | 30.188–31.859 | 1.69% | 0.74% |
| Resting additions | 1,000 | 1,000 | 39.860 | 38.723–40.224 | 1.28% | 1.16% |
| Resting additions | 10,000 | 10,000 | 41.478 | 39.871–42.162 | 1.78% | 0.06% |
| One-to-one full fills | 100 | 50 | 27.217 | 26.545–27.718 | 1.53% | 1.91% |
| One-to-one full fills | 1,000 | 500 | 49.038 | 47.790–49.690 | 1.14% | 0.54% |
| One-to-one full fills | 10,000 | 5,000 | 53.245 | 51.133–54.254 | 1.84% | 1.60% |

At N = 10,000, resting additions have 1.78% sample CV and 0.06% run-median difference; full fills have 1.84% CV and 1.60% run-median difference. These describe observed variability, not a confidence interval or a guarantee of future performance. [Raw run 1](evidence/2026-10-09-throughput/session-1.json), [raw run 2](evidence/2026-10-09-throughput/session-2.json), and the [machine-readable summary](evidence/2026-10-09-throughput/summary.json) preserve every repetition and statistic.

## Interpretation and limitations

The favorable full-fill case has a narrow, predictable distribution and retains one populated price level throughout. It does not establish throughput for changing level counts, partial fills, SMP, cancellations, or realistic mixed traffic. The resting case grows an initially empty book; it is not a fixed-depth steady-state workload.

The smaller batches report lower rates. Fixed timing/framework costs amortize differently, and batch size also changes book footprint, allocator/cache behavior, and population trajectory. The differences cannot be attributed solely to timer overhead. Large-batch figures remain synthetic, warmed measurements with real allocator costs under these conditions.

The empty callback excludes logging, serialization, event queues, persistence, and downstream trade processing. The experiment also excludes network ingestion and queueing, concurrency, and setup/teardown costs. CPU placement and other machine activity were not independently controlled. The measurements have not been replicated across machines, operating systems, or toolchains; `-march=native`, LTO, the standard library, and allocator affect results.

Throughput reciprocals are amortized costs, not individual-order latency samples. No individual-operation median or p99 latency is established by this experiment. On the measured machine, `mach_absolute_time()` advances in ticks of 125/3 ≈ 41.7 ns (24 MHz timebase), coarser than the roughly 24 ns amortized cost per resting addition reported here, so single operations cannot be resolved with that timer.

## Reproduction

### Supported environment and dependency preparation

The recorded protocol was exercised on macOS/Apple Silicon with Apple Clang 21 and CMake 4.2.0. Install the platform command-line toolchain and provide CMake 3.16 or newer, Git, Bash, and Python 3.9 or newer. Python uses only its standard library. Other Clang/GNU environments may build the project, but the published rates and reproduction helper have not been validated there.

From a fresh clone's root, this helper acquires the project's existing dependencies at the exact recorded versions and invokes the unchanged runner:

```bash
bash docs/evidence/2026-10-09-throughput/reproduce.sh
```

The helper downloads Google Benchmark v1.8.3 and Google Test v1.14.0 into `.cache/throughput-deps/`, checks their exact commits and clean working trees, and then sets the runner's dependency-path variables. It reuses valid existing checkouts without overwriting them. Initial acquisition requires network access; subsequent CMake configuration is disconnected.

Equivalent explicit preparation and invocation:

```bash
mkdir -p .cache/throughput-deps
git clone --depth 1 --branch v1.8.3 https://github.com/google/benchmark.git \
  .cache/throughput-deps/benchmark
git clone --depth 1 --branch v1.14.0 https://github.com/google/googletest.git \
  .cache/throughput-deps/googletest

test "$(git -C .cache/throughput-deps/benchmark rev-parse HEAD)" = \
  344117638c8ff7e239044fd0fa7085839fc03021
test "$(git -C .cache/throughput-deps/googletest rev-parse HEAD)" = \
  f8d7d77c06936315286eb55f8de22cd23c188571

BENCHMARK_SOURCE_DIR="$PWD/.cache/throughput-deps/benchmark" \
GOOGLETEST_SOURCE_DIR="$PWD/.cache/throughput-deps/googletest" \
CXX=/usr/bin/clang++ bash scripts/run_throughput.sh repaired
```

These clone commands assume absent destination directories; the helper handles existing clean caches. `THROUGHPUT_DEPS_DIR` selects another cache for the helper, and `CXX` selects a compiler. Use AC power and record other substantial machine activity; the helper does not change power settings or pin cores.

### Build, checks, and output

The runner first configures a separate Debug build with `BUILD_TESTS=ON`, `BUILD_BENCHMARKS=OFF`, and the supplied Google Test source directory. It builds and runs all C++ tests with ASan/UBSan, then the six Python accounting tests. It next configures Release with `BUILD_TESTS=OFF`, `BUILD_BENCHMARKS=ON`, and the supplied Google Benchmark source directory; builds `order_book_bench`; verifies optimization/LTO/C++20 flags and absence of sanitizers; records the binary hash; and runs:

```bash
# The runner executes this for each of two consecutive process runs.
"$results/build-release/order_book_bench" \
  --benchmark_filter='^(BM_AddOnly_Resting|BM_MatchOneToOne)/[0-9]+/real_time$' \
  --benchmark_min_time=1s --benchmark_min_warmup_time=1 \
  --benchmark_repetitions=5 --benchmark_enable_random_interleaving=true \
  --benchmark_out="$results/session-$session.json" --benchmark_out_format=json
```

`results` and `session` are runner variables, not commands to paste without initialization. See the [archived exact command trace](evidence/2026-10-09-throughput/commands.sh) and archived runner in the source archive for all CMake/build options and inline Python steps; recorded absolute paths describe the measured machine, not required paths for a fresh clone.

New files go to `benchmark_results/<UTC timestamp>-repaired/`: raw JSON and console output, summaries, environment records, compiler commands, source snapshots/manifests, binary hash, and test/build logs. The runner refuses to overwrite an existing directory and stops on build, correctness, or accounting failure. Do not replace the published evidence with a new run without reviewing its source and conditions.

To rebuild the **exact measured code**, use a separate full-history clone checked out at `a8edeb48184757bfab19abc042a66788a5deb5d5`, then run the explicit dependency preparation and runner invocation above. The repository evidence helper is a later documentation addition and need not exist at that older checkout. To inspect the recorded source without checking out that commit, use the source archive and manifest. Matching source does not guarantee identical rates or executable hashes under a different toolchain, SDK, path, or machine.

## Supporting evidence

The [compact repository bundle](evidence/2026-10-09-throughput/README.md) contains raw JSON/console output, summary statistics, recorded commands, environment and compile/link flags, dependency identities, a hashed measured-source archive, executable provenance, and correctness logs. It excludes executables, generated build trees, and the large disassembly dump.

Correctness evidence records **81 passing C++ sanitizer-enabled tests**, including nine focused workload tests, and **six passing Python accounting tests** before measurement. The Python fixtures are explicitly synthetic tests, not reported measurements. Inspect the [C++ log](evidence/2026-10-09-throughput/tests-sanitizers.txt) and [Python log](evidence/2026-10-09-throughput/tests-summary.txt).

Validate the bundle without running benchmarks or rewriting recorded results:

```bash
PYTHONDONTWRITEBYTECODE=1 python3 \
  docs/evidence/2026-10-09-throughput/verify_evidence.py
```

The verifier checks artifact/source hashes, measured-code commit identity, all raw repetitions and rate/outcome accounting, the summary, dependency/test records, and that evidence files are not Git-ignored. No measurements need to be repeated to inspect the published result calculations.
