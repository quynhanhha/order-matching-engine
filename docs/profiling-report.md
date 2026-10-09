# Hot-path profiling report

This report examines where the matching engine spends time in its two validated throughput workloads, plus several workloads those benchmarks do not cover. It is diagnostic: no engine, benchmark or test code was changed, and no optimization has been implemented or measured. The validated throughput figures remain those in the [throughput report](throughput-report.md).

Evidence:
- **Raw data:** the [evidence bundle](evidence/2026-10-09-profiling/README.md).
- **Methods and full results:** [METHODOLOGY.md](evidence/2026-10-09-profiling/METHODOLOGY.md).
- **Verification:** the [verifier](evidence/2026-10-09-profiling/verify_profiling_evidence.py) recomputes every number below from the raw files.

Labels used throughout:
- **Measured**: observed experimentally on the recorded machine.
- **Confirmed**: established from source code or disassembly.
- **Hypothesized**: consistent with the data but not directly measured.

## 1. Executive summary

- **Order-ID index heap traffic is the largest measured cost in both validated workloads.** Each resting addition allocates one 32-byte `std::unordered_map` node, and each removed resting order frees one (**Measured**, exact counts).
  - **Additions:** in the 10,000-order benchmark, allocation takes 44.9% of timed samples and hash-table code 11.1%.
  - **One-to-one fills:** freeing takes 54.1% and hash-table erase code 20.4%.
  - **Partial fills:** a fill that removes no resting order costs 4.77 ns, against 18.31 ns for a full fill.
- **Price-level lookup is the second hot spot for additions and cancellations.**
  - **Location (Measured):** the `lower_bound` searches over the sorted level vectors receive 39.3% of instruction-level samples for additions.
  - **Variation (Measured):** the cost of an addition varies with the number of levels per side by up to about 40 cycles, while instruction count barely changes.
  - **Cause:** not measurable here (**Hypothesized**).
