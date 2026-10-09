# Order Matching Engine

A C++20 limit order book and matching engine with price–time priority, partial and full fills, cancellation, and self-match prevention. The project focuses on explicit memory management and measurable API behavior.

## Features and matching behavior

- Limit orders rest on the book or match immediately when they cross the spread.
- Buys match the lowest ask; sells match the highest bid. Trades execute at the resting order's price, with FIFO priority within a price level.
- Unfilled incoming quantity rests, except when self-match prevention encounters the same participant at the head of the opposing queue: the incoming remainder is cancelled.
- A synchronous, templated callback receives each trade's buy order ID, sell order ID, price, and quantity.
- Best bid/ask access returns the best price level in constant time.

The engine currently implements limit orders for a single book. Market/IOC/FOK order types, network ingestion, persistence, and concurrent book access are outside its implemented scope. Callers supply unique order IDs, positive quantities, and sufficient pool capacity. Duplicate IDs are not detected and are unsupported: they can leave resting orders uncancellable ([correctness finding](docs/profiling-report.md#82-correctness-finding-duplicate-order-ids)).

## Architecture and memory management

| Component | Design and behavior |
|---|---|
| `Order` | Side, integer price/quantity, order and participant IDs, sequence number, and intrusive next/previous links |
| `OrderPool` | Fixed-capacity contiguous storage with a free list for order reuse |
| `PriceLevel` | Aggregate quantity and an intrusive FIFO queue of orders |
| `OrderBook<TradeCallback>` | Sorted price-level vectors, a reserved order-ID hash index, and a templated trade callback |

Bids are stored ascending and asks descending, with the best price at each vector's back. Price lookup uses `lower_bound`; inserting or erasing levels can shift vector elements. Order-ID lookup is average O(1), but a full cancellation also looks up a price level and may erase it, so cancellation is not universally O(1).

The pool and price-level vector capacity are allocated during construction; the ID index reserves buckets up front. **Resting additions still allocate hash-map nodes.** Fully filled incoming orders use pool storage and do not insert an incoming index node, but matching can deallocate filled resting orders' hash nodes. A resting remainder can allocate an index node, and callbacks can allocate independently. Execution is therefore not universally allocation-free.

FIFO links, positive resting quantities, aggregate quantities, sorted levels, index cleanup, and empty-book transitions are covered by tests. Debug builds also enable assertions and sanitizers. Best-price pointers are borrowed and should not be retained across book mutations that can invalidate vector elements.

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

The public API also provides `bestBid()` and `bestAsk()`, returning `const PriceLevel*` or `nullptr`. Cancelling an absent ID is a no-op. See [the book implementation](include/order_book.h), [trade types](include/types.h), and [matching tests](tests/order_book_matching_tests.cpp).

## Build and test

The published benchmark environment is macOS on Apple Silicon with Apple Clang 21. Build prerequisites are a C++20 compiler, CMake 3.16 or newer, Git, and the platform toolchain. The throughput workflow additionally uses Bash and Python 3.9 or newer (standard library only). Clang/GNU build flags are configured; other platforms and compilers have not been validated for the published rates.

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

CMake fetches Google Test **v1.14.0** for tests and Google Benchmark **v1.8.3** when benchmarks are enabled. Initial dependency acquisition requires network access. The benchmark reproduction helper below prepares pinned checkouts explicitly and then uses disconnected builds.

## Performance

Measured on Apple M3 Pro with 36 GiB memory, macOS 26.5.2, Apple Clang 21, C++20, and Release flags including `-O3 -DNDEBUG -march=native -flto`:

| Synthetic workload | Median rate | Range across ten measurements |
|---|---:|---:|
| 10,000 non-crossing additions into an empty book | **41.478M resting additions/sec** | 39.871–42.162M/sec |
| 5,000 one-to-one full fills against 10,000 quantity-one sells at one price | **53.245M incoming full fills/sec** | 51.133–54.254M/sec |

These are single-threaded, amortized batch API rates with an empty trade callback, measured in two consecutive process runs with five repetitions each. Setup and teardown are excluded; allocation/deallocation inside API calls remains included. The matching workload is a favorable case with no price-level removal, partial fills, or SMP events. These results do not establish production exchange capacity or individual-order latency percentiles.

The [throughput report](docs/throughput-report.md) is the authoritative methodology and results document. The [repository evidence bundle](docs/evidence/2026-10-09-throughput/README.md) contains raw results, source identity, compiler/environment records, and test evidence.

The [hot-path profiling report](docs/profiling-report.md) is a diagnostic investigation of where this time goes, and it adds no new throughput claims. Its main finding is that the order-ID index's per-order node allocation and free is the largest cost in both workloads. It also covers price-level lookup and cancellation behavior, and it has its own [evidence bundle](docs/evidence/2026-10-09-profiling/README.md).

## Reproduce throughput from a fresh clone

From the repository root on the supported macOS/Apple Clang environment:

```bash
bash docs/evidence/2026-10-09-throughput/reproduce.sh
```

This prepares clean, pinned Google Benchmark v1.8.3 and Google Test v1.14.0 checkouts in `.cache/throughput-deps/`, then invokes the existing runner. It runs correctness checks before building Release and recording both measurement sessions. New output is written to `benchmark_results/<UTC timestamp>-repaired/`; existing results are never overwritten.

To inspect the published evidence without running benchmarks:

```bash
PYTHONDONTWRITEBYTECODE=1 python3 \
  docs/evidence/2026-10-09-throughput/verify_evidence.py
```

See the [report's reproduction instructions](docs/throughput-report.md#reproduction) for explicit dependency commands, rebuilding the exact measured source, compiler flags, and limitations. Other registered benchmarks and the latency harness remain diagnostic; their outputs are not part of the current performance claims.

## Repository layout

- `include/`, `src/`: engine and core data structures.
- `tests/`: matching, FIFO/price priority, SMP, cancellation, allocation, pool, and workload checks.
- `benchmarks/`: Google Benchmark suite, shared throughput inputs/validation, and latency harness.
- `scripts/`: throughput runner and result accounting/summary checks.
- `docs/`: throughput and profiling reports with compact published evidence.

[MIT License](LICENSE).
