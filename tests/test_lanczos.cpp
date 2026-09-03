#include "claimledger/graph.hpp"
#include "claimledger/lanczos.hpp"
#include "claimledger/laplacian.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>

using namespace claimledger;

static Graph cycle_graph(int n) {
    std::vector<Paper> papers;
    std::vector<Citation> cites;
    for (int i = 0; i < n; ++i) {
        Paper p;
        p.id = "synth-" + std::to_string(i);
        p.field = "cycle";
        papers.push_back(p);
    }
    for (int i = 0; i < n; ++i) {
        cites.push_back({papers[static_cast<std::size_t>(i)].id,
                         papers[static_cast<std::size_t>((i + 1) % n)].id, 2020});
    }
    return Graph::from_papers_and_citations(std::move(papers), std::move(cites));
}

TEST(Lanczos, RecoversCycleSpectrum) {
    // Normalized Laplacian of C_n has λ_k = 1 - cos(2π k / n).
    constexpr int n = 6;
    auto g = cycle_graph(n);
    auto L = normalized_laplacian(g);
    LanczosOptions opt;
    opt.k = n;
    opt.max_steps = n;
    opt.seed = 1;
    auto r = lanczos_smallest(L, opt);
    ASSERT_GE(static_cast<int>(r.pairs.size()), 3);
    EXPECT_NEAR(r.pairs[0].value, 0.0, 1e-6);
    // Next two should both be 1 - cos(2π/6) = 0.5
    EXPECT_NEAR(r.pairs[1].value, 0.5, 2e-3);
    EXPECT_NEAR(r.pairs[2].value, 0.5, 2e-3);
    EXPECT_LT(r.max_residual, 1e-5);
}

TEST(Lanczos, CompleteGraphFiedler) {
    std::vector<Paper> papers;
    std::vector<Citation> cites;
    for (int i = 0; i < 5; ++i) {
        Paper p;
        p.id = "synth-" + std::to_string(i);
        p.field = "k";
        papers.push_back(p);
    }
    for (int i = 0; i < 5; ++i) {
        for (int j = 0; j < i; ++j) {
            cites.push_back({papers[static_cast<std::size_t>(i)].id, papers[static_cast<std::size_t>(j)].id, 2020});
        }
    }
    auto g = Graph::from_papers_and_citations(papers, cites);
    auto L = normalized_laplacian(g);
    LanczosOptions opt;
    opt.k = 5;
    opt.max_steps = 5;
    auto r = lanczos_smallest(L, opt);
    ASSERT_GE(r.pairs.size(), 2u);
    EXPECT_NEAR(r.pairs[0].value, 0.0, 1e-6);
    // λ = n/(n-1) = 1.25 for K_5
    EXPECT_NEAR(r.pairs[1].value, 5.0 / 4.0, 2e-3);
}

TEST(Jacobi, DiagonalMatrix) {
    std::vector<double> A = {3, 0, 0, 0, 1, 0, 0, 0, 2};
    std::vector<double> evals, evecs;
    jacobi_symmetric(A, 3, evals, evecs);
    std::sort(evals.begin(), evals.end());
    EXPECT_NEAR(evals[0], 1.0, 1e-10);
    EXPECT_NEAR(evals[1], 2.0, 1e-10);
    EXPECT_NEAR(evals[2], 3.0, 1e-10);
}
