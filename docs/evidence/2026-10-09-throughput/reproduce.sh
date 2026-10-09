#!/usr/bin/env bash
# Acquire the existing project dependencies, then invoke the unchanged throughput runner.
set -euo pipefail

if [[ "${1:-}" == --help ]]; then
  echo 'Usage: bash docs/evidence/2026-10-09-throughput/reproduce.sh [--prepare-only]'
  echo 'THROUGHPUT_DEPS_DIR selects a dependency cache; CXX selects the compiler.'
  exit 0
fi
if [[ $# -gt 1 || ( $# -eq 1 && "${1:-}" != --prepare-only ) ]]; then
  echo 'Expected no argument or --prepare-only.' >&2
  exit 2
fi

repo="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
deps="${THROUGHPUT_DEPS_DIR:-$repo/.cache/throughput-deps}"
mkdir -p "$deps"
deps="$(cd "$deps" && pwd)"

prepare() {
  local url="$1" tag="$2" commit="$3" destination="$4"
  if [[ ! -e "$destination" ]]; then
    git clone --depth 1 --branch "$tag" "$url" "$destination"
  fi
  if [[ "$(git -C "$destination" rev-parse HEAD)" != "$commit" ||
        "$(git -C "$destination" describe --tags --always)" != "$tag" ||
        -n "$(git -C "$destination" status --porcelain=v1)" ]]; then
    echo "Dependency must be a clean $tag checkout at $commit: $destination" >&2
    exit 2
  fi
}

prepare https://github.com/google/benchmark.git v1.8.3 \
  344117638c8ff7e239044fd0fa7085839fc03021 "$deps/benchmark"
prepare https://github.com/google/googletest.git v1.14.0 \
  f8d7d77c06936315286eb55f8de22cd23c188571 "$deps/googletest"

if [[ "${1:-}" == --prepare-only ]]; then
  echo "Pinned dependencies prepared in $deps"
  exit 0
fi

cd "$repo"
export BENCHMARK_SOURCE_DIR="$deps/benchmark"
export GOOGLETEST_SOURCE_DIR="$deps/googletest"
bash scripts/run_throughput.sh repaired
