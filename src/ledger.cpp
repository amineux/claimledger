#include "claimledger/ledger.hpp"

#include "claimledger/csv.hpp"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <unordered_map>

namespace claimledger {
namespace fs = std::filesystem;

std::vector<JournalEntry> post_citations(const Graph& g) {
    std::vector<JournalEntry> journal;
    journal.reserve(g.citations().size());
    int id = 1;
    for (const auto& c : g.citations()) {
        JournalEntry je;
        je.je_id = id++;
        int y = c.year > 0 ? c.year : 2020;
        je.date = y * 10000 + 615;  // synthetic mid-year posting date
        je.debit = c.citing;
        je.credit = c.cited;
        je.amount_cents = 100;
        je.memo = "citation debt";
        journal.push_back(std::move(je));
    }
    return journal;
}

TrialBalance trial_balance(const std::vector<JournalEntry>& journal, const Graph& g) {
    std::unordered_map<std::string, AccountBalance> acc;
    acc.reserve(static_cast<std::size_t>(g.n()));
    for (const auto& p : g.papers()) {
        AccountBalance a;
        a.id = p.id;
        acc.emplace(p.id, std::move(a));
    }
    for (const auto& je : journal) {
        acc[je.debit].id = je.debit;
        acc[je.debit].debit_cents += je.amount_cents;
        acc[je.credit].id = je.credit;
        acc[je.credit].credit_cents += je.amount_cents;
    }
    TrialBalance tb;
    tb.accounts.reserve(acc.size());
    for (auto& [_, a] : acc) {
        a.net_cents = a.credit_cents - a.debit_cents;
        tb.total_debit += a.debit_cents;
        tb.total_credit += a.credit_cents;
        tb.accounts.push_back(std::move(a));
    }
    std::sort(tb.accounts.begin(), tb.accounts.end(), [](const AccountBalance& a, const AccountBalance& b) {
        if (a.net_cents != b.net_cents) {
            return a.net_cents > b.net_cents;
        }
        return a.id < b.id;
    });
    tb.balanced = tb.total_debit == tb.total_credit;
    return tb;
}

std::string journal_csv(const std::vector<JournalEntry>& journal) {
    std::ostringstream os;
    os << "je_id,date,debit_account,credit_account,amount_cents,memo\n";
    for (const auto& je : journal) {
        os << std::setfill('0') << std::setw(8) << je.je_id << ',' << je.date << ',' << csv_escape(je.debit)
           << ',' << csv_escape(je.credit) << ',' << je.amount_cents << ',' << csv_escape(je.memo) << '\n';
    }
    return os.str();
}

std::string journal_dat(const std::vector<JournalEntry>& journal) {
    // 8 + 8 + 16 + 16 + 10 + 38 = 96 characters, then '\n'
    std::string out;
    out.reserve(journal.size() * 97);
    auto pad = [](const std::string& s, std::size_t w) {
        std::string t = s.substr(0, w);
        t.resize(w, ' ');
        return t;
    };
    for (const auto& je : journal) {
        char buf[97];
        std::snprintf(buf, sizeof(buf), "%08d%08d%-16.16s%-16.16s%010d%-38.38s", je.je_id, je.date,
                      pad(je.debit, 16).c_str(), pad(je.credit, 16).c_str(), je.amount_cents,
                      pad(je.memo, 38).c_str());
        out.append(buf, 96);
        out.push_back('\n');
    }
    return out;
}

std::string trial_balance_csv(const TrialBalance& tb) {
    std::ostringstream os;
    os << "account,debit_cents,credit_cents,net_cents\n";
    for (const auto& a : tb.accounts) {
        os << csv_escape(a.id) << ',' << a.debit_cents << ',' << a.credit_cents << ',' << a.net_cents << '\n';
    }
    os << "TOTAL," << tb.total_debit << ',' << tb.total_credit << ',' << (tb.total_credit - tb.total_debit)
       << '\n';
    return os.str();
}

std::string ledger_json(const TrialBalance& tb, const std::vector<JournalEntry>& journal,
                        std::size_t journal_sample) {
    JsonWriter j(2);
    j.begin_object();
    j.key("version");
    j.value(1);
    j.key("balanced");
    j.value(tb.balanced);
    j.key("total_debit_cents");
    j.value(static_cast<int>(tb.total_debit));
    j.key("total_credit_cents");
    j.value(static_cast<int>(tb.total_credit));
    j.key("journal_entries");
    j.value(static_cast<int>(journal.size()));
    j.key("accounts");
    j.begin_array();
    for (const auto& a : tb.accounts) {
        j.begin_object();
        j.key("id");
        j.value(a.id);
        j.key("debit_cents");
        j.value(a.debit_cents);
        j.key("credit_cents");
        j.value(a.credit_cents);
        j.key("net_cents");
        j.value(a.net_cents);
        j.end_object();
    }
    j.end_array();
    j.key("journal_sample");
    j.begin_array();
    const std::size_t n = std::min(journal_sample, journal.size());
    for (std::size_t i = 0; i < n; ++i) {
        const auto& je = journal[i];
        j.begin_object();
        j.key("je_id");
        j.value(je.je_id);
        j.key("date");
        j.value(je.date);
        j.key("debit");
        j.value(je.debit);
        j.key("credit");
        j.value(je.credit);
        j.key("amount_cents");
        j.value(je.amount_cents);
        j.key("memo");
        j.value(je.memo);
        j.end_object();
    }
    j.end_array();
    j.end_object();
    return j.str();
}

void write_ledger_bundle(const std::string& dir, const std::vector<JournalEntry>& journal,
                         const TrialBalance& tb) {
    fs::create_directories(dir);
    auto write = [&](const std::string& name, const std::string& body) {
        std::ofstream out(fs::path(dir) / name, std::ios::binary);
        if (!out) {
            throw std::runtime_error("cannot write ledger file " + name);
        }
        out << body;
    };
    write("journal.csv", journal_csv(journal));
    write("journal.dat", journal_dat(journal));
    write("trial_balance.csv", trial_balance_csv(tb));
}

bool verify_journal(const std::vector<JournalEntry>& journal, std::string* reason) {
    long long d = 0;
    long long c = 0;
    for (const auto& je : journal) {
        if (je.debit.empty() || je.credit.empty()) {
            if (reason) {
                *reason = "empty account on JE " + std::to_string(je.je_id);
            }
            return false;
        }
        if (je.debit == je.credit) {
            if (reason) {
                *reason = "self-entry on JE " + std::to_string(je.je_id);
            }
            return false;
        }
        if (je.amount_cents <= 0) {
            if (reason) {
                *reason = "non-positive amount on JE " + std::to_string(je.je_id);
            }
            return false;
        }
        d += je.amount_cents;
        c += je.amount_cents;
    }
    if (d != c) {
        if (reason) {
            *reason = "debit/credit totals diverge";
        }
        return false;
    }
    return true;
}

}  // namespace claimledger
