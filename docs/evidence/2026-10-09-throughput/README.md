# Evidence: 2026-10-09 batch API throughput

This bundle preserves throughput measurements for source commit `a8edeb48184757bfab19abc042a66788a5deb5d5`. Raw measurement files are copied unchanged from the recorded run. Rates describe synthetic, single-threaded batch API throughput with an empty callback, not individual-order latency or production capacity. The [evidence index](../README.md) distinguishes these measurements from the results supporting the current-facing reports and identifies shared tooling and files.

## Results and conditions

- [session-1.json](session-1.json), [session-2.json](session-2.json): all benchmark iteration and aggregate rows; each run has five repetitions for all six cases.
- [session-1.txt](session-1.txt), [session-2.txt](session-2.txt): original console output, including framework context.
- [summary.json](summary.json), [summary.txt](summary.txt): statistics using all ten rates per case.
- [protocol.txt](protocol.txt): filter, repetitions, warmup, measurement duration, and callback policy.
- [environment-before.txt](environment-before.txt), [environment-after.txt](environment-after.txt): recorded hardware, OS, compiler, power, and thermal-query output.
- [compile_commands.json](compile_commands.json), [link-command.txt](link-command.txt): original effective compilation and linking commands. Absolute paths are the recorded build paths; they are not fresh-clone prerequisites.

## Source and dependencies

- [provenance.json](provenance.json): measured-code identity, exact dependency identities, and original executable SHA-256.
- [measured-source.tar.gz](measured-source.tar.gz): the 21 original build, engine, benchmark, test, and runner source files; no build tree, executable, or documentation is packaged.
- [source-sha256.txt](source-sha256.txt): per-file source hashes, selected from the original run manifest. Every packaged file matches commit `a8edeb48184757bfab19abc042a66788a5deb5d5` byte-for-byte.
- [source-commit.txt](source-commit.txt), [source.patch](source.patch): the recorded base commit and tracked-file changes at measurement. The full source archive also contains files that were new at measurement and therefore absent from that patch.
- [source-stable.txt](source-stable.txt): the original runner's source-stability check result.
- [binary-sha256.txt](binary-sha256.txt): identity of the measured executable; the executable itself is excluded. A new build need not have the same hash.
- [benchmark-version.txt](benchmark-version.txt), [benchmark-commit.txt](benchmark-commit.txt), [gtest-version.txt](gtest-version.txt), [gtest-commit.txt](gtest-commit.txt): recorded dependency tags and commit IDs.

## Commands and validation

- [commands.sh](commands.sh): original command trace, with the recorded absolute paths. It is an audit record rather than a standalone replay script; stdin Python bodies and output redirections are in the archived runner source.
- [reproduce.sh](reproduce.sh): fresh-clone preparation of the same existing dependencies, followed by the unchanged throughput runner. `--prepare-only` prepares and validates dependency checkouts without running builds or benchmarks.
- [tests-sanitizers.txt](tests-sanitizers.txt), [tests-summary.txt](tests-summary.txt): original test logs, renamed from `.log`; 81 C++ sanitizer tests and six Python tests passed before measurement.
- [verify_evidence.py](verify_evidence.py): read-only source/hash, result-accounting, summary, dependency, and recorded-test verification. It requires the measured-code commit in local Git history, as supplied by a normal full-history clone.
- [SHA256SUMS](SHA256SUMS): hashes of every other file in this bundle.

From the repository root:

```bash
PYTHONDONTWRITEBYTECODE=1 python3 \
  docs/evidence/2026-10-09-throughput/verify_evidence.py
bash docs/evidence/2026-10-09-throughput/reproduce.sh --prepare-only
```

The helper runs the current checkout. To reproduce this bundle, use a separate clean Git checkout of `a8edeb48184757bfab19abc042a66788a5deb5d5` and run its `scripts/run_throughput.sh` with the prepared dependency paths. The runner requires Git metadata to record provenance; an extracted source archive alone is sufficient for compilation but not its complete provenance workflow. For a local full-history clone, after preparing dependencies above:

```bash
archive_source="benchmark_results/$(date -u +%Y%m%dT%H%M%SZ)-throughput-source"
git clone --no-hardlinks . "$archive_source"
git -C "$archive_source" checkout --detach a8edeb48184757bfab19abc042a66788a5deb5d5
BENCHMARK_SOURCE_DIR="$PWD/.cache/throughput-deps/benchmark" \
GOOGLETEST_SOURCE_DIR="$PWD/.cache/throughput-deps/googletest" \
CXX=/usr/bin/clang++ bash "$archive_source/scripts/run_throughput.sh" repaired
```

Preparation downloads Google Benchmark v1.8.3 and Google Test v1.14.0 if needed; no new dependency is introduced. New runs write to a new `benchmark_results/` directory and do not overwrite this bundle.

The original artifacts remain intact in the local archive. This bundle is deliberately separate from ignored generated results; `.gitignore` excludes only generated and local paths, so evidence files, including JSON, are tracked without exceptions. GitHub links resolve within the repository when these publication files are committed and pushed.
