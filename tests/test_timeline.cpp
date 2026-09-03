#include "claimledger/graph.hpp"
#include "claimledger/io.hpp"
#include "claimledger/timeline.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <string>

using namespace claimledger;

TEST(Timeline, TinyFixtureIsCumulative) {
    const std::string dir = std::string(CLAIMLEDGER_FIXTURE_DIR) + "/tiny";
    auto corp = load_corpus_dir(dir);
    TimelineOptions opt;
    opt.seed = 20260903;
    opt.k = 3;
    auto tl = compute_timeline(corp.papers, corp.citations, opt);
    ASSERT_GE(tl.slices.size(), 2u);

    EXPECT_TRUE(std::is_sorted(tl.slices.begin(), tl.slices.end(),
                               [](const TimelineSlice& a, const TimelineSlice& b) {
                                   return a.year < b.year;
                               }));

    for (std::size_t i = 1; i < tl.slices.size(); ++i) {
        EXPECT_GE(tl.slices[i].n, tl.slices[i - 1].n);
        EXPECT_GE(tl.slices[i].year, tl.slices[i - 1].year);
    }

    const auto& last = tl.slices.back();
    EXPECT_EQ(last.n, static_cast<int>(corp.papers.size()));
    EXPECT_FALSE(last.top_bridge_id.empty());
    // Final snapshot is the planted liaison graph; λ2 is positive.
    EXPECT_GT(last.lambda2, 0.0);
}

TEST(Timeline, JsonContract) {
    Timeline tl;
    TimelineSlice s;
    s.year = 2020;
    s.n = 10;
    s.m = 16;
    s.lambda2 = 0.12;
    s.top_bridge_id = "synth-0007";
    tl.slices.push_back(s);
    const std::string js = timeline_json(tl);
    EXPECT_NE(js.find("\"year\""), std::string::npos);
    EXPECT_NE(js.find("\"n\""), std::string::npos);
    EXPECT_NE(js.find("\"m\""), std::string::npos);
    EXPECT_NE(js.find("\"lambda2\""), std::string::npos);
    EXPECT_NE(js.find("synth-0007"), std::string::npos);
}

TEST(Timeline, EmptyCorpus) {
    auto tl = compute_timeline({}, {});
    EXPECT_TRUE(tl.slices.empty());
}

TEST(Graph, CumulativeYearDropsFutureCitations) {
    std::vector<Paper> papers = {
        {"synth-0001", "A", 2018, "cs.LG", "cs", ""},
        {"synth-0002", "B", 2019, "cs.LG", "cs", ""},
        {"synth-0003", "C", 2020, "math.ST", "math", ""},
    };
    std::vector<Citation> cites = {
        {"synth-0002", "synth-0001", 2019},
        {"synth-0003", "synth-0001", 2020},
    };
    auto g18 = Graph::cumulative_at_year(papers, cites, 2018);
    EXPECT_EQ(g18.n(), 1);
    EXPECT_EQ(g18.undirected_edges(), 0);
    auto g19 = Graph::cumulative_at_year(papers, cites, 2019);
    EXPECT_EQ(g19.n(), 2);
    EXPECT_EQ(g19.undirected_edges(), 1);
    auto g20 = Graph::cumulative_at_year(papers, cites, 2020);
    EXPECT_EQ(g20.n(), 3);
    EXPECT_EQ(g20.undirected_edges(), 2);
}
