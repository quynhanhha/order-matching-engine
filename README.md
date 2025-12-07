# **Order Matching Engine**

A high-performance limit order book (LOB) and matching engine implemented in modern C++.

This project demonstrates systems-level thinking, low-latency architecture, and memory-safe, allocation-free data structures inspired by real-world electronic trading engines.

---

## **Overview**

The engine maintains a central limit order book with strict **price–time priority**, supports adding, cancelling, and executing orders, and performs matching between aggressive and resting orders.

It is designed to be:

* **Deterministic**

* **Cache-efficient**

* **Allocation-free in the hot path**

* **Benchmarkable and debuggable**

The implementation uses intrusive linked lists, preallocated memory pools, sorted price levels, and a predictable control-flow path suitable for low-latency workloads.

---

## **Features**

### **Core Functionality**

* Add limit orders

* Cancel resting orders

* Administrative execution for testing

* Automatic matching when incoming orders cross the spread

* Partial and full fills

* Trade event generation via callback interface

### **Data Structure Highlights**

* Preallocated fixed-size `OrderPool`

* No dynamic allocations during matching

* Price levels stored in sorted vectors

* Intrusive FIFO queues for price-time priority

* O(1) cancel via order ID index

* Deterministic sequence-based ordering

### **Performance-Oriented Design**

* Hot code paths free of heap allocation

* Compact `Order` layout for cache locality

* `lower_bound` search for price-level lookup

* Optional branch prediction hints

* Google Benchmark for microbenchmarks

* perf-based profiling for IPC, branch misses, cache misses

---

## **Architecture**

### **Order Representation**

Each order is represented as an intrusive node in a doubly-linked list:

```cpp
struct Order {
    uint64_t orderId;
    uint32_t price;
    uint32_t quantity;
    uint64_t sequence;
    Side side;
    Order* next;
    Order* prev;
};
```

### **Price Levels**

A price level maintains a FIFO queue of orders:

```cpp
struct PriceLevel {
    uint32_t price;
    uint32_t totalQuantity;
    Order* head;
    Order* tail;
    void addToTail(Order* o);
    void remove(Order* o);
};
```

### **Order Pool**

A fixed-size contiguous pool ensures predictable memory behavior:

```cpp
OrderPool pool(maxOrders);
// allocate() and deallocate() operate via a free list
```

### **Order Book**

The `OrderBook` maintains:

* sorted vectors of bid and ask price levels

* `unordered_map` index for O(1) order lookup (pre-reserved)

* sequence counter for price-time priority

* a trade callback interface

---

## **API Summary**

```cpp
void addLimitOrder(Side side, uint32_t price, uint32_t quantity, uint64_t orderId);
void cancelOrder(uint64_t orderId);
void executeRestingOrder(uint64_t orderId, uint32_t quantity); // testing only
```

Trade event callback:

```cpp
struct Trade {
    uint64_t restingOrderId;
    uint64_t incomingOrderId;
    uint32_t price;
    uint32_t quantity;
};
```

---

## **Matching Logic**

### **BUY incoming**

Matches against lowest ask levels:

```
while incoming.price >= bestAsk.price and qty > 0:
    fill against head of bestAsk FIFO
```

### **SELL incoming**

Matches against highest bid levels:

```
while incoming.price <= bestBid.price and qty > 0:
    fill against head of bestBid FIFO
```

Unfilled quantity becomes a resting order.

---

## **Invariants**

The engine maintains strict invariants:

### **Order Invariants**

* FIFO order preserved via `sequence`

* Linked lists contain no cycles

* `quantity > 0` for all resting orders

### **Price Level Invariants**

* Sorted: bids descending, asks ascending

* `totalQuantity` matches the sum of quantities in the list

* No empty levels remained stored

### **Order Book Invariants**

* `orderIndex[id]` is correct or absent

* `bestBid` and `bestAsk` always point to first non-empty levels

These invariants are verified in tests and debug builds.

---

## **Project Structure**

```
.
├── include/
│   ├── order_book.h
│   ├── order_pool.h
│   ├── price_level.h
│   └── types.h
├── src/
│   ├── order_book.cpp
│   ├── order_pool.cpp
│   ├── price_level.cpp
│   └── main.cpp
├── tests/
│   ├── order_book_test.cpp
│   └── invariants_test.cpp
├── benchmarks/
│   └── order_book_bench.cpp
├── docs/
│   ├── architecture.md
│   └── perf-report.md
└── CMakeLists.txt
```

---

## **Benchmarking**

Benchmarks are implemented using Google Benchmark and include:

* Add-order throughput

* Cancel performance

* Mixed workloads (70% add, 20% cancel, 10% execute)

* Latency distribution calculation for p50 / p99 / p999

Example output will be included in `docs/perf-report.md`.

---

## **Performance Goals**

* **100,000+ operations/sec** single-threaded

* **p99 < 1 µs** for add/cancel (hot path)

* **Cache miss rate < 5%**

* **Branch mispredict < 2%**

* **No allocations in hot path**

Results and perf output will be documented in `docs/perf-report.md`.

---

## **Building & Running**

### **Build**

```bash
mkdir build && cd build
cmake ..
make
```

### **Run Tests**

```bash
ctest
```

### **Run Benchmarks**

```bash
./order_book_bench
```

---

## **Future Improvements**

* Lock-free or wait-free matching engine variant

* NUMA-aware memory placement

* Market/IOC/FOK order types

* Instrumentation hooks (ETW, LTTng)

* FPGA or DPDK-style order ingestion layer

* Multi-symbol support

