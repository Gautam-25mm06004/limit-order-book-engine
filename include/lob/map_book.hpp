#pragma once
// Reference order book: std::map of price levels, each a FIFO std::list; hash map for O(1) cancel lookup.
#include <functional>
#include <list>
#include <map>
#include <unordered_map>
#include "lob/types.hpp"

namespace lob {

class MapBook {
public:
    ExecResult add_limit(OrderId id, Side side, Price px, Qty qty, TimeInForce tif = TimeInForce::GTC);
    ExecResult add_market(OrderId id, Side side, Qty qty);
    bool cancel(OrderId id);
    bool reduce(OrderId id, Qty new_qty);   // shrink keeps time priority

    Price best_bid() const { return bids_.empty() ? 0 : bids_.begin()->first; }
    Price best_ask() const { return asks_.empty() ? 0 : asks_.begin()->first; }
    bool has_bid() const { return !bids_.empty(); }
    bool has_ask() const { return !asks_.empty(); }
    Qty depth_at(Side side, Price px) const;
    std::size_t order_count() const { return index_.size(); }

private:
    struct Resting { OrderId id; Qty qty; };
    using Level = std::list<Resting>;
    using BidMap = std::map<Price, Level, std::greater<Price>>;
    using AskMap = std::map<Price, Level, std::less<Price>>;
    struct Loc { Side side; Price px; Level::iterator it; };

    template <class Map, class Cross>
    void match(Map& book, OrderId taker, Qty& qty, Cross crosses, ExecResult& out);
    Qty available(Side taker_side, Price limit, Qty need) const;

    BidMap bids_;
    AskMap asks_;
    std::unordered_map<OrderId, Loc> index_;
};

}  // namespace lob
