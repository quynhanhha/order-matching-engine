# Profiling methodology and full results

This document describes the measurements in [this profiling bundle](README.md). It covers four things:
- how each measurement was taken and validated (§1–§4)
- the complete result matrices (§5)
- an audit note on the verification scope (§6)
- reproduction commands (§7)

Every value here is recomputed by [`verify_profiling_evidence.py`](verify_profiling_evidence.py).

## 1. Environment, code identity and tools

| Property | Value | Evidence |
|---|---|---|
| Machine | Apple M3 Pro (Mac15,6): 6 performance and 6 efficiency cores, 36 GiB. 128-byte cache line; 128 KiB L1D and 16 MiB L2 per performance cluster. | [environment-before.txt](environment-before.txt) |
| OS, compiler | macOS 26.5.2 (Darwin 25.5.0); Apple Clang 21.0.0 (`clang-2100.1.1.101`) | same |
| Power | AC power. No thermal or performance warning recorded before or after. Core placement and frequency not controlled. | [environment-before.txt](environment-before.txt), [environment-after.txt](environment-after.txt) |
| Flags | `-std=c++20 -O3 -DNDEBUG -march=native -flto` for the benchmark executable and the harness | [harness/build.sh](harness/build.sh), [throughput link command](../2026-10-09-throughput/link-command.txt) |
| Benchmark executable | The archived throughput executable (SHA-256 `ce8ce5ee…b17b6`, built from `a8edeb4`), rechecked before and after | [preflight.txt](preflight.txt), [postflight.txt](postflight.txt) |
| Harness source tree | Clean `0bad3d2`. Differs from `a8edeb4` only in comments. Engine translation units are token-identical under `-DNDEBUG`. | [source-equivalence.txt](source-equivalence.txt) |
| Machine-code identity | `addLimitOrder` is instruction-identical in the executable and the harness builds | [disassembly/codegen-identity.txt](disassembly/codegen-identity.txt) |
| Harness revisions | Three source revisions. Edits affected only `levelsK` input generation; timed code is identical. | [harness/PROVENANCE.md](harness/PROVENANCE.md) |

**Tools** ([tools-availability.txt](tools-availability.txt), re-queried at publication):

| Method | Status | Establishes | Cannot establish |
|---|---|---|---|
| `/usr/bin/sample` on the unmodified executable | available | wall-clock stacks every 1 ms; shares by function | breakdown inside inlined code |
| In-process `SIGPROF` PC sampler (harness) | built for this study | instruction-level histogram restricted to timed regions | precise attribution (samples can land on or after the stalling instruction). The rate is low: 229–374 samples per timed second, against 4 kHz requested. |
| `proc_pid_rusage` (`ri_instructions`, `ri_cycles`, `ri_p*`) | available, no privileges | instructions, cycles, IPC per window | cache misses, branch mispredictions, causes of stalls |
| Replacement `operator new`/`delete` | separate build | exact allocation and free counts, size histogram | allocation time (no timing is reported from this build) |
| `otool -tvV` | available | inlining, calls, data sizes | runtime cost |
| Instruments / `xctrace`, `llvm-mca` | unavailable (Command Line Tools only) | — | — |
| DTrace | blocked by System Integrity Protection | — | — |
| Hardware event counters (kperf) | need root; not used | — | — |
| Linux `perf`, Valgrind, VTune | unavailable | — | — |

## 2. Sampling the archived benchmarks

Procedure ([samples/run_sample.sh](samples/run_sample.sh)):
1. Run one benchmark case with `--benchmark_min_time=40s`; the workload is unchanged.
2. Wait 4 s.
3. Record 25 s with `sample <pid> 25 1`.

**Separating timed from untimed work.** Each stack is assigned to the call site in the benchmark function that leads to it, using return-address offsets ([mapping](disassembly/measured-benchmark-call-sites.txt)). The `TIMED` site is the `addLimitOrder` call between `ResumeTiming` and `PauseTiming`; construction, prepopulation and teardown are reported separately. Leaf frames are classified by symbol after image names are stripped ([summarize_samples.py](samples/summarize_samples.py), outputs in `samples/*.summary.txt`). The rates measured during sampling were inside the archived ranges (§5.1).

