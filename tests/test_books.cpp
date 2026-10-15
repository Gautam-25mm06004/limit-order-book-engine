// Scenario tests for both book implementations plus a differential test: 200k random operations are
// applied to MapBook (simple, obviously-correct reference) and LadderBook (fast); every trade, best price
// and depth must agree.
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include "lob/ladder_book.hpp"
#include "lob/map_book.hpp"
#include "../bench/flow.hpp"

using namespace lob;
static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)

template <class Book>
void scenario_tests(Book b, const char* name) {
    // price-time priority: two sells at 101, the earlier one fills first
    b.add_limit(1, Side::Sell, 101, 5);
    b.add_limit(2, Side::Sell, 101, 5);
    b.add_limit(3, Side::Sell, 102, 5);
    b.add_limit(4, Side::Buy, 99, 10);
    CHECK(b.best_ask() == 101 && b.best_bid() == 99);
    auto r = b.add_limit(5, Side::Buy, 101, 7);
    CHECK(r.trades.size() == 2 && r.trades[0].maker_id == 1 && r.trades[0].qty == 5);
    CHECK(r.trades[1].maker_id == 2 && r.trades[1].qty == 2 && r.resting == 0);
    CHECK(b.depth_at(Side::Sell, 101) == 3);
    // trade price is the maker's price, sweep through levels
    r = b.add_market(6, Side::Buy, 6);
    CHECK(r.filled == 6 && r.trades[0].price == 101 && r.trades[1].price == 102);
    CHECK(b.best_ask() == 102 && b.depth_at(Side::Sell, 102) == 2);
    // cancel and reduce
    CHECK(b.cancel(4));
    CHECK(!b.cancel(4));
    CHECK(!b.has_bid());
    b.add_limit(7, Side::Buy, 100, 10);
    b.add_limit(8, Side::Buy, 100, 10);
    CHECK(b.reduce(7, 4));           // keeps priority
    r = b.add_limit(9, Side::Sell, 100, 5);
    CHECK(r.trades.size() == 2 && r.trades[0].maker_id == 7 && r.trades[0].qty == 4 && r.trades[1].maker_id == 8);
    // IOC never rests, FOK is all-or-nothing
    r = b.add_limit(10, Side::Sell, 100, 50, TimeInForce::IOC);
    CHECK(r.filled == 9 && r.resting == 0 && !b.has_bid());
    b.add_limit(11, Side::Buy, 98, 3);
    r = b.add_limit(12, Side::Sell, 98, 5, TimeInForce::FOK);
    CHECK(r.rejected && b.depth_at(Side::Buy, 98) == 3);
    r = b.add_limit(13, Side::Sell, 98, 3, TimeInForce::FOK);
    CHECK(!r.rejected && r.filled == 3);
    // duplicate id and zero qty rejected
    b.add_limit(14, Side::Buy, 90, 1);
    CHECK(b.add_limit(14, Side::Buy, 91, 1).rejected);
    CHECK(b.add_limit(15, Side::Buy, 91, 0).rejected);
    std::printf("scenario tests (%s) done\n", name);
}

int main() {
    scenario_tests(MapBook{}, "MapBook");
    scenario_tests(LadderBook(0, 1000, 1024), "LadderBook");

    auto ops = make_flow(200000, 2024);
    MapBook a;
    LadderBook b(40000, 60000, 1 << 18);
    std::size_t trades = 0;
    for (std::size_t i = 0; i < ops.size(); ++i) {
        const Op& o = ops[i];
        if (o.kind == Op::Cancel) { CHECK(a.cancel(o.id) == b.cancel(o.id)); }
        else {
            ExecResult ra = o.kind == Op::Limit ? a.add_limit(o.id, o.side, o.px, o.qty) : a.add_market(o.id, o.side, o.qty);
            ExecResult rb = o.kind == Op::Limit ? b.add_limit(o.id, o.side, o.px, o.qty) : b.add_market(o.id, o.side, o.qty);
            CHECK(ra.trades.size() == rb.trades.size());
            for (std::size_t t = 0; t < ra.trades.size() && t < rb.trades.size(); ++t)
                CHECK(ra.trades[t].maker_id == rb.trades[t].maker_id && ra.trades[t].price == rb.trades[t].price && ra.trades[t].qty == rb.trades[t].qty);
            trades += ra.trades.size();
        }
        CHECK(a.has_bid() == b.has_bid() && a.has_ask() == b.has_ask());
        if (a.has_bid()) CHECK(a.best_bid() == b.best_bid());
        if (a.has_ask()) CHECK(a.best_ask() == b.best_ask());
        CHECK(!(a.has_bid() && a.has_ask()) || a.best_bid() < a.best_ask());   // book never crossed
        if (failures > 20) break;
    }
    CHECK(a.order_count() == b.order_count());
    std::printf("differential test: %zu ops, %zu trades, %zu resting orders\n", ops.size(), trades, a.order_count());
    if (failures) { std::printf("%d FAILURES\n", failures); return 1; }
    std::printf("ALL TESTS PASSED\n");
    return 0;
}
