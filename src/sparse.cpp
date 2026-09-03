#include "claimledger/sparse.hpp"

#include <algorithm>
#include <cmath>

namespace claimledger {

void SparseMatrix::multiply(std::span<const double> x, std::span<double> y) const {
    std::fill(y.begin(), y.end(), 0.0);
    for (int i = 0; i < n; ++i) {
        double acc = 0.0;
        for (int p = row_ptr[i]; p < row_ptr[i + 1]; ++p) {
            acc += values[p] * x[static_cast<std::size_t>(col_idx[p])];
        }
        y[static_cast<std::size_t>(i)] = acc;
    }
}

double SparseMatrix::quadratic_form(std::span<const double> x) const {
    double acc = 0.0;
    for (int i = 0; i < n; ++i) {
        double xi = x[static_cast<std::size_t>(i)];
        for (int p = row_ptr[i]; p < row_ptr[i + 1]; ++p) {
            acc += values[p] * xi * x[static_cast<std::size_t>(col_idx[p])];
        }
    }
    return acc;
}

}  // namespace claimledger
