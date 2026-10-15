#include "lob/ladder_book.hpp"
#include <algorithm>
#include <stdexcept>

namespace lob {

LadderBook::LadderBook(Price min_px, Price max_px, std::size_t max_orders)
    : min_px_(min_px), n_levels_(static_cast<std::int32_t>(max_px - min_px + 1)),
      bid_lv_(n_levels_), ask_lv_(n_levels_), best_ask_(n_levels_) {
    nodes_.reserve(max_orders);
    free_.reserve(max_orders);
}

std::int32_t LadderBook::alloc_node() {
    if (!free_.empty()) { auto n = free_.back(); free_.pop_back(); return n; }
    nodes_.push_back({});
    return static_cast<std::int32_t>(nodes_.size() - 1);
}

void LadderBook::push_back(std::int32_t li, Side side, std::int32_t n) {
    Level& L = side == Side::Buy ? bid_lv_[li] : ask_lv_[li];
    Node& x = nodes_[n];
    x.prev = L.tail; x.next = NIL; x.level = li; x.side = side;
    if (L.tail != NIL) nodes_[L.tail].next = n; else L.head = n;
    L.tail = n;
    L.total += x.qty;
}

void LadderBook::unlink(std::int32_t n) {
    Node& x = nodes_[n];
    Level& L = x.side == Side::Buy ? bid_lv_[x.level] : ask_lv_[x.level];
    if (x.prev != NIL) nodes_[x.prev].next = x.next; else L.head = x.next;
    if (x.next != NIL) nodes_[x.next].prev = x.prev; else L.tail = x.prev;
    L.total -= x.qty;
    x.live = false;
    id_to_node_[x.id] = NIL;
    free_.push_back(n);
    --live_;
}

void LadderBook::fix_best_after_remove(Side side, std::int32_t lvl) {
    if (side == Side::Buy) {
        if (lvl == best_bid_ && bid_lv_[lvl].head == NIL)
            while (best_bid_ >= 0 && bid_lv_[best_bid_].head == NIL) --best_bid_;
    } else {
        if (lvl == best_ask_ && ask_lv_[lvl].head == NIL)
            while (best_ask_ < n_levels_ && ask_lv_[best_ask_].head == NIL) ++best_ask_;
    }
}

Qty LadderBook::available(Side taker_side, Price limit, bool has_limit, Qty need) const {
    Qty got = 0;
    if (taker_side == Side::Buy) {
        std::int32_t hi = has_limit ? std::min<std::int32_t>(n_levels_ - 1, static_cast<std::int32_t>(limit - min_px_)) : n_levels_ - 1;
        for (std::int32_t i = best_ask_; i <= hi && got < need; ++i) got += ask_lv_[i].total;
    } else {
        std::int32_t lo = has_limit ? std::max<std::int32_t>(0, static_cast<std::int32_t>(limit - min_px_)) : 0;
        for (std::int32_t i = best_bid_; i >= lo && got < need; --i) got += bid_lv_[i].total;
    }
    return got;
}

void LadderBook::match(Side taker_side, OrderId taker, Price limit, bool has_limit, Qty& qty, ExecResult& out) {
    if (taker_side == Side::Buy) {
        while (qty > 0 && best_ask_ < n_levels_ && (!has_limit || min_px_ + best_ask_ <= limit)) {
            Level& L = ask_lv_[best_ask_];
            while (qty > 0 && L.head != NIL) {
                std::int32_t n = L.head;
                Node& m = nodes_[n];
                Qty q = std::min(qty, m.qty);
                out.trades.push_back({m.id, taker, min_px_ + best_ask_, q});
                out.filled += q; qty -= q; m.qty -= q; L.total -= q;
                if (m.qty == 0) unlink(n);
            }
            if (L.head == NIL) fix_best_after_remove(Side::Sell, best_ask_);
        }
    } else {
        while (qty > 0 && best_bid_ >= 0 && (!has_limit || min_px_ + best_bid_ >= limit)) {
            Level& L = bid_lv_[best_bid_];
            while (qty > 0 && L.head != NIL) {
                std::int32_t n = L.head;
                Node& m = nodes_[n];
                Qty q = std::min(qty, m.qty);
                out.trades.push_back({m.id, taker, min_px_ + best_bid_, q});
                out.filled += q; qty -= q; m.qty -= q; L.total -= q;
                if (m.qty == 0) unlink(n);
            }
            if (L.head == NIL) fix_best_after_remove(Side::Buy, best_bid_);
        }
    }
}

ExecResult LadderBook::add_limit(OrderId id, Side side, Price px, Qty qty, TimeInForce tif) {
    ExecResult out;
    if (qty <= 0 || px < min_px_ || px >= min_px_ + n_levels_) { out.rejected = true; return out; }
    if (id >= id_to_node_.size()) id_to_node_.resize(std::max<std::size_t>(id + 1, id_to_node_.size() * 2), NIL);
    if (id_to_node_[id] != NIL) { out.rejected = true; return out; }
    if (tif == TimeInForce::FOK && available(side, px, true, qty) < qty) { out.rejected = true; return out; }
    match(side, id, px, true, qty, out);
    if (qty > 0 && tif == TimeInForce::GTC) {
        std::int32_t n = alloc_node();
        nodes_[n].id = id; nodes_[n].qty = qty; nodes_[n].live = true;
        auto li = static_cast<std::int32_t>(px - min_px_);
        push_back(li, side, n);
        id_to_node_[id] = n;
        ++live_;
        if (side == Side::Buy) best_bid_ = std::max(best_bid_, li);
        else best_ask_ = std::min(best_ask_, li);
        out.resting = qty;
    }
    return out;
}

ExecResult LadderBook::add_market(OrderId id, Side side, Qty qty) {
    ExecResult out;
    if (qty <= 0) { out.rejected = true; return out; }
    match(side, id, 0, false, qty, out);
    return out;
}

bool LadderBook::cancel(OrderId id) {
    if (id >= id_to_node_.size() || id_to_node_[id] == NIL) return false;
    std::int32_t n = id_to_node_[id];
    Side s = nodes_[n].side;
    std::int32_t lvl = nodes_[n].level;
    unlink(n);
    fix_best_after_remove(s, lvl);
    return true;
}

bool LadderBook::reduce(OrderId id, Qty new_qty) {
    if (id >= id_to_node_.size() || id_to_node_[id] == NIL) return false;
    Node& x = nodes_[id_to_node_[id]];
    if (new_qty >= x.qty) return false;
    if (new_qty <= 0) return cancel(id);
    Level& L = x.side == Side::Buy ? bid_lv_[x.level] : ask_lv_[x.level];
    L.total -= x.qty - new_qty;
    x.qty = new_qty;
    return true;
}

Qty LadderBook::depth_at(Side side, Price px) const {
    if (px < min_px_ || px >= min_px_ + n_levels_) return 0;
    auto li = static_cast<std::size_t>(px - min_px_);
    return side == Side::Buy ? bid_lv_[li].total : ask_lv_[li].total;
}

}  // namespace lob
