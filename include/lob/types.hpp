#pragma once
#include <cstdint>
#include <vector>

namespace lob {

using OrderId = std::uint64_t;
using Price = std::int64_t;   // integer ticks: no floating-point prices in the matching path
using Qty = std::int64_t;

enum class Side : std::uint8_t { Buy, Sell };
enum class TimeInForce : std::uint8_t { GTC, IOC, FOK };

struct Trade {
    OrderId maker_id;
    OrderId taker_id;
    Price price;
    Qty qty;
};

struct ExecResult {
    std::vector<Trade> trades;
    Qty filled = 0;
    Qty resting = 0;      // quantity left on the book
    bool rejected = false;
};

}  // namespace lob
