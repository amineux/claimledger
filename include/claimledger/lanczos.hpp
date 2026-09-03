#pragma once

#include "claimledger/sparse.hpp"
#include "claimledger/types.hpp"

#include <cstddef>
#include <vector>

namespace claimledger {

struct LanczosOptions {
    int k = 8;                 // number of eigenpairs requested (including the trivial mode)
    int max_steps = 0;         // 0 = auto (min(n-1, max(4k+16, 64)))
    double residual_tol = 1e-8;
    unsigned seed = 20260903;
    bool skip_trivial = false; // if true, drop eigenvalues ~ 0 from the returned set
};

struct LanczosResult {
    std::vector<EigenPair> pairs;  // sorted ascending by eigenvalue
    int steps = 0;
    int converged = 0;
    double max_residual = 0.0;
};

/// Self-contained Lanczos + Jacobi Rayleigh-Ritz eigensolver for the smallest
/// eigenpairs of a symmetric sparse matrix (the normalized Laplacian).
LanczosResult lanczos_smallest(const SparseMatrix& A, const LanczosOptions& opt = {});

/// Dense Jacobi eigensolver for a small symmetric matrix stored row-major.
void jacobi_symmetric(std::vector<double>& A, int n, std::vector<double>& evals,
                      std::vector<double>& evecs, double tol = 1e-14, int max_sweeps = 80);

}  // namespace claimledger
