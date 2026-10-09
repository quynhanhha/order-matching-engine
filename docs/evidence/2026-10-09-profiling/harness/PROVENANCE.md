# Harness revision provenance

The profiling harness source was edited twice during the recorded run. Both edits changed only how the price-level (`levelsK`) workload builds its inputs. The published [`prof_harness.cpp`](prof_harness.cpp) is the final revision, **rev3**.

**Verdict: provenance verified.**
- Every published measurement maps to a known revision.
- All three revisions have identical timed code.
- Every published price-level input is reproduced exactly by the final harness.

## Revisions

| Revision | Change | Built | Evidence |
|---|---|---|---|
| rev1 | Initial harness. `levelsK` with K ≠ 10 used a generic K-level generator (seed 42). K = 10 selected the archived `add/N` generator. Outputs are named `add-levelsK`. | before 17:16:06 | Reconstructed by reverse-applying both patches. Its rebuilt `prof_timing` disassembly is identical, all 11,731 lines with addresses, to `prof_timing.asm.txt` in the local archive. That file was dumped from the original first build before any measurement; it is not published because of its size, and the comparison result is in [provenance-audit.txt](provenance-audit.txt). |
| rev2 | [rev1-to-rev2.patch](rev1-to-rev2.patch): `levelsK` always uses the generic generator, including K = 10. Outputs are named `add-glevelsK`. | 17:28:05–17:28:42 | No rev2 binary was archived. rev2 lies between two verified revisions and differs from rev3 only in the seed source, whose default value is the same. |
| rev3 | [rev2-to-rev3.patch](rev2-to-rev3.patch): optional `GLEVELS_SEED` environment override (default 42). | 17:30:14–17:30:20 | Rebuilding the published source reproduces all four archived final binaries **byte-for-byte** (identical SHA-256). |

Times are local file modification times of the archived outputs and binaries, read without modification. The [command trace](../commands.sh) records both rebuilds as comments, at lines 41 and 60.

**Scope of the edits** ([provenance-audit.txt](provenance-audit.txt)). Between revisions, only these functions differ:
- the `AddWorkload` constructor (input generation, untimed)
- the `CancelWorkload` constructor, one instruction that passes `generic = false`
- `main`, for workload dispatch

This holds in every build variant. The engine's `addLimitOrder`, every `Workload::batch` function (which contains the timed region and the inlined timing, counter, allocation and sampling code), and all instrumentation are instruction-identical across all three revisions. Shared timing, instrumentation and the input generation of every other workload were unaffected.

## Mapping of measurements to revisions

| Revision and binary | Measurements (raw files) | Output time |
|---|---|---|
| rev1 (`prof_timing`, `prof_rusage`, `prof_alloc`, `prof_pcsamp`) | `alloc/add-*`, `alloc/match-*`; `timing/add-{100,1000,10000}`, `timing/match-{100,1000,10000}`; `rusage/control-*`; `rusage/{add,match}-{1000,10000}`; `timing/` and `rusage/` `iso-*` including `.run2`; `pcsamp/{add,match}-10000`; `timing/` and `rusage/` `levels{1,64,256,2048}-10000` | 17:18:16–17:28:05 |
| rev2 (`prof_rusage`) | `rusage/scan-levels{1…1024}.txt`: the full level-count table | 17:28:42–17:29:50 |
| rev3 (archived binaries; `prof_asan` built from the same source at 17:36:17) | `rusage/seed-levels*`; every `cancel-*`, `partial-*` and `sweep*` file in `timing/`, `rusage/` and `alloc/`; `pcsamp/cancel-*`; all `asan/`; `{add,match,cancel-*}-{100000,1000000}` | 17:30:25 onward |

The `sample` measurements used the archived benchmark executable, not the harness.

**Published price-level values:**

| Report figure | Raw evidence | Revision |
|---|---|---|
| Level-count table (18 values of K) | `rusage/scan-levels{K}.txt` | rev2 only |
| Seed comparison (K ∈ {4, 6, 10, 16, 48, 64} × seeds {42, 7, 1234, 99}) | `rusage/seed-levels{K}-s{seed}.txt` | rev3 only |
| 2,048-level point (90.41 ns, 1,274.5 instructions, 341.5 cycles) | `timing/levels2048-10000.txt`, `rusage/levels2048-10000.txt` | rev1 |

No published table mixes revisions.

## Input equivalence

