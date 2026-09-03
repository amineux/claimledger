#pragma once

#include "claimledger/types.hpp"

#include <string>
#include <vector>

namespace claimledger {

struct TimelineSlice {
    int year = 0;
    int n = 0;
    int m = 0;
    double lambda2 = 0.0;
    std::string top_bridge_id;
};

struct Timeline {
    std::vector<TimelineSlice> slices;
};

struct TimelineOptions {
    unsigned seed = 20260903;
    int lanczos_steps = 0;
    int k = 3;
    int bridges = 1;
};

/// Cumulative year snapshots: for each distinct paper year t, build G_t on
/// papers with year ≤ t and citations with citing year ≤ t, then report
/// n, m, λ2, and the top heuristic bridge. Leave-one-out is skipped here —
/// each slice is one eigensolve so a decade of the 928-node corpus stays cheap.
Timeline compute_timeline(const std::vector<Paper>& papers, const std::vector<Citation>& citations,
                          const TimelineOptions& opt = {});

}  // namespace claimledger
