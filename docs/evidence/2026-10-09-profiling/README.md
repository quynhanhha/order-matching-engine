# Evidence: 2026-10-09 hot-path profiling

This bundle supports the [profiling report](../../profiling-report.md). The profiling was diagnostic: no engine, benchmark or test code was changed, and no optimization was measured.

Raw outputs are copied unchanged from the recorded profiling run. Only the following files were produced for publication, from the recorded artifacts:
- `README.md`
- `SHA256SUMS`
- `verify_profiling_evidence.py`
- `source-equivalence.txt`
- the excerpts and checks in `disassembly/`
- `METHODOLOGY.md`
- `tools-availability.txt` (re-queried at publication)
- the harness provenance files in `harness/`

Executables, debug symbols, full disassembly dumps, archive-wide mtime listings and the diagnostic working notes are excluded.

[METHODOLOGY.md](METHODOLOGY.md) gives the measurement methods, validation rules, workload definitions, complete result matrices and reproduction commands that the report summarizes.

Verify the bundle and every value quoted in the report, without running a benchmark:

```bash
PYTHONDONTWRITEBYTECODE=1 python3 \
  docs/evidence/2026-10-09-profiling/verify_profiling_evidence.py
```

The verifier checks:
- file hashes and Git visibility
- the recorded provenance and integrity results
- that the derived summaries regenerate byte-for-byte from the raw data
- each published sampling, timing, counter, allocation and PC-sampling figure

It uses only the Python standard library.

## Provenance, environment and integrity

- [preflight.txt](preflight.txt), [postflight.txt](postflight.txt): records taken before and after profiling.
  - the source commit `0bad3d2` and a clean working tree
  - the profiled executable's SHA-256, which matches the [throughput evidence](../2026-10-09-throughput/binary-sha256.txt)
  - the throughput evidence verifier passing
  - the 1,024 archived result files left unchanged
- [environment-before.txt](environment-before.txt), [environment-after.txt](environment-after.txt): hardware, OS, compiler, cache sizes, power and thermal queries.
- [source-equivalence.txt](source-equivalence.txt): shows that the measured executable's commit (`a8edeb4`) and the harness's commit (`0bad3d2`) differ only in comments, by both a whole-tree diff check and preprocessed token comparison.
- [disassembly/codegen-identity.txt](disassembly/codegen-identity.txt): `addLimitOrder` is instruction-identical in the measured executable and in the harness builds.

## Sampling of the archived benchmarks

`samples/` holds the following.

- `*.sample.txt`: raw `/usr/bin/sample` call graphs, 1 ms interval, 25 s, for `BM_AddOnly_Resting` and `BM_MatchOneToOne` at N = 1,000 and 10,000.
- `*.bench.json`, `*.bench.txt`: the benchmark's own output for each sampled run.
- `*.summary.txt`: timed and untimed shares by call site, produced by [summarize_samples.py](samples/summarize_samples.py).
- [run_sample.sh](samples/run_sample.sh): the sampling wrapper. It contains the recorded absolute path of the executable.
- [disassembly/measured-benchmark-call-sites.txt](disassembly/measured-benchmark-call-sites.txt): maps the call-site offsets to timed and untimed calls.
- [disassembly/measured-addLimitOrder-cancelOrder.txt](disassembly/measured-addLimitOrder-cancelOrder.txt): `otool` excerpt of the measured executable.

## Profiling harness

