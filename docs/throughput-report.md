# Reproducible synthetic throughput report

Measured 2026-10-09. These are API throughput measurements, not individual-order latency percentiles or production exchange capacity. No engine performance improvement is claimed.

## Source and environment

- Base commit: `3c3541a913269f44a10dc783d69078fb68541e62`, plus the uncommitted benchmark-rehabilitation files captured in the repaired run's `source/`, `source.patch`, and `source-sha256.txt`.
- Engine implementation and public API unchanged. The existing `docs/perf-notes.md` and README are unchanged.
- Apple M3 Pro, 12 physical/logical cores, 36 GiB memory, macOS 26.5.2 (Darwin 25.5.0), arm64.
- Apple Clang 21.0.0 (`clang-2100.1.1.101`), CMake 4.2.0, Google Benchmark v1.8.3. Google Test v1.14.0 is used only for correctness checks.
- Release project flags: `-O3 -DNDEBUG -march=native -flto -std=c++20 -arch arm64`. Core-library strict warning flags and repeated benchmark-target flags are recorded in `compile_commands.json`.
- AC power; low-power mode disabled. Affinity and frequency are not controlled. Power/thermal queries are saved before and after; no recorded warning is not a measurement of temperature or frequency. Other-machine reproducibility is not established.

## Workload and measurement definitions

Both cases are single-threaded, use precomputed inputs, a fixed-capacity order pool, the normal unordered-map index, and an empty trade callback. Each iteration uses a newly constructed book. API-internal allocation/deallocation costs remain timed; construction, prepopulation, validation, and destruction are excluded. Input-loop and optimization-barrier costs, plus residual framework timing-transition costs, remain included. Thus these are amortized batch API throughput measurements.

**Resting add:** seed 42 with `std::mt19937_64`; alternating buys/sells; bids uniformly 90–99, asks 110–119, quantities uniformly 1–100, participants uniformly 1–100; unique sequential IDs. Batch N is 100, 1,000, or 10,000; capacity N + 100; depth grows from zero to N across up to 20 price levels. All inputs rest; zero trades and zero SMP cancellations. Hash-node allocation inside each add is included.

**One-to-one matching:** N initial sells at one price level (100), each quantity 1 and participant 1; N/2 incoming buys at 100, each quantity 1 and participant 2, with disjoint sequential IDs. Capacity N + N/2 + 100. Every incoming order fully fills the next resting FIFO order and generates one trade. Depth falls from N to N/2; no incoming remainder and no SMP cancellation. Pool work and hash-node erase/deallocation during matching are included. Trade-output processing is excluded by the empty callback.

Untimed preflight replays the same generated inputs and capacity with a recording callback. It checks complete book contents, quantities, prices, participants, FIFO and links; cancellations after inspection also check that orders are indexed. Matching checks every trade's IDs, price and full quantity, plus all surviving orders. The measured matching replay additionally checks final best prices and total quantity outside timing. Outcome counters describe the validated deterministic trace, not additional timed callback instrumentation.

Google Benchmark uses `UseRealTime()` for both primary cases. `items_per_second` equals incoming calls per iteration divided by measured elapsed seconds per iteration; CPU time is retained separately. Timing excludes paused setup/cleanup, so this is not whole-process sustained throughput. The matching registration argument is initial population, not the incoming-call count.

Two independent process sessions each retain five repetitions. Each case requests one second of warmup and at least one second of measured time. Repetitions are randomly interleaved. The summary below uses all ten rates, their median, full range, sample standard deviation divided by mean (CV), and the absolute difference between session medians divided by their mean. No repetitions were discarded. Counts and rates reconcile against the raw JSON.

## Repaired results

Rates are millions of incoming API calls per measured elapsed second. The 10,000-population cases are the representative large-batch results; all smaller cases remain visible to show batch-size effects.

| Workload | Initial-population argument N | Incoming calls/batch | Median M/s | Full range M/s | Sample CV | Session median difference |
|---|---:|---:|---:|---:|---:|---:|
| Resting add | 100 | 100 | 31.293 | 30.188–31.859 | 1.69% | 0.74% |
| Resting add | 1,000 | 1,000 | 39.860 | 38.723–40.224 | 1.28% | 1.16% |
| Resting add | 10,000 | 10,000 | 41.478 | 39.871–42.162 | 1.78% | 0.06% |
| One-to-one matching | 100 | 50 | 27.217 | 26.545–27.718 | 1.53% | 1.91% |
| One-to-one matching | 1,000 | 500 | 49.038 | 47.790–49.690 | 1.14% | 0.54% |
| One-to-one matching | 10,000 | 5,000 | 53.245 | 51.133–54.254 | 1.84% | 1.60% |

