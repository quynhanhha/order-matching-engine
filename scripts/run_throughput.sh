#!/usr/bin/env bash
# Run from any directory. Existing dependency checkouts are reused without downloads.
set -euo pipefail

method="${1:-repaired}"
case "$method" in
  legacy) filter='^(BM_AddOnly_Resting|BM_MatchHeavy)/[0-9]+$' ;;
  repaired) filter='^(BM_AddOnly_Resting|BM_MatchOneToOne)/[0-9]+/real_time$' ;;
  *) echo 'Usage: bash scripts/run_throughput.sh [legacy|repaired] [new-results-directory]' >&2; exit 2 ;;
esac

repo="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo"
results="${2:-$repo/benchmark_results/$(date -u '+%Y%m%dT%H%M%SZ')-$method}"
if [[ -e "$results" ]]; then
  echo "Refusing to overwrite existing results: $results" >&2
  exit 2
fi
mkdir -p "$results"
results="$(cd "$results" && pwd)"
echo "Results: $results"
exec > >(tee "$results/runner.log") 2>&1

run() {
  printf '%q ' "$@" >> "$results/commands.sh"
  printf '\n' >> "$results/commands.sh"
  "$@"
}

compiler="${CXX:-/usr/bin/clang++}"
benchmark_source="${BENCHMARK_SOURCE_DIR:-$repo/build-release/_deps/benchmark-src}"
gtest_source="${GOOGLETEST_SOURCE_DIR:-$repo/build/_deps/googletest-src}"
[[ -f "$benchmark_source/CMakeLists.txt" && -f "$gtest_source/CMakeLists.txt" ]] || {
  echo 'Set BENCHMARK_SOURCE_DIR and GOOGLETEST_SOURCE_DIR to existing pinned checkouts.' >&2
  exit 2
}

run git rev-parse HEAD > "$results/source-commit.txt"
run git status --porcelain=v1 > "$results/working-tree-before.txt"
run git diff --binary HEAD > "$results/source.patch"
run git -C "$benchmark_source" rev-parse HEAD > "$results/benchmark-commit.txt"
run git -C "$benchmark_source" describe --tags --always > "$results/benchmark-version.txt"
run git -C "$benchmark_source" status --porcelain=v1 > "$results/benchmark-dirty.txt"
run git -C "$gtest_source" rev-parse HEAD > "$results/gtest-commit.txt"
run git -C "$gtest_source" describe --tags --always > "$results/gtest-version.txt"
run git -C "$gtest_source" status --porcelain=v1 > "$results/gtest-dirty.txt"
[[ "$(cat "$results/benchmark-version.txt")" == v1.8.3 && ! -s "$results/benchmark-dirty.txt" ]]
[[ "$(cat "$results/gtest-version.txt")" == v1.14.0 && ! -s "$results/gtest-dirty.txt" ]]

# Include new task files as well as tracked files: a commit hash alone is insufficient.
run python3 - "$results" <<'PY'
import hashlib
import pathlib
import shutil
import subprocess
import sys

output = pathlib.Path(sys.argv[1])
paths = subprocess.check_output(
    ["git", "ls-files", "--cached", "--others", "--exclude-standard", "-z"]
).decode().split("\0")
manifest = []
for name in sorted(set(filter(None, paths))):
    source = pathlib.Path(name)
    if source.parts[0] not in {"include", "src", "tests", "benchmarks", "scripts", "docs"} and name not in {"CMakeLists.txt", "README.md", ".gitignore"}:
        continue
    if not source.is_file():
        continue
    target = output / "source" / source
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, target)
    manifest.append(f"{hashlib.sha256(source.read_bytes()).hexdigest()}  {name}\n")
(output / "source-sha256.txt").write_text("".join(manifest))
PY

