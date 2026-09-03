#pragma once

#include "claimledger/graph.hpp"
#include "claimledger/sparse.hpp"

namespace claimledger {

/// Symmetric normalized Laplacian L = I - D^{-1/2} A D^{-1/2}.
/// Isolated vertices get a zero diagonal (they contribute trivial kernel modes).
SparseMatrix normalized_laplacian(const Graph& g);

/// Rayleigh quotient x^T L x / x^T x for a unit or non-unit vector.
double rayleigh_quotient(const SparseMatrix& L, std::span<const double> x);

}  // namespace claimledger
