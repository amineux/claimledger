#include "claimledger/graph.hpp"
#include "claimledger/laplacian.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <numeric>

using namespace claimledger;

static Graph path_graph(int n) {
    std::vector<Paper> papers;
    std::vector<Citation> cites;
    for (int i = 0; i < n; ++i) {
        Paper p;
        p.id = "synth-" + std::to_string(i);
        p.category = "path";
        p.field = "path";
        papers.push_back(p);
        if (i > 0) {
            cites.push_back({papers.back().id, papers[static_cast<std::size_t>(i - 1)].id, 2000 + i});
        }
    }
    return Graph::from_papers_and_citations(std::move(papers), std::move(cites));
}

TEST(Laplacian, RowSumsToZeroOnKernel) {
    auto g = path_graph(8);
    auto L = normalized_laplacian(g);
    // For a connected graph the kernel of L_sym is D^{1/2} 1.
    std::vector<double> x(static_cast<std::size_t>(g.n()));
    for (int i = 0; i < g.n(); ++i) {
        x[static_cast<std::size_t>(i)] = std::sqrt(g.degree()[static_cast<std::size_t>(i)]);
    }
    std::vector<double> y(x.size(), 0.0);
    L.multiply(x, y);
    double max_abs = 0.0;
    for (double v : y) {
        max_abs = std::max(max_abs, std::abs(v));
    }
    EXPECT_LT(max_abs, 1e-10);
}

TEST(Laplacian, CompleteGraphSpectrumHint) {
    // K_4: every pair cited. Normalized Laplacian has λ=0 once and 4/3 elsewhere.
    std::vector<Paper> papers;
    std::vector<Citation> cites;
    for (int i = 0; i < 4; ++i) {
        Paper p;
        p.id = "synth-" + std::to_string(i);
        p.field = "k";
        papers.push_back(p);
    }
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < i; ++j) {
            cites.push_back({papers[static_cast<std::size_t>(i)].id, papers[static_cast<std::size_t>(j)].id, 2020});
        }
    }
    auto g = Graph::from_papers_and_citations(papers, cites);
    auto L = normalized_laplacian(g);
    EXPECT_EQ(L.n, 4);
    // Diagonal of L_sym is 1 for every non-isolated vertex.
    for (int i = 0; i < 4; ++i) {
        bool found = false;
        for (int p = L.row_ptr[static_cast<std::size_t>(i)]; p < L.row_ptr[static_cast<std::size_t>(i) + 1]; ++p) {
            if (L.col_idx[static_cast<std::size_t>(p)] == i) {
                EXPECT_NEAR(L.values[static_cast<std::size_t>(p)], 1.0, 1e-12);
                found = true;
            }
        }
        EXPECT_TRUE(found);
    }
}

TEST(Laplacian, IsolatedVertexHasZeroRow) {
    std::vector<Paper> papers = {
        {"synth-0001", "A", 2018, "a", "a", ""},
        {"synth-0002", "B", 2019, "a", "a", ""},
        {"synth-0003", "lonely", 2020, "a", "a", ""},
    };
    std::vector<Citation> cites = {{"synth-0002", "synth-0001", 2019}};
    auto g = Graph::from_papers_and_citations(papers, cites);
    auto L = normalized_laplacian(g);
    const int lonely = *g.index_of("synth-0003");
    EXPECT_EQ(L.row_ptr[static_cast<std::size_t>(lonely) + 1] - L.row_ptr[static_cast<std::size_t>(lonely)], 0);
}