- [harness/prof_harness.cpp](harness/prof_harness.cpp): profiling-only harness source, final revision (rev3).
- [harness/build.sh](harness/build.sh): recorded build script. It locates the repository relative to its original directory. From a fresh clone, use [METHODOLOGY §7](METHODOLOGY.md#7-reproduction-commands).
- [harness/PROVENANCE.md](harness/PROVENANCE.md): the two in-run source edits, which revision and binary produced each measurement, input-equivalence and cross-run checks, and items that were superseded or not archived. **Verdict: provenance verified.**
- [harness/rev1-to-rev2.patch](harness/rev1-to-rev2.patch), [harness/rev2-to-rev3.patch](harness/rev2-to-rev3.patch): the reconstructed edits.
- [harness/provenance_audit.sh](harness/provenance_audit.sh): rebuilds all revisions in a temporary directory and compares them with the archive. Its recorded output is [harness/provenance-audit.txt](harness/provenance-audit.txt). Rebuilding the published source reproduces the archived final binaries byte-for-byte. Rebuilding rev1 reproduces the archived first-build disassembly.

## Harness measurements

- `timing/`: primary wall-clock timing. Each line is one repetition; `SUMMARY` gives the median, minimum and maximum ns per call.
- `rusage/`: instruction and cycle counters from the counter build, with controls `control-empty-1.txt` and `control-chain-*.txt`.
  - [rusage/analyze.py](rusage/analyze.py) applies bracket-overhead correction and the validation gates. Its output is [rusage/analysis.txt](rusage/analysis.txt), which lists which results were suppressed.
  - `scan-levels*.txt` and `seed-levels*.txt` hold the level-count and seed experiments. Their summaries are `scan-levels.summary.txt` and `seed-levels.summary.txt`, which the verifier recomputes.
  - `*.run2.txt` files are repeats of two isolated microbenchmarks whose first runs failed the timing-agreement gate. Both runs are kept.
- `alloc/`: allocation and free counts per call, plus size histograms, from the counting build. These files report no timing.
- `pcsamp/`: in-process PC histograms (`*.txt`, by symbol and by symbol+offset) and the corresponding run output (`*.run.txt`).
  - [pcsamp/regions.py](pcsamp/regions.py) groups `addLimitOrder` offsets into source regions. Its output is [pcsamp/regions.txt](pcsamp/regions.txt).
  - Offsets map to [disassembly/harness-pcsamp-addLimitOrder-CancelWorkload.txt](disassembly/harness-pcsamp-addLimitOrder-CancelWorkload.txt).
- `asan/`: every harness workload, including the 100,000- and 1,000,000-order cases, run under AddressSanitizer, UndefinedBehaviorSanitizer and engine assertions. [asan/summary.txt](asan/summary.txt) records 13 passes.

## Correctness finding

- [correctness/dup_id_repro.cpp](correctness/dup_id_repro.cpp): probe for duplicate order IDs.
- [correctness/output-asan-debug.txt](correctness/output-asan-debug.txt): its sanitizer-enabled output.

See [§8.2 of the report](../../profiling-report.md#82-correctness-finding-duplicate-order-ids).

## Command trace

[commands.sh](commands.sh) is the recorded audit trail, not a standalone script. Commands ran from the profiling output directory, and `<out>` stands for that directory. Some entries are shorthand; their exact forms are:

- `run_sample.sh … {A,B,C}`: one `run_sample.sh` invocation per case: `BM_MatchOneToOne/10000/real_time match10000`, `BM_MatchOneToOne/1000/real_time match1000` and `BM_AddOnly_Resting/1000/real_time add1000`. The four sampled cases ran consecutively.
- `summarize_samples.py ...`: the per-file arguments are listed in `verify_profiling_evidence.py`, which reruns them.
- The ASan harness build was `clang++ -std=c++20 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude -Ibenchmarks src/order_pool.cpp src/price_level.cpp prof_harness.cpp`. Assertions were enabled (no `-DNDEBUG`). Runs used `ASAN_OPTIONS=halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1` and arguments `<workload> <N> 0.2 1`.
- The duplicate-ID probe was built with `clang++ -std=c++20 -O0 -g -fsanitize=address,undefined -Iinclude src/order_pool.cpp src/price_level.cpp dup_id_repro.cpp`. It ran with `ASAN_OPTIONS=halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1`.
- The level-count scan and seed summaries were computed by inline scripts using the same bracket correction as `analyze.py`. The verifier reproduces them.

Absolute paths and the host name in raw outputs describe the recorded machine; they are not prerequisites for a fresh clone.

## Hashes

[SHA256SUMS](SHA256SUMS) lists every other file in this bundle.
