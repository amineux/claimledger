#include "claimledger/io.hpp"

#include "claimledger/csv.hpp"
#include "claimledger/json.hpp"

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <unordered_map>

namespace claimledger {
namespace fs = std::filesystem;

namespace {

int parse_int(std::string_view s, int fallback = 0) {
    if (s.empty()) {
        return fallback;
    }
    try {
        return std::stoi(std::string(s));
    } catch (...) {
        return fallback;
    }
}

}  // namespace

LoadedCorpus load_corpus(const std::string& papers_path, const std::string& citations_path,
                         const std::string& categories_path) {
    LoadedCorpus corp;
    auto papers = read_csv(papers_path);
    corp.papers.reserve(papers.rows.size());
    for (const auto& row : papers.rows) {
        Paper p;
        p.id = std::string(papers.get(row, "id"));
        p.title = std::string(papers.get(row, "title"));
        p.year = parse_int(papers.get(row, "year"));
        p.category = std::string(papers.get(row, "category"));
        p.field = std::string(papers.get(row, "field"));
        p.authors = std::string(papers.get(row, "authors"));
        if (p.id.empty()) {
            continue;
        }
        corp.papers.push_back(std::move(p));
    }

    auto cites = read_csv(citations_path);
    corp.citations.reserve(cites.rows.size());
    for (const auto& row : cites.rows) {
        Citation c;
        c.citing = std::string(cites.get(row, "citing"));
        c.cited = std::string(cites.get(row, "cited"));
        c.year = parse_int(cites.get(row, "year"));
        if (c.citing.empty() || c.cited.empty()) {
            continue;
        }
        corp.citations.push_back(std::move(c));
    }

    if (!categories_path.empty() && fs::exists(categories_path)) {
        auto cats = read_csv(categories_path);
        for (const auto& row : cats.rows) {
            Category cat;
            cat.id = std::string(cats.get(row, "id"));
            cat.name = std::string(cats.get(row, "name"));
            cat.group = std::string(cats.get(row, "group"));
            if (!cat.id.empty()) {
                corp.categories.push_back(std::move(cat));
            }
        }
        std::unordered_map<std::string, std::string> group_of;
        for (const auto& c : corp.categories) {
            group_of[c.id] = c.group.empty() ? c.id : c.group;
        }
        for (auto& p : corp.papers) {
            if (p.field.empty()) {
                auto it = group_of.find(p.category);
                if (it != group_of.end()) {
                    p.field = it->second;
                } else if (!p.category.empty()) {
                    const auto dot = p.category.find('.');
                    p.field = dot == std::string::npos ? p.category : p.category.substr(0, dot);
                }
            }
        }
    }
    return corp;
}

LoadedCorpus load_corpus_dir(const std::string& dir) {
    const fs::path root(dir);
    return load_corpus((root / "papers.csv").string(), (root / "citations.csv").string(),
                       (root / "categories.csv").string());
}

void write_text_file(const std::string& path, std::string_view text) {
    fs::path p(path);
    if (p.has_parent_path()) {
        fs::create_directories(p.parent_path());
    }
    std::ofstream out(p, std::ios::binary);
    if (!out) {
        throw std::runtime_error("cannot write " + path);
    }
    out.write(text.data(), static_cast<std::streamsize>(text.size()));
}

std::string embedding_json(const Graph& g, const Embedding& emb) {
    JsonWriter j(2);
    j.begin_object();
    j.key("version");
    j.value(2);
    j.key("k");
    j.value(emb.k);
    j.key("algebraic_connectivity");
    j.value(emb.algebraic_connectivity);
    j.key("trivial_eigenvalue");
    j.value(emb.trivial_eigenvalue);
    j.key("lanczos_steps");
    j.value(emb.lanczos_steps);
    j.key("converged");
    j.value(emb.converged);
    j.key("eigenvalues");
    j.begin_array();
    for (double ev : emb.eigenvalues) {
        j.value(ev);
    }
    j.end_array();
    j.key("nodes");
    j.begin_array();
    for (const auto& node : emb.nodes) {
        const auto& paper = g.paper(node.index);
        j.begin_object();
        j.key("id");
        j.value(node.id);
        j.key("title");
        j.value(paper.title);
        j.key("year");
        j.value(paper.year);
        j.key("category");
        j.value(paper.category);
        j.key("field");
        j.value(paper.field.empty() ? paper.category : paper.field);
        j.key("authors");
        j.value(paper.authors);
        j.key("cluster");
        j.value(node.cluster);
        j.key("degree");
        j.value(g.degree_of(node.index));
        j.key("radius");
        j.value(node.radius);
        j.key("z");
        j.value(node.coords.size() > 2 ? node.coords[2] : 0.0);
        j.key("x");
        j.begin_array();
        for (double c : node.coords) {
            j.value(c);
        }
        j.end_array();
        j.key("u");
        j.begin_array();
        for (double c : node.unit_coords) {
            j.value(c);
        }
        j.end_array();
        j.end_object();
    }
    j.end_array();
    j.end_object();
    return j.str();
}

std::string bridges_json(const BridgeResult& br) {
    JsonWriter j(2);
    j.begin_object();
    j.key("version");
    j.value(2);
    j.key("method");
    j.value(br.method);
    j.key("prefilter");
    j.value(br.prefilter);
    j.key("loo_candidates");
    j.value(br.loo_candidates);
    j.key("loo_evaluated");
    j.value(br.loo_evaluated);
    j.key("pair_a");
    j.value(br.pair_a);
    j.key("pair_b");
    j.value(br.pair_b);
    j.key("pair_algebraic_connectivity");
    j.value(br.pair_algebraic_connectivity);
    j.key("bridges");
    j.begin_array();
    for (const auto& b : br.bridges) {
        j.begin_object();
        j.key("id");
        j.value(b.id);
        j.key("rank");
        j.value(b.rank);
        j.key("score");
        j.value(b.score);
        j.key("participation_entropy");
        j.value(b.participation_entropy);
        j.key("rayleigh_energy");
        j.value(b.rayleigh_energy);
        j.key("fiedler_abs");
        j.value(b.fiedler_abs);
        j.key("cross_field_fraction");
        j.value(b.cross_field_fraction);
        j.key("delta_lambda2");
        if (b.has_delta_lambda2) {
            j.value(b.delta_lambda2);
        } else {
            j.value(nullptr);
        }
        j.key("loo");
        j.value(b.loo);
        j.key("explanation");
        j.value(b.explanation);
        j.key("fields");
        j.begin_array();
        for (const auto& f : b.fields) {
            j.value(f);
        }
        j.end_array();
        j.end_object();
    }
    j.end_array();
    j.end_object();
    return j.str();
}

std::string graph_meta_json(const Graph& g, const Embedding& emb, const std::vector<Category>& cats,
                            const BridgeResult& br) {
    JsonWriter j(2);
    j.begin_object();
    j.key("version");
    j.value(2);
    j.key("generator");
    j.value("claimledger");
    j.key("claimledger_version");
    j.value(std::string(kVersion));
    j.key("n");
    j.value(g.n());
    j.key("undirected_edges");
    j.value(g.undirected_edges());
    j.key("directed_citations");
    j.value(g.directed_citations());
    j.key("components");
    j.value(g.component_count());
    j.key("algebraic_connectivity");
    j.value(emb.algebraic_connectivity);
    j.key("embedding_k");
    j.value(emb.k);
    j.key("bridge_method");
    j.value(br.method);
    j.key("loo_candidates");
    j.value(br.loo_candidates);
    j.key("loo_evaluated");
    j.value(br.loo_evaluated);
    j.key("bridge_pair");
    j.begin_array();
    j.value(br.pair_a);
    j.value(br.pair_b);
    j.end_array();
    j.key("categories");
    j.begin_array();
    for (const auto& c : cats) {
        j.begin_object();
        j.key("id");
        j.value(c.id);
        j.key("name");
        j.value(c.name);
        j.key("group");
        j.value(c.group);
        j.end_object();
    }
    j.end_array();

    std::unordered_map<std::string, int> field_count;
    for (const auto& p : g.papers()) {
        const std::string f = p.field.empty() ? p.category : p.field;
        field_count[f] += 1;
    }
    j.key("fields");
    j.begin_array();
    for (const auto& [name, count] : field_count) {
        j.begin_object();
        j.key("id");
        j.value(name);
        j.key("count");
        j.value(count);
        j.end_object();
    }
    j.end_array();
    j.end_object();
    return j.str();
}

std::string timeline_json(const Timeline& tl) {
    JsonWriter j(2);
    j.begin_object();
    j.key("version");
    j.value(2);
    j.key("slices");
    j.begin_array();
    for (const auto& s : tl.slices) {
        j.begin_object();
        j.key("year");
        j.value(s.year);
        j.key("n");
        j.value(s.n);
        j.key("m");
        j.value(s.m);
        j.key("lambda2");
        j.value(s.lambda2);
        j.key("top_bridge_id");
        j.value(s.top_bridge_id);
        j.end_object();
    }
    j.end_array();
    j.end_object();
    return j.str();
}

}  // namespace claimledger