The audit compiled each revision's own `AddWorkload` constructor and hashed the inputs it generates ([provenance-audit.txt](provenance-audit.txt)):

- **K ≠ 10:** for all 18 values of K other than 10, rev1, rev2 and rev3 generate byte-identical inputs. The rev1 2,048-level point therefore used exactly the inputs that rev3 generates for `levels2048`.
- **K = 10:** rev2 and rev3 generate identical inputs, with the generic generator. rev1 would have used the archived generator, but rev1 `levels10` was never run.
- **Generic K = 10 vs archived inputs:** the generic K = 10 inputs equal the archived `add/10000` inputs apart from a constant +9,910 price offset on each side. This supports comparing the scan's K = 10 point with the archived addition workload.
- **Default seed:** `GLEVELS_SEED=42` generates the same inputs as leaving the variable unset.

## Cross-run consistency of the same inputs

Repeat measurements of identical inputs with identical timed code, taken in separate runs:

| K | Earlier run | Later run | Cycle difference |
|---|---|---|---:|
| 1 | rev1 `levels` 393.1 instr / 65.4 cycles | rev2 scan 393.6 / 65.8 | +0.4 |
| 64 | rev1 `levels` 439.8 / 75.0 | rev2 scan 439.8 / 75.2 | +0.2 |
| 256 | rev1 `levels` 472.6 / 92.7 | rev2 scan 473.3 / 92.3 | −0.4 |
| 4 | rev2 scan 406.6 / 86.5 | rev3 seed 42: 406.6 / 87.4 | +0.8 |
| 6 | rev2 scan 411.8 / 67.8 | rev3 seed 42: 411.8 / 69.5 | +1.6 |
| 10 | rev2 scan 416.7 / 90.3 | rev3 seed 42: 417.0 / 91.1 | +0.8 |
| 16 | rev2 scan 422.0 / 76.4 | rev3 seed 42: 421.4 / 83.2 | **+6.8 (8.9%)** |
| 48 | rev2 scan 436.4 / 93.6 | rev3 seed 42: 436.9 / 96.8 | +3.1 |
| 64 | rev2 scan 439.8 / 75.2 | rev3 seed 42: 440.5 / 76.3 | +1.1 |

Instruction counts agree within 0.7 per addition. Cycle counts can differ between runs by up to 6.8 per addition, which is more than within-run variation. Level-count differences smaller than about 7 cycles are therefore not meaningful. The report's conclusions rely only on larger contrasts, such as K = 6 at 68–70 cycles versus K = 10 at 90–96, which hold across runs.

## Superseded, unrecoverable and unrecorded items

- **Superseded and retained:** the first `iso-delete32` and `iso-umap-insert` runs failed the timing-agreement gate. Their `.run2` repeats are used.
- **Unpublished:** an initial sampling launch failed before it started. Its empty output is excluded.
- **Not archived:**
  - The rev1 and rev2 binaries were overwritten by later builds. rev1 is verified through its archived disassembly. rev2 is bounded by rev1 and rev3, as described above.
  - rev1's `prof_pcsamp` disassembly was overwritten. The audit shows that its `addLimitOrder`, `AddWorkload::batch` and `MatchWorkload::batch` have the same instructions and function-relative offsets as rev3's. The add/match PC-sample offsets therefore map correctly onto the [published excerpt](../disassembly/harness-pcsamp-addLimitOrder-CancelWorkload.txt).
- **Not in the command trace:** the `otool` disassembly dumps, the source edits themselves (only the rebuild markers were logged), and the exact sanitizer build commands. The sanitizer binaries are archived; both link the AddressSanitizer runtime and contain assertion calls, consistent with the documented flags.

## Reproducing the audit

From the repository root:

```bash
bash docs/evidence/2026-10-09-profiling/harness/provenance_audit.sh [path-to-local-archive]
```

The script reconstructs rev1 and rev2 from the patches and rebuilds all twelve variants in a temporary directory. It then compares them with each other and, when the local archive is present, with the archived binaries. It also regenerates the input hashes. Byte-identical binaries are expected only with the recorded toolchain (Apple Clang 21.0.0) and target.

The final harness reproduces every published workload without modification:
- `levelsK` (with `GLEVELS_SEED` unset or 42) regenerates the inputs of both `add-levelsK` and `add-glevelsK` runs. Only the output name differs.
- `GLEVELS_SEED` selects the seed-comparison inputs.
