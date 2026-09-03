#pragma once

#include "claimledger/graph.hpp"
#include "claimledger/json.hpp"

#include <string>
#include <vector>

namespace claimledger {

/// One double-entry posting: citing paper is debited (intellectual debt),
/// cited paper is credited (intellectual capital). Amount is integer cents.
struct JournalEntry {
    int je_id = 0;
    int date = 0;  // YYYYMMDD
    PaperId debit;
    PaperId credit;
    int amount_cents = 100;
    std::string memo;
};

struct AccountBalance {
    PaperId id;
    int debit_cents = 0;
    int credit_cents = 0;
    int net_cents = 0;  // credit - debit  (positive = net idea supplier)
};

struct TrialBalance {
    std::vector<AccountBalance> accounts;
    long long total_debit = 0;
    long long total_credit = 0;
    bool balanced = false;
};

std::vector<JournalEntry> post_citations(const Graph& g);
TrialBalance trial_balance(const std::vector<JournalEntry>& journal, const Graph& g);

/// Canonical CSV interchange (see SPECS.md).
std::string journal_csv(const std::vector<JournalEntry>& journal);
/// Fixed-width 96-byte records + newline, matching cobol/copy/JOURNAL.cpy.
std::string journal_dat(const std::vector<JournalEntry>& journal);
std::string trial_balance_csv(const TrialBalance& tb);
std::string ledger_json(const TrialBalance& tb, const std::vector<JournalEntry>& journal,
                        std::size_t journal_sample = 48);

void write_ledger_bundle(const std::string& dir, const std::vector<JournalEntry>& journal,
                         const TrialBalance& tb);

/// Verify that every journal line is a balanced pair and totals match.
bool verify_journal(const std::vector<JournalEntry>& journal, std::string* reason = nullptr);

}  // namespace claimledger
