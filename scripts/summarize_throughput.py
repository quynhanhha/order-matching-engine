"""Check operation accounting and summarize repetitions without comparing methodologies."""

import json
import math
import pathlib
import statistics
import sys
from collections import defaultdict
from typing import Any


def summarize(directory: pathlib.Path) -> dict[str, Any]:
    method = (directory / "protocol.txt").read_text().splitlines()[0].split("=", 1)[1]
    if method not in {"legacy", "repaired"}:
        raise ValueError(f"Unknown methodology: {method}")
    grouped: dict[str, list[tuple[int, float]]] = defaultdict(list)
    matching_name = "BM_MatchHeavy" if method == "legacy" else "BM_MatchOneToOne"
    expected_names = {
        f"{operation}/{size}" + ("/real_time" if method == "repaired" else "")
        for operation in ("BM_AddOnly_Resting", matching_name)
        for size in (100, 1000, 10000)
    }
    for session in (1, 2):
        data = json.loads((directory / f"session-{session}.json").read_text())
        counts: dict[str, int] = defaultdict(int)
        repetition_indices: dict[str, set[int]] = defaultdict(set)
        for row in data["benchmarks"]:
            if row.get("error_occurred"):
                raise ValueError(f"Benchmark failed: {row}")
            if row.get("run_type") != "iteration":
                continue
            name = row["name"]
            if name not in expected_names:
                raise ValueError(f"Unexpected benchmark: {name}")
            population = int(name.split("/")[1])
            matching = name.startswith(matching_name + "/")
            incoming = population // 2 if matching else population
            scale = {"ns": 1e-9, "us": 1e-6, "ms": 1e-3, "s": 1.0}[row["time_unit"]]
            denominator = row["real_time"] if method == "repaired" else row["cpu_time"]
            rate = row["items_per_second"]
            if not math.isfinite(rate) or rate <= 0 or row["iterations"] <= 0:
                raise ValueError(f"Invalid rate or iteration count: {row}")
            if not math.isclose(rate, incoming / (denominator * scale), rel_tol=1e-8):
                raise ValueError(f"Rate does not reconcile with calls/time: {row}")
            if method == "repaired":
                expected_counters = {
                    "incoming_orders_per_batch": incoming,
                    "initial_orders": population if matching else 0,
                    "final_orders": population - incoming if matching else population,
                    "validated_trades_per_batch": incoming if matching else 0,
                    "validated_no_smp": 1,
                }
                for counter, expected in expected_counters.items():
                    if row.get(counter) != expected:
                        raise ValueError(f"Invalid {counter}: {row}")
            counts[name] += 1
            repetition_indices[name].add(row["repetition_index"])
            grouped[name].append((session, rate))
        if set(counts) != expected_names or any(count != 5 for count in counts.values()):
            raise ValueError(f"Missing benchmarks/repetitions in session {session}: {counts}")
        if any(indices != set(range(5)) for indices in repetition_indices.values()):
            raise ValueError(f"Missing repetition indices in session {session}")
    results: dict[str, Any] = {"methodology": method, "benchmarks": {}}
    for name, samples in sorted(grouped.items()):
        rates = [rate for _, rate in samples]
        mean = statistics.mean(rates)
        session_medians = [statistics.median([r for s, r in samples if s == session]) for session in (1, 2)]
        results["benchmarks"][name] = {
            "repetitions": len(rates),
            "median_orders_per_second": statistics.median(rates),
            "min_orders_per_second": min(rates),
            "max_orders_per_second": max(rates),
            "sample_cv_percent": statistics.stdev(rates) / mean * 100,
            "session_median_orders_per_second": session_medians,
            "session_median_difference_percent": abs(session_medians[0] - session_medians[1]) / statistics.mean(session_medians) * 100,
        }
    return results


def main() -> None:
    directory = pathlib.Path(sys.argv[1])
    results = summarize(directory)
    (directory / "summary.json").write_text(json.dumps(results, indent=2) + "\n")
    print(f"Methodology: {results['methodology']}; two sessions, five repetitions each")
    print("Units: million incoming API calls per measured second; no individual-call percentiles")
    for name, stats in results["benchmarks"].items():
        print(f"{name}: median {stats['median_orders_per_second'] / 1e6:.3f} M/s; "
              f"range {stats['min_orders_per_second'] / 1e6:.3f}-{stats['max_orders_per_second'] / 1e6:.3f} M/s; "
              f"sample CV {stats['sample_cv_percent']:.2f}%; "
              f"session median difference {stats['session_median_difference_percent']:.2f}%")


if __name__ == "__main__":
    main()
