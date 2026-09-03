# ClaimLedger specification

## Product

ClaimLedger is a research instrument that treats the scientific literature as
two dual systems:

1. a **spectral system** — the normalized Laplacian of a citation graph,
   its Fiedler coordinate, and a ranking of cross-field bridges;
2. an **accounting system** — a double-entry ledger in which every citation
   posts intellectual debt against the citing paper and intellectual
   capital to the cited paper.

The instrument is a C++20 CLI, a GnuCOBOL ledger (with a Python fallback),
and a static atlas under `docs/`.

## Non-goals

- Not a crawler. The CLI never hits the network. `scripts/fetch_arxiv_slice.py`
  is an optional offline-cached fetch of a few dozen public Atom records.
- Not a replacement for Semantic Scholar / OpenAlex.
- Not an FPTAS for the \(k>1\) interdiction problem in PROBLEM.md. \(k=1\)
  is solved exactly on a heuristic shortlist.
- No secrets, no accounts, no network I/O in the CLI.

## CLI

```
claimledger [command] [options]
```

| command   | effect |
|-----------|--------|
| `run`     | Full pipeline (default): embed + LOO bridges + timeline + export. |
| `build`   | Load corpus, write `graph_meta.json` only. |
| `embed`   | Laplacian + Lanczos + k-means, write `embedding.json`. |
| `bridges` | Score bridges (heuristic + optional leave-one-out), write `bridges.json`. |
| `timeline`| Cumulative year snapshots, write `timeline.json`. |
| `export`  | Same as `run` (JSON + ledger + optional `--docs` copy). |

| option | default | meaning |
|--------|---------|---------|
| `--data DIR` | `data/fixtures` | Directory with `papers.csv`, `citations.csv`, `categories.csv` |
| `--papers PATH` | *from --data* | Override papers CSV |
| `--citations PATH` | *from --data* | Override citations CSV |
| `--categories PATH` | *from --data* | Override categories CSV |
| `--out DIR` | `out` | Artifact directory |
| `--docs DIR` | *(off)* | Also copy atlas JSON here (`docs/data`) |
| `--k N` | `8` | Embedding dimension (nontrivial eigenvectors) |
| `--clusters N` | `#fields` | k-means \(k\) |
| `--bridges N` | `32` | Top bridges emitted |
| `--lanczos-steps N` | auto | Krylov dimension |
| `--seed N` | `20260903` | RNG seed (Lanczos start + k-means++) |
| `--loo` / `--no-loo` | on | Leave-one-out Δλ₂ on the heuristic shortlist |
| `--loo-candidates N` | `64` | Pre-filter width; if \(n\le N\), every vertex is LOO'd |
| `--timeline` / `--no-timeline` | on | Write cumulative year slices |

Exit status is `0` on success, `1` on user/data/solver errors. Diagnostics
go to stderr; artifacts are files.

## Input schema

See `data/SCHEMA.md`. Required columns:

- papers: `id,title,year,category,field,authors`
- citations: `citing,cited,year`
- categories (optional): `id,name,group`

CSV is RFC-ish: quoted fields, doubled quotes. Dangling citation ids and
self-loops are skipped, not invented.

## Graph construction

- Vertices = papers, in file order, index `0..n-1`.
- Directed edge `citing → cited` is stored for the ledger.
- Undirected adjacency is the sum of both directions (multiplicity = number
  of directed observations). Weights are those multiplicities.
- Isolated papers are kept (zero degree, zero Laplacian row).

## Spectral pipeline

1. Assemble \(L_{\mathrm{sym}}\) as a CSR matrix (diagonal 1 on non-isolates,
   off-diagonal \(-A_{ij}/\sqrt{d_i d_j}\)).
2. Lanczos with full (double) reorthogonalization, Jacobi on the Ritz
   matrix, request `k+1` smallest eigenpairs so the trivial mode can be
   dropped.
3. Embedding coordinates = eigenvectors of \(\lambda_2,\dots,\lambda_{k+1}\).
   Sign convention: the largest-magnitude entry of each vector is positive.
4. Row-normalize for k-means++ (`unit_coords`). Cluster count defaults to
   the number of distinct `field` values.
5. Bridge score as in PROBLEM.md (Rayleigh × participation). This is the
   pre-filter.
6. Leave-one-out (default on): take the top `loo-candidates` by score
   (or all vertices if \(n\) is that small). For each candidate \(v\),
   rebuild \(L_{\mathrm{sym}}\) of \(G[A\cup B]-v\) via degree deflation,
   recompute \(\lambda_2\), set `delta_lambda2 = λ2 − λ2_without`. Stable
   sort by Δλ₂ desc, score desc, id asc.

Residuals \(\|Lq-\lambda q\|\) are computed; they are diagnostic, not a
hard failure.

### Timeline

For each distinct paper year \(t\), build the cumulative graph on papers
with `year <= t` and citations with citing `year <= t`. Record
`n`, `m` (undirected edges), `lambda2`, `top_bridge_id` (heuristic, no
LOO). Written to `timeline.json`.

## Ledger

### Posting rule

For each surviving directed citation, in citation-file order:

