#include "claimledger/timeline.hpp"

#include "claimledger/bridges.hpp"
#include "claimledger/embedding.hpp"
#include "claimledger/graph.hpp"
#include "claimledger/laplacian.hpp"

#include <algorithm>
#include <set>

namespace claimledger {

Timeline compute_timeline(const std::vector<Paper>& papers, const std::vector<Citation>& citations,
                          const TimelineOptions& opt) {
    Timeline tl;
    std::set<int> years;
    for (const auto& p : papers) {
        if (p.year > 0) {
            years.insert(p.year);
        }
    }
    for (int year : years) {
        TimelineSlice sl;
        sl.year = year;
        auto g = Graph::cumulative_at_year(papers, citations, year);
        sl.n = g.n();
        sl.m = g.undirected_edges();
        if (g.n() < 2) {
            sl.lambda2 = 0.0;
            tl.slices.push_back(std::move(sl));
            continue;
        }
        auto L = normalized_laplacian(g);
        sl.lambda2 = algebraic_connectivity(L, opt.seed, opt.lanczos_steps);
        const int k = std::max(1, std::min(opt.k, g.n() - 1));
        try {
            auto emb = embed_graph(g, L, k, opt.seed, opt.lanczos_steps);
            sl.lambda2 = emb.algebraic_connectivity;
            BridgeOptions bopt;
            bopt.top_k = std::max(1, opt.bridges);
            bopt.leave_one_out = false;
            bopt.seed = opt.seed;
            bopt.lanczos_steps = opt.lanczos_steps;
            auto br = detect_bridges(g, L, emb, bopt);
            if (!br.bridges.empty()) {
                sl.top_bridge_id = br.bridges.front().id;
            }
        } catch (...) {
            // Degenerate slice (empty Laplacian, all isolates): keep λ2 from
            // algebraic_connectivity and omit a bridge.
        }
        tl.slices.push_back(std::move(sl));
    }
    return tl;
}

}  // namespace claimledger
