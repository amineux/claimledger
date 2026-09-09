# ClaimLedger

[![CI](https://github.com/amineux/claimledger/actions/workflows/ci.yml/badge.svg)](https://github.com/amineux/claimledger/actions/workflows/ci.yml)
[![Pages](https://img.shields.io/badge/atlas-live-e6b35a?label=Pages)](https://amineux.github.io/claimledger/)

**The literature is a ledger. Interdisciplinarity is a thin cut.**

ClaimLedger treats scientific papers as accounts and citations as
double-entry postings, then asks a spectral question of the same graph:
which documents are the **bridges** whose removal would most increase the
algebraic-connectivity gap between two fields?

v2 ships a **true leave-one-out** Δλ₂ (heuristic pre-filter, then rebuild
L_sym without v), **cumulative year slices**, and an atlas that can orbit
928 points at 60 fps. The COBOL trial balance is unchanged.

**Atlas:** <https://amineux.github.io/claimledger/>

Read [PROBLEM.md](PROBLEM.md) for the math, [SPECS.md](SPECS.md) for the
contracts, [ARCHITECTURE.md](ARCHITECTURE.md) for the build graph.

---

## What you get on clone

```
data/fixtures/                 10-block synthetic citation corpus (~900 papers)
data/fixtures/tiny/            10-node planted-cut fixture (liaison synth-0007)
data/fixtures/arxiv-citations/ real arXiv metadata + bibliographic citation graph
data/fixtures/arxiv-slice/     real arXiv metadata + author/cross-list coupling
src/ include/                  C++20 core (no Eigen required)
cobol/                         POST-CITATION / TRIAL-BALANCE / REPORT + COPY books
docs/                          Pages atlas (vanilla JS, checked-in JSON)
scripts/                       fixture generator, arXiv / S2 fetch, ledger verifier
```

Default demo ids are `synth-NNNN`. Two optional real-id snapshots are
committed; they are not interchangeable:

- `arxiv-citations` — real bibliography. Every id was returned by the
  arXiv Atom API; every directed edge is a Semantic Scholar
  `references.externalIds.ArXiv` row induced on that closed set.
- `arxiv-slice` — real metadata, but its `citations.csv` is an
  **author / cross-list coupling graph**, not a bibliography.

See [data/SCHEMA.md](data/SCHEMA.md).

## Build and test

Requires a C++20 compiler and CMake ≥ 3.20. GoogleTest is fetched at
configure time.

```bash
cmake -B build -DCMAKE_CXX_COMPILER=g++
cmake --build build
ctest --test-dir build --output-on-failure
```

## Run the pipeline

```bash
./build/claimledger --data data/fixtures --out out --docs docs/data
```

That is `build + embed + bridges + timeline + export`. Leave-one-out Δλ₂
is on by default (every vertex on the tiny fixture; heuristic top 64 on
the 928-node corpus). Subcommands:

```bash
./build/claimledger build    --data data/fixtures --out out
./build/claimledger embed    --data data/fixtures --out out --k 8
./build/claimledger bridges  --data data/fixtures --out out --bridges 32
./build/claimledger timeline --data data/fixtures --out out
./build/claimledger export   --data data/fixtures --out out --docs docs/data
```

`--no-loo` keeps the v1 Rayleigh × participation ranking.
`--loo-candidates N` changes the pre-filter width (default 64).
`--no-timeline` skips year slices.

Artifacts:

| file | what |
|------|------|
| `out/embedding.json` | spectral coordinates (`x` plus scalar `z`), clusters |
| `out/bridges.json` | ranked liaisons with `delta_lambda2` |
| `out/timeline.json` | `{year, n, m, lambda2, top_bridge_id}` slices |
| `out/graph_meta.json` | \(n, m, \lambda_2\), field legend |
| `out/ledger.json` | trial balance for the atlas |
| `out/ledger/journal.csv` | canonical journal |
| `out/ledger/journal.dat` | 96-byte COBOL records |

The same command on either public snapshot:

```bash
./build/claimledger --data data/fixtures/arxiv-citations --out out-cite
./build/claimledger --data data/fixtures/arxiv-slice --out out-arxiv
```

## Audit the ledger

The same posting rule is implemented three times on purpose.

```bash
python3 scripts/verify_ledger.py --journal out/ledger/journal.csv

# Optional GnuCOBOL path (no-op with a message if cobc is missing):
make -C cobol
```

A closed citation book **balances**. If it does not, either the parser or
the universe has a bug.

## Atlas

Checked-in `docs/data/*.json` make Pages work without a generator on the
server. After regenerating artifacts:

```bash
python3 -m http.server --directory docs 8000
```

`/` focuses search, `[` `]` cycle bridges, `?` opens the cheatsheet.
The year slider dims papers that did not exist yet and updates n / m / λ₂.

GitHub Pages is served from `/docs`. Workflows in `.github/workflows/`
build C++ on Ubuntu and deploy the site. No repository secrets required.

## Regenerate fixtures

```bash
python3 scripts/generate_fixtures.py --out data/fixtures --per-block 90 --bridges 28
python3 scripts/fetch_citation_slice.py --out data/fixtures/arxiv-citations          # arXiv + S2; 3.1s between requests
python3 scripts/fetch_citation_slice.py --out data/fixtures/arxiv-citations --offline
python3 scripts/fetch_arxiv_slice.py --out data/fixtures/arxiv-slice                  # hits arXiv; 3.1s between requests
python3 scripts/fetch_arxiv_slice.py --out data/fixtures/arxiv-slice --offline        # CI path
```

Synthetic generator seed `20260903`. The fetch scripts respect
[arXiv API terms](https://info.arxiv.org/help/api/tou.html) and the
[Semantic Scholar API license](https://www.semanticscholar.org/product/api/license).
No API key is required. `--offline` validates the committed snapshot;
`--from-cache` rebuilds CSVs from cached Atom/JSON.

## The hard problem, in one paragraph

Finding the \(k\) vertices whose deletion most drops \(\lambda_2\) between
two scientific fields is a spectral interdiction problem (NP-hard for
\(k>1\)). ClaimLedger still computes one sparse eigensolve and a
Rayleigh × participation ranking, then — for the shortlist — rebuilds
\(L_{\mathrm{sym}}\) without each candidate and reports
\(\delta\lambda_2(v)=\lambda_2(G[A\cup B])-\lambda_2(G[A\cup B]-v)\).
The unit test plants two cliques and one liaison; the liaison has the
largest Δλ₂. Complexity lives in [PROBLEM.md](PROBLEM.md).
