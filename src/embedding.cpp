#include "claimledger/embedding.hpp"

#include <cmath>
#include <stdexcept>

namespace claimledger {

Embedding embed_graph(const Graph& g, const SparseMatrix& L, int k, unsigned seed, int lanczos_steps) {
    if (g.n() == 0) {
        throw std::runtime_error("embed: empty graph");
    }
    const int want = std::max(2, k + 1);  // include trivial mode
    LanczosOptions opt;
    opt.k = std::min(want, g.n());
    opt.max_steps = lanczos_steps;
    opt.seed = seed;
    opt.skip_trivial = false;
    auto lr = lanczos_smallest(L, opt);

    Embedding emb;
    emb.lanczos_steps = lr.steps;
    emb.converged = lr.converged;
    if (lr.pairs.empty()) {
        throw std::runtime_error("embed: Lanczos returned no eigenpairs");
    }
    emb.trivial_eigenvalue = lr.pairs.front().value;

    // Drop near-kernel modes, keep the next k.
    std::vector<const EigenPair*> nontrivial;
    for (const auto& p : lr.pairs) {
        if (std::abs(p.value) < 1e-8 && nontrivial.empty()) {
            continue;
        }
        nontrivial.push_back(&p);
        if (static_cast<int>(nontrivial.size()) >= k) {
            break;
        }
    }
    if (nontrivial.empty()) {
        // Degenerate (complete graph-ish): keep whatever we have after the first.
        for (std::size_t i = 1; i < lr.pairs.size() && static_cast<int>(nontrivial.size()) < k; ++i) {
            nontrivial.push_back(&lr.pairs[i]);
        }
    }
    if (nontrivial.empty()) {
        nontrivial.push_back(&lr.pairs.front());
    }

    emb.k = static_cast<int>(nontrivial.size());
    emb.eigenvalues.reserve(nontrivial.size());
    for (const auto* p : nontrivial) {
        emb.eigenvalues.push_back(p->value);
    }
    emb.algebraic_connectivity = emb.eigenvalues.front();

    const int n = g.n();
    emb.nodes.resize(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        EmbeddedNode node;
        node.index = i;
        node.id = g.paper(i).id;
        node.coords.resize(static_cast<std::size_t>(emb.k));
        double r2 = 0.0;
        for (int d = 0; d < emb.k; ++d) {
            const double v = nontrivial[static_cast<std::size_t>(d)]->vector[static_cast<std::size_t>(i)];
            node.coords[static_cast<std::size_t>(d)] = v;
            r2 += v * v;
        }
        node.radius = std::sqrt(r2);
        node.unit_coords = node.coords;
        if (node.radius > 1e-15) {
            for (double& v : node.unit_coords) {
                v /= node.radius;
            }
        }
        emb.nodes[static_cast<std::size_t>(i)] = std::move(node);
    }
    return emb;
}

}  // namespace claimledger