## 3. Profiling harness

[harness/prof_harness.cpp](harness/prof_harness.cpp) is a profiling-only program, outside the project build and tests.

- **Inputs:** it reuses the benchmark input generators and validators in [`throughput_workloads.h`](../../../benchmarks/throughput_workloads.h).
- **Book instantiation:** it uses the benchmark's `OrderBook<void(*)(const Trade&)>` instantiation.
- **Batches:** each batch builds a fresh book outside timing, times only the API calls of interest, checks the post-state outside timing, then destroys the book outside timing.
- **Repetitions:** after 1 s of warmup, batches repeat for a fixed wall time per repetition, normally 5 repetitions of 2 s. The reported value is the median ns per call.

Four build variants come from the same source, so instrumentation never affects primary timing:

| Variant | Instrumentation | Reports |
|---|---|---|
| `prof_timing` | none | ns per call (primary timing) |
| `prof_rusage` (`-DUSE_RUSAGE`) | `proc_pid_rusage` before and after each timed batch | instructions, cycles, IPC, performance-core fractions |
| `prof_alloc` (`-DCOUNT_ALLOCS`) | every throwing, non-throwing, sized and aligned `new`/`delete` overload replaced. Counting is active only inside timed batches and uses plain counters and a fixed histogram, with no allocation or I/O. | allocation and free counts, size histogram |
| `prof_pcsamp` (`-DPC_SAMPLE`) | `ITIMER_PROF` at 250 µs. The handler stores the interrupted PC only inside timed batches; symbolization happens after the run. | PC histogram by symbol and by offset |

**Workloads.** Each is validated in an untimed replay before timing.

| Workload | Definition | Calls timed per batch |
|---|---|---|
| `add/N` | The archived resting-addition inputs: seed 42; bids 90–99, asks 110–119; quantities and participants 1–100 | N additions |
| `match/N` | The archived one-to-one inputs: N quantity-1 sells at price 100, then N/2 crossing buys | N/2 incoming orders |
| `partial/N` | As `match/N`, but each resting order has quantity N, so every incoming order partially fills resting order 1 | N/2 incoming orders |
| `sweepk/L` | L ask levels with one quantity-1 order each. Each buy has quantity k and removes k levels. | L/k incoming orders |
| `cancel-fifo/N`, `cancel-shuffled/N` | Cancel all orders of the `add/N` book in ID order, or in a fixed shuffled order (seed 123) | N cancellations |
| `levelsK/N` | N non-crossing additions uniform over K prices per side; seed 42 or `GLEVELS_SEED` | N additions |
| `iso-umap-insert`, `iso-umap-erase` | Standalone `unordered_map<uint64_t, Order*>` with the same reserve, load factor and key sequence | N inserts or N/2 erases |
| `iso-new32`, `iso-delete32` | Bare 32-byte `operator new` or `delete` with the same counts | N or N/2 |
| `control-empty`, `control-chain/K` | An empty timed region; a K-iteration loop of exactly 4 instructions with a 2-cycle dependency chain | 1 or K |

Every workload, including the 100,000- and 1,000,000-order cases, passed a Debug build with AddressSanitizer, UndefinedBehaviorSanitizer and engine assertions enabled ([asan/summary.txt](asan/summary.txt)).

## 4. Counter validation and suppression

Every `proc_pid_rusage` window includes the cost of the query itself.

- **Bracket overhead:** an empty timed region costs 6,024.6 instructions and 1,378.8 cycles per window (median of 5 repetitions). This is subtracted from every result ([rusage/analyze.py](rusage/analyze.py)).
- **Accuracy:** the 4-instruction control loop reads 4.001 instructions and 2.004 cycles per iteration uncorrected at 10 million iterations. After correction it reads 4.00 instructions and 2.00–2.01 cycles at 100,000 and 10 million iterations, matching its construction.

A counter result is reported only if all three gates pass:

| Gate | Threshold |
|---|---|
| bracket overhead as a share of window instructions | below 5% |
| repetition-to-repetition coefficient of variation of corrected instructions and cycles | below 2% |
| instrumented ns per call vs uninstrumented build | within 3% |

