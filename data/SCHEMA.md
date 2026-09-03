# ClaimLedger data schema

The **default** fixture identifiers are synthetic (`synth-NNNN`). They are
not arXiv ids and do not name real papers.

A second, optional fixture `data/fixtures/arxiv-slice/` holds **real
public arXiv metadata** (see below). It is not the Pages default.

## `papers.csv`

| column   | type   | notes                                              |
|----------|--------|----------------------------------------------------|
| id       | string | Primary key. Pattern `synth-` + 4 zero-padded digits. |
| title    | string | Synthetic title; may contain commas (CSV-quoted).  |
| year     | int    | Publication year, 2016–2025.                       |
| category | string | arXiv-shaped category such as `cs.LG`.             |
| field    | string | Coarse field: `cs`, `stat`, `math`, `physics`, `qbio`. |
| authors  | string | Semicolon-separated synthetic author list.         |

## `citations.csv`

| column | type   | notes                                         |
|--------|--------|-----------------------------------------------|
| citing | string | Paper id that incurs intellectual debt.       |
| cited  | string | Paper id that receives intellectual credit.   |
| year   | int    | Year of the citing paper (`citing.year >= cited.year`). |

Self-loops and dangling ids are dropped by the CSR loader.

The spectral pipeline treats the citation relation as an **undirected**
multigraph (`A + Aᵀ`); the COBOL ledger keeps the **directed** posting.

## `categories.csv`

| column | type   | notes                          |
|--------|--------|--------------------------------|
| id     | string | Category code (`cs.LG`, …).    |
| name   | string | Long name.                     |
| group  | string | Coarse field (same as `field`). |

## Ledger interchange (see SPECS.md)

### `journal.csv`

```
je_id,date,debit_account,credit_account,amount_cents,memo
00000001,20180615,synth-0002,synth-0001,100,citation debt
```

Amounts are integer cents. One citation = 100 cents = 1.00 ledger unit.

### `journal.dat` (GnuCOBOL `LINE SEQUENTIAL`, 96 bytes + `\n`)

Matches `cobol/copy/JOURNAL.cpy`:

| offset | length | PIC            | field          |
|--------|--------|----------------|----------------|
| 0      | 8      | `9(8)`         | JE-ID          |
| 8      | 8      | `9(8)`         | JE-DATE        |
| 16     | 16     | `X(16)`        | DEBIT-ACCT     |
| 32     | 16     | `X(16)`        | CREDIT-ACCT    |
| 48     | 10     | `9(10)`        | AMOUNT-CENTS   |
| 58     | 38     | `X(38)`        | MEMO           |

## Atlas JSON (`out/*.json`, copied to `docs/data/`)

Version field is `2`.

- `embedding.json` — nodes with spectral coordinates `x` (raw), `u`
  (row-normalized), and scalar `z` (third nontrivial coordinate).
- `bridges.json` — ranked spectral bridges, Fiedler field pair, and
  `delta_lambda2` from leave-one-out (or `null` if `--no-loo`).
- `timeline.json` — cumulative year slices
  `{year, n, m, lambda2, top_bridge_id}`.
- `graph_meta.json` — `n`, `m`, components, `λ₂`, category legend, LOO counts.
- `ledger.json` — trial balance and a journal sample for the drill-down panel.

## Tiny fixture

`data/fixtures/tiny/` is a 10-node hand-planted two-clique + liaison graph used
by unit tests. The liaison id is `synth-0007`. Leave-one-out on this graph
is exhaustive (n < 64); the liaison has the largest `delta_lambda2`.

## arXiv slice (real public metadata)

`data/fixtures/arxiv-slice/` is a committed snapshot from
`scripts/fetch_arxiv_slice.py`. It queries the
[arXiv API](https://info.arxiv.org/help/api/user-manual.html) under the
[API terms](https://info.arxiv.org/help/api/tou.html): identified
User-Agent, ≥ 3 seconds between requests, a few dozen records only.

Paper ids are real (`1706.03762`, `1412.6980`, …) and only appear if the
API returned them. CI runs `--offline` and never hits the network.

### These are not citation edges

`citations.csv` in this directory keeps the pipeline column names
(`citing,cited,year`) so `claimledger` runs unchanged, but the rows are
an **author-coupling / cross-list graph**:

1. shared author (normalized `lastname|first-initial`);
2. different primary categories with nonempty category-set overlap;
3. Atom `rel=related` links, when the feed supplies them.

Each pair is emitted once, lexicographic order,
`year = max(year_a, year_b)`. The COBOL ledger will still post DR/CR;
read that as coupling mass, not a bibliographic citation. Details:
`data/fixtures/arxiv-slice/SOURCE.md`.

Do not treat this fixture as a citation graph and do not impersonate
papers the API did not return.
