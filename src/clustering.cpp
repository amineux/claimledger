#include "claimledger/clustering.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <random>
#include <unordered_set>

namespace claimledger {
namespace {

double dist2(const std::vector<double>& a, const std::vector<double>& b) {
    double s = 0.0;
    const std::size_t d = std::min(a.size(), b.size());
    for (std::size_t i = 0; i < d; ++i) {
        const double t = a[i] - b[i];
        s += t * t;
    }
    return s;
}

}  // namespace

void spectral_kmeans(Embedding& emb, const KMeansOptions& opt) {
    const int n = static_cast<int>(emb.nodes.size());
    if (n == 0 || emb.k == 0) {
        return;
    }
    int k = opt.k;
    if (k <= 0) {
        k = std::min(8, std::max(2, emb.k));
    }
    k = std::min(k, n);

    std::mt19937 rng(opt.seed);
    std::vector<std::vector<double>> cents(static_cast<std::size_t>(k));

    // k-means++
    std::uniform_int_distribution<int> pick(0, n - 1);
    cents[0] = emb.nodes[static_cast<std::size_t>(pick(rng))].unit_coords;
    std::vector<double> nearest(static_cast<std::size_t>(n), 0.0);
    for (int c = 1; c < k; ++c) {
        double sum = 0.0;
        for (int i = 0; i < n; ++i) {
            double best = std::numeric_limits<double>::infinity();
            for (int j = 0; j < c; ++j) {
                best = std::min(best, dist2(emb.nodes[static_cast<std::size_t>(i)].unit_coords, cents[static_cast<std::size_t>(j)]));
            }
            nearest[static_cast<std::size_t>(i)] = best;
            sum += best;
        }
        std::uniform_real_distribution<double> ur(0.0, std::max(sum, 1e-15));
        double r = ur(rng);
        int chosen = n - 1;
        for (int i = 0; i < n; ++i) {
            r -= nearest[static_cast<std::size_t>(i)];
            if (r <= 0.0) {
                chosen = i;
                break;
            }
        }
        cents[static_cast<std::size_t>(c)] = emb.nodes[static_cast<std::size_t>(chosen)].unit_coords;
    }

    std::vector<int> assign(static_cast<std::size_t>(n), 0);
    for (int iter = 0; iter < opt.max_iters; ++iter) {
        bool changed = false;
        for (int i = 0; i < n; ++i) {
            int best = 0;
            double best_d = std::numeric_limits<double>::infinity();
            for (int c = 0; c < k; ++c) {
                const double d = dist2(emb.nodes[static_cast<std::size_t>(i)].unit_coords,
                                       cents[static_cast<std::size_t>(c)]);
                if (d < best_d) {
                    best_d = d;
                    best = c;
                }
            }
            if (assign[static_cast<std::size_t>(i)] != best) {
                assign[static_cast<std::size_t>(i)] = best;
                changed = true;
            }
        }
        std::vector<std::vector<double>> next(static_cast<std::size_t>(k));
        std::vector<int> count(static_cast<std::size_t>(k), 0);
        const int dim = emb.k;
        for (int c = 0; c < k; ++c) {
            next[static_cast<std::size_t>(c)].assign(static_cast<std::size_t>(dim), 0.0);
        }
        for (int i = 0; i < n; ++i) {
            const int c = assign[static_cast<std::size_t>(i)];
            ++count[static_cast<std::size_t>(c)];
            const auto& x = emb.nodes[static_cast<std::size_t>(i)].unit_coords;
            for (int d = 0; d < dim; ++d) {
                next[static_cast<std::size_t>(c)][static_cast<std::size_t>(d)] +=
                    d < static_cast<int>(x.size()) ? x[static_cast<std::size_t>(d)] : 0.0;
            }
        }
        for (int c = 0; c < k; ++c) {
            if (count[static_cast<std::size_t>(c)] == 0) {
                cents[static_cast<std::size_t>(c)] =
                    emb.nodes[static_cast<std::size_t>(pick(rng))].unit_coords;
                continue;
            }
            double nrm = 0.0;
            for (double v : next[static_cast<std::size_t>(c)]) {
                nrm += v * v;
            }
            nrm = std::sqrt(nrm);
            if (nrm > 1e-15) {
                for (double& v : next[static_cast<std::size_t>(c)]) {
                    v /= nrm;
                }
            }
            cents[static_cast<std::size_t>(c)] = std::move(next[static_cast<std::size_t>(c)]);
        }
        if (!changed && iter > 0) {
            break;
        }
    }
    for (int i = 0; i < n; ++i) {
        emb.nodes[static_cast<std::size_t>(i)].cluster = assign[static_cast<std::size_t>(i)];
    }
}

}  // namespace claimledger