Results that failed a gate and are reported with timing only ([rusage/analysis.txt](rusage/analysis.txt)):
- `control-chain/1000`
- `sweep1/2000`
- `add/1000000`
- `cancel-fifo/1000000`
- `cancel-shuffled/1000000`
- the first runs of `iso-delete32` and `iso-umap-insert`, which showed bimodal timing. Their repeats (`*.run2.txt`) pass and are used.

**Cross-run variation** is larger than within-run variation. Identical inputs with identical timed code, measured in separate runs, differ by up to 6.8 cycles per addition ([harness/PROVENANCE.md](harness/PROVENANCE.md)).

**Kernel accounting** attributes 95.7–100% of instructions to performance cores per window, and derived cycles per nanosecond range from 3.5 to 4.0. Neither is evidence of core placement or frequency.

## 5. Complete results

### 5.1 Sampling of the archived benchmarks ([samples/](samples/))

| Benchmark | Rate during sampling | Archived range | Timed samples | `new` path | `delete` path (memset part) | Hash-table code | Inlined engine | Callback |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| `BM_AddOnly_Resting/10000` | 41.486M/s | 39.871–42.162M/s | 14,041 | 44.9% | — | 11.1% | 43.9% | — |
| `BM_AddOnly_Resting/1000` | 39.067M/s | 38.723–40.224M/s | 12,931 | 43.9% | — | 10.3% | 45.4% | — |
| `BM_MatchOneToOne/10000` | 53.462M/s | 51.133–54.254M/s | 5,866 | — | 54.1% (10.8%) | 20.4% | 23.9% | 1.6% |
| `BM_MatchOneToOne/1000` | 48.948M/s | 47.790–49.690M/s | 5,538 | — | 55.8% (10.7%) | 19.7% | 23.0% | 1.5% |

**Untimed work:**
- Addition teardown, which frees every index node, took 28.4% of that benchmark's samples. 22.4% of the teardown samples were memset or bzero inside `free`.
- Matching prepopulation took 49.7% of its benchmark's samples.

### 5.2 Harness agreement with the archive ([timing/](timing/), [throughput summary](../2026-10-09-throughput/summary.json))

| Case | Harness ns per call → M/s | Archived median |
|---|---:|---:|
| addition, N = 10,000 | 23.69 → 42.21 | 41.478M/s |
| one-to-one fill, N = 10,000 | 18.31 → 54.63 | 53.245M/s |
| addition, N = 100 | 23.11 → 43.27 | 31.293M/s |
| one-to-one fill, N = 100 | 18.68 → 53.54 | 27.217M/s |

Further harness timings: addition at N = 1,000 is 23.99 ns, and a one-to-one fill at N = 1,000 is 18.31 ns.

### 5.3 Per-call costs ([timing/](timing/), [rusage/analysis.txt](rusage/analysis.txt), [alloc/](alloc/))

| Workload | ns per call | Instructions | Cycles | IPC | Heap operations per call |
|---|---:|---:|---:|---:|---|
| `add/10000` | 23.69 | 417.2 | 90.3 | 4.6 | 1 `new` (32 B); none larger (also at N = 100 and 1,000) |
| `match/10000` | 18.31 | 420.3 | 70.0 | 6.0 | 1 `delete` (also at N = 100 and 1,000) |
| `partial/10000` | 4.77 | 127.0 | 18.7 | 6.8 | 0 |
| `sweep1/2000` | 18.63 | suppressed | | | 1 `delete` |
| `sweep10/2000` | 160.62 (16.06 per fill) | 3,399.1 (339.9 per fill) | 620.8 | 5.5 | 10 `delete` |
| `sweep100/2000` | 1,507.38 (15.07 per fill) | 33,130.0 (331.3 per fill) | 5,914.7 | 5.6 | 100 `delete` |
| `cancel-fifo/10000` | 23.49 | 343.5 | 86.5 | 4.0 | 1 `delete` |
| `cancel-shuffled/10000` | 38.53 | 359.5 | 145.1 | 2.5 | 1 `delete` |
| `iso-umap-insert` (includes `new`) | 13.05 | 297.7 | 48.9 | 6.1 | — |
| `iso-umap-erase` (includes `delete`) | 13.55 | 278.3 | 51.2 | 5.4 | — |
| `iso-new32` | 10.55 | 232.2 | 39.6 | 5.9 | — |
| `iso-delete32` | 10.46 | 203.3 | 38.7 | 5.3 | — |

