#include "claimledger/bridges.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace claimledger {
namespace {

double shannon(const std::unordered_map<std::string, int>& hist) {
    int tot = 0;
    for (const auto& [_, c] : hist) {
        tot += c;
    }
    if (tot <= 0) {
        return 0.0;
    }
    double h = 0.0;
    for (const auto& [_, c] : hist) {
        if (c <= 0) {
            continue;
        }
        const double p = static_cast<double>(c) / static_cast<double>(tot);
        h -= p * std::log(p);
    }
    return h;
}

std::string field_of(const Paper& p) {
    if (!p.field.empty()) {
        return p.field;
    }
    const auto pos = p.category.find('.');
    if (pos == std::string::npos) {
        return p.category;
    }
    return p.category.substr(0, pos);
}

}  // namespace

BridgeResult detect_bridges(const Graph& g, const SparseMatrix& L, const Embedding& emb, int top_k) {
    (void)L;
    BridgeResult out;
    const int n = g.n();
    if (n == 0 || emb.nodes.empty()) {
        return out;
    }

    std::vector<double> fiedler(static_cast<std::size_t>(n), 0.0);
    for (int i = 0; i < n; ++i) {
        if (!emb.nodes[static_cast<std::size_t>(i)].coords.empty()) {
            fiedler[static_cast<std::size_t>(i)] = emb.nodes[static_cast<std::size_t>(i)].coords[0];
        }
    }

    // Identify the two coarsest fields by Fiedler sign majority — used as the
    // "algebraic connectivity gap" pair in the heuristic write-up.
    std::unordered_map<std::string, std::pair<int, double>> field_fiedler;
    for (int i = 0; i < n; ++i) {
        const auto f = field_of(g.paper(i));
        auto& acc = field_fiedler[f];
        acc.first += 1;
        acc.second += fiedler[static_cast<std::size_t>(i)];
    }
    std::vector<std::pair<std::string, double>> field_means;
    field_means.reserve(field_fiedler.size());
    for (const auto& [name, acc] : field_fiedler) {
        field_means.push_back({name, acc.second / std::max(1, acc.first)});
    }
    std::sort(field_means.begin(), field_means.end(),
              [](const auto& a, const auto& b) { return a.second < b.second; });
    if (field_means.size() >= 2) {
        out.pair_a = field_means.front().first;
        out.pair_b = field_means.back().first;
    }
    out.pair_algebraic_connectivity = emb.algebraic_connectivity;

    const auto row = g.row_ptr();
    const auto col = g.col_idx();
    const auto wts = g.weights();
    const auto deg = g.degree();

    std::vector<BridgeRecord> scored;
    scored.reserve(static_cast<std::size_t>(n));

    for (int v = 0; v < n; ++v) {
        const auto& pv = g.paper(v);
        const std::string fv = field_of(pv);
        std::unordered_map<std::string, int> hist;
        hist[fv] += 1;  // self mass so isolates are not infinite-entropy
        double rayleigh_cross = 0.0;
        int cross = 0;
        int neigh = 0;
        const double dv = deg[static_cast<std::size_t>(v)];
        const double inv_sv = dv > 0.0 ? 1.0 / std::sqrt(dv) : 0.0;
        const double phiv = fiedler[static_cast<std::size_t>(v)];

        for (int p = row[static_cast<std::size_t>(v)]; p < row[static_cast<std::size_t>(v) + 1]; ++p) {
            const int u = col[static_cast<std::size_t>(p)];
            const auto fu = field_of(g.paper(u));
            hist[fu] += 1;
            ++neigh;
            if (fu != fv) {
                ++cross;
            }
            const double du = deg[static_cast<std::size_t>(u)];
            const double inv_su = du > 0.0 ? 1.0 / std::sqrt(du) : 0.0;
            const double diff = phiv * inv_sv - fiedler[static_cast<std::size_t>(u)] * inv_su;
            // Each undirected edge is stored twice; take half so energy matches x^T L x / 2.
            if (fu != fv) {
                rayleigh_cross += 0.5 * wts[static_cast<std::size_t>(p)] * diff * diff;
            }
        }

        const double H = shannon(hist);
        const double frac = neigh > 0 ? static_cast<double>(cross) / static_cast<double>(neigh) : 0.0;
        const double fiedler_abs = std::abs(phiv);
        // Nodes that sit near the Fiedler cut (small |φ|) while carrying
        // cross-field Rayleigh energy and high participation entropy.
        const double cut_proximity = 1.0 / (1.0 + 8.0 * fiedler_abs);
        const double score = rayleigh_cross * (1.0 + H) * (0.35 + 0.65 * frac) * (0.25 + 0.75 * cut_proximity);

        BridgeRecord rec;
        rec.id = pv.id;
        rec.index = v;
        rec.score = score;
        rec.participation_entropy = H;
        rec.rayleigh_energy = rayleigh_cross;
        rec.fiedler_abs = fiedler_abs;
        rec.cross_field_fraction = frac;

        std::vector<std::pair<int, std::string>> ranked;
        for (const auto& [name, c] : hist) {
            ranked.push_back({c, name});
        }
        std::sort(ranked.begin(), ranked.end(), [](const auto& a, const auto& b) {
            return a.first > b.first;
        });
        for (const auto& [c, name] : ranked) {
            if (static_cast<int>(rec.fields.size()) >= 4) {
                break;
            }
            rec.fields.push_back(name);
        }

        std::ostringstream ex;
        ex << "H=" << H << " cross=" << frac << " R_cross=" << rayleigh_cross << " |φ|=" << fiedler_abs;
        rec.explanation = ex.str();
        scored.push_back(std::move(rec));
    }

    std::sort(scored.begin(), scored.end(), [](const BridgeRecord& a, const BridgeRecord& b) {
        if (a.score != b.score) {
            return a.score > b.score;
        }
        return a.id < b.id;
    });

    const int keep = std::min(top_k, static_cast<int>(scored.size()));
    out.bridges.reserve(static_cast<std::size_t>(keep));
    for (int i = 0; i < keep; ++i) {
        scored[static_cast<std::size_t>(i)].rank = i + 1;
        out.bridges.push_back(std::move(scored[static_cast<std::size_t>(i)]));
    }
    return out;
}

}  // namespace claimledger
