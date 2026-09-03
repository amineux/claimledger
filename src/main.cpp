#include "claimledger/bridges.hpp"
#include "claimledger/clustering.hpp"
#include "claimledger/embedding.hpp"
#include "claimledger/graph.hpp"
#include "claimledger/io.hpp"
#include "claimledger/laplacian.hpp"
#include "claimledger/ledger.hpp"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace fs = std::filesystem;
using namespace claimledger;

namespace {

struct Options {
    std::string command = "run";
    std::string data = "data/fixtures";
    std::string papers;
    std::string citations;
    std::string categories;
    std::string out = "out";
    std::string docs;
    int k = 8;
    int clusters = 0;
    int bridges = 32;
    int lanczos_steps = 0;
    unsigned seed = 20260903;
};

void usage() {
    std::cerr << "claimledger " << kVersion << " — spectral citation atlas + COBOL intellectual-debt ledger\n\n"
              << "Usage: claimledger [command] [options]\n\n"
              << "Commands:\n"
              << "  run       build + embed + bridges + export (default)\n"
              << "  build     load corpus, construct CSR graph, write graph_meta stub\n"
              << "  embed     Laplacian + Lanczos embedding + spectral k-means\n"
              << "  bridges   score spectral bridges\n"
              << "  export    write JSON + ledger files for the atlas\n\n"
              << "Options:\n"
              << "  --data DIR            corpus directory (papers.csv, citations.csv, categories.csv)\n"
              << "  --papers PATH         papers CSV\n"
              << "  --citations PATH      citations CSV\n"
              << "  --categories PATH     categories CSV\n"
              << "  --out DIR             output directory (default: out)\n"
              << "  --docs DIR            also write atlas JSON into DIR (e.g. docs/data)\n"
              << "  --k N                 embedding dimension (default: 8)\n"
              << "  --clusters N          k-means clusters (default: #fields)\n"
              << "  --bridges N           top bridges to emit (default: 32)\n"
              << "  --lanczos-steps N     Lanczos Krylov dimension (default: auto)\n"
              << "  --seed N              RNG seed\n"
              << "  --help\n";
}

std::string require_arg(const std::vector<std::string>& argv, std::size_t& i) {
    if (i + 1 >= argv.size()) {
        throw std::runtime_error("missing argument for " + argv[i]);
    }
    return argv[++i];
}

Options parse(int argc, char** argv) {
    Options o;
    std::vector<std::string> args;
    for (int i = 1; i < argc; ++i) {
        args.emplace_back(argv[i]);
    }
    bool cmd_set = false;
    for (std::size_t i = 0; i < args.size(); ++i) {
        const auto& a = args[i];
        if (a == "--help" || a == "-h") {
            usage();
            std::exit(0);
        } else if (a == "--data") {
            o.data = require_arg(args, i);
        } else if (a == "--papers") {
            o.papers = require_arg(args, i);
        } else if (a == "--citations") {
            o.citations = require_arg(args, i);
        } else if (a == "--categories") {
            o.categories = require_arg(args, i);
        } else if (a == "--out") {
            o.out = require_arg(args, i);
        } else if (a == "--docs") {
            o.docs = require_arg(args, i);
        } else if (a == "--k") {
            o.k = std::stoi(require_arg(args, i));
        } else if (a == "--clusters") {
            o.clusters = std::stoi(require_arg(args, i));
        } else if (a == "--bridges") {
            o.bridges = std::stoi(require_arg(args, i));
        } else if (a == "--lanczos-steps") {
            o.lanczos_steps = std::stoi(require_arg(args, i));
        } else if (a == "--seed") {
            o.seed = static_cast<unsigned>(std::stoul(require_arg(args, i)));
        } else if (!a.empty() && a[0] != '-' && !cmd_set) {
            o.command = a;
            cmd_set = true;
        } else {
            throw std::runtime_error("unknown argument: " + a);
        }
    }
    if (o.papers.empty()) {
        o.papers = (fs::path(o.data) / "papers.csv").string();
    }
    if (o.citations.empty()) {
        o.citations = (fs::path(o.data) / "citations.csv").string();
    }
    if (o.categories.empty()) {
        o.categories = (fs::path(o.data) / "categories.csv").string();
    }
    return o;
}

int field_count(const std::vector<Paper>& papers) {
    std::vector<std::string> f;
    f.reserve(papers.size());
    for (const auto& p : papers) {
        f.push_back(p.field.empty() ? p.category : p.field);
    }
    std::sort(f.begin(), f.end());
    f.erase(std::unique(f.begin(), f.end()), f.end());
    return static_cast<int>(f.size());
}

void write_outputs(const Options& o, const Graph& g, const Embedding& emb, const BridgeResult& br,
                   const std::vector<Category>& cats) {
    fs::create_directories(o.out);
    write_text_file((fs::path(o.out) / "embedding.json").string(), embedding_json(g, emb));
    write_text_file((fs::path(o.out) / "bridges.json").string(), bridges_json(br));
    write_text_file((fs::path(o.out) / "graph_meta.json").string(), graph_meta_json(g, emb, cats, br));

    auto journal = post_citations(g);
    auto tb = trial_balance(journal, g);
    write_ledger_bundle((fs::path(o.out) / "ledger").string(), journal, tb);
    write_text_file((fs::path(o.out) / "ledger.json").string(), ledger_json(tb, journal));

    if (!o.docs.empty()) {
        fs::create_directories(o.docs);
        auto copyj = [&](const std::string& name) {
            fs::copy_file(fs::path(o.out) / name, fs::path(o.docs) / name,
                          fs::copy_options::overwrite_existing);
        };
        copyj("embedding.json");
        copyj("bridges.json");
        copyj("graph_meta.json");
        copyj("ledger.json");
    }
}

int run(const Options& o) {
    std::cerr << "claimledger " << kVersion << "  command=" << o.command << '\n';
    auto corp = load_corpus(o.papers, o.citations, o.categories);
    std::cerr << "  papers=" << corp.papers.size() << " citations=" << corp.citations.size()
              << " categories=" << corp.categories.size() << '\n';
    auto g = Graph::from_papers_and_citations(std::move(corp.papers), std::move(corp.citations));
    std::cerr << "  graph n=" << g.n() << " undirected_m=" << g.undirected_edges()
              << " components=" << g.component_count() << '\n';

    if (o.command == "build") {
        Embedding empty;
        BridgeResult br;
        fs::create_directories(o.out);
        write_text_file((fs::path(o.out) / "graph_meta.json").string(),
                        graph_meta_json(g, empty, corp.categories, br));
        std::cerr << "  wrote " << o.out << "/graph_meta.json\n";
        return 0;
    }

    auto L = normalized_laplacian(g);
    std::cerr << "  Laplacian nnz=" << L.nnz() << '\n';
    auto emb = embed_graph(g, L, o.k, o.seed, o.lanczos_steps);
    std::cerr << "  λ2=" << emb.algebraic_connectivity << " k=" << emb.k
              << " lanczos_steps=" << emb.lanczos_steps << '\n';

    int ck = o.clusters;
    if (ck <= 0) {
        ck = std::max(2, field_count(g.papers()));
    }
    KMeansOptions km;
    km.k = ck;
    km.seed = o.seed;
    spectral_kmeans(emb, km);

    if (o.command == "embed") {
        fs::create_directories(o.out);
        write_text_file((fs::path(o.out) / "embedding.json").string(), embedding_json(g, emb));
        std::cerr << "  wrote " << o.out << "/embedding.json\n";
        return 0;
    }

    auto br = detect_bridges(g, L, emb, o.bridges);
    std::cerr << "  bridges=" << br.bridges.size();
    if (!br.bridges.empty()) {
        std::cerr << " top=" << br.bridges.front().id << " score=" << br.bridges.front().score;
    }
    std::cerr << "  pair=" << br.pair_a << "↔" << br.pair_b << '\n';

    if (o.command == "bridges") {
        fs::create_directories(o.out);
        write_text_file((fs::path(o.out) / "bridges.json").string(), bridges_json(br));
        std::cerr << "  wrote " << o.out << "/bridges.json\n";
        return 0;
    }

    write_outputs(o, g, emb, br, corp.categories);
    std::cerr << "  wrote artifacts under " << o.out;
    if (!o.docs.empty()) {
        std::cerr << " and " << o.docs;
    }
    std::cerr << '\n';
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        auto opt = parse(argc, argv);
        if (opt.command != "run" && opt.command != "build" && opt.command != "embed" &&
            opt.command != "bridges" && opt.command != "export") {
            throw std::runtime_error("unknown command: " + opt.command);
        }
        return run(opt);
    } catch (const std::exception& ex) {
        std::cerr << "claimledger: " << ex.what() << '\n';
        return 1;
    }
}
