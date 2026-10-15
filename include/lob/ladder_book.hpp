#pragma once
// Fast order book for a bounded tick range: a flat array of price levels (index = price - min_price),
// orders stored in a pooled intrusive doubly-linked list per level (no allocation per order after warm-up),
// and best-bid/ask tracking by scanning only when the best level empties.
#include <vector>
#include "lob/types.hpp"

namespace lob {

class LadderBook {
public:
    LadderBook(Price min_px, Price max_px, std::size_t max_orders);

    ExecResult add_limit(OrderId id, Side side, Price px, Qty qty, TimeInForce tif = TimeInForce::GTC);
    ExecResult add_market(OrderId id, Side side, Qty qty);
    bool cancel(OrderId id);
    bool reduce(OrderId id, Qty new_qty);

    Price best_bid() const { return best_bid_ >= 0 ? min_px_ + best_bid_ : 0; }
    Price best_ask() const { return best_ask_ < n_levels_ ? min_px_ + best_ask_ : 0; }
    bool has_bid() const { return best_bid_ >= 0; }
    bool has_ask() const { return best_ask_ < n_levels_; }
    Qty depth_at(Side side, Price px) const;
    std::size_t order_count() const { return live_; }

private:
    static constexpr std::int32_t NIL = -1;
    struct Node { OrderId id; Qty qty; std::int32_t prev, next, level; Side side; bool live; };
    struct Level { std::int32_t head = NIL, tail = NIL; Qty total = 0; };

    std::int32_t alloc_node();
    void unlink(std::int32_t n);
    void push_back(std::int32_t lvl_idx, Side side, std::int32_t n);
    void fix_best_after_remove(Side side, std::int32_t lvl);
    void match(Side taker_side, OrderId taker, Price limit, bool has_limit, Qty& qty, ExecResult& out);
    Qty available(Side taker_side, Price limit, bool has_limit, Qty need) const;

    Price min_px_;
    std::int32_t n_levels_;
    std::vector<Level> bid_lv_, ask_lv_;
    std::vector<Node> nodes_;
    std::vector<std::int32_t> free_;
    std::vector<std::int32_t> id_to_node_;   // order ids are dense in this engine (exchange-assigned sequence)
    std::int32_t best_bid_ = -1, best_ask_;
    std::size_t live_ = 0;
};

}  // namespace lob