| id | date | debit | credit | amount_cents | memo |
|----|------|-------|--------|--------------|------|
| sequential `1..M` | `year*10000+615` | citing id | cited id | `100` | `citation debt` |

Date is a synthetic mid-year posting (`YYYY0615`) so COBOL `PIC 9(8)` is
populated without inventing a day of month from thin air.

### Files (canonical, both written by C++)

`out/ledger/journal.csv` — header + rows, UTF-8, `\n`.

`out/ledger/journal.dat` — LINE SEQUENTIAL, **96 bytes + newline**:

```
01 JOURNAL-REC.
   05 JE-ID         PIC 9(8).
   05 JE-DATE       PIC 9(8).
   05 DEBIT-ACCT    PIC X(16).
   05 CREDIT-ACCT   PIC X(16).
   05 AMOUNT-CENTS  PIC 9(10).
   05 MEMO          PIC X(38).
```

Paper ids longer than 16 characters are truncated in the DAT file (fixture
ids are 10). CSV keeps the full id.

`out/ledger/trial_balance.csv` — per account `debit_cents,credit_cents,net_cents`
plus a `TOTAL` row. `net = credit - debit`. A paper that is only cited is a
net creditor (idea supplier).

### Invariants (enforced by C++, COBOL, and `scripts/verify_ledger.py`)

- Every line has two distinct nonempty accounts and a positive amount.
- \(\sum \mathrm{debit} = \sum \mathrm{credit}\).
- Re-posting the same citation file is deterministic (same je_id order).

### COBOL programs

| program | input | output |
|---------|-------|--------|
| `POST-CITATION` | `citations.csv` | `journal.dat`, `journal.csv` |
| `TRIAL-BALANCE` | `journal.dat` | `trial_balance.csv` |
| `REPORT` | `trial_balance.csv` | `trial_balance.rpt` |

Working directory is wherever the files live (typically `out/ledger/` after
copying `citations.csv` in). Compile:

```
make -C cobol
# or
cmake --build build --target cobol-ledger
```

If `cobc` is missing, sources still ship and the Python verifier is the
supported check. This is documented, not a silent skip of correctness.

## Output JSON (atlas contract)

All files are pretty-printed JSON, version field `2`.

### `embedding.json`

```
{
  "version": 2,
  "k": 8,
  "algebraic_connectivity": 0.02,
  "trivial_eigenvalue": 1e-16,
  "eigenvalues": [...],
  "nodes": [
    {
      "id": "synth-0001",
      "title": "...",
      "year": 2018,
      "category": "cs.LG",
      "field": "cs",
      "authors": "...",
      "cluster": 2,
      "degree": 12,
      "radius": 0.04,
      "z": 0.01,
      "x": [/* k raw coords */],
      "u": [/* k unit coords */]
    }
  ]
}
```

`z` is the third nontrivial coordinate (0 if \(k<3\)). `x` remains the
full vector.

### `bridges.json`

```
{
  "version": 2,
  "method": "leave-one-out",
  "prefilter": "rayleigh-participation",
  "loo_candidates": 64,
  "loo_evaluated": 64,
  "pair_a": "qbio",
  "pair_b": "cs",
  "pair_algebraic_connectivity": 0.02,
  "bridges": [{ "id", "rank", "score", "participation_entropy",
                "rayleigh_energy", "fiedler_abs", "cross_field_fraction",
                "delta_lambda2", "loo", "explanation", "fields": [] }]
}
```

`delta_lambda2` is `null` when `--no-loo`. `method` is `leave-one-out`
or `rayleigh-participation`.

### `timeline.json`

```
{
  "version": 2,
  "slices": [{ "year": 2018, "n": 40, "m": 80, "lambda2": 0.11,
               "top_bridge_id": "synth-0012" }]
}
```

### `graph_meta.json`

`n`, `undirected_edges`, `directed_citations`, `components`,
`algebraic_connectivity`, `loo_candidates`, `loo_evaluated`,
`categories[]`, `fields[]`.

### `ledger.json`

`balanced`, totals in cents, full `accounts[]`, `journal_sample[]` (first 48).

## Atlas

`docs/index.html` is Pages-ready: no build step. It must render from the
checked-in `docs/data/*.json`. A local preview is
`python3 -m http.server --directory docs 8000`.

Keyboard: `/` search, `[` `]` cycle bridges, `?` cheatsheet, `2`/`3`
dimension, `b` bridges-only. The year slider restyles nodes that do not
yet exist. 928 points are drawn with a cached projection and O(n) knn
strokes; the target is 60 fps on a laptop.

## Build / test

```
cmake -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

C++20, `-Wall -Wextra -Wpedantic`. GoogleTest via FetchContent. Tests cover
CSR symmetry, Laplacian kernel, cycle and complete-graph spectra, the
planted liaison (heuristic and leave-one-out Δλ₂), deflate-vs-rebuild
agreement, cumulative year slices, journal conservation, and CSV/JSON
edge cases.

## Reproducibility

Same corpus + same `--seed --k --lanczos-steps` ⇒ same eigenvalues up to
normal floating-point noise, same bridge ranking on the planted fixture,
bit-identical journal CSV.

## License / security

Public repository, no credentials, no telemetry. Fixture text is original
synthetic prose.
