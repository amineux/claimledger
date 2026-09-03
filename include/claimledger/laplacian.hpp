#pragma once

#include "claimledger/graph.hpp"
#include "claimledger/sparse.hpp"

namespace claimledger {

/// Symmetric normalized Laplacian L = I - D^{-1/2} A D^{-1/2}.
/// Isolated vertices get a zero diagonal (they contribute trivial kernel modes).
SparseMatrix normalized_laplacian(const Graph& g);

/// L_sym of G-v assembled by a degree deflation (subtract the deleted
/// adjacency from neighbor degrees) and a remap of the remaining n-1 vertices.
/// This is *not* a Kron / Schur complement — that would add a clique among
/// neighbors. Verified against `normalized_laplacian(g.without_vertex(v))`.
SparseMatrix normalized_laplacian_except(const Graph& g, NodeId drop);

/// Algebraic connectivity λ2: second-smallest eigenvalue of L_sym.
/// Disconnected graphs have λ2 = 0 (kernel multiplicity ≥ 2).
double algebraic_connectivity(const SparseMatrix& L, unsigned seed = 20260903,
                              int lanczos_steps = 0);

/// Rayleigh quotient x^T L x / x^T x for a unit or non-unit vector.
double rayleigh_quotient(const SparseMatrix& L, std::span<const double> x);

}  // namespace claimledger
