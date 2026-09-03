# ClaimLedger

**The literature is a ledger. Interdisciplinarity is a thin cut.**

ClaimLedger treats scientific papers as accounts and citations as
double-entry postings, then asks a spectral question of the same graph:
which documents are the **bridges** whose removal would most increase the
algebraic-connectivity gap between two fields?

It is a C++20 research instrument — sparse normalized Laplacian, a
self-contained Lanczos eigensolver, a planted-cut bridge heuristic — sitting
on top of a GnuCOBOL trial balance. The atlas is a static page.

**Atlas (GitHub Pages):** <https://amineux.github.io/claimledger/>

Read [PROBLEM.md](PROBLEM.md) for the math, [SPECS.md](SPECS.md) for the
contracts, [ARCHITECTURE.md](ARCHITECTURE.md) for the build graph.

---

## What you get on clone

```
data/fixtures/     10-block synthetic citation corpus (~900 papers)
                   + a 10-node planted-cut fixture for tests
src/ include/      C++20 core (no Eigen required)
cobol/             POST-CITATION / TRIAL-BALANCE / REPORT + COPY books
docs/              Pages atlas (vanilla JS, checked-in JSON)
scripts/           fixture generator + ledger verifier
```

Every paper id is `synth-NNNN`. Nothing here impersonates a real arXiv id.

## Build and test

Requires a C++20 compiler and CMake ≥ 3.20. GoogleTest is fetched at
configure time.

```bash
cmake -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## Run the pipeline

```bash
./build/claimledger --data data/fixtures --out out --docs docs/data
```

That is `build + embed + bridges + export`. Subcommands exist if you want
them separately:

```bash
./build/claimledger build   --data data/fixtures --out out
./build/claimledger embed   --data data/fixtures --out out --k 8
./build/claimledger bridges --data data/fixtures --out out --bridges 32
./build/claimledger export  --data data/fixtures --out out --docs docs/data
```

Artifacts:

| file | what |
|------|------|
| `out/embedding.json` | spectral coordinates, clusters, metadata |
| `out/bridges.json` | ranked liaisons and the Fiedler field pair |
| `out/graph_meta.json` | \(n, m, \lambda_2\), field legend |
| `out/ledger.json` | trial balance for the atlas |
| `out/ledger/journal.csv` | canonical journal |
| `out/ledger/journal.dat` | 96-byte COBOL records |

## Audit the ledger

The same posting rule is implemented three times on purpose.

```bash
# C++ already wrote the journal. Check conservation:
python3 scripts/verify_ledger.py --journal out/ledger/journal.csv

# Optional GnuCOBOL path (no-op with a message if cobc is missing):
make -C cobol
# then, from a directory that contains citations.csv / journal.dat:
#   post-citation && trial-balance && report
```

A closed citation book **balances**. If it does not, either the parser or
the universe has a bug.

## Atlas

Checked-in `docs/data/*.json` make Pages work without a generator on the
server. After regenerating artifacts:

```bash
python3 -m http.server --directory docs 8000
```

GitHub Pages is served from `/docs`. Enable it on the repository:
Settings → Pages → Deploy from a branch → `main` / `docs`.
Workflows in `.github/workflows/` build C++ on Ubuntu and deploy the site.

## Regenerate the big fixture

```bash
python3 scripts/generate_fixtures.py --out data/fixtures --per-block 90 --bridges 28
```

Deterministic. Seed `20260903`.

## The hard problem, in one paragraph

Finding the \(k\) vertices whose deletion most increases \(\lambda_2\)
between two scientific fields is a spectral interdiction problem (NP-hard
in the combinatorial form). ClaimLedger computes one sparse eigensolve and
ranks papers by their cross-field Dirichlet energy, neighbor-field entropy,
and distance to the Fiedler cut. The unit test plants two cliques and one
liaison; the liaison is rank 1. Details, complexity, and the accounting
dual live in [PROBLEM.md](PROBLEM.md).
