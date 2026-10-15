#pragma once
// Synthetic order flow: limit orders clustered around a drifting mid, cancels of random live orders,
// and market orders. Mix and depth profile are configurable.
#include <cstdint>
#include <random>
#include <vector>
#include "lob/types.hpp"

namespace lob {

struct Op {
    enum Kind : std::uint8_t { Limit, Market, Cancel } kind;
    Side side;
    OrderId id;
    Price px;
    Qty qty;
};

inline std::vector<Op> make_flow(std::size_t n, std::uint64_t seed, double p_limit = 0.55, double p_cancel = 0.35,
                                 Price mid0 = 50000, Price min_px = 40000, Price max_px = 60000) {
    std::mt19937_64 rng(seed);
    std::uniform_real_distribution<double> U(0, 1);
    std::geometric_distribution<int> depth(0.15);      // distance from mid in ticks
    std::geometric_distribution<int> size(0.2);
    std::vector<Op> ops;
    ops.reserve(n);
    std::vector<OrderId> live;
    OrderId next = 1;
    double mid = static_cast<double>(mid0);
    for (std::size_t i = 0; i < n; ++i) {
        mid += (U(rng) < 0.5 ? -0.05 : 0.05);
        double u = U(rng);
        Side s = U(rng) < 0.5 ? Side::Buy : Side::Sell;
        if (u < p_limit || live.empty()) {
            Price d = depth(rng) + 1;
            Price px = static_cast<Price>(mid) + (s == Side::Buy ? -d : d);
            if (U(rng) < 0.05) px = static_cast<Price>(mid) + (s == Side::Buy ? 1 : -1);  // occasional marketable limit
            px = std::min(std::max(px, min_px), max_px);
            ops.push_back({Op::Limit, s, next, px, static_cast<Qty>(size(rng) + 1)});
            live.push_back(next++);
        } else if (u < p_limit + p_cancel) {
            std::size_t k = static_cast<std::size_t>(U(rng) * live.size());
            ops.push_back({Op::Cancel, s, live[k], 0, 0});
            live[k] = live.back();
            live.pop_back();
        } else {
            ops.push_back({Op::Market, s, next++, 0, static_cast<Qty>(size(rng) + 1)});
        }
    }
    return ops;
}

}  // namespace lob