### 5.4 Depth ([timing/](timing/), [rusage/analysis.txt](rusage/analysis.txt))

Runs at 100,000 and 1,000,000 orders used 3 repetitions of 3 s.

| Workload | 10,000 | 100,000 | 1,000,000 |
|---|---:|---:|---:|
| addition ns (instructions / cycles) | 23.69 (417.2 / 90.3) | 23.85 (416.5 / 91.0) | 23.83 (suppressed) |
| one-to-one fill ns (instructions / cycles) | 18.31 (420.3 / 70.0) | 18.83 (420.7 / 69.8) | 18.96 (420.7 / 69.5) |
| cancel in ID order, ns | 23.49 | 23.41 | 25.27 (range 23.42–28.16; suppressed) |
| cancel shuffled, ns (instructions / cycles) | 38.53 (359.5 / 145.1) | 44.35 (359.6 / 168.7) | 186.22 (range 182.00–193.18; suppressed) |

### 5.5 Price-level count ([rusage/scan-levels.summary.txt](rusage/scan-levels.summary.txt), [rusage/seed-levels.summary.txt](rusage/seed-levels.summary.txt))

Bracket-corrected medians of 3 repetitions of 1 s, 10,000 additions, harness revision rev2 ([provenance](harness/PROVENANCE.md)):

| K | 1 | 2 | 3 | 4 | 6 | 8 | 10 | 12 | 16 | 24 | 32 | 48 | 64 | 96 | 128 | 256 | 512 | 1024 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Instructions | 393.6 | 402.9 | 402.2 | 406.6 | 411.8 | 413.4 | 416.7 | 419.1 | 422.0 | 427.8 | 430.4 | 436.4 | 439.8 | 447.6 | 452.4 | 473.3 | 524.2 | 708.2 |
| Cycles | 65.8 | 64.6 | 65.3 | 86.5 | 67.8 | 80.0 | 90.3 | 77.7 | 76.4 | 85.3 | 75.3 | 93.6 | 75.2 | 106.2 | 80.7 | 92.3 | 116.9 | 178.6 |

Seed comparison (rev3; seeds 42, 7, 1234, 99): cycles per addition ranged as follows.

| K | 4 | 6 | 10 | 16 | 48 | 64 |
|---|---:|---:|---:|---:|---:|---:|
| Cycles | 87.3–88.0 | 68.9–70.0 | 91.1–96.3 | 78.2–83.2 | 96.8–97.1 | 76.0–81.6 |

At 2,048 levels per side (rev1, 5 × 2 s), an addition costs 90.41 ns, 1,274.5 instructions and 341.5 cycles. The scan's K = 10 inputs equal the archived `add/10000` inputs apart from a constant price offset ([provenance-audit.txt](harness/provenance-audit.txt)).

### 5.6 PC sampler ([pcsamp/](pcsamp/), [regions.txt](pcsamp/regions.txt), [disassembly excerpt](disassembly/harness-pcsamp-addLimitOrder-CancelWorkload.txt))

| Workload (timed samples) | Inlined engine | Allocator libraries | Hash table | Notable regions |
|---|---:|---:|---:|---|
| `add/10000` (4,372) | 54.9% | 32.8% | 10.7% | `lower_bound` 39.3% (bids 19.9%, asks 19.4%); pool allocate and init 6.9%; level hit/insert, append and emplace call 5.6% |
| `match/10000` (2,178) | 31.1% | 47.9% | 17.5% | pool allocate and init 10.9%; loop head, fill and dispatch 8.6%; callback 1.2% |
| `cancel-fifo/10000` (2,200) | 54.5% | 36.2% | 8.1% | `lower_bound` loop bodies 35.1% |
| `cancel-shuffled/10000` (3,352) | 73.3% | 20.7% | 5.3% | `lower_bound` loop bodies 26.5%. Dependent loads: bucket head (+0x130), node (+0x158), `Order` pointer and side (+0x1b0/+0x1b4), side-dispatch landing (+0x1c0). |

