# Evidence inventory and redundancy audit

## Current document support

The three current-facing documents use this minimal arrangement: one measurement bundle, one dependency-preparation helper, and one profiling harness. Shared tools stay at their canonical paths; they are not copied into each bundle.

| Documents / purpose | Canonical evidence or tooling |
|---|---|
| README and throughput results | [2026-10-09-current](2026-10-09-current/README.md): raw sessions, statistics, source identities, compiler/environment records, and correctness logs |
| Profiling results | The same bundle's [diagnostic summary](2026-10-09-current/profiling-summary.json) and `profiling/*.txt` |
| Throughput dependency preparation | [reproduce.sh](2026-10-09-throughput/reproduce.sh), invoking the root [runner](../../scripts/run_throughput.sh) |
| Diagnostic reproduction | [prof_harness.cpp](2026-10-09-profiling/harness/prof_harness.cpp), compiled against the source being measured |
| Current-source verification | [verify_evidence.py](2026-10-09-current/verify_evidence.py) |

`2026-10-09-current` is the primary evidence for the current-facing numbers. It is not self-contained: its helper and harness are shared with the other directories, and three metadata/log paths are relative links to canonical throughput-bundle files. The remaining directories preserve distinct experiments and the source/tooling needed to inspect and reproduce them. No bundle can be deleted under a requirement to preserve all measurement information and reproducibility.

## Exact duplicates and removal

A SHA-256 comparison of every file in all three bundles finds exactly three duplicate-content pairs; there are no byte-identical raw timing, counter, sampling, or throughput-session files between bundles.

| Current path | Canonical identical file | Duplicate bytes |
|---|---|---:|
| `2026-10-09-current/benchmark-commit.txt` | `2026-10-09-throughput/benchmark-commit.txt` | 41 |
| `2026-10-09-current/googletest-commit.txt` | `2026-10-09-throughput/gtest-commit.txt` | 41 |
| `2026-10-09-current/tests-summary.txt` | `2026-10-09-throughput/tests-summary.txt` | 104 |

These three current-bundle copies are replaced with relative symbolic links. Their logical paths, complete contents, and manifest content hashes remain intact. The identical Python log bytes do not imply a shared execution: bundle context still identifies each measurement's test record. This removes 186 bytes of duplicate payload, not a meaningful amount of disk space. Local verification follows the links; viewers that display link text can follow the canonical paths above.

Ten of the 21 files inside the throughput source archive also match files in the working tree byte-for-byte. The archive is retained intact: it preserves a complete measured-source snapshot, including the distinct header, benchmarks, tests, and build comments, and its exact archive hash.

## Measurement overlap and differences

| Evidence | Overlap | Why both records remain |
|---|---|---|
| Throughput sessions | Six identical named workloads, two sessions of five repetitions, one-second measurement/warmup settings, the same generated inputs and dependency versions | Source adds a pre-mutation ID lookup; session spacing, environment records, executable identities, and observed rates also differ. The archived sessions are consecutive; current sessions are separated by benchmark work. |
| Diagnostic timing | 23 current cases have direct counterparts in the profiling bundle's `timing/`, including 19 engine cases and four isolated heap/map cases | Current timing uses five 0.5-second repetitions; archived principal cases use five 2-second repetitions and depth cases generally use three 1-second repetitions. Raw batches, timing, and observed spread differ. |
| Price-level scans | Eleven current K values overlap the archived scan's workload family; three also have uninstrumented timing counterparts | Much of the archived scan uses a counter-instrumented build, more K values, and multiple seeds. Current scans use an uninstrumented timing build, seed 42, and five repetitions. |
| Allocation counting | Eight matching workload/size pairs report identical normalized allocation, free, byte, and maximum-size metrics | Raw counts, batches, repetitions, and instrumented source builds are different; the archived runs retain two repetitions, current runs one. Equal metrics are not duplicate observations. |
| Sanitizer diagnostics | Addition, matching, partial fills, cancellation, and sweeps occur in both | The archived bundle has 13 workload/size checks, including deep books; the current bundle has eight, often at different sizes. |

The benchmark generator, benchmark implementation, and CMake configuration differ only in comments/spacing when compared with the measured source archive; the functional engine change is the duplicate-ID guard. That does **not** make the runs differ only by engine version: protocol and uncontrolled runtime conditions also vary. The four isolated map/allocation cases do not call `OrderBook`, so their timing differences cannot be attributed to that guard. No causal performance delta is inferred from these bundles.

## Unique evidence and supersession

| Bundle | Files at audit entry | Unique retained information |
|---|---:|---|
| `2026-10-09-current` | 73 | Duplicate-check source identity; current throughput sessions and 31 timing cases; 11 uninstrumented K points; eight allocation and sanitizer checks; 93 Debug / 72 Release test records; hardware/layout/timer records; current-source verifier |
| `2026-10-09-throughput` | 29 | Distinct 60 throughput repetition rows, console output, source patch and 21-file archive, full compile/link records, source-stability/provenance data, 81-test sanitizer record, dependency helper, measured-commit verifier |
| `2026-10-09-profiling` | 186 | Four stack-sampling runs and summarizer, PC histograms and region analysis, instruction/cycle counters with controls and suppression gates, wider K/multi-seed scans, additional timings/repeats, allocation size histograms, 13 sanitizer checks, disassembly/codegen/source equivalence, duplicate-ID probe, three-revision harness provenance and audit tooling |

**The current bundle fully supersedes neither older bundle.** It supplies the current documents' numerical claims, but cannot replace the archived raw runs, sampling/counter coverage, exact source archive, or provenance chain. The profiling verifier also consumes the throughput bundle's executable identity, summary, and measured-source archive. Its source/contract checks are bound to that measured source and commit's README, rather than the current README.

Derived summaries remain alongside raw files because the verifiers check their exact regeneration. Source/manifest records, console context, and command traces have distinct roles and are not deleted merely because they restate a statistic.

## Verification and reproduction

Run from the repository root; Python uses only its standard library:

```bash
PYTHONDONTWRITEBYTECODE=1 python3 docs/evidence/2026-10-09-current/verify_evidence.py
PYTHONDONTWRITEBYTECODE=1 python3 docs/evidence/2026-10-09-throughput/verify_evidence.py
PYTHONDONTWRITEBYTECODE=1 python3 docs/evidence/2026-10-09-profiling/verify_profiling_evidence.py
```

The archived verifiers require the measured commits in local Git history. Checks validate content hashes, source identities, all throughput rows and outcome accounting, diagnostic statistics, and recorded correctness results; they do not rerun measurements or promise identical timing on another machine.

Use the current-facing reports' reproduction commands for the current engine. Archived throughput reproduction uses a separate clean checkout of its measured commit with prepared dependencies; [the bundle README](2026-10-09-throughput/README.md#commands-and-validation) gives the invocation. The runner requires Git provenance, so an extracted source archive alone is insufficient for its full workflow. For archived diagnostics, [METHODOLOGY §7](2026-10-09-profiling/METHODOLOGY.md#7-reproduction-commands) compiles the shared harness against the extracted source snapshot. The stored `harness/build.sh` and `samples/run_sample.sh` preserve commands tied to their original locations; the methodology supplies portable build/sampling commands. The provenance audit accepts `PROFILING_SOURCE_DIR` to select the measured engine snapshot, avoiding a misleading comparison against a different checkout.

All raw measurements, source archives, original command records, and unique tooling are retained. Only the three duplicate payload copies are removed; their paths remain aliases. Engine code and tests are unchanged.