The observed variability supports these narrowly described synthetic throughput results on this machine. Small batches show greater sensitivity to framework overhead. These measurements do not establish mixed-workload, multi-level-sweep, SMP, ingestion, queueing, persistence, or useful trade-output throughput. No individual-operation p50/p99 is reported or inferred by taking reciprocals.

Optimized disassembly in the repaired artifacts shows `addLimitOrder` calls between ResumeTiming and PauseTiming, with real allocation/deallocation code retained in the shared function. The engine's existing allocation tests also pass. Correctness gates passed before measurement: all 81 C++ tests under `-O0 -g -fsanitize=address,undefined`, including nine focused workload tests; six Python summary-accounting tests passed. Summary tests use explicitly synthetic fixtures, not fabricated benchmark results.

## Reproduction and durable evidence

From the repository root, with existing clean Google Benchmark v1.8.3 and Google Test v1.14.0 source checkouts:

```bash
bash scripts/run_throughput.sh repaired
```

The runner defaults to `build-release/_deps/benchmark-src` and `build/_deps/googletest-src`. Set `BENCHMARK_SOURCE_DIR` and `GOOGLETEST_SOURCE_DIR` to other existing pinned checkouts if needed; `CXX` selects a compiler. The runner refuses to overwrite an existing result directory, uses disconnected dependency configuration, gates measurements on sanitizer and accounting tests, checks effective Release flags, preserves source and binary hashes, and rejects incomplete/mismatched result JSON. No downloads or new dependencies are introduced. Python uses only its standard library.

Durable local artifacts (not temporary directories):

- [Repaired summary](../benchmark_results/20261009T035923Z-repaired/summary.txt), [raw session 1](../benchmark_results/20261009T035923Z-repaired/session-1.json), [raw session 2](../benchmark_results/20261009T035923Z-repaired/session-2.json).
- [Exact command trace](../benchmark_results/20261009T035923Z-repaired/commands.sh) and [archived runner](../benchmark_results/20261009T035923Z-repaired/source/scripts/run_throughput.sh). The runner contains the inline Python bodies used by stdin commands in the trace.
- [Compiler commands](../benchmark_results/20261009T035923Z-repaired/compile_commands.json), [machine metadata](../benchmark_results/20261009T035923Z-repaired/environment-before.txt), [sanitizer results](../benchmark_results/20261009T035923Z-repaired/tests-sanitizers.log), [source manifest](../benchmark_results/20261009T035923Z-repaired/source-sha256.txt), [source stability check](../benchmark_results/20261009T035923Z-repaired/source-stable.txt).
- [Legacy summary](../benchmark_results/20261009T035122Z-legacy/summary.txt) and its separate raw sessions and source snapshot. This fresh, fixed-engine legacy run uses CPU-time rates, includes destruction, and uses a different aggressive-order distribution; it is not a comparable improvement baseline. Historical executables were not executed.
- [Review stages](../benchmark_results/rehabilitation-review/) contain separate workload, timing, accounting, and reproducibility patches. No commit was created.

`benchmark_results/` is already Git-ignored. The artifacts persist locally but are not automatically published on GitHub; raw JSON, source snapshots and metadata need to accompany a public report as repository data or release assets. This report does not upload or publish anything externally.

## CV wording

“Built a C++20 order matching engine; measured approximately 41M resting additions/sec and 53M one-to-one matching orders/sec in reproducible single-threaded synthetic benchmarks on Apple M3 Pro.”

Use this with the workload definitions available for interview discussion: resting batches of 10,000 additions into an empty book, and 5,000 one-to-one incoming fills against 10,000 one-price-level resting orders, both using an empty callback. Do not describe these as latency guarantees, production capacity, or an improvement over the legacy methodology.

## Proposed commit boundaries (not committed)

1. Workloads identify resting additions and one-to-one fills: shared inputs, untimed validators, focused C++ tests, and their CMake test target.
2. Throughput timing covers operation batches consistently: setup/destruction boundaries, observation barriers, and elapsed-time registrations.
3. Benchmark rates distinguish incoming calls from trades: operation and validated-outcome counters.
4. Results include reproducible commands and provenance: runner, summary checker and its tests, and this separate report.