- **Access order and depth matter for cancellation (Measured).** Random-order cancellation slows from 38.5 ns at 10,000 resting orders to 186 ns at 1,000,000, with an unchanged instruction count. Additions and fills are flat up to 1,000,000 orders.
- **Some operations are already cheap (Measured):** price-level removal during matching, the intrusive FIFO, and trade-callback dispatch (1.2–1.6%).
- **Correctness finding, separate from performance.** Duplicate order IDs are not detected, and they can leave live orders uncancellable ([§8.2](#82-correctness-finding-duplicate-order-ids)).
- **Conclusion.** Per-order allocation and free of index nodes is ready for a controlled implementation experiment ([§7](#7-optimization-hypotheses-and-tradeoffs)). The other candidates need more measurement or more representative workloads. No performance improvement is claimed.

## 2. Engine architecture and hot paths

`OrderBook<TradeCallback>` ([source](../include/order_book.h)) is made of:
- a fixed-capacity `OrderPool` of 56-byte `Order` objects with a LIFO free list
- two sorted `std::vector<PriceLevel>`, bids ascending and asks descending, with the best price at `back()`, each reserved to 4,096 levels
- an intrusive FIFO per 24-byte `PriceLevel`
- a `std::unordered_map<uint64_t, Order*>` index reserved to pool capacity with `max_load_factor(0.7)`

**Confirmed** in the [measured executable's disassembly](evidence/2026-10-09-profiling/disassembly/measured-addLimitOrder-cancelOrder.txt), built with `-O3 -march=native -flto`:
- **Inlined into `addLimitOrder`:** pool allocate and free, level append and remove, both matching loops, and the level search.
- **Out-of-line calls:** the hash-table operations, `operator new`/`delete`, level-vector insertion, and the trade callback.
- **Callback dispatch:** an indirect call only because the benchmark passes a function pointer.
- **Bucket selection:** uses a hardware divide (`id % bucket_count`).
- **Release builds:** keep writing the pool's `isAllocated_` flag, which only assertions read.

| Path | Index operations | Heap operations (**Measured**) | Other work |
|---|---|---|---|
| Non-crossing addition | `try_emplace`: divide, chain walk, link | 1 × `new(32)` | free-list pop; branch-free `lower_bound` with a data-dependent trip count; a new level shifts later levels: O(levels) |
| Full fill (per resting order) | `erase(id)` | 1 × `delete` | self-match check, fill arithmetic, indirect callback, FIFO unlink. The filled incoming order returns to the pool unindexed. |
| Partial fill | none | none | the resting order stays at the head |
| Level exhausted by matching | none | none | O(1) decrement and one `resize` (**Confirmed**) |
| Cancellation | `find`, then `remove(iterator)` | 1 × `delete` | `lower_bound`, unlink; an emptied level is erased by shifting later levels: O(levels − position) |

**Invariants any optimization must keep:**
- FIFO priority within a level.
- Each level's aggregate quantity equals the sum of its orders.
- The index holds exactly the resting orders.
- No empty levels, and both sides stay sorted with the best level at `back()`.
- Self-match prevention cancels the incoming remainder.
- Pool slots are either free or in use.

The 4,096-level capacity is asserted only in Debug builds; Release builds reallocate.

## 3. Methodology

**Environment.**
- **Machine:** Apple M3 Pro (6 performance and 6 efficiency cores, 128-byte cache lines), macOS 26.5.2, Apple Clang 21.0.0, AC power.
- **Flags:** `-std=c++20 -O3 -DNDEBUG -march=native -flto`.
- **Not controlled:** core placement and frequency ([environment](evidence/2026-10-09-profiling/environment-before.txt)).

**Code under test.**
- **Benchmark binary:** sampling used the archived throughput executable itself (SHA-256 `ce8ce5ee…`), rechecked before and after ([preflight](evidence/2026-10-09-profiling/preflight.txt), [postflight](evidence/2026-10-09-profiling/postflight.txt)).
- **Harness equivalence:** the profiling harness was built from a later commit that differs only in comments. Its `addLimitOrder` machine code is identical to the executable's ([source equivalence](evidence/2026-10-09-profiling/source-equivalence.txt), [codegen identity](evidence/2026-10-09-profiling/disassembly/codegen-identity.txt)).
- **Harness revisions:** the harness source was revised twice during profiling, only in the input generation of the price-level workload. Its timed code is identical across revisions, and rebuilding the published source reproduces the archived binaries byte-for-byte ([harness provenance](evidence/2026-10-09-profiling/harness/PROVENANCE.md)).

**Methods** (details in [METHODOLOGY §2–4](evidence/2026-10-09-profiling/METHODOLOGY.md#2-sampling-the-archived-benchmarks)):

1. **Stack sampling of the unmodified benchmarks.** macOS `sample` at 1 ms intervals for 25 s. Samples are split by call site into timed calls and untimed setup and teardown.
2. **A profiling-only harness.** It reuses the benchmark input generators, times only the API calls of interest on a fresh book per batch, and reports the median of repetitions. Separate builds add:
   - instruction and cycle counters (`proc_pid_rusage`)
   - exact allocation counting (replacement `new`/`delete`)
   - an in-process PC sampler

   None of these instruments the primary timing build.
3. **Controls and validation.**
   - The counters reproduce a known 4-instruction, 2-cycle loop (4.00 instructions and 2.00–2.01 cycles per iteration).
   - A fixed per-window overhead of 6,024.6 instructions and 1,378.8 cycles is subtracted.
   - Results that fail noise or instrumentation-overhead gates are suppressed.
   - Every harness workload passed AddressSanitizer, UndefinedBehaviorSanitizer and engine assertions.

**Tool limits** ([tools](evidence/2026-10-09-profiling/tools-availability.txt)): Instruments and `xctrace` are not installed, DTrace is blocked by System Integrity Protection, and hardware event counters require root and were not used. **Cache misses, TLB misses and branch mispredictions were therefore not measured.**

## 4. Measured results

### 4.1 Where the validated benchmarks spend time

Shares of samples taken at the timed call site ([sample summaries](evidence/2026-10-09-profiling/samples/)). Benchmark rates during sampling stayed inside the archived ranges.

| Benchmark | Timed samples | `new` path | `delete` path | Hash-table code | Inlined engine code | Callback |
|---|---:|---:|---:|---:|---:|---:|
| Additions, N = 10,000 | 14,041 | **44.9%** | — | 11.1% | 43.9% | — |
| One-to-one fills, N = 10,000 | 5,866 | — | **54.1%** | 20.4% | 23.9% | 1.6% |

N = 1,000 gives the same picture ([METHODOLOGY §5.1](evidence/2026-10-09-profiling/METHODOLOGY.md#51-sampling-of-the-archived-benchmarks-samples)). About a fifth of the free path (10.8 of the 54.1 percentage points) is the system allocator zeroing freed memory, a platform-specific behavior.

The harness is within 3% of the archive at N = 10,000: 42.21 vs 41.478M additions/s, and 54.63 vs 53.245M fills/s. At N = 100, its per-call cost stays flat. The archived rates at N = 100 are 24.6% (additions, 31.293M/s) and 48.9% (fills, 27.217M/s) below their N = 10,000 values. This suggests framework overhead dominates the archive's small-batch results (**Hypothesized**; [§5.2](evidence/2026-10-09-profiling/METHODOLOGY.md#52-harness-agreement-with-the-archive-timing-throughput-summary)).

### 4.2 Per-call costs

Harness medians ([timing](evidence/2026-10-09-profiling/timing/), [counters](evidence/2026-10-09-profiling/rusage/analysis.txt), [allocations](evidence/2026-10-09-profiling/alloc/)):

| Workload (10,000 orders unless noted) | ns per call | Instructions | Cycles | Heap operations per call |
|---|---:|---:|---:|---|
| Resting addition | 23.69 | 417.2 | 90.3 | 1 `new` (32 B) |
| One-to-one full fill | 18.31 | 420.3 | 70.0 | 1 `delete` |
| **Partial fill** (no resting order removed) | **4.77** | 127.0 | 18.7 | 0 |
| Fill plus level removal (2,000 levels) | 18.63 | suppressed | | 1 `delete` |
| 10-level sweep (2,000 levels; per fill) | 16.06 | 339.9 | — | 1 `delete` |
| Cancel, ID order | 23.49 | 343.5 | 86.5 | 1 `delete` |
| Cancel, shuffled order | 38.53 | 359.5 | 145.1 | 1 `delete` |
| Isolated `unordered_map` erase (includes `delete`) | 13.55 | 278.3 | 51.2 | — |
| Isolated `operator new(32)` | 10.55 | 232.2 | 39.6 | — |

Independent methods agree (**Measured**):
- **Full vs partial fill:** a full fill costs 13.5 ns more than a partial fill, matching an isolated erase plus free (13.55 ns).
- **Allocation share of an addition:** an isolated allocation is 44.5% of an addition, matching the 44.9% sampling share.
- **Level removal:** removing a level adds no measurable cost.

These decompositions are approximate, because isolated microbenchmarks run in different cache and allocator states. The full matrix, including 100-level sweeps and isolated insert and free, is in [METHODOLOGY §5.3](evidence/2026-10-09-profiling/METHODOLOGY.md#53-per-call-costs-timing-rusageanalysistxt-alloc).

### 4.3 Book depth

| ns per call | 10,000 | 100,000 | 1,000,000 |
|---|---:|---:|---:|
| Resting addition | 23.69 | 23.85 | 23.83 |
| One-to-one fill | 18.31 | 18.83 | 18.96 |
| Cancel, ID order | 23.49 | 23.41 | 25.27 (noisy: 23.42–28.16) |
| Cancel, shuffled | 38.53 | 44.35 | **186.22** |

Shuffled cancellation keeps the same instruction count at depth (359.5 and 359.6 at 10,000 and 100,000) while cycles rise from 145.1 to 168.7 (**Measured**). At 1,000,000 orders the counters were too noisy to report ([METHODOLOGY §5.4](evidence/2026-10-09-profiling/METHODOLOGY.md#54-depth-timing-rusageanalysistxt)).

### 4.4 Price-level count

Additions spread over K price levels per side ([full matrix](evidence/2026-10-09-profiling/METHODOLOGY.md#55-price-level-count-rusagescan-levelssummarytxt-rusageseed-levelssummarytxt)):

| Levels per side | 1 | 4 | 6 | 10 | 16 | 48 | 64 | 128 | 512 | 1024 | 2048¹ |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Instructions | 393.6 | 406.6 | 411.8 | 416.7 | 422.0 | 436.4 | 439.8 | 452.4 | 524.2 | 708.2 | 1,274.5 |
| Cycles | 65.8 | 86.5 | 67.8 | 90.3 | 76.4 | 93.6 | 75.2 | 80.7 | 116.9 | 178.6 | 341.5 |
| Cycle range over 4 seeds | | 87.3–88.0 | 68.9–70.0 | 91.1–96.3 | 78.2–83.2 | 96.8–97.1 | 76.0–81.6 | | | | |

¹ From a separate, longer run (5 × 2 s) with byte-identical inputs and identical timed code. Every other column comes from a single scan run (3 × 1 s). The seed ranges come from a third run.

**Measured:**
- Up to 128 levels, instructions rise by less than 60, while cycles vary non-monotonically between 64.6 and 106.2.
- The pattern follows the level count, not the input seed.
- The archived workload's 10 levels per side is an expensive point: 90.3 cycles, against 67.8 at 6 levels.
- Repeated runs of identical inputs differ by up to 6.8 cycles, so only larger differences are meaningful ([provenance](evidence/2026-10-09-profiling/harness/PROVENANCE.md#cross-run-consistency-of-the-same-inputs)).
- From 512 levels, shifting the level vector dominates. At 2,048 levels an addition costs 90.41 ns.

**Hypothesized:** the cycle variation comes from how predictable the branch-free search's exit is, or from a related front-end effect.

### 4.5 Instruction-level breakdown

PC-sampler shares of timed samples ([histograms](evidence/2026-10-09-profiling/pcsamp/), [regions](evidence/2026-10-09-profiling/pcsamp/regions.txt)):

| Workload | Inlined engine | Allocator | Hash table | Notable |
|---|---:|---:|---:|---|
| Additions | 54.9% | 32.8% | 10.7% | `lower_bound` 39.3% |
| One-to-one fills | 31.1% | 47.9% | 17.5% | pool allocate and init 10.9%; callback 1.2% |
| Cancel, ID order | 54.5% | 36.2% | 8.1% | `lower_bound` loops 35.1% |
| Cancel, shuffled | 73.3% | 20.7% | 5.3% | `lower_bound` loops 26.5%; hot dependent loads: bucket → node → `Order` |

The two samplers rank components the same way but differ on the allocation share of additions (44.9% vs 32.8%). The report therefore treats that share as roughly one-third to one-half.

## 5. Allocation and memory layout

**Measured:**
- **Per-call heap operations:** exactly one 32-byte node allocation per resting addition, and one free per removed resting order. No heap operation for fully filled incoming orders or partial fills.
- **No rehash:** no timed allocation exceeded 32 bytes.
- **Construction-time allocation:** the pool, level vectors and bucket array are allocated only at construction.

**Pool coverage (Confirmed and Measured):** the pool covers orders but not index nodes, which are the costliest per-call objects. An isolated allocation or free costs about 10.5 ns and 200–230 instructions.

**Layout (Confirmed):**
- 56-byte orders do not align with the 128-byte cache line.
- Index nodes come from the general allocator, unrelated in address to their orders.
- Each bucket holds an 8-byte pointer.

**Access order:**
- **Sequential access:** additions, fills and ID-order cancellations are insensitive to depth (**Measured**). Their accesses are sequential and prefetch-friendly (**Hypothesized**).
- **Random cancellation:** follows a chain of dependent loads (**Measured** hot spots), and its cost grows with depth at constant instruction count. Cache and TLB misses are the likely cause (**Hypothesized**).

**Level vector:** keeping the best level at the back makes level removal during matching O(1). Lookup and sparse-book shifting are the remaining costs (§4.4). The engine is single-threaded, so false sharing does not apply.

## 6. Bottleneck assessment

| Rank | Component | Evidence | Confidence | Affected | Hypothesis to test | Correctness risk |
|---|---|---|---|---|---|---|
| 1 | Index-node free and hash erase | **Measured** three ways: sampling 74.5% of fills; full-minus-partial 13.5 ns ≈ isolated 13.55 ns; PC sampling | High | Fills, sweeps, cancels | Serve nodes from preallocated storage | Index/book equality; duplicate IDs |
| 2 | Index-node allocation and insert | **Measured**: sampling 56.0%; PC sampling 43.6%; exactly 1 allocation per addition | High | Additions | As rank 1 | As rank 1 |
| 3 | Dense-book `lower_bound` | **Measured** location (39.3% of additions; 26.5–35.1% of cancels) and level-count variation; cause **not measured** | Medium (cost), Low (cause) | Additions, cancels | Check the best level first; compare branching and branch-free search | Sort order, insert position |
| 4 | Random cancellation at depth | **Measured** slowdown; **Hypothesized** cache/TLB cause | Medium | Cancels | Flat index without node indirection | As rank 1 |
| 5 | Level shifting on sparse books | **Measured** from 512 levels per side | Medium-high when sparse | Sparse books | Different level container, if sparse books matter | Ordering guarantees |
| — | Callback, `isAllocated_` writes, bucket divide | Callback **Measured** 1.2–1.6%; others **Confirmed** present, cost not isolated | — | — | Not a priority | — |

## 7. Optimization hypotheses and tradeoffs

None of these has been implemented or measured, and no gain is predicted. Each would be compared using:
- identical workloads, compiler, flags and machine
- the existing [throughput runner](../scripts/run_throughput.sh), with both sessions and all repetitions
- the full sanitizer test suite

**Experiment 1: preallocated node storage for the existing index** (recommended first).
- **Change:** an allocator for the index that serves single-node requests from a free list sized to pool capacity and forwards bucket-array requests to `std::allocator`. Hashing and container semantics stay the same.
- **Tradeoffs:** more up-front memory; node exhaustion must be impossible within pool capacity. The benefit depends on the platform allocator: zero-on-free inflates free cost here.
- **Tests:** existing tests under sanitizers, plus a capacity-churn test. The allocation test's expectation of one allocation per addition would need an explicit, reviewed update.
- **Revert if:** benchmark ranges overlap the baseline in both sessions, the partial, sweep or cancellation diagnostics regress beyond their observed ranges, or any test fails.

**Experiment 2: open-addressing ID index.** Pursue only if hash-table code or random cancellation remains significant after Experiment 1.
- **Change:** a flat, power-of-two table holding `{id, Order*}` inline, which removes the node indirection and the divide.
- **Risks:** deletion correctness, load bounds under churn, and explicitly defined duplicate-ID behavior (§8.2).
- **Validation:** a randomized differential test against `std::unordered_map`.

**Experiment 3: dense-book price-level lookup.**
- **Change:** check the best level before binary search, or scan briefly from it.
- **Prerequisite:** a validated workload with a realistic, touch-concentrated price distribution, because the current uniform 10-level case is an unusually expensive point (§4.4).
- **Risk:** may slow sparse books.
- **Validation:** the level-count × seed scan before and after.

## 8. Limitations and conclusions

### 8.1 Limitations

- **No hardware event counters:** cache misses, TLB misses and branch mispredictions are unmeasured. The causes given for ranks 3 and 4 are hypotheses.
- **Sampling precision:** the PC sampler ran at 229–374 samples per timed second and can attribute a stall to a later instruction. The two samplers disagree on one share (§4.5).
- **Run-to-run variation:** counter results can vary between runs by more than within-run variation (up to 6.8 cycles per addition). Some deep and small cases were suppressed by the validation gates.
- **Uncontrolled environment:** core placement and frequency were not controlled. Kernel accounting attributes 95.7–100% of instructions to performance cores, which is not a guarantee.
- **Single platform:** one machine, OS, allocator and toolchain, synthetic inputs, and an empty callback. The harness workloads are diagnostic and do not extend the published throughput claims.

### 8.2 Correctness finding: duplicate order IDs

This finding is independent of the performance analysis. It was reproduced with a sanitizer-enabled probe ([source](evidence/2026-10-09-profiling/correctness/dup_id_repro.cpp), [output](evidence/2026-10-09-profiling/correctness/output-asan-debug.txt)).

**Confirmed:**
- The README requires unique IDs, but the engine neither enforces nor reports duplicates.
- `try_emplace` silently ignores a second index entry while the order itself still rests.

**Measured:**
1. A resting duplicate has no index entry and can never be cancelled.
2. Filling the duplicate erases the index entry of the *other, still-resting* order with that ID, which then also becomes uncancellable.
3. The same happens across book sides.

No memory error occurs; the index and the book silently diverge. **Duplicate order IDs are unsupported and unsafe.** Callers must guarantee uniqueness until the engine rejects or asserts on duplicates.

### 8.3 Conclusions

Per-order heap allocation and free of order-ID index nodes is the dominant measured cost of both validated workloads, and of cancellations and sweeps. Three independent methods support this, and it justifies Experiment 1. Several items need more work before any change:
- **Price-level search:** its mechanism must be measured, and a representative price-distribution workload added.
- **Random cancellation at depth:** a decision is needed on whether deep random cancellation is a target.
- **Sparse-book shifting:** a decision is needed on whether sparse books are a target.

No optimization has been performed, and no performance change is claimed.

## 9. Reproduction and evidence

Verify every published value without running a benchmark:

```bash
PYTHONDONTWRITEBYTECODE=1 python3 \
  docs/evidence/2026-10-09-profiling/verify_profiling_evidence.py
```

Supporting material:
- **Rebuilding and re-running:** harness build commands, invocation and benchmark re-sampling are in [METHODOLOGY §7](evidence/2026-10-09-profiling/METHODOLOGY.md#7-reproduction-commands).
- **Harness provenance:** the audit can be reproduced with [provenance_audit.sh](evidence/2026-10-09-profiling/harness/provenance_audit.sh).
- **Command trace:** the [command trace](evidence/2026-10-09-profiling/commands.sh) lists every recorded command.

Results depend on the machine, OS, allocator and toolchain. New runs should be compared with this evidence, not substituted for it.
