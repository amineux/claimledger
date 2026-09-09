# arxiv-citations schema

Closed bibliographic citation graph. Distinct from
`data/fixtures/arxiv-slice/`, which is an author / cross-list **coupling**
graph, not a bibliography.

Paper identifiers are real arXiv ids returned by the Atom API. Citation
rows are real Semantic Scholar `references.externalIds.ArXiv` edges
induced on that closed set. No identifiers or edges are invented, and
author/category coupling is never used as a substitute for a reference.

## `papers.csv`

| column   | type   | notes |
|----------|--------|-------|
| id       | string | Canonical arXiv id (`YYMM.NNNNN` or old `archive/YYMMNNN`). Sorted lexicographically. Only ids the arXiv API returned. |
| title    | string | Atom title; whitespace collapsed; CSV-quoted when needed. |
| year     | int    | arXiv **published** year (`published[0:4]`), not the latest version year. |
| category | string | Atom primary category (`arxiv:primary_category`, else first `category`). |
| field    | string | Coarse field derived from the primary category (`cs`, `stat`, `math`, …). |
| authors  | string | Atom author names, semicolon-separated (`"; "`). |

## `citations.csv`

| column | type   | notes |
|--------|--------|-------|
| citing | string | arXiv id of the paper whose latest-version S2 bibliography lists `cited`. |
| cited  | string | arXiv id present in `references.externalIds.ArXiv` and in this closed set. |
| year   | int    | Published year of the **citing** paper (from `papers.csv`). |

Semantics:

- Directed, one row per unique `(citing, cited)`.
- No self-loops, no dangling ids, no duplicates.
- Both endpoints are rows in `papers.csv`.
- Edges are bibliographic only. Shared authors or shared categories never create a row.
- Latest-version bibliographies can list a reference whose original arXiv
  published year is later than the citing paper. Those real S2 edges are
  **preserved**; `year` stays the citing paper's published year and is not
  rewritten to satisfy `citing.year >= cited.year`.

The C++ pipeline treats the relation as an undirected multigraph
(`A + Aᵀ`) and posts the directed rows to the ledger.

## `categories.csv`

| column | type   | notes |
|--------|--------|-------|
| id     | string | arXiv category term observed on any kept paper (primary or secondary). |
| name   | string | Same as `id` (Atom does not supply a long name). |
| group  | string | Coarse field (same mapping as `papers.field`). |

## `nodes.csv`

| column      | type   | notes |
|-------------|--------|-------|
| idx         | int    | Dense `0..n-1` in `papers.csv` order. |
| arxiv_id    | string | Same as `papers.id`. |
| primary_cat | string | Same as `papers.category`. |

## `edges.csv`

| column | type   | notes |
|--------|--------|-------|
| u      | int    | Endpoint index; `u < v`. |
| v      | int    | Endpoint index. |
| w      | int    | Number of directed `citations.csv` rows in either direction. Always `w > 0`. |

Each undirected pair is written once. Reciprocal citations contribute
`w = 2`.

## `ledger_edges.txt`

| column | type   | notes |
|--------|--------|-------|
| FROM   | string | Citing arXiv id (debit / intellectual debt). |
| TO     | string | Cited arXiv id (credit / intellectual capital). |
| AMOUNT | string | `1.00` per directed citation. |

Header is `FROM,TO,AMOUNT`. Row count equals `citations.csv` (excluding
its header). Order matches `citations.csv`.

## `manifest.json`

Snapshot metadata: counts, API endpoints, `fetched_at` UTC,
`every_id_from_api_response`, `citation_edges_bibliographic_only`, and
SHA-256 digests of the data files listed above. Digests are hex SHA-256
of the file bytes as committed.

## `SOURCE.md`

Provenance: exact arXiv `id_list` batches, S2 POST batch method,
User-Agent, rate limits, ToU / license links, and the statement that no
ids or edges were invented.

## Pipeline consumption

`claimledger --data data/fixtures/arxiv-citations` reads only
`papers.csv`, `citations.csv`, and `categories.csv`. Isolated papers are
kept. The extra node/edge/ledger files are the explicit graph and
double-entry views of the same snapshot.
