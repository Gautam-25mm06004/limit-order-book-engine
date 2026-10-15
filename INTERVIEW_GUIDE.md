# Interview guide — Limit Order Book Engine

## 30-second version
"I built a matching engine in C++17 with price-time priority, limit/market/cancel and IOC/FOK. I wrote two books: a simple
std::map version as a reference and a fast flat-array version with pooled linked lists so nothing allocates on the hot
path. A differential test replays 200,000 random orders through both and checks every trade matches. The fast one does
about 16.5 million ops a second, with p99 latency 2–2.6× lower than the map version."

## Explain these
- **Price-time priority**: best price first; at the same price, earliest order first. Trade happens at the *resting*
  (maker) order's price.
- **Why a flat array**: price is an integer tick, so `price − min` is an array index → O(1) level lookup, contiguous
  memory, no tree rebalancing. Trade-off: needs a bounded price band.
- **Intrusive list + pool**: each order is a node in a vector with prev/next indices; freed nodes are reused. Cancel is
  O(1): id → node index → unlink.
- **Best-price tracking**: update on insert; when the best level empties, scan to the next non-empty level. Usually a few
  ticks because liquidity sits near the touch.
- **FOK**: check available quantity up to the limit price *before* touching the book; IOC: match then drop the rest.
- **Differential testing**: when you have a slow-but-simple implementation, run both on random input and compare. It catches
  bugs that hand-written tests miss.

## Likely questions
1. *Why integer prices?* Floating-point equality is unreliable; exchanges use ticks.
2. *Where does tail latency come from?* Heap allocation, cache misses, sweeping many levels, scanning for a new best price.
3. *How would you make it multi-threaded?* Usually you don't — one thread per instrument (sharding), with lock-free queues
   (SPSC ring buffers) feeding orders in and publishing market data out.
4. *How did you measure latency?* `steady_clock` around each operation, percentiles with `nth_element`; I note the ~20 ns
   clock overhead. For tighter numbers you would use `rdtsc` and pin the thread to a core.
5. *What would you add for a real exchange?* Self-trade prevention, risk/price-band checks, order replace, persistence /
   journaling for recovery, market-data feed.
