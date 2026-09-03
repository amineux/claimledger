#include "claimledger/graph.hpp"
#include "claimledger/ledger.hpp"

#include <gtest/gtest.h>

#include <algorithm>

using namespace claimledger;

TEST(Ledger, TrialBalanceZeroes) {
    std::vector<Paper> papers = {
        {"synth-0001", "A", 2018, "cs.LG", "cs", ""},
        {"synth-0002", "B", 2019, "cs.LG", "cs", ""},
        {"synth-0003", "C", 2020, "math.ST", "math", ""},
    };
    std::vector<Citation> cites = {
        {"synth-0002", "synth-0001", 2019},
        {"synth-0003", "synth-0001", 2020},
        {"synth-0003", "synth-0002", 2020},
    };
    auto g = Graph::from_papers_and_citations(papers, cites);
    auto journal = post_citations(g);
    ASSERT_EQ(journal.size(), 3u);
    std::string reason;
    EXPECT_TRUE(verify_journal(journal, &reason)) << reason;
    auto tb = trial_balance(journal, g);
    EXPECT_TRUE(tb.balanced);
    EXPECT_EQ(tb.total_debit, tb.total_credit);
    EXPECT_EQ(tb.total_debit, 300);
    // synth-0001 is only cited → pure credit.
    auto it = std::find_if(tb.accounts.begin(), tb.accounts.end(),
                           [](const AccountBalance& a) { return a.id == "synth-0001"; });
    ASSERT_NE(it, tb.accounts.end());
    EXPECT_EQ(it->credit_cents, 200);
    EXPECT_EQ(it->debit_cents, 0);
}

TEST(Ledger, FixedWidthRecordIs96) {
    JournalEntry je;
    je.je_id = 1;
    je.date = 20200615;
    je.debit = "synth-0002";
    je.credit = "synth-0001";
    je.amount_cents = 100;
    je.memo = "citation debt";
    auto dat = journal_dat({je});
    ASSERT_EQ(dat.size(), 97u);  // 96 + newline
    EXPECT_EQ(dat.substr(0, 8), "00000001");
    EXPECT_EQ(dat.substr(8, 8), "20200615");
    EXPECT_EQ(dat.substr(16, 16), std::string("synth-0002      "));
}

TEST(Ledger, RejectsUnbalancedShape) {
    JournalEntry je;
    je.je_id = 1;
    je.debit = "synth-0001";
    je.credit = "synth-0001";
    je.amount_cents = 100;
    std::string reason;
    EXPECT_FALSE(verify_journal({je}, &reason));
}
