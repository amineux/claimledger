#pragma once

#include "claimledger/bridges.hpp"
#include "claimledger/embedding.hpp"
#include "claimledger/graph.hpp"
#include "claimledger/timeline.hpp"
#include "claimledger/types.hpp"

#include <string>
#include <vector>

namespace claimledger {

struct LoadedCorpus {
    std::vector<Paper> papers;
    std::vector<Citation> citations;
    std::vector<Category> categories;
};

LoadedCorpus load_corpus(const std::string& papers_path, const std::string& citations_path,
                         const std::string& categories_path = {});

LoadedCorpus load_corpus_dir(const std::string& dir);

std::string embedding_json(const Graph& g, const Embedding& emb);
std::string bridges_json(const BridgeResult& br);
std::string graph_meta_json(const Graph& g, const Embedding& emb, const std::vector<Category>& cats,
                            const BridgeResult& br);
std::string timeline_json(const Timeline& tl);

void write_text_file(const std::string& path, std::string_view text);

}  // namespace claimledger
