#include "lob/map_book.hpp"
#include <algorithm>

namespace lob {

template <class Map, class Cross>
void MapBook::match(Map& book, OrderId taker, Qty& qty, Cross crosses, ExecResult& out) {
    while (qty > 0 && !book.empty()) {
        auto lvl_it = book.begin();
        if (!crosses(lvl_it->first)) break;
        Level& lvl = lvl_it->second;
        while (qty > 0 && !lvl.empty()) {
            Resting& maker = lvl.front();
            Qty q = std::min(qty, maker.qty);
            out.trades.push_back({maker.id, taker, lvl_it->first, q});
            out.filled += q;
            qty -= q;
            maker.qty -= q;
            if (maker.qty == 0) {
                index_.erase(maker.id);
                lvl.pop_front();
            }
        }
        if (lvl.empty()) book.erase(lvl_it);
    }
}

Qty MapBook::available(Side taker_side, Price limit, Qty need) const {
    Qty got = 0;
    if (taker_side == Side::Buy) {
        for (auto& [px, lvl] : asks_) {
            if (px > limit) break;
            for (auto& r : lvl) { got += r.qty; if (got >= need) return got; }
        }
    } else {
        for (auto& [px, lvl] : bids_) {
            if (px < limit) break;
            for (auto& r : lvl) { got += r.qty; if (got >= need) return got; }
        }
    }
    return got;
}

ExecResult MapBook::add_limit(OrderId id, Side side, Price px, Qty qty, TimeInForce tif) {
    ExecResult out;
    if (qty <= 0 || index_.count(id)) { out.rejected = true; return out; }
    if (tif == TimeInForce::FOK && available(side, px, qty) < qty) { out.rejected = true; return out; }
    if (side == Side::Buy) match(asks_, id, qty, [px](Price p) { return p <= px; }, out);
    else match(bids_, id, qty, [px](Price p) { return p >= px; }, out);
    if (qty > 0 && tif == TimeInForce::GTC) {
        if (side == Side::Buy) {
            auto& lvl = bids_[px];
            lvl.push_back({id, qty});
            index_[id] = {side, px, std::prev(lvl.end())};
        } else {
            auto& lvl = asks_[px];
            lvl.push_back({id, qty});
            index_[id] = {side, px, std::prev(lvl.end())};
        }
        out.resting = qty;
    }
    return out;
}

ExecResult MapBook::add_market(OrderId id, Side side, Qty qty) {
    ExecResult out;
    if (qty <= 0) { out.rejected = true; return out; }
    if (side == Side::Buy) match(asks_, id, qty, [](Price) { return true; }, out);
    else match(bids_, id, qty, [](Price) { return true; }, out);
    return out;  // unfilled market quantity is cancelled
}

bool MapBook::cancel(OrderId id) {
    auto f = index_.find(id);
    if (f == index_.end()) return false;
    Loc loc = f->second;
    if (loc.side == Side::Buy) {
        auto lvl = bids_.find(loc.px);
        lvl->second.erase(loc.it);
        if (lvl->second.empty()) bids_.erase(lvl);
    } else {
        auto lvl = asks_.find(loc.px);
        lvl->second.erase(loc.it);
        if (lvl->second.empty()) asks_.erase(lvl);
    }
    index_.erase(f);
    return true;
}

bool MapBook::reduce(OrderId id, Qty new_qty) {
    auto f = index_.find(id);
    if (f == index_.end() || new_qty >= f->second.it->qty) return false;
    if (new_qty <= 0) return cancel(id);
    f->second.it->qty = new_qty;
    return true;
}

Qty MapBook::depth_at(Side side, Price px) const {
    Qty d = 0;
    if (side == Side::Buy) { auto it = bids_.find(px); if (it != bids_.end()) for (auto& r : it->second) d += r.qty; }
    else { auto it = asks_.find(px); if (it != asks_.end()) for (auto& r : it->second) d += r.qty; }
    return d;
}

}  // namespace lob