`addLimitOrder` region boundaries come from its disassembly, which is identical in the measured executable and all harness builds ([regions.py](pcsamp/regions.py)). Cancel `lower_bound` loop bodies are `[0x1dc,0x204)` and `[0x220,0x248)` within the inlined `CancelWorkload::batch`.

## 6. Verification scope

[`verify_profiling_evidence.py`](verify_profiling_evidence.py) recomputes the recorded quantitative values in this bundle. It also checks bundle hashes, Git visibility, recorded integrity results, the provenance audit outcome, and byte-for-byte regeneration of the derived summaries. Source/contract checks use the measured-source archive and the recorded source commit's README, not the current checkout's contract.

Code/disassembly checks refer to the measured source and the bundled excerpts. Examples: the 56-byte `Order` (divide by 56 via a multiply in `addLimitOrder`), 24-byte levels (stride `0x18`), 8-byte bucket pointers (`lsl #3`), and the indirect callback (`blr`).

## 7. Reproduction commands

Verify without measuring:

```bash
PYTHONDONTWRITEBYTECODE=1 python3 docs/evidence/2026-10-09-profiling/verify_profiling_evidence.py
```

Build the harness into a new ignored directory, from the repository root:

```bash
out="benchmark_results/$(date -u +%Y%m%dT%H%M%SZ)-profiling-repro"; mkdir -p "$out"
mkdir "$out/source"
tar -xzf docs/evidence/2026-10-09-throughput/measured-source.tar.gz -C "$out/source"
h=docs/evidence/2026-10-09-profiling/harness/prof_harness.cpp
common=(-std=c++20 -O3 -DNDEBUG -march=native -flto -I"$out/source/include" -I"$out/source/benchmarks" "$out/source/src/order_pool.cpp" "$out/source/src/price_level.cpp" "$h")
/usr/bin/clang++ "${common[@]}" -o "$out/prof_timing"
/usr/bin/clang++ "${common[@]}" -DUSE_RUSAGE -o "$out/prof_rusage"
/usr/bin/clang++ "${common[@]}" -DCOUNT_ALLOCS -o "$out/prof_alloc"
/usr/bin/clang++ "${common[@]}" -DPC_SAMPLE -o "$out/prof_pcsamp"
```

Invocation is `<binary> <workload> <N> [seconds per repetition] [repetitions] [PC-sample output file]`. For example:

```bash
"$out/prof_timing" add 10000 2 5
"$out/prof_rusage" control-empty 1 2 5
"$out/prof_alloc"  match 10000 1 2
"$out/prof_pcsamp" add 10000 4 5 "$out/pcsamp-add-10000.txt"
GLEVELS_SEED=7 "$out/prof_rusage" levels10 10000 1 3
```

The exact commands behind the published data are in [commands.sh](commands.sh). The [bundle README](README.md) expands its shorthand entries.

To rebuild the three harness revisions against the measured engine snapshot, use the source directory prepared above:

```bash
PROFILING_SOURCE_DIR="$PWD/$out/source" \
  bash docs/evidence/2026-10-09-profiling/harness/provenance_audit.sh
```

The audit compares binaries with its local archive when available, and checks revision code and generated-input equivalence. Without `PROFILING_SOURCE_DIR`, it builds against the current checkout; that does not reproduce this bundle's engine version.

**Re-sampling the benchmarks** needs a Release benchmark build, for example one produced by the [throughput reproduction](../../throughput-report.md#reproduction):

```bash
bin=path/to/build-release/order_book_bench
"$bin" --benchmark_filter='^BM_AddOnly_Resting/10000/real_time$' \
  --benchmark_min_time=40s --benchmark_min_warmup_time=1 &
sleep 4; /usr/bin/sample $! 25 1 -mayDie -file "$out/add10000.sample.txt"; wait
```

The call-site offsets that separate timed from untimed samples are specific to the recorded executable; a rebuild needs its own, taken from `otool -tvV`. Results depend on the machine, OS, allocator and toolchain. Compare new runs with the recorded evidence; do not substitute them for it.
