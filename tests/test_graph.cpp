#include "claimledger/graph.hpp"
#include "claimledger/io.hpp"

#include <gtest/gtest.h>

#include <string>

using namespace claimledger;

TEST(Graph, BuildsSymmetricCSR) {
    std::vector<Paper> papers = {
        {"synth-0001", "A", 2018, "cs.LG", "cs", "Ada"},
        {"synth-0002", "B", 2019, "cs.LG", "cs", "Alan"},
        {"synth-0003", "C", 2020, "math.ST", "math", "Emmy"},
    };
    std::vector<Citation> cites = {
        {"synth-0002", "synth-0001", 2019},
        {"synth-0003", "synth-0001", 2020},
        {"synth-0003", "synth-0002", 2020},
    };
    auto g = Graph::from_papers_and_citations(papers, cites);
    EXPECT_EQ(g.n(), 3);
    EXPECT_EQ(g.undirected_edges(), 3);
    EXPECT_EQ(g.directed_citations(), 3);
    EXPECT_TRUE(g.is_symmetric());
    EXPECT_EQ(g.component_count(), 1);
    EXPECT_TRUE(g.index_of("synth-0002").has_value());
    EXPECT_EQ(g.degree_of(*g.index_of("synth-0001")), 2);
}

TEST(Graph, SkipsDanglingAndSelfLoops) {
    std::vector<Paper> papers = {
        {"synth-0001", "A", 2018, "cs.LG", "cs", ""},
        {"synth-0002", "B", 2019, "cs.LG", "cs", ""},
    };
    std::vector<Citation> cites = {
        {"synth-0001", "synth-0001", 2018},
        {"synth-0002", "missing-paper", 2019},
        {"synth-0002", "synth-0001", 2019},
    };
    auto g = Graph::from_papers_and_citations(papers, cites);
    EXPECT_EQ(g.directed_citations(), 1);
    EXPECT_EQ(g.undirected_edges(), 1);
}

TEST(Graph, LoadsTinyFixture) {
    const std::string dir = std::string(CLAIMLEDGER_FIXTURE_DIR) + "/tiny";
    auto corp = load_corpus_dir(dir);
    auto g = Graph::from_papers_and_citations(corp.papers, corp.citations);
    EXPECT_GE(g.n(), 7);
    EXPECT_GE(g.undirected_edges(), 8);
    EXPECT_EQ(g.component_count(), 1);
}
