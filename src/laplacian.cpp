#include "claimledger/laplacian.hpp"

#include "claimledger/lanczos.hpp"

#include <cmath>
#include <numeric>
#include <stdexcept>

namespace claimledger {

SparseMatrix normalized_laplacian(const Graph& g) {
    SparseMatrix L;
    const int n = g.n();
    L.n = n;
    L.row_ptr.assign(static_cast<std::size_t>(n + 1), 0);

    const auto deg = g.degree();
    const auto row = g.row_ptr();
    const auto col = g.col_idx();
    const auto wts = g.weights();

    // Count nonzeros: diagonal (if deg>0) plus each off-diagonal neighbor.
    std::vector<int> counts(static_cast<std::size_t>(n), 0);
    for (int i = 0; i < n; ++i) {
        counts[static_cast<std::size_t>(i)] = (row[static_cast<std::size_t>(i) + 1] - row[static_cast<std::size_t>(i)]);
        if (deg[static_cast<std::size_t>(i)] > 0.0) {
            ++counts[static_cast<std::size_t>(i)];  // diagonal
        }
    }
    for (int i = 0; i < n; ++i) {
        L.row_ptr[static_cast<std::size_t>(i + 1)] = L.row_ptr[static_cast<std::size_t>(i)] + counts[static_cast<std::size_t>(i)];
    }
    L.col_idx.resize(static_cast<std::size_t>(L.row_ptr.back()));
    L.values.resize(L.col_idx.size());

    for (int i = 0; i < n; ++i) {
        int p = L.row_ptr[static_cast<std::size_t>(i)];
        const double di = deg[static_cast<std::size_t>(i)];
        const double inv_sqrt_di = di > 0.0 ? 1.0 / std::sqrt(di) : 0.0;
        const int a0 = row[static_cast<std::size_t>(i)];
        const int a1 = row[static_cast<std::size_t>(i) + 1];
        bool wrote_diag = false;
        auto write = [&](int j, double v) {
            L.col_idx[static_cast<std::size_t>(p)] = j;
            L.values[static_cast<std::size_t>(p)] = v;
            ++p;
        };
        for (int a = a0; a < a1; ++a) {
            const int j = col[static_cast<std::size_t>(a)];
            if (!wrote_diag && j > i && di > 0.0) {
                write(i, 1.0);
                wrote_diag = true;
            }
            const double dj = deg[static_cast<std::size_t>(j)];
            const double inv_sqrt_dj = dj > 0.0 ? 1.0 / std::sqrt(dj) : 0.0;
            write(j, -wts[static_cast<std::size_t>(a)] * inv_sqrt_di * inv_sqrt_dj);
        }
        if (!wrote_diag && di > 0.0) {
            write(i, 1.0);
        }
    }
    return L;
}

SparseMatrix normalized_laplacian_except(const Graph& g, NodeId drop) {
    const int n0 = g.n();
    if (drop < 0 || drop >= n0) {
        throw std::runtime_error("normalized_laplacian_except: index out of range");
    }
    const int n = n0 - 1;
    SparseMatrix L;
    L.n = n;
    if (n <= 0) {
        L.row_ptr = {0};
        return L;
    }

    auto remap = [drop](int i) { return i < drop ? i : i - 1; };

    const auto deg0 = g.degree();
    const auto row = g.row_ptr();
    const auto col = g.col_idx();
    const auto wts = g.weights();

    std::vector<double> deg(static_cast<std::size_t>(n), 0.0);
    for (int i = 0; i < n0; ++i) {
        if (i == drop) {
            continue;
        }
        double d = deg0[static_cast<std::size_t>(i)];
        for (int p = row[static_cast<std::size_t>(i)]; p < row[static_cast<std::size_t>(i) + 1]; ++p) {
            if (col[static_cast<std::size_t>(p)] == drop) {
                d -= wts[static_cast<std::size_t>(p)];
            }
        }
        if (d < 0.0 && d > -1e-15) {
            d = 0.0;
        }
        deg[static_cast<std::size_t>(remap(i))] = d;
    }

    L.row_ptr.assign(static_cast<std::size_t>(n + 1), 0);
    std::vector<int> counts(static_cast<std::size_t>(n), 0);
    for (int i = 0; i < n0; ++i) {
        if (i == drop) {
            continue;
        }
        const int ii = remap(i);
        int neigh = 0;
        for (int p = row[static_cast<std::size_t>(i)]; p < row[static_cast<std::size_t>(i) + 1]; ++p) {
            if (col[static_cast<std::size_t>(p)] != drop) {
                ++neigh;
            }
        }
        counts[static_cast<std::size_t>(ii)] = neigh;
        if (deg[static_cast<std::size_t>(ii)] > 0.0) {
            ++counts[static_cast<std::size_t>(ii)];
        }
    }
    for (int i = 0; i < n; ++i) {
        L.row_ptr[static_cast<std::size_t>(i + 1)] =
            L.row_ptr[static_cast<std::size_t>(i)] + counts[static_cast<std::size_t>(i)];
    }
    L.col_idx.resize(static_cast<std::size_t>(L.row_ptr.back()));
    L.values.resize(L.col_idx.size());

    for (int i = 0; i < n0; ++i) {
        if (i == drop) {
            continue;
        }
        const int ii = remap(i);
        int p = L.row_ptr[static_cast<std::size_t>(ii)];
        const double di = deg[static_cast<std::size_t>(ii)];
        const double inv_sqrt_di = di > 0.0 ? 1.0 / std::sqrt(di) : 0.0;
        bool wrote_diag = false;
        auto write = [&](int j, double v) {
            L.col_idx[static_cast<std::size_t>(p)] = j;
            L.values[static_cast<std::size_t>(p)] = v;
            ++p;
        };
        for (int a = row[static_cast<std::size_t>(i)]; a < row[static_cast<std::size_t>(i) + 1]; ++a) {
            const int j0 = col[static_cast<std::size_t>(a)];
            if (j0 == drop) {
                continue;
            }
            const int jj = remap(j0);
            if (!wrote_diag && jj > ii && di > 0.0) {
                write(ii, 1.0);
                wrote_diag = true;
            }
            const double dj = deg[static_cast<std::size_t>(jj)];
            const double inv_sqrt_dj = dj > 0.0 ? 1.0 / std::sqrt(dj) : 0.0;
            write(jj, -wts[static_cast<std::size_t>(a)] * inv_sqrt_di * inv_sqrt_dj);
        }
        if (!wrote_diag && di > 0.0) {
            write(ii, 1.0);
        }
    }
    return L;
}

double algebraic_connectivity(const SparseMatrix& L, unsigned seed, int lanczos_steps) {
    if (L.n < 2) {
        return 0.0;
    }
    LanczosOptions opt;
    opt.k = std::min(L.n, 4);
    opt.seed = seed;
    opt.max_steps = lanczos_steps;
    auto r = lanczos_smallest(L, opt);
    if (r.pairs.size() < 2) {
        return 0.0;
    }
    // Fiedler numbering: λ2 is the second-smallest eigenvalue. A disconnected
    // graph has kernel multiplicity ≥ 2, so this value is ~0. Snap numerical
    // kernel leftovers so leave-one-out deltas compare cleanly.
    const double lam = r.pairs[1].value;
    return lam < 1e-8 ? 0.0 : lam;
}

double rayleigh_quotient(const SparseMatrix& L, std::span<const double> x) {
    double num = L.quadratic_form(x);
    double den = 0.0;
    for (double v : x) {
        den += v * v;
    }
    if (den <= 0.0) {
        return 0.0;
    }
    return num / den;
}

}  // namespace claimledger