metadata() {
  run date -u '+%Y-%m-%dT%H:%M:%SZ'
  run uname -a
  run "$compiler" --version
  run cmake --version
  if [[ "$(uname -s)" == Darwin ]]; then
    run sw_vers
    for key in hw.model machdep.cpu.brand_string hw.memsize hw.ncpu hw.physicalcpu hw.logicalcpu; do
      printf '%s: ' "$key"
      run sysctl -n "$key" || echo 'unavailable (query failed)'
    done
    run pmset -g batt || true
    run pmset -g custom || true
    run pmset -g therm || true
  fi
}
metadata > "$results/environment-before.txt" 2>&1
printf '%s\n' "method=$method" "filter=$filter" \
  'sessions=2 repetitions=5 minimum_measurement_seconds=1 warmup_seconds=1' \
  'callback=no-op; random_interleaving=true; affinity/frequency not controlled' \
  > "$results/protocol.txt"

# Correctness gate: Debug uses the project's ASan/UBSan flags, never its benchmark target.
run cmake -S "$repo" -B "$results/build-sanitizers" \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER="$compiler" \
  -DBUILD_TESTS=ON -DBUILD_BENCHMARKS=OFF \
  -DFETCHCONTENT_FULLY_DISCONNECTED=ON \
  -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST="$gtest_source" \
  > "$results/configure-sanitizers.log" 2>&1
run cmake --build "$results/build-sanitizers" --parallel 2 \
  > "$results/build-sanitizers.log" 2>&1
run env ASAN_OPTIONS=halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
  ctest --test-dir "$results/build-sanitizers" --output-on-failure \
  > "$results/tests-sanitizers.log" 2>&1
echo 'Sanitizer correctness gate passed.'
run env PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s tests -p test_throughput_summary.py \
  > "$results/tests-summary.log" 2>&1

run cmake -S "$repo" -B "$results/build-release" \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER="$compiler" \
  -DBUILD_TESTS=OFF -DBUILD_BENCHMARKS=ON \
  -DFETCHCONTENT_FULLY_DISCONNECTED=ON \
  -DFETCHCONTENT_SOURCE_DIR_BENCHMARK="$benchmark_source" \
  > "$results/configure-release.log" 2>&1
run cmake --build "$results/build-release" --target order_book_bench --parallel 2 \
  > "$results/build-release.log" 2>&1
run cp "$results/build-release/compile_commands.json" "$results/compile_commands.json"
run python3 - "$results/compile_commands.json" <<'PY'
import json
import pathlib
import sys

commands = json.loads(pathlib.Path(sys.argv[1]).read_text())
project = [entry["command"] for entry in commands if "/_deps/" not in entry["file"]]
assert project
for command in project:
    for flag in ("-O3", "-DNDEBUG", "-march=native", "-flto", "-std=c++20"):
        assert flag in command, (flag, command)
    assert "-fsanitize" not in command, command
PY
run shasum -a 256 "$results/build-release/order_book_bench" > "$results/binary-sha256.txt"
run "$results/build-release/order_book_bench" --benchmark_list_tests=true \
  > "$results/benchmark-list.txt" 2>&1
echo 'Fresh Release build and effective flags verified.'

for session in 1 2; do
  echo "Starting $method session $session (all five repetitions retained)."
  run "$results/build-release/order_book_bench" \
    --benchmark_filter="$filter" --benchmark_min_time=1s \
    --benchmark_min_warmup_time=1 --benchmark_repetitions=5 \
    --benchmark_enable_random_interleaving=true \
    --benchmark_out="$results/session-$session.json" --benchmark_out_format=json \
    > "$results/session-$session.txt" 2>&1
done
metadata > "$results/environment-after.txt" 2>&1
run git status --porcelain=v1 > "$results/working-tree-after.txt"
run python3 - "$results" <<'PY'
import hashlib
import pathlib
import sys

output = pathlib.Path(sys.argv[1])
for line in (output / "source-sha256.txt").read_text().splitlines():
    digest, name = line.split("  ", 1)
    assert hashlib.sha256(pathlib.Path(name).read_bytes()).hexdigest() == digest, name
(output / "source-stable.txt").write_text("All captured source hashes unchanged during build and measurement.\n")
PY
if [[ -f scripts/summarize_throughput.py ]]; then
  run python3 scripts/summarize_throughput.py "$results" \
    > "$results/summary.txt"
fi
echo "Completed: $results"
