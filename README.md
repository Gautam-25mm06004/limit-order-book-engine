# Limit Order Book Matching Engine (C++17)

A price-time-priority matching engine with two interchangeable book implementations, a differential test that
checks them against each other on 200,000 random operations, and a latency/throughput benchmark.

| Implementation | Data structure | Purpose |
|---|---|---|
| `MapBook` | `std::map<price, std::list<order>>` + `unordered_map` index | Simple, obviously-correct reference |
| `LadderBook` | Flat array of price levels, pooled intrusive linked lists, dense id → node table, tracked best bid/ask | Fast path: no allocation per order after warm-up, O(1) cancel |

**Supported:** limit (GTC / IOC / FOK), market, cancel, reduce-quantity (keeps queue priority), integer tick prices,
trades at the resting order's price, multi-level sweeps.

## Results (2,000,000 synthetic operations: 55% limit, 35% cancel, 10% market; Intel Xeon 2.1 GHz cloud VM, `-O2`)

| | MapBook | LadderBook | Speed-up |
|---|---|---|---|
| Throughput | 4.7 M ops/s | **16.5 M ops/s** | 3.5× |
| Limit order p50 / p99 | 114 / 954 ns | 51 / 372 ns | 2.2× / 2.6× |
| Market order p50 / p99 | 134 / 1,250 ns | 95 / 474 ns | 1.4× / 2.6× |
| Cancel p50 / p99 | 213 / 686 ns | 94 / 347 ns | 2.3× / 2.0× |

Latencies include ~20–25 ns of `steady_clock` overhead per measurement; on a shared VM, runs vary by about ±20%.
Raw output: [`results/bench.txt`](results/bench.txt), [`results/bench.json`](results/bench.json).

**Correctness:** scenario tests (priority, sweeps, IOC/FOK, reduce, duplicate ids) pass for both books, and the
differential test replays 200,000 random operations (60,859 trades) through both and finds identical trades, best prices
and order counts after every step, with the book never crossed. See [`results/tests.txt`](results/tests.txt).

## Build and run
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j
./build/test_books          # scenario + differential tests
./build/bench 2000000       # writes results/bench.json
```

## Layout
```
include/lob/   types.hpp, map_book.hpp, ladder_book.hpp
src/           map_book.cpp, ladder_book.cpp
tests/         test_books.cpp      (scenario + differential tests)
bench/         flow.hpp (synthetic order flow), bench.cpp
```
See `REPORT.md` for design notes and `INTERVIEW_GUIDE.md` for an explanation and likely questions.
