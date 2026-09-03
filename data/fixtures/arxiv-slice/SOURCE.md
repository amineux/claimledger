# arXiv slice (public metadata, committed snapshot)

Fetched via the [arXiv API](https://info.arxiv.org/help/api/user-manual.html).
Terms: https://info.arxiv.org/help/api/tou.html

- fetched_at (UTC): 2026-09-03T20:56:40Z
- user-agent: `ClaimLedger/0.2 (https://github.com/amineux/claimledger; research fixture)`
- papers: 42
- coupling edges: 347 ({'cross-list': 345, 'shared-author': 2})

## Queries

- `id_list=1706.03762,1412.6980,1502.03167,1606.09470,1706.02515,1509.06461,0907.1815,1802.05957` (max_results=8)
- `search_query=cat:cs.LG AND cat:stat.ML` (max_results=18)
- `search_query=cat:math.ST` (max_results=16)

## Edge rule (not citations)

`citations.csv` keeps the pipeline column names (`citing,cited,year`)
but the rows are **not** bibliographic citations. An undirected edge
exists when any of the following hold:

1. **shared-author** — both papers list the same normalized author
   (`lastname|first-initial`).
2. **cross-list** — the papers share two or more arXiv categories.
3. **related** — the Atom feed supplied a `rel=related` link.

Each pair is written once, lexicographic order, `year = max(year_a, year_b)`.
The COBOL ledger will still post DR/CR on these rows; read that as
coupling mass, not a real citation of record.

Do not treat this fixture as a citation graph. The default demo
corpus remains `data/fixtures/` (`synth-NNNN`).

