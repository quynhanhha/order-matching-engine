# Order Matching Engine

A single-threaded C++20 limit order book with price–time priority, partial and full fills, cancellation, and self-match prevention (SMP).

## Features and matching behavior

- Limit orders rest on the book or match immediately when they cross the spread.
- Buys match the lowest ask; sells match the highest bid. Trades execute at the resting order's price, with FIFO priority within a price level.
- Unfilled incoming quantity rests, except when self-match prevention encounters the same participant at the head of the opposing queue: the incoming remainder is cancelled.
- A synchronous, templated callback receives each trade's buy order ID, sell order ID, price, and quantity.
- `bestBid()` and `bestAsk()` return the best price level in constant time, or `nullptr` for an empty side. Returned pointers are borrowed and may be invalidated by book mutations.
- Cancelling an absent ID is a no-op.

Callers supply positive quantities and sufficient pool capacity, including a slot for the incoming order. IDs must be unique among resting orders in each book, across both sides and all participants. `addLimitOrder` throws `std::invalid_argument` on an indexed ID before allocation, mutation, or callback, even if the order would execute immediately or encounter SMP. IDs are reusable after cancellation, full execution, or SMP cancellation; a resting remainder keeps its ID reserved.

Market/IOC/FOK orders, network ingestion, persistence, concurrent access, and callbacks that recursively mutate the same book are outside the supported scope.

## Architecture and memory management

| Component | Design and behavior |
|---|---|
| `Order` | Side, integer price/quantity, order and participant IDs, sequence number, and intrusive next/previous links |
| `OrderPool` | Fixed-capacity contiguous storage with a free list for order reuse |
| `PriceLevel` | Aggregate quantity and an intrusive FIFO queue of orders |
| `OrderBook<TradeCallback>` | Sorted price-level vectors, a reserved order-ID hash index, and a templated trade callback |

Bids are ascending and asks descending, with the best level at the back. Price lookup uses `lower_bound`; level insertion and cancellation can shift vector elements. Cancellation combines an average O(1) ID lookup with O(log L) price lookup and up to O(L) level shifting, where L is the side's level count.

Construction allocates the fixed-capacity pool, reserves 4,096 levels per side, and reserves index buckets. Resting orders allocate hash-map nodes; filled or cancelled resting orders free them. Fully executed incoming orders allocate no index node. Duplicate checks allocate nothing for valid inputs; exceptions and callbacks may allocate. The level-capacity bound is asserted in Debug; Release can grow the vectors.

Tests cover FIFO links, aggregates, price priority, matching, SMP, cancellation, duplicate rejection, ID reuse, allocation, and empty-book transitions.

## API example

```cpp
#include <iostream>
#include <algorithm>
#include "order_book.h"

int main() {
    OrderBook book(10000, [](const Trade& trade) -> void {
        std::cout << trade.buyOrderId << " buys from " << trade.sellOrderId
                  << ": " << trade.quantity << " @ " << trade.price << '\n';
    });

    book.addLimitOrder(Side::Sell, 100, 10, 1, 101);
    book.addLimitOrder(Side::Buy, 100, 4, 2, 202); // Partial fill; six remain on the ask.
    book.cancelOrder(1);
    return 0;
}
```

See [the book API](include/order_book.h), [trade types](include/types.h), and [matching tests](tests/order_book_matching_tests.cpp).

## Build and test

Requirements: a C++20 compiler, CMake 3.16+, Git, and the platform toolchain. The benchmark workflow also needs Bash and Python 3.9+ (standard library only). Clang/GNU flags are configured; reported measurements use macOS/Apple Silicon and Apple Clang 21.

A Debug build enables AddressSanitizer and UndefinedBehaviorSanitizer with Clang/GNU:

```bash
cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_TESTS=ON -DBUILD_BENCHMARKS=OFF
cmake --build build-debug --parallel 2
ASAN_OPTIONS=halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
  ctest --test-dir build-debug --output-on-failure
PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover \
  -s tests -p test_throughput_summary.py
```

CMake fetches Google Test **v1.14.0** and, with benchmarks enabled, Google Benchmark **v1.8.3**. Initial acquisition requires network access.

## Performance

Measured on Apple M3 Pro with 36 GB RAM, macOS 26.5.2, Apple Clang 21, and `-O3 -DNDEBUG -march=native -flto`:

| Synthetic workload | Median rate | Range across ten measurements |
|---|---:|---:|
| 10,000 non-crossing additions into an empty book | **40.895M resting additions/sec** | 40.209–41.474M/sec |
| 5,000 one-to-one full fills against 10,000 quantity-one sells at one price | **50.791M incoming full fills/sec** | 44.123–51.369M/sec |

These are amortized batch API rates with an empty callback, two process sessions, and five repetitions per session. Setup and teardown are excluded; API allocation/deallocation is included. Full fills keep one populated price level and exercise no partial fills or SMP. Rates do not establish production capacity or individual-order latency; CPU placement and frequency are uncontrolled.

The [throughput report](docs/throughput-report.md) defines workloads and measurement limits. The [profiling report](docs/profiling-report.md) examines API costs, memory traffic, depth, and price-level count. The [evidence index](docs/evidence/README.md#current-document-support) identifies the raw results, source hashes, environment records, correctness logs, and reproduction tools.

## Reproduce throughput from a fresh clone

From the repository root on the supported macOS/Apple Clang environment:

```bash
bash docs/evidence/2026-10-09-throughput/reproduce.sh
```

This prepares pinned dependencies in `.cache/throughput-deps/`, runs correctness checks, builds Release, and records two sessions in a new `benchmark_results/<UTC timestamp>-repaired/` directory.

To inspect the published evidence without running benchmarks:

```bash
PYTHONDONTWRITEBYTECODE=1 python3 \
  docs/evidence/2026-10-09-current/verify_evidence.py
```

See [reproduction details](docs/throughput-report.md#reproduction) for dependency overrides and runner settings. Other benchmarks and the latency harness are diagnostic.

## Repository layout

- `include/`, `src/`: engine and core data structures.
- `tests/`: matching, FIFO/price priority, SMP, cancellation, allocation, pool, and workload checks.
- `benchmarks/`: Google Benchmark suite, shared throughput inputs/validation, and latency harness.
- `scripts/`: throughput runner and result accounting/summary checks.
- `docs/`: throughput and profiling reports with compact published evidence.

[MIT License](LICENSE).
