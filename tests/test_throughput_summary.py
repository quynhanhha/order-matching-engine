"""Synthetic fixtures test rejection of corrupt results; these are not measurements."""

import json
import pathlib
import tempfile
import unittest
from typing import Any

from scripts.summarize_throughput import summarize


class ThroughputSummaryTest(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.directory = pathlib.Path(self.temporary.name)
        (self.directory / "protocol.txt").write_text("method=repaired\n")
        self.rows: list[dict[str, Any]] = []
        for operation in ("BM_AddOnly_Resting", "BM_MatchOneToOne"):
            for population in (100, 1000, 10000):
                matching = operation == "BM_MatchOneToOne"
                incoming = population // 2 if matching else population
                for repetition in range(5):
                    self.rows.append({
                        "name": f"{operation}/{population}/real_time",
                        "run_type": "iteration", "repetition_index": repetition,
                        "iterations": 100, "time_unit": "us",
                        "real_time": 20.0, "cpu_time": 10.0,
                        "items_per_second": incoming / 20e-6,
                        "incoming_orders_per_batch": incoming,
                        "initial_orders": population if matching else 0,
                        "final_orders": population - incoming if matching else population,
                        "validated_trades_per_batch": incoming if matching else 0,
                        "validated_no_smp": 1,
                    })

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def write_sessions(self) -> None:
        for session in (1, 2):
            (self.directory / f"session-{session}.json").write_text(json.dumps({"benchmarks": self.rows}))

    def test_uses_elapsed_time_and_retains_ten_repetitions(self) -> None:
        self.write_sessions()
        result = summarize(self.directory)
        self.assertEqual(len(result["benchmarks"]), 6)
        for stats in result["benchmarks"].values():
            self.assertEqual(stats["repetitions"], 10)
            self.assertEqual(stats["sample_cv_percent"], 0)

    def test_rejects_missing_repetition(self) -> None:
        self.rows.pop()
        self.write_sessions()
        with self.assertRaises(ValueError):
            summarize(self.directory)

    def test_rejects_duplicate_repetition_index(self) -> None:
        self.rows[1]["repetition_index"] = 0
        self.write_sessions()
        with self.assertRaises(ValueError):
            summarize(self.directory)

    def test_rejects_wrong_rate_denominator(self) -> None:
        self.rows[0]["items_per_second"] *= 2
        self.write_sessions()
        with self.assertRaises(ValueError):
            summarize(self.directory)

    def test_rejects_invalid_fill_accounting(self) -> None:
        self.rows[-1]["validated_trades_per_batch"] = 0
        self.write_sessions()
        with self.assertRaises(ValueError):
            summarize(self.directory)

    def test_rejects_reported_benchmark_failure(self) -> None:
        self.rows[0]["error_occurred"] = True
        self.write_sessions()
        with self.assertRaises(ValueError):
            summarize(self.directory)


if __name__ == "__main__":
    unittest.main()
