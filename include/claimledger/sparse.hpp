#pragma once

#include <span>
#include <vector>

namespace claimledger {

/// Compressed-sparse-row matrix with a matrix-vector product.
struct SparseMatrix {
    int n = 0;
    std::vector<int> row_ptr;
    std::vector<int> col_idx;
    std::vector<double> values;

    void multiply(std::span<const double> x, std::span<double> y) const;
    [[nodiscard]] double quadratic_form(std::span<const double> x) const;
    [[nodiscard]] int nnz() const noexcept {
        return static_cast<int>(values.size());
    }
};

}  // namespace claimledger
