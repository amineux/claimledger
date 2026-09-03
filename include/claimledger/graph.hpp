#pragma once

#include "claimledger/types.hpp"

#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace claimledger {

/// Undirected citation graph in CSR form, plus the directed edges used by the ledger.
class Graph {
public:
    Graph() = default;

    static Graph from_papers_and_citations(std::vector<Paper> papers,
                                           std::vector<Citation> citations);

    [[nodiscard]] int n() const noexcept { return static_cast<int>(papers_.size()); }
    [[nodiscard]] int undirected_edges() const noexcept { return nnz_ / 2; }
    [[nodiscard]] int directed_citations() const noexcept {
        return static_cast<int>(citations_.size());
    }

    [[nodiscard]] const std::vector<Paper>& papers() const noexcept { return papers_; }
    [[nodiscard]] const std::vector<Citation>& citations() const noexcept { return citations_; }

    [[nodiscard]] std::optional<NodeId> index_of(std::string_view paper_id) const;
    [[nodiscard]] const Paper& paper(NodeId i) const { return papers_.at(static_cast<std::size_t>(i)); }

    [[nodiscard]] std::span<const int> row_ptr() const noexcept { return row_ptr_; }
    [[nodiscard]] std::span<const int> col_idx() const noexcept { return col_idx_; }
    [[nodiscard]] std::span<const double> weights() const noexcept { return weights_; }
    [[nodiscard]] std::span<const double> degree() const noexcept { return degree_; }

    [[nodiscard]] int degree_of(NodeId i) const;
    [[nodiscard]] int component_count() const;
    [[nodiscard]] std::vector<int> components() const;

    [[nodiscard]] bool is_symmetric() const;

private:
    std::vector<Paper> papers_;
    std::vector<Citation> citations_;
    std::unordered_map<std::string, NodeId> id_to_index_;
    std::vector<int> row_ptr_;
    std::vector<int> col_idx_;
    std::vector<double> weights_;
    std::vector<double> degree_;
    int nnz_ = 0;
};

}  // namespace claimledger
