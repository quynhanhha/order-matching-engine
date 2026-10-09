"""Read-only verification of current-engine throughput and diagnostic evidence."""

import hashlib
import json
import pathlib
import re
import runpy
import statistics


def verify_manifest(manifest: pathlib.Path, root: pathlib.Path) -> None:
    for line in manifest.read_text().splitlines():
        digest, name = line.split("  ", 1)
        assert hashlib.sha256((root / name).read_bytes()).hexdigest() == digest, name


def main() -> None:
    bundle = pathlib.Path(__file__).resolve().parent
    repo = bundle.parents[2]
    verify_manifest(bundle / "SHA256SUMS", bundle)
    listed = {line.split("  ", 1)[1] for line in (bundle / "SHA256SUMS").read_text().splitlines()}
    present = {str(path.relative_to(bundle)) for path in bundle.rglob("*") if path.is_file()} - {"SHA256SUMS"}
    assert listed == present, sorted(listed ^ present)
    shared = {
        "benchmark-commit.txt": "../2026-10-09-throughput/benchmark-commit.txt",
        "googletest-commit.txt": "../2026-10-09-throughput/gtest-commit.txt",
        "tests-summary.txt": "../2026-10-09-throughput/tests-summary.txt",
    }
    for name, target in shared.items():
        alias = bundle / name
        assert alias.is_symlink() and alias.readlink() == pathlib.Path(target), name
    verify_manifest(bundle / "source-sha256.txt", repo)
    verify_manifest(bundle / "harness-sha256.txt", repo)

    summarize = runpy.run_path(str(repo / "scripts/summarize_throughput.py"))["summarize"]
    assert summarize(bundle) == json.loads((bundle / "summary.json").read_text())
    assert (bundle / "benchmark-commit.txt").read_text().strip() == "344117638c8ff7e239044fd0fa7085839fc03021"
    assert (bundle / "googletest-commit.txt").read_text().strip() == "f8d7d77c06936315286eb55f8de22cd23c188571"
    for command in json.loads((bundle / "effective-commands.json").read_text()):
        for flag in ("-O3", "-DNDEBUG", "-march=native", "-flto", "-std=c++20"):
            assert flag in command, (flag, command)
        assert "-fsanitize" not in command

    diagnostics = json.loads((bundle / "profiling-summary.json").read_text())
    assert len(diagnostics["timing"]) == 31
    for name, expected in diagnostics["timing"].items():
        raw = (bundle / "profiling" / (name.replace("/", "-") + ".txt")).read_text()
        samples = [float(value) for value in re.findall(r" rep=\d+ .*? ns_per_op=([\d.]+)", raw)]
        assert len(samples) == 5, name
        assert statistics.median(samples) == expected["median_ns"], name
        assert min(samples) == expected["min_ns"] and max(samples) == expected["max_ns"], name
        assert abs(1000 / expected["median_ns"] - expected["median_mops"]) < 0.002, name

    assert len(diagnostics["allocations"]) == 8
    for name, expected in diagnostics["allocations"].items():
        stem = name.replace("/", "-") + ".txt"
        raw = (bundle / "profiling" / ("alloc-" + stem)).read_text()
        values = re.search(
            r"alloc_per_op=([\d.]+) free_per_op=([\d.]+) bytes_per_op=([\d.]+) max_size=(\d+)", raw
        )
        assert values is not None, name
        actual = dict(zip(("allocations_per_call", "frees_per_call", "bytes_per_call", "max_size"),
                          map(float, values.groups())))
        assert actual == expected, name
        sanitizer = (bundle / "profiling" / ("asan-" + stem)).read_text()
        assert " SUMMARY " in sanitizer and "FATAL:" not in sanitizer, name
        assert "runtime error:" not in sanitizer and "ERROR: AddressSanitizer" not in sanitizer, name

    assert "100% tests passed, 0 tests failed out of 93" in (bundle / "tests-debug.txt").read_text()
    assert "100% tests passed, 0 tests failed out of 72" in (bundle / "tests-release.txt").read_text()
    assert "Ran 6 tests" in (bundle / "tests-summary.txt").read_text()
    assert "OK" in (bundle / "tests-summary.txt").read_text()
    print("Verified artifact and current-source hashes, six throughput cases with ten repetitions each,")
    print("31 diagnostic timing cases, eight allocation/sanitizer cases, and correctness logs.")


if __name__ == "__main__":
    main()
