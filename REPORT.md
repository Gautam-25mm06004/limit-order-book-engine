# Design and Evaluation of a Price-Time-Priority Limit Order Book

**Gautam Jain** — IIT Bhubaneswar

## 1. Problem
An exchange matching engine must, for every incoming order, find the best opposite price, fill against resting orders
in arrival order (price-time priority), and store any remainder, while supporting cancels in constant time. Latency and
its tail matter more than average throughput.

## 2. Designs
**MapBook (reference).** Bids and asks are `std::map` from price to a FIFO `std::list` of resting orders; an
`unordered_map` from order id to list iterator gives O(1) cancel. Best price is `map.begin()` (O(1)), inserting a new
level is O(log L). Every order and every level allocates on the heap.

**LadderBook (fast).** For a bounded price band (here 20,001 ticks), price levels live in a flat array indexed by
`price − min_price`. Each level keeps head/tail indices into a pool of nodes forming an intrusive doubly-linked list, plus
its total quantity. Freed nodes go on a free list, so steady-state operation does not allocate. Order ids map directly to
node indices (exchange-assigned dense ids). The best bid/ask index is updated on insert and, when the best level empties,
by scanning to the next non-empty level; with order flow concentrated near the touch, the scan is short.

Both designs use integer tick prices so matching never compares floating-point numbers.

## 3. Verification
1. **Scenario tests** cover priority within a level, maker-price execution, multi-level sweeps, cancel, reduce keeping
   priority, IOC remainder dropped, FOK reject when liquidity is short, duplicate-id and zero-quantity rejects.
2. **Differential testing.** A seeded random flow of 200,000 operations is applied to both books. After every operation the
   trades (maker id, price, quantity), best bid/ask presence and value must match exactly, and the book must not be crossed.

## 4. Benchmark
The flow generator draws distances from the mid from a geometric distribution (mean ≈ 6.7 ticks), sizes from a
geometric distribution, makes 5% of limit orders marketable, and cancels uniformly random live orders. On 2M operations
LadderBook runs 3.5× faster (16.5 vs 4.7 M ops/s) and cuts p99 latency 2–2.6× for every operation type. The largest gain is
in the tail, where MapBook pays for heap allocation and pointer chasing through tree nodes.

## 5. Limitations and next steps
Single-threaded and in-memory; no persistence, risk checks or market-data publication. A bounded price band is required
for the ladder (production engines re-centre or fall back to a tree outside the band). Next steps: hugepage-backed pools,
cache-aware level layout, and replay of real exchange messages (e.g., NASDAQ ITCH) for a realistic workload.

## References
1. NASDAQ TotalView-ITCH 5.0 specification (order book semantics: add, execute, cancel, delete, replace).
2. J.-P. Bouchaud, J. Bonart, J. Donier, M. Gould. *Trades, Quotes and Prices: Financial Markets Under the Microscope*. Cambridge University Press, 2018 (order-book dynamics).
