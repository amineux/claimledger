#pragma once

#include "claimledger/embedding.hpp"
#include "claimledger/graph.hpp"
#include "claimledger/sparse.hpp"

#include <string>
#include <vector>

namespace claimledger {

struct BridgeRecord {
    PaperId id;
    NodeId index = 0;
    int rank = 0;
    double score = 0.0;
    double participation_entropy = 0.0;
    double rayleigh_energy = 0.0;
    double fiedler_abs = 0.0;
    double cross_field_fraction = 0.0;
    std::vector<std::string> fields;
    std::string explanation;
};

struct BridgeResult {
    std::vector<BridgeRecord> bridges;
    std::string method = "rayleigh-participation";
    std::string pair_a;
    std::string pair_b;
    double pair_algebraic_connectivity = 0.0;
};

/// Score papers by how much they hold distant fields together.
/// Combines (1) contribution to the normalized-Laplacian Rayleigh quotient on
/// cross-field edges, (2) neighbor-field Shannon entropy, (3) proximity to the
/// Fiedler cut. This is the polynomial heuristic for the NP-hard interdiction
/// form stated in PROBLEM.md.
BridgeResult detect_bridges(const Graph& g, const SparseMatrix& L, const Embedding& emb,
                            int top_k = 32);

}  // namespace claimledger
