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

- Not a crawler. We do not fetch live arXiv. Fixtures are synthetic.
- Not a replacement for Semantic Scholar / OpenAlex.
- Not an exact solver of the NP-hard interdiction problem in PROBLEM.md.
- No secrets, no accounts, no network I/O in the CLI.

## CLI

```
claimledger [command] [options]
```

| command   | effect |
|-----------|--------|
| `run`     | Full pipeline (default). |
| `build`   | Load corpus, write `graph_meta.json` only. |
| `embed`   | Laplacian + Lanczos + k-means, write `embedding.json`. |
| `bridges` | Score bridges, write `bridges.json`. |
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
5. Bridge score as in PROBLEM.md. Stable sort by score desc, id asc.

Residuals \(\|Lq-\lambda q\|\) are computed; they are diagnostic, not a
hard failure.

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

All files are pretty-printed JSON, version field `1`.

### `embedding.json`

```
{
  "version": 1,
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
      "x": [/* k raw coords */],
      "u": [/* k unit coords */]
    }
  ]
}
```

### `bridges.json`

```
{
  "version": 1,
  "method": "rayleigh-participation",
  "pair_a": "qbio",
  "pair_b": "cs",
  "pair_algebraic_connectivity": 0.02,
  "bridges": [{ "id", "rank", "score", "participation_entropy",
                "rayleigh_energy", "fiedler_abs", "cross_field_fraction",
                "explanation", "fields": [] }]
}
```

### `graph_meta.json`

`n`, `undirected_edges`, `directed_citations`, `components`,
`algebraic_connectivity`, `categories[]`, `fields[]`.

### `ledger.json`

`balanced`, totals in cents, full `accounts[]`, `journal_sample[]` (first 48).

## Atlas

`docs/index.html` is Pages-ready: no build step. It must render from the
checked-in `docs/data/*.json`. A local preview is
`python3 -m http.server --directory docs 8000`.

## Build / test

```
cmake -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

C++20, `-Wall -Wextra -Wpedantic`. GoogleTest via FetchContent. Tests cover
CSR symmetry, Laplacian kernel, cycle and complete-graph spectra, the
planted liaison, journal conservation, and CSV/JSON edge cases.

## Reproducibility

Same corpus + same `--seed --k --lanczos-steps` ⇒ same eigenvalues up to
normal floating-point noise, same bridge ranking on the planted fixture,
bit-identical journal CSV.

## License / security

Public repository, no credentials, no telemetry. Fixture text is original
synthetic prose.
