# Matching-engine hot paths

This report combines source inspection, batch API timings, and allocation counting. The [throughput report](throughput-report.md) defines the published API rates; the [evidence index](evidence/README.md#current-document-support) identifies diagnostic results, source hashes, and reproduction tools. **Measured** denotes an observation from these runs; **confirmed** denotes a source or layout fact; explanations of hardware behavior are hypotheses.

## Architecture and operation costs

The book uses a fixed-capacity pool of 56-byte orders, sorted vectors of 24-byte price levels, intrusive FIFO links, and an `unordered_map<uint64_t, Order*>` with maximum load factor 0.7. Each side reserves 4,096 levels, with the best at the back. Debug asserts the level bound; Release can reallocate. Sizes are specific to the measured toolchain.

Every `addLimitOrder` first checks the resting-ID index. A duplicate throws `std::invalid_argument` before allocation, mutation, or callback, including crossing and SMP submissions. Removal releases an ID for reuse; a partial resting remainder retains it.

| Path | Index work | Memory and book work |
|---|---|---|
| Resting addition | `contains`, then `try_emplace` | Pool allocation; price lookup/insertion; FIFO append; one index-node allocation |
| Fully executed incoming order | `contains`; `erase` per exhausted resting order | FIFO unlink; node free; pool returns; no incoming index insertion |
| Partial fill without removal | Incoming `contains` only | Quantity/aggregate updates and callback; no index-node allocation or free |
| Matching exhausts a level | No additional index work | Remove levels from the vector's back in O(1) per level |
| Cancellation | `find`, then `erase(iterator)` | O(log L) price lookup, FIFO unlink, node free; up to O(L) shifting if a level empties |

Construction allocates the pool, reserved level storage, and index buckets. Reserving buckets does not reserve map nodes. **Measured:** each resting addition allocates one 32-byte node; each exhausted or cancelled resting order frees one. Partial fills perform neither. Allocation counts exclude construction, prepopulation, and teardown. Exceptions and user callbacks can allocate independently.

## API timings

Diagnostic medians in nanoseconds per incoming API call, except isolated heap/container operations. Standard cases use 10,000 initial or submitted orders; sweeps use 2,000 quantity-one ask levels. Calls execute an empty callback.

| Workload | ns/call | Fills/call | Timed index-node allocations / frees per call |
|---|---:|---:|---:|
| Resting addition | 24.11 | 0 | 1 / 0 |
| One-to-one full fill | 19.11 | 1 | 0 / 1 |
| Partial fill, no resting removal | 5.31 | 1 | 0 / 0 |
| Fill with level removal | 19.45 | 1 | 0 / 1 |
| 10-level sweep | 161.29 | 10 | 0 / 10 |
| 100-level sweep | 1,493.21 | 100 | 0 / 100 |
| Cancellation, ascending ID | 23.09 | 0 | 0 / 1 |
| Cancellation, shuffled ID | 38.40 | 0 | 0 / 1 |
| Isolated map insertion | 13.22 | — | — |
| Isolated map erase, including free | 13.56 | — | — |
| Isolated `operator new(32)` | 10.85 | — | — |
| Isolated `operator delete` of 32 bytes | 10.29 | — | — |

Full fills cost about 13.80 ns more than partial fills, close to the isolated erase/free cost. Isolated allocation is also substantial relative to addition time. These observations support testing preallocated index-node storage; they are approximate comparisons across different cache and allocator states, not additive cost decompositions or predicted speedups. The level-removal case is close to the single-level full-fill case within the observed ranges.

## Depth and price-level count

**Measured:** increasing depth has a much larger effect on shuffled cancellation than on additions, full fills, or ascending-ID cancellation.

| Median ns/call | 10,000 orders | 100,000 | 1,000,000 |
|---|---:|---:|---:|
| Resting addition | 24.11 | 24.66 | 24.63 |
| One-to-one full fill | 19.11 | 19.33 | 20.28 |
| Cancellation, ascending ID | 23.09 | 23.59 | 23.87 |
| Cancellation, shuffled ID | 38.40 | 51.49 | 190.48 |

Shuffled cancellation at 100,000 orders ranges from 44.17 to 54.50 ns across repetitions; at 1,000,000 it ranges from 187.54 to 195.18 ns. Random cancellation follows dependent bucket → node → order accesses. Cache/TLB effects are plausible explanations, but are not measured.

For 10,000 additions distributed uniformly over K available prices per side (seed 42):

| K | Median ns/addition | K | Median ns/addition |
|---:|---:|---:|---:|
| 1 | 18.12 | 64 | 20.19 |
| 4 | 23.99 | 128 | 21.74 |
| 6 | 18.75 | 512 | 31.36 |
| 10 | 24.91 | 1,024 | 48.98 |
| 16 | 20.82 | 2,048 | 91.34 |
| 48 | 25.53 | | |

K is the generator's price range, not a guarantee that every price is populated. Cost is non-monotonic at small K and grows at large K. Binary search and vector shifting are confirmed work; their individual shares and the cause of the small-K pattern are unmeasured. A single seed and five repetitions cannot establish seed independence.

## Optimization candidates and correctness constraints

| Candidate | Motivation | Tradeoffs and validation |
|---|---|---|
| Preallocated map-node storage | One general-allocator node per resting order | More up-front memory; free-list exhaustion and bucket allocation handling; capacity/churn tests and an explicit allocation-contract update |
| Flat ID index | Removes node indirection; may help shuffled cancellation | Deletion and load bounds under churn; differential testing against `unordered_map` |
| Best-level check before binary search | Touch-concentrated traffic may avoid lookup work | Can penalize other price distributions; validate across level counts, seeds, and sparse books |
| Different price-level container | Large-K vector insertion shifts many elements | Ordering, locality, and level-removal tradeoffs; first establish representative sparse-book traffic |

Preallocated node storage is the first candidate to measure. Every experiment needs identical inputs, machine, compiler and flags, all throughput repetitions, the sanitizer suite, and partial-fill/sweep/cancellation diagnostics. Gains must exceed observed variation without correctness failures or material diagnostic regressions; no gain is established by these hypotheses.

After completed calls, the index must contain exactly the positive-quantity resting orders, with unique IDs across sides and participants. Preserve FIFO priority, aggregate sums, sorted nonempty levels, pool ownership, and SMP cancellation of the incoming remainder. Duplicate rejection must leave state and callbacks unchanged; cancellation and full execution must permit ID reuse. Concurrent access and callbacks that recursively mutate the book are unsupported.

## Method and reproduction

The [harness](evidence/2026-10-09-profiling/harness/prof_harness.cpp) builds against the engine and benchmark input generators. Timing and allocation counting use separate binaries so allocation instrumentation does not affect the timing results. Each workload warms up for one second; timings retain five repetitions of 0.5 seconds of wall time each, with only the API batches included in reported nanoseconds. Setup, prepopulation, validation, and destruction are excluded. Eight engine workload shapes also pass ASan/UBSan and engine assertions.

On macOS/Apple Silicon with Apple Clang 21, from the repository root:

```bash
profile_dir="benchmark_results/profile-$(date -u +%Y%m%dT%H%M%SZ)"
mkdir "$profile_dir"
clang++ -std=c++20 -O3 -DNDEBUG -march=native -flto \
  -Iinclude -Ibenchmarks src/order_pool.cpp src/price_level.cpp \
  docs/evidence/2026-10-09-profiling/harness/prof_harness.cpp \
  -o "$profile_dir/timing"
"$profile_dir/timing" add 10000 0.5 5
"$profile_dir/timing" match 10000 0.5 5
```

Use a new output directory for each run. Add `-DCOUNT_ALLOCS` for allocation counts; use `-O1 -g -fsanitize=address,undefined` without `-DNDEBUG` for sanitizer checks. The [command record](evidence/2026-10-09-current/profiling-commands.json) lists all workload sizes and build variants; the [summary](evidence/2026-10-09-current/profiling-summary.json) retains medians and full ranges.

These batch diagnostics use one platform and synthetic inputs. CPU placement, frequency, thermal conditions, and background activity are uncontrolled. Sampling shares, instruction/cycle counts, cache/TLB misses, and branch mispredictions are not established by these measurements; diagnostic rates do not replace the validated throughput protocol or establish individual-order latency.

Verify hashes, source identity, and result calculations without running workloads:

```bash
PYTHONDONTWRITEBYTECODE=1 python3 docs/evidence/2026-10-09-current/verify_evidence.py
```
