# arXiv bibliographic citations (committed snapshot)

Real public metadata and **real bibliographic citation edges**.
This is not the author / cross-list coupling graph in
`data/fixtures/arxiv-slice/`.

No paper identifiers were invented. An id is present only if the
arXiv Atom API returned that record. No citation edges were
synthesized from shared authors, shared categories, or `rel=related`
links. An edge exists only when Semantic Scholar listed the cited
paper's arXiv id in `references.externalIds.ArXiv` and that id is in
this closed 80-id set.

- fetched_at (UTC): 2026-09-09T16:16:20Z
- user-agent: `ClaimLedger/0.2 (https://github.com/amineux/claimledger; research fixture)`
- arXiv endpoint: `https://export.arxiv.org/api/query`
- Semantic Scholar endpoint: `POST https://api.semanticscholar.org/graph/v1/paper/batch?fields=externalIds,title,year,references.externalIds`
- papers: 80
- directed citations: 236
- undirected pairs: 230
- incident nodes: 69
- isolates: 11
- seed ids omitted by arXiv: (none)

## Independent harvest (acceptance)

An independent harvest of this exact id set produced 80 papers,
236 directed citations, 230 undirected pairs, 69 incident nodes,
and 11 isolates. Live API versions can differ (latest-version
bibliographies, S2 graph rebuilds). Counts below are what this
snapshot actually received. Differences are documented, never padded.

- `papers`: 80 (acceptance 80; match)
- `directed_citations`: 236 (acceptance 236; match)
- `undirected_pairs`: 230 (acceptance 230; match)
- `incident_nodes`: 69 (acceptance 69; match)
- `isolates`: 11 (acceptance 11; match)

## arXiv queries

Closed set: the 80 ids listed in `scripts/fetch_citation_slice.py`
(`SEED_IDS`). Sequential `GET` requests to
`https://export.arxiv.org/api/query` with:

- `id_list=<up to 20 comma-separated ids>`
- `start=0`
- `max_results=<chunk size>`

One request at a time (no connection pool, no parallelism).
Sleep ≥ 3.1 seconds between requests. Identify the client
with User-Agent `ClaimLedger/0.2 (https://github.com/amineux/claimledger; research fixture)`.

Ids the feed does not return are dropped. Extra feed entries that
are not in `SEED_IDS` are ignored.

## Semantic Scholar batch method

`POST https://api.semanticscholar.org/graph/v1/paper/batch?fields=externalIds,title,year,references.externalIds`

Body:

```json
{"ids": ["ARXIV:<id>", "..."]}
```

Chunks of 20 paper ids, same User-Agent, no API key.
Sleep ≥ 3.1 seconds between successful chunks. HTTP 429:
honor `Retry-After` when present, otherwise exponential backoff,
then retry the same chunk. 5xx is retried the same way. Only
`references[].externalIds.ArXiv` values that resolve to an id in
the closed set are kept. Self-loops and duplicate pairs are dropped.

S2 titles/years are not copied into `papers.csv`. Paper rows come
from arXiv. Citation `year` is the citing paper's arXiv published year.

## Latest-version bibliography vs published year

Semantic Scholar's reference list follows the **latest** arXiv
version it has indexed. A paper first posted in year Y1 can later
cite a paper whose original arXiv published year is Y2 > Y1.
Those edges are real bibliography rows. This fixture preserves
them and does not rewrite `citations.year` or drop the row.

These rows are real S2 references. They are kept. Years are not rewritten.

- `1312.6114` (2013) → `1401.4082` (2014)
- `1502.03167` (2015) → `1608.06993` (2016)
- `1711.05101` (2017) → `1904.09237` (2019)

## Rate limits and terms

- arXiv API terms of use: https://info.arxiv.org/help/api/tou.html
- arXiv API user manual: https://info.arxiv.org/help/api/user-manual.html
- Semantic Scholar API license: https://www.semanticscholar.org/product/api/license
- Semantic Scholar API license (alternate): https://api.semanticscholar.org/license/
- Semantic Scholar Graph API docs: https://api.semanticscholar.org/api-docs/graph

This script does not harvest the full arXiv or S2 corpora. It
asks for 80 known records and their references, caches the raw
responses under `--cache`, and writes a committed snapshot so CI
can run `--offline` with no network.

## Reproduce

```bash
python3 scripts/fetch_citation_slice.py --out data/fixtures/arxiv-citations
python3 scripts/fetch_citation_slice.py --out data/fixtures/arxiv-citations --from-cache
python3 scripts/fetch_citation_slice.py --out data/fixtures/arxiv-citations --offline
```

Default live mode reuses cached chunks when present. `--refresh`
ignores the cache and hits both APIs again. `--from-cache`
rebuilds CSVs from cached Atom/JSON only. `--offline` validates
the committed snapshot and never writes.

