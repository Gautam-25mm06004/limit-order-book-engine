// Throughput and per-operation latency for both books on the same synthetic flow.
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <string>
#include <vector>
#include "lob/ladder_book.hpp"
#include "lob/map_book.hpp"
#include "flow.hpp"

using namespace lob;
using clk = std::chrono::steady_clock;

struct Lat { std::vector<double> v[3]; };

static double pct(std::vector<double>& v, double p) {
    if (v.empty()) return 0;
    std::size_t k = static_cast<std::size_t>(p / 100.0 * (v.size() - 1));
    std::nth_element(v.begin(), v.begin() + k, v.end());
    return v[k];
}

template <class Book>
void run(const char* name, Book& book, const std::vector<Op>& ops, FILE* json, bool last) {
    // pass 1: throughput (no timing per op)
    std::size_t sink = 0;
    auto t0 = clk::now();
    for (const Op& o : ops) {
        if (o.kind == Op::Cancel) sink += book.cancel(o.id);
        else if (o.kind == Op::Limit) sink += book.add_limit(o.id, o.side, o.px, o.qty).trades.size();
        else sink += book.add_market(o.id, o.side, o.qty).trades.size();
    }
    double secs = std::chrono::duration<double>(clk::now() - t0).count();
    std::printf("%-11s %8.2f M ops/s  (%zu ops in %.3f s, checksum %zu)\n", name, ops.size() / secs / 1e6, ops.size(), secs, sink);
    if (json) std::fprintf(json, "  \"%s\": {\"mops_per_s\": %.3f", name, ops.size() / secs / 1e6);
    (void)last;
}

template <class Book>
void latency(const char* name, Book book, const std::vector<Op>& ops, FILE* json, bool last) {
    Lat L;
    for (const Op& o : ops) {
        auto t0 = clk::now();
        if (o.kind == Op::Cancel) book.cancel(o.id);
        else if (o.kind == Op::Limit) book.add_limit(o.id, o.side, o.px, o.qty);
        else book.add_market(o.id, o.side, o.qty);
        L.v[o.kind].push_back(std::chrono::duration<double, std::nano>(clk::now() - t0).count());
    }
    const char* kinds[3] = {"limit", "market", "cancel"};
    for (int k = 0; k < 3; ++k) {
        double p50 = pct(L.v[k], 50), p99 = pct(L.v[k], 99), p999 = pct(L.v[k], 99.9);
        std::printf("  %-7s n=%-8zu p50 %6.0f ns  p99 %7.0f ns  p99.9 %8.0f ns\n", kinds[k], L.v[k].size(), p50, p99, p999);
        if (json) std::fprintf(json, ", \"%s_p50_ns\": %.0f, \"%s_p99_ns\": %.0f, \"%s_p999_ns\": %.0f", kinds[k], p50, kinds[k], p99, kinds[k], p999);
    }
    if (json) std::fprintf(json, "}%s\n", last ? "" : ",");
}

int main(int argc, char** argv) {
    std::size_t n = argc > 1 ? std::stoul(argv[1]) : 2000000;
    auto ops = make_flow(n, 7);
    std::size_t lim = 0, mkt = 0, can = 0;
    for (auto& o : ops) (o.kind == Op::Limit ? lim : o.kind == Op::Market ? mkt : can)++;
    std::printf("flow: %zu ops (%zu limit, %zu market, %zu cancel)\n", n, lim, mkt, can);
    FILE* json = std::fopen("results/bench.json", "w");
    if (json) std::fprintf(json, "{\n  \"ops\": %zu, \"limit\": %zu, \"market\": %zu, \"cancel\": %zu,\n", n, lim, mkt, can);
    { MapBook m; run("MapBook", m, ops, json, false); }
    latency("MapBook", MapBook{}, ops, json, false);
    { LadderBook l(40000, 60000, n); run("LadderBook", l, ops, json, false); }
    latency("LadderBook", LadderBook(40000, 60000, n), ops, json, true);
    if (json) { std::fprintf(json, "}\n"); std::fclose(json); }
}
