#include "claimledger/graph.hpp"

#include <algorithm>
#include <cmath>
#include <queue>
#include <stdexcept>
#include <unordered_set>
#include <utility>

namespace claimledger {

Graph Graph::from_papers_and_citations(std::vector<Paper> papers, std::vector<Citation> citations) {
    Graph g;
    g.papers_ = std::move(papers);
    g.id_to_index_.reserve(g.papers_.size() * 2);
    for (int i = 0; i < static_cast<int>(g.papers_.size()); ++i) {
        const auto& id = g.papers_[static_cast<std::size_t>(i)].id;
        if (g.id_to_index_.contains(id)) {
            throw std::runtime_error("duplicate paper id: " + id);
        }
        g.id_to_index_.emplace(id, i);
    }

    using EdgeKey = std::uint64_t;
    auto pack = [](int a, int b) -> EdgeKey {
        return (static_cast<EdgeKey>(a) << 32) | static_cast<EdgeKey>(b);
    };

    std::unordered_map<EdgeKey, double> undirected;
    undirected.reserve(citations.size() * 2);
    g.citations_.reserve(citations.size());

    for (const auto& c : citations) {
        auto ia = g.id_to_index_.find(c.citing);
        auto ib = g.id_to_index_.find(c.cited);
        if (ia == g.id_to_index_.end() || ib == g.id_to_index_.end()) {
            continue;  // dangling citation — skip rather than invent nodes
        }
        if (ia->second == ib->second) {
            continue;  // self-loop
        }
        g.citations_.push_back(c);
        const int u = ia->second;
        const int v = ib->second;
        undirected[pack(u, v)] += 1.0;
        undirected[pack(v, u)] += 1.0;
    }

    const int n = g.n();
    std::vector<std::vector<std::pair<int, double>>> adj(static_cast<std::size_t>(n));
    for (const auto& [key, w] : undirected) {
        const int u = static_cast<int>(key >> 32);
        const int v = static_cast<int>(key & 0xffffffffu);
        adj[static_cast<std::size_t>(u)].push_back({v, w});
    }
    for (auto& row : adj) {
        std::sort(row.begin(), row.end(),
                  [](const auto& a, const auto& b) { return a.first < b.first; });
    }

    g.row_ptr_.assign(static_cast<std::size_t>(n + 1), 0);
    for (int i = 0; i < n; ++i) {
        g.row_ptr_[static_cast<std::size_t>(i + 1)] =
            g.row_ptr_[static_cast<std::size_t>(i)] + static_cast<int>(adj[static_cast<std::size_t>(i)].size());
    }
    g.nnz_ = g.row_ptr_.back();
    g.col_idx_.resize(static_cast<std::size_t>(g.nnz_));
    g.weights_.resize(static_cast<std::size_t>(g.nnz_));
    g.degree_.assign(static_cast<std::size_t>(n), 0.0);
    for (int i = 0; i < n; ++i) {
        int p = g.row_ptr_[static_cast<std::size_t>(i)];
        for (const auto& [v, w] : adj[static_cast<std::size_t>(i)]) {
            g.col_idx_[static_cast<std::size_t>(p)] = v;
            g.weights_[static_cast<std::size_t>(p)] = w;
            g.degree_[static_cast<std::size_t>(i)] += w;
            ++p;
        }
    }
    return g;
}

std::optional<NodeId> Graph::index_of(std::string_view paper_id) const {
    auto it = id_to_index_.find(std::string(paper_id));
    if (it == id_to_index_.end()) {
        return std::nullopt;
    }
    return it->second;
}

int Graph::degree_of(NodeId i) const {
    return row_ptr_[static_cast<std::size_t>(i) + 1] - row_ptr_[static_cast<std::size_t>(i)];
}

std::vector<int> Graph::components() const {
    const int n = this->n();
    std::vector<int> comp(static_cast<std::size_t>(n), -1);
    int cid = 0;
    std::vector<int> stack;
    for (int s = 0; s < n; ++s) {
        if (comp[static_cast<std::size_t>(s)] >= 0) {
            continue;
        }
        stack.clear();
        stack.push_back(s);
        comp[static_cast<std::size_t>(s)] = cid;
        while (!stack.empty()) {
            const int u = stack.back();
            stack.pop_back();
            for (int p = row_ptr_[static_cast<std::size_t>(u)]; p < row_ptr_[static_cast<std::size_t>(u) + 1]; ++p) {
                const int v = col_idx_[static_cast<std::size_t>(p)];
                if (comp[static_cast<std::size_t>(v)] < 0) {
                    comp[static_cast<std::size_t>(v)] = cid;
                    stack.push_back(v);
                }
            }
        }
        ++cid;
    }
    return comp;
}

int Graph::component_count() const {
    auto c = components();
    if (c.empty()) {
        return 0;
    }
    return *std::max_element(c.begin(), c.end()) + 1;
}

bool Graph::is_symmetric() const {
    const int n = this->n();
    for (int i = 0; i < n; ++i) {
        for (int p = row_ptr_[static_cast<std::size_t>(i)]; p < row_ptr_[static_cast<std::size_t>(i) + 1]; ++p) {
            const int j = col_idx_[static_cast<std::size_t>(p)];
            const double w = weights_[static_cast<std::size_t>(p)];
            bool found = false;
            for (int q = row_ptr_[static_cast<std::size_t>(j)]; q < row_ptr_[static_cast<std::size_t>(j) + 1]; ++q) {
                if (col_idx_[static_cast<std::size_t>(q)] == i) {
                    if (std::abs(weights_[static_cast<std::size_t>(q)] - w) > 1e-12) {
                        return false;
                    }
                    found = true;
                    break;
                }
            }
            if (!found) {
                return false;
            }
        }
    }
    return true;
}

}  // namespace claimledger
