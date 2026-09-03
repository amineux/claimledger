#include "claimledger/laplacian.hpp"

#include <cmath>
#include <numeric>

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
