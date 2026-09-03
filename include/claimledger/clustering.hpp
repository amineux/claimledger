#pragma once

#include "claimledger/embedding.hpp"

namespace claimledger {

struct KMeansOptions {
    int k = 0;  // 0 = infer from distinct field labels on the graph papers
    int max_iters = 80;
    unsigned seed = 20260903;
};

/// k-means++ on the row-normalized spectral embedding (Ng–Jordan–Weiss).
void spectral_kmeans(Embedding& emb, const KMeansOptions& opt = {});

}  // namespace claimledger
