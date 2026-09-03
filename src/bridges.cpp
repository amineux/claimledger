#include "claimledger/bridges.hpp"

#include "claimledger/laplacian.hpp"

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

double leave_one_out_delta(const Graph& g, NodeId v, double lambda2_full, unsigned seed,
                           int lanczos_steps) {
    if (g.n() <= 1) {
        return 0.0;
    }
    auto Lexc = normalized_laplacian_except(g, v);
    const double lam = algebraic_connectivity(Lexc, seed, lanczos_steps);
    return lambda2_full - lam;
}

BridgeResult detect_bridges(const Graph& g, const SparseMatrix& L, const Embedding& emb, int top_k) {
    BridgeOptions opt;
    opt.top_k = top_k;
    return detect_bridges(g, L, emb, opt);
}

BridgeResult detect_bridges(const Graph& g, const SparseMatrix& L, const Embedding& emb,
                            const BridgeOptions& opt) {
    (void)L;
    BridgeResult out;
    out.prefilter = "rayleigh-participation";
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

    if (opt.leave_one_out && !scored.empty()) {
        const int pre = opt.loo_prefilter > 0 ? opt.loo_prefilter : static_cast<int>(scored.size());
        const int cand = std::min(pre, static_cast<int>(scored.size()));
        out.loo_candidates = cand;

        // Interdiction is defined on G[A ∪ B] (PROBLEM.md). Fall back to the
        // full graph when the Fiedler pair is missing or collapses to one field.
        const Graph* host = &g;
        Graph pair_g;
        double lam_full = emb.algebraic_connectivity;
        if (!out.pair_a.empty() && !out.pair_b.empty() && out.pair_a != out.pair_b) {
            pair_g = g.induced_fields(out.pair_a, out.pair_b);
            if (pair_g.n() >= 3 && pair_g.n() < g.n()) {
                host = &pair_g;
                lam_full = algebraic_connectivity(normalized_laplacian(pair_g), opt.seed, opt.lanczos_steps);
            }
        }

        for (int i = 0; i < cand; ++i) {
            auto& rec = scored[static_cast<std::size_t>(i)];
            auto idx = host->index_of(rec.id);
            if (!idx.has_value()) {
                rec.delta_lambda2 = 0.0;
            } else {
                rec.delta_lambda2 =
                    leave_one_out_delta(*host, *idx, lam_full, opt.seed, opt.lanczos_steps);
            }
            rec.has_delta_lambda2 = true;
            rec.loo = true;
            std::ostringstream ex;
            ex << rec.explanation << " Δλ2=" << rec.delta_lambda2;
            rec.explanation = ex.str();
        }
        std::sort(scored.begin(), scored.begin() + cand, [](const BridgeRecord& a, const BridgeRecord& b) {
            if (a.has_delta_lambda2 != b.has_delta_lambda2) {
                return a.has_delta_lambda2;
            }
            if (a.has_delta_lambda2 && b.has_delta_lambda2 &&
                std::abs(a.delta_lambda2 - b.delta_lambda2) > 1e-8) {
                return a.delta_lambda2 > b.delta_lambda2;
            }
            if (a.score != b.score) {
                return a.score > b.score;
            }
            return a.id < b.id;
        });
        out.method = "leave-one-out";
        out.loo_evaluated = cand;
    } else {
        out.method = "rayleigh-participation";
    }

    const int keep = std::min(opt.top_k, static_cast<int>(scored.size()));
    out.bridges.reserve(static_cast<std::size_t>(keep));
    for (int i = 0; i < keep; ++i) {
        scored[static_cast<std::size_t>(i)].rank = i + 1;
        out.bridges.push_back(std::move(scored[static_cast<std::size_t>(i)]));
    }
    return out;
}

}  // namespace claimledger
