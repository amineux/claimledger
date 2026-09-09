#include "claimledger/graph.hpp"
#include "claimledger/io.hpp"
#include "claimledger/csv.hpp"

#include <gtest/gtest.h>

#include <fstream>
#include <regex>
#include <string>
#include <unordered_set>
#include <vector>

using namespace claimledger;

namespace {

const std::regex kArxivId{R"(^(\d{4}\.\d{4,5}|[a-z-]+(?:\.[a-z0-9-]+)?/\d{7})$)",
                          std::regex::icase};

std::string fixture_dir() {
    return std::string(CLAIMLEDGER_FIXTURE_DIR) + "/arxiv-citations";
}

std::vector<std::string> read_lines(const std::string& path) {
    std::ifstream in(path);
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        lines.push_back(line);
    }
    return lines;
}

}  // namespace

TEST(ArxivCitations, FixtureInvariants) {
    const std::string dir = fixture_dir();
    auto corp = load_corpus_dir(dir);
    ASSERT_GE(corp.papers.size(), 50u);

    std::unordered_set<std::string> ids;
    std::vector<std::string> ordered;
    ordered.reserve(corp.papers.size());
    for (const auto& p : corp.papers) {
        EXPECT_FALSE(p.id.rfind("synth-", 0) == 0);
        EXPECT_TRUE(std::regex_match(p.id, kArxivId)) << p.id;
        EXPECT_TRUE(ids.insert(p.id).second) << "duplicate " << p.id;
        if (!ordered.empty()) {
            EXPECT_LT(ordered.back(), p.id);
        }
        ordered.push_back(p.id);
    }
    EXPECT_EQ(ids.size(), corp.papers.size());
    EXPECT_TRUE(ids.contains("1706.03762"));
    EXPECT_TRUE(ids.contains("math/0409186"));

    for (const auto& c : corp.citations) {
        EXPECT_NE(c.citing, c.cited);
        EXPECT_TRUE(ids.contains(c.citing)) << c.citing;
        EXPECT_TRUE(ids.contains(c.cited)) << c.cited;
    }

    auto g = Graph::from_papers_and_citations(corp.papers, corp.citations);
    EXPECT_EQ(static_cast<std::size_t>(g.n()), corp.papers.size());
    EXPECT_EQ(static_cast<std::size_t>(g.directed_citations()), corp.citations.size());
    EXPECT_TRUE(g.is_symmetric());
    EXPECT_GE(g.undirected_edges(), 1);
}

TEST(ArxivCitations, DenseNodesEdgesAndLedger) {
    const std::string dir = fixture_dir();
    auto papers = read_csv(dir + "/papers.csv");
    auto cites = read_csv(dir + "/citations.csv");
    auto nodes = read_csv(dir + "/nodes.csv");
    auto edges = read_csv(dir + "/edges.csv");

    ASSERT_GE(papers.rows.size(), 50u);
    ASSERT_EQ(nodes.rows.size(), papers.rows.size());

    for (std::size_t i = 0; i < nodes.rows.size(); ++i) {
        EXPECT_EQ(std::stoi(std::string(nodes.get(nodes.rows[i], "idx"))), static_cast<int>(i));
        EXPECT_EQ(nodes.get(nodes.rows[i], "arxiv_id"), papers.get(papers.rows[i], "id"));
        EXPECT_EQ(nodes.get(nodes.rows[i], "primary_cat"), papers.get(papers.rows[i], "category"));
    }

    const int n = static_cast<int>(papers.rows.size());
    int weight_sum = 0;
    for (const auto& row : edges.rows) {
        const int u = std::stoi(std::string(edges.get(row, "u")));
        const int v = std::stoi(std::string(edges.get(row, "v")));
        const int w = std::stoi(std::string(edges.get(row, "w")));
        EXPECT_LT(u, v);
        EXPECT_GE(u, 0);
        EXPECT_LT(v, n);
        EXPECT_GT(w, 0);
        weight_sum += w;
    }
    EXPECT_EQ(weight_sum, static_cast<int>(cites.rows.size()));

    auto ledger_lines = read_lines(dir + "/ledger_edges.txt");
    ASSERT_FALSE(ledger_lines.empty());
    EXPECT_EQ(ledger_lines.front(), "FROM,TO,AMOUNT");
    EXPECT_EQ(ledger_lines.size() - 1, cites.rows.size());
    for (std::size_t i = 0; i < cites.rows.size(); ++i) {
        const std::string expected = std::string(cites.get(cites.rows[i], "citing")) + "," +
                                     std::string(cites.get(cites.rows[i], "cited")) + ",1.00";
        EXPECT_EQ(ledger_lines[i + 1], expected);
    }
}

TEST(ArxivCitations, CommittedSnapshotCounts) {
    const std::string dir = fixture_dir();
    auto corp = load_corpus_dir(dir);
    auto g = Graph::from_papers_and_citations(corp.papers, corp.citations);
    EXPECT_EQ(g.n(), 80);
    EXPECT_EQ(g.directed_citations(), 236);
    EXPECT_EQ(g.undirected_edges(), 230);
    int isolates = 0;
    for (int i = 0; i < g.n(); ++i) {
        isolates += g.degree_of(i) == 0 ? 1 : 0;
    }
    EXPECT_EQ(isolates, 11);
}
