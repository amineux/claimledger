#include "claimledger/bridges.hpp"
#include "claimledger/clustering.hpp"
#include "claimledger/embedding.hpp"
#include "claimledger/graph.hpp"
#include "claimledger/io.hpp"
#include "claimledger/laplacian.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <string>

using namespace claimledger;

TEST(Bridges, PlantedCutRanksTheLiaison) {
    // Two cliques (cs / qbio) joined only through synth-bridge.
    std::vector<Paper> papers;
    std::vector<Citation> cites;
    auto add = [&](const std::string& id, const std::string& field) {
        Paper p;
        p.id = id;
        p.title = id;
        p.year = 2020;
        p.category = field == "cs" ? "cs.LG" : "q-bio.NC";
        p.field = field;
        papers.push_back(p);
    };
    for (int i = 0; i < 6; ++i) {
        add("synth-cs-" + std::to_string(i), "cs");
    }
    for (int i = 0; i < 6; ++i) {
        add("synth-qb-" + std::to_string(i), "qbio");
    }
    add("synth-bridge", "cs");

    auto cite = [&](const std::string& a, const std::string& b) { cites.push_back({a, b, 2021}); };
    for (int i = 0; i < 6; ++i) {
        for (int j = 0; j < i; ++j) {
            cite("synth-cs-" + std::to_string(i), "synth-cs-" + std::to_string(j));
            cite("synth-qb-" + std::to_string(i), "synth-qb-" + std::to_string(j));
        }
    }
    // The liaison is the only cross-field vertex.
    for (int i = 0; i < 6; ++i) {
        cite("synth-bridge", "synth-cs-" + std::to_string(i));
        cite("synth-bridge", "synth-qb-" + std::to_string(i));
    }

    auto g = Graph::from_papers_and_citations(papers, cites);
    auto L = normalized_laplacian(g);
    auto emb = embed_graph(g, L, 3, 7, 24);
    KMeansOptions km;
    km.k = 2;
    spectral_kmeans(emb, km);
    auto br = detect_bridges(g, L, emb, 5);
    ASSERT_FALSE(br.bridges.empty());
    EXPECT_EQ(br.bridges.front().id, "synth-bridge");
    EXPECT_GT(br.bridges.front().cross_field_fraction, 0.4);
    EXPECT_GT(br.bridges.front().score, br.bridges.back().score);
}

TEST(Bridges, TinyFixtureSurfacesABridge) {
    const std::string dir = std::string(CLAIMLEDGER_FIXTURE_DIR) + "/tiny";
    auto corp = load_corpus_dir(dir);
    auto g = Graph::from_papers_and_citations(corp.papers, corp.citations);
    auto L = normalized_laplacian(g);
    auto emb = embed_graph(g, L, 3, 1, 16);
    auto br = detect_bridges(g, L, emb, 3);
    ASSERT_FALSE(br.bridges.empty());
    // The hand-planted liaison in the tiny fixture is synth-0007.
    bool found = false;
    for (const auto& b : br.bridges) {
        if (b.id == "synth-0007") {
            found = true;
        }
    }
    EXPECT_TRUE(found);
}

TEST(Bridges, TinyFixtureLiaisonHasLargestDeltaLambda2) {
    const std::string dir = std::string(CLAIMLEDGER_FIXTURE_DIR) + "/tiny";
    auto corp = load_corpus_dir(dir);
    auto g = Graph::from_papers_and_citations(corp.papers, corp.citations);
    auto L = normalized_laplacian(g);
    auto emb = embed_graph(g, L, 3, 20260903, 0);
    BridgeOptions opt;
    opt.top_k = g.n();
    opt.leave_one_out = true;
    opt.loo_prefilter = 64;
    opt.seed = 20260903;
    auto br = detect_bridges(g, L, emb, opt);
    ASSERT_FALSE(br.bridges.empty());
    EXPECT_EQ(br.method, "leave-one-out");
    EXPECT_EQ(br.loo_evaluated, g.n());
    EXPECT_EQ(br.bridges.front().id, "synth-0007");
    ASSERT_TRUE(br.bridges.front().has_delta_lambda2);
    EXPECT_GT(br.bridges.front().delta_lambda2, 0.0);
    for (std::size_t i = 1; i < br.bridges.size(); ++i) {
        ASSERT_TRUE(br.bridges[i].has_delta_lambda2);
        EXPECT_GE(br.bridges.front().delta_lambda2, br.bridges[i].delta_lambda2);
    }
    // Math hub synth-0009 is an articulation point of the full graph (it
    // isolates synth-0010) but is outside the Fiedler pair (cs, qbio), so
    // its interdiction Δλ2 is zero.
    const BridgeRecord* math = nullptr;
    for (const auto& b : br.bridges) {
        if (b.id == "synth-0009") {
            math = &b;
        }
    }
    ASSERT_NE(math, nullptr);
    EXPECT_LT(math->delta_lambda2, br.bridges.front().delta_lambda2);
}

TEST(Bridges, LaplacianExceptMatchesRebuild) {
    const std::string dir = std::string(CLAIMLEDGER_FIXTURE_DIR) + "/tiny";
    auto corp = load_corpus_dir(dir);
    auto g = Graph::from_papers_and_citations(corp.papers, corp.citations);
    const double lam_full = algebraic_connectivity(normalized_laplacian(g), 7, 0);
    for (int v = 0; v < g.n(); ++v) {
        auto Ldef = normalized_laplacian_except(g, v);
        auto rebuilt = g.without_vertex(v);
        auto Lreb = normalized_laplacian(rebuilt);
        ASSERT_EQ(Ldef.n, Lreb.n);
        const double a = algebraic_connectivity(Ldef, 11, 0);
        const double b = algebraic_connectivity(Lreb, 11, 0);
        EXPECT_NEAR(a, b, 5e-3) << "vertex " << g.paper(v).id;
        const double d1 = leave_one_out_delta(g, v, lam_full, 11, 0);
        EXPECT_NEAR(d1, lam_full - a, 5e-3) << "delta " << g.paper(v).id;
    }
}

TEST(Bridges, HeuristicOnlyOmitsDelta) {
    const std::string dir = std::string(CLAIMLEDGER_FIXTURE_DIR) + "/tiny";
    auto corp = load_corpus_dir(dir);
    auto g = Graph::from_papers_and_citations(corp.papers, corp.citations);
    auto L = normalized_laplacian(g);
    auto emb = embed_graph(g, L, 3, 1, 16);
    BridgeOptions opt;
    opt.top_k = 3;
    opt.leave_one_out = false;
    auto br = detect_bridges(g, L, emb, opt);
    ASSERT_FALSE(br.bridges.empty());
    EXPECT_EQ(br.method, "rayleigh-participation");
    EXPECT_FALSE(br.bridges.front().has_delta_lambda2);
    EXPECT_FALSE(br.bridges.front().loo);
}
