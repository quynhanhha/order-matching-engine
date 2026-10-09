# Current-engine measurement evidence

This bundle supports the [throughput report](../../throughput-report.md) and [profiling report](../../profiling-report.md). The source manifest identifies the engine with pre-mutation duplicate-ID rejection, including benchmark inputs, tests, runner, and build configuration. Measurements use macOS/Apple Silicon, Apple Clang 21, C++20, and Release flags `-O3 -DNDEBUG -march=native -flto`.

The [evidence index](../README.md#current-document-support) lists shared tooling and bundle dependencies. The dependency-commit files and Python test log are relative links to byte-identical canonical files in `2026-10-09-throughput`; their names and content hashes are preserved. This bundle depends on both other directories and does not replace their distinct measurements.

## Throughput

- `session-1.json`, `session-2.json`: all six validated workloads, five repetitions per session, one-second warmup/minimum measurement, randomized interleaving, and an empty callback.
- `summary.json`, `summary.txt`: pooled medians, full ranges, sample CV, and session-median differences from the project summarizer.
- `protocol.txt`: measurement settings. Sessions are separated by other benchmark work; builds and correctness suites do not run concurrently with measurements.
- `effective-commands.json`, `binary-sha256.txt`: compiler commands and executable identity. Recorded absolute paths identify the measurement workspace.
- `environment-before.txt`, `environment-after.txt`: compiler, OS, power, and metadata query output. Thermal and frequency queries are restricted; placement and frequency are uncontrolled. `hardware.json` records model, chip, memory, and core counts from `system_profiler` without device identifiers.
- `benchmark-commit.txt`, `googletest-commit.txt`: pinned dependency commits.
- `tests-debug.txt`, `tests-release.txt`, `tests-summary.txt`: 93 Debug sanitizer cases, 72 relevant Release cases, and six Python accounting cases passing. Release excludes assertion-dependent pool death tests.

## Diagnostics

- `profiling/*.txt`: 31 timing workloads, eight allocation-counting workloads, and eight ASan/UBSan workload checks. Timing uses one-second warmup and five repetitions of 0.5 seconds wall time; allocation and sanitizer runs use one 0.2-second repetition after warmup. Only API batches contribute to reported operation costs.
- `profiling-summary.json`: timing medians/ranges and allocation/free counts.
- `profiling-commands.json`, `profiling-binary-sha256.txt`: exact build/workload commands and executable hashes. Separate timing and allocation binaries prevent counting instrumentation from affecting timing.
- `harness-sha256.txt`: identity of the harness built against current engine headers.
- `layout-timer.txt`: order, level, and pointer sizes and the timer's nanoseconds per tick.
- `profiling-environment.txt`: diagnostic environment and power records.

`source-commit.txt` records the source commit; `source-sha256.txt` identifies the measured implementation independently of documentation changes. Executables and generated build trees are excluded. Diagnostic medians are batch averages on one platform, not individual-operation latency samples or component sampling shares.

Verify source identity, all artifact hashes, and result calculations without running workloads or writing files:

```bash
PYTHONDONTWRITEBYTECODE=1 python3 docs/evidence/2026-10-09-current/verify_evidence.py
```

Reproduce throughput with the dependency helper and runner described in the throughput report. Reproduce diagnostics using the profiling report's build commands and the workload arguments in `profiling-commands.json`, writing outputs to a new directory. `SHA256SUMS` covers every other file in this bundle.
