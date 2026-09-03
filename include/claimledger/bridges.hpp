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
    /// λ2(G) − λ2(G−v). Large positive ⇒ deleting v drops algebraic connectivity
    /// (a vital bridge). NaN if this vertex was not leave-one-out evaluated.
    double delta_lambda2 = 0.0;
    bool has_delta_lambda2 = false;
    bool loo = false;
    std::vector<std::string> fields;
    std::string explanation;
};

struct BridgeResult {
    std::vector<BridgeRecord> bridges;
    std::string method = "rayleigh-participation";
    std::string prefilter = "rayleigh-participation";
    std::string pair_a;
    std::string pair_b;
    double pair_algebraic_connectivity = 0.0;
    int loo_candidates = 0;
    int loo_evaluated = 0;
};

struct BridgeOptions {
    int top_k = 32;
    bool leave_one_out = true;
    /// Heuristic pre-filter width. If n ≤ this, every vertex is LOO'd.
    int loo_prefilter = 64;
    unsigned seed = 20260903;
    int lanczos_steps = 0;
};

/// Score papers by how much they hold distant fields together.
/// Combines (1) contribution to the normalized-Laplacian Rayleigh quotient on
/// cross-field edges, (2) neighbor-field Shannon entropy, (3) proximity to the
/// Fiedler cut. When leave-one-out is on (default), the heuristic is a
/// pre-filter: the top `loo_prefilter` vertices get a true Δλ2 from rebuilding
/// L_sym without v, and the emitted ranking is by Δλ2.
BridgeResult detect_bridges(const Graph& g, const SparseMatrix& L, const Embedding& emb,
                            int top_k = 32);

BridgeResult detect_bridges(const Graph& g, const SparseMatrix& L, const Embedding& emb,
                            const BridgeOptions& opt);

/// True leave-one-out: δλ2(v) = λ2(G) − λ2(G−v). Uses the degree-deflated
/// Laplacian (verified against a full Graph rebuild).
double leave_one_out_delta(const Graph& g, NodeId v, double lambda2_full, unsigned seed = 20260903,
                           int lanczos_steps = 0);

}  // namespace claimledger
