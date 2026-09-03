# Architecture

ClaimLedger is three programs that share one journal and one Laplacian.

```
 papers.csv  citations.csv  categories.csv
            │
            ▼
   ┌────────────────────┐
   │  C++20 core        │  Graph CSR → L_sym → Lanczos → k-means → bridges
   │  libclaimledger    │  + double-entry poster
   └────────┬───────────┘
            │
            ├─ out/embedding.json
            ├─ out/bridges.json
            ├─ out/graph_meta.json
            ├─ out/ledger.json
            └─ out/ledger/journal.{csv,dat}
                    │
                    ▼
        ┌───────────────────────┐         ┌─────────────────────────┐
        │  COBOL (cobc)         │         │  Python verifier        │
        │  POST-CITATION        │         │  scripts/verify_ledger  │
        │  TRIAL-BALANCE        │         │  (always available)     │
        │  REPORT               │         └─────────────────────────┘
        └───────────────────────┘
                    │
                    ▼
              docs/data/*.json  ──►  docs/index.html  (GitHub Pages)
```

## Components

### `include/claimledger/` + `src/`

| unit | responsibility |
|------|----------------|
| `graph` | Paper/citation tables → undirected CSR + directed citation list |
| `sparse` | CSR matvec and \(x^\top A x\) |
| `laplacian` | \(L_{\mathrm{sym}} = I - D^{-1/2}AD^{-1/2}\) |
| `lanczos` | Smallest eigenpairs, full reorth. + Jacobi Ritz |
| `embedding` | Drop \(\lambda \approx 0\), export raw + row-normalized coords |
| `clustering` | \(k\)-means++ on the unit embedding (Ng–Jordan–Weiss) |
| `bridges` | Rayleigh-participation score (PROBLEM.md) |
| `ledger` | Post citations, trial balance, CSV + 96-byte `journal.dat` |
| `csv` / `json` | Zero-dependency interchange |
| `io` | Corpus load and atlas writers |
| `main` | CLI: `run` / `build` / `embed` / `bridges` / `export` |

The library is a static target `claimledger_core`. The CLI links it. Tests
link it plus GoogleTest (FetchContent, v1.14.0).

### COBOL (`cobol/`)

GnuCOBOL free-format programs, compiled with `cobc -free -I cobol/copy`
when `cobc` is on `PATH`. CMake exposes an optional `cobol-ledger` target;
`cobol/Makefile` is the standalone path.

COPY books `JOURNAL.cpy` and `ACCOUNT.cpy` are the contract with C++
`journal_dat()`. Record length is 96 bytes, documented in `data/SCHEMA.md`
and `SPECS.md`.

`TRIAL-BALANCE` holds up to 2000 in-memory accounts — enough for the
checked-in fixture, not a substitute for a real indexed file. The Python
verifier has no such ceiling and is what CI runs when `cobc` is missing.

### Atlas (`docs/`)

Vanilla HTML / CSS / Canvas. No bundler, no npm. GitHub Pages serves
`/docs`. The page fetches `docs/data/*.json` and draws a 2D/3D projection
of the first spectral coordinates, colored by field, with bridges lit.

### CI (`.github/workflows/`)

- `ci.yml` — configure, build, `ctest`, run the CLI on the tiny fixture,
  run `scripts/verify_ledger.py`, compile COBOL if `cobc` is installed.
- `pages.yml` — deploy `docs/` to GitHub Pages.

## Build graph

```
CMakeLists.txt
 ├─ claimledger_core (static)
 │    src/{sparse,graph,laplacian,lanczos,embedding,
 │         clustering,bridges,csv,json,io,ledger}.cpp
 ├─ claimledger          → src/main.cpp + core
 ├─ claimledger_tests    → tests/*.cpp + core + GTest
 └─ cobol-ledger         → cobc (optional)
```

Command the repo expects:

```
cmake -B build
cmake --build build
ctest --test-dir build --output-on-failure
./build/claimledger --data data/fixtures --out out --docs docs/data
python3 scripts/verify_ledger.py --journal out/ledger/journal.csv
```

## Data flow invariants

1. CSR is symmetric; the directed citation list is not.
2. Isolated vertices have a zero Laplacian row.
3. Every journal line is a balanced pair (debit paper ≠ credit paper,
   amount > 0). \(\sum \mathrm{DR} = \sum \mathrm{CR}\).
4. Atlas JSON is a pure function of the corpus + `--k/--clusters/--bridges/--seed`.
5. Fixture ids match `synth-[0-9]{4}`. No forged arXiv ids.

## Why COBOL

The ledger is the same information as the graph, restated in the oldest
language still used to move money. A citation is a transfer. A trial
balance that does not zero is a bug in science *or* in the parser. Putting
that check in GnuCOBOL is not a joke about enterprise software — it is a
type system for conservation laws that graph libraries usually skip.
