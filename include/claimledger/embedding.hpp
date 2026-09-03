#pragma once

#include "claimledger/graph.hpp"
#include "claimledger/lanczos.hpp"
#include "claimledger/sparse.hpp"

#include <string>
#include <vector>

namespace claimledger {

struct EmbeddedNode {
    NodeId index = 0;
    PaperId id;
    std::vector<double> coords;       // raw eigenvectors 1..k (skip λ≈0)
    std::vector<double> unit_coords;  // row-normalized (Ng–Jordan–Weiss)
    int cluster = -1;
    double radius = 0.0;
};

struct Embedding {
    int k = 0;
    std::vector<double> eigenvalues;  // nontrivial, ascending
    double algebraic_connectivity = 0.0;
    double trivial_eigenvalue = 0.0;
    std::vector<EmbeddedNode> nodes;
    int lanczos_steps = 0;
    int converged = 0;
};

Embedding embed_graph(const Graph& g, const SparseMatrix& L, int k, unsigned seed = 20260903,
                      int lanczos_steps = 0);

}  // namespace claimledger
