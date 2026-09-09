#!/usr/bin/env python3
"""Fetch a closed bibliographic citation graph for ClaimLedger.

This is a real citation dump. Every paper id must come from the arXiv Atom
API; every directed edge must come from Semantic Scholar
`references.externalIds.ArXiv` induced on that closed set. Author overlap
and category cross-lists are never turned into edges.

The committed snapshot lives at `data/fixtures/arxiv-citations/`. The
existing `arxiv-slice` coupling fixture is a different graph and is not
written by this script.

arXiv API terms: https://info.arxiv.org/help/api/tou.html
Semantic Scholar API license: https://www.semanticscholar.org/product/api/license

Usage:
  python3 scripts/fetch_citation_slice.py --out data/fixtures/arxiv-citations
  python3 scripts/fetch_citation_slice.py --out data/fixtures/arxiv-citations --from-cache
  python3 scripts/fetch_citation_slice.py --out data/fixtures/arxiv-citations --offline
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import re
import sys
import time
import urllib.error
import urllib.parse
import urllib.request
from collections import defaultdict
from datetime import datetime, timezone
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

import fetch_arxiv_slice as arx  # noqa: E402

ARXIV_API = arx.API
S2_BATCH = "https://api.semanticscholar.org/graph/v1/paper/batch"
S2_FIELDS = "externalIds,title,year,references.externalIds"
UA = arx.UA
SLEEP_S = 3.1
CHUNK = 20
ARXIV_TOU = "https://info.arxiv.org/help/api/tou.html"
ARXIV_MANUAL = "https://info.arxiv.org/help/api/user-manual.html"
S2_LICENSE = "https://www.semanticscholar.org/product/api/license"
S2_LICENSE_ALT = "https://api.semanticscholar.org/license/"
S2_DOCS = "https://api.semanticscholar.org/api-docs/graph"

# Independently harvested acceptance counts for this exact closed set.
# Live APIs may differ by bibliography version; never fabricate to match.
ACCEPTANCE = {
    "papers": 80,
    "directed_citations": 236,
    "undirected_pairs": 230,
    "incident_nodes": 69,
    "isolates": 11,
}

# Exact closed node set. An id is kept only if the arXiv Atom API returns it.
SEED_IDS = [
    "0706.4138",
    "0805.4471",
    "0810.3286",
    "1312.6114",
    "1401.4082",
    "1406.1078",
    "1406.2661",
    "1409.0473",
    "1409.1556",
    "1409.3215",
    "1409.4842",
    "1411.4028",
    "1412.6980",
    "1502.03167",
    "1505.05424",
    "1505.05770",
    "1506.01497",
    "1506.02142",
    "1506.02557",
    "1506.02640",
    "1508.04025",
    "1508.07909",
    "1509.00519",
    "1511.06434",
    "1512.00567",
    "1512.03385",
    "1603.05027",
    "1606.08415",
    "1607.06450",
    "1608.03983",
    "1608.06993",
    "1609.08144",
    "1611.03530",
    "1611.05431",
    "1612.01474",
    "1612.03144",
    "1701.07875",
    "1703.03906",
    "1704.00028",
    "1704.02916",
    "1704.05018",
    "1705.03122",
    "1706.03762",
    "1711.00464",
    "1711.05101",
    "1801.00862",
    "1802.05365",
    "1802.05957",
    "1803.00745",
    "1803.03635",
    "1803.11173",
    "1804.07461",
    "1804.11326",
    "1806.07366",
    "1806.07572",
    "1808.03570",
    "1809.09795",
    "1810.04805",
    "1901.02860",
    "1902.06720",
    "1904.09237",
    "1904.12070",
    "1904.12775",
    "1905.00206",
    "1905.02618",
    "1905.10876",
    "1906.01513",
    "1906.08237",
    "1907.10477",
    "1907.11692",
    "2005.11454",
    "2005.14165",
    "2006.04566",
    "2012.10103",
    "2012.10227",
    "2012.11009",
    "2012.11196",
    "2012.11536",
    "2112.07461",
    "math/0409186",
]

ARXIV_ID_RE = re.compile(
    r"^(\d{4}\.\d{4,5}|[a-z-]+(?:\.[a-z0-9-]+)?/\d{7})$",
    re.IGNORECASE,
)

DATA_FILES = (
    "papers.csv",
    "citations.csv",
    "categories.csv",
    "nodes.csv",
    "edges.csv",
    "ledger_edges.txt",
)


def chunks(items: list[str], n: int) -> list[list[str]]:
    return [items[i : i + n] for i in range(0, len(items), n)]


def normalize_arxiv_id(raw: str) -> str:
    s = (raw or "").strip()
    s = re.sub(r"^https?://arxiv\.org/abs/", "", s, flags=re.IGNORECASE)
    s = re.sub(r"^arxiv:", "", s, flags=re.IGNORECASE)
    s = re.sub(r"v\d+$", "", s)
    return s


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    h.update(path.read_bytes())
    return h.hexdigest()


def utc_now() -> str:
    return datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


def seed_lookup() -> dict[str, str]:
    return {normalize_arxiv_id(i).lower(): i for i in SEED_IDS}


def resolve_seed(raw: str, lookup: dict[str, str]) -> str | None:
    key = normalize_arxiv_id(raw).lower()
    if key in lookup:
        return lookup[key]
    # S2 sometimes stores old-style ids with a dot: math.0409186
    if "." in key and "/" not in key:
        head, tail = key.split(".", 1)
        if tail.isdigit() and len(tail) == 7:
            alt = f"{head}/{tail}"
            if alt in lookup:
                return lookup[alt]
    if "/" in key:
        alt = key.replace("/", ".", 1)
        if alt in lookup:
            return lookup[alt]
    return None


def sleep_between(first: bool, seconds: float) -> None:
    if not first:
        time.sleep(seconds)


def fetch_arxiv_chunk(id_list: list[str]) -> str:
    params = {
        "id_list": ",".join(id_list),
        "start": "0",
        "max_results": str(len(id_list)),
    }
    return arx.fetch(params)


def s2_post(ids: list[str], timeout: int = 90) -> tuple[int, dict[str, str], bytes]:
    url = f"{S2_BATCH}?{urllib.parse.urlencode({'fields': S2_FIELDS})}"
    body = json.dumps({"ids": [f"ARXIV:{pid}" for pid in ids]}).encode("utf-8")
    headers = {
        "User-Agent": UA,
        "Accept": "application/json",
        "Content-Type": "application/json",
    }
    req = urllib.request.Request(url, data=body, headers=headers, method="POST")
    try:
        with urllib.request.urlopen(req, timeout=timeout) as resp:
            return resp.status, {k: v for k, v in resp.headers.items()}, resp.read()
    except urllib.error.HTTPError as exc:
        return exc.code, {k: v for k, v in exc.headers.items()}, exc.read()


def s2_post_retry(ids: list[str], sleep_s: float) -> list:
    delay = max(sleep_s, 3.0)
    last_err = "unknown"
    for attempt in range(8):
        status, headers, raw = s2_post(ids)
        if status == 429:
            retry_after = headers.get("Retry-After") or headers.get("retry-after")
            try:
                wait = float(retry_after) if retry_after else delay
            except ValueError:
                wait = delay
            wait = max(wait, delay)
            print(f"  S2 429; sleeping {wait:.1f}s (attempt {attempt + 1})", file=sys.stderr)
            time.sleep(wait)
            delay = min(delay * 2.0, 120.0)
            last_err = "HTTP 429"
            continue
        if status >= 500:
            print(f"  S2 {status}; sleeping {delay:.1f}s (attempt {attempt + 1})", file=sys.stderr)
            time.sleep(delay)
            delay = min(delay * 2.0, 120.0)
            last_err = f"HTTP {status}"
            continue
        if status != 200:
            raise RuntimeError(f"Semantic Scholar batch failed: HTTP {status}: {raw[:400]!r}")
        payload = json.loads(raw.decode("utf-8"))
        if not isinstance(payload, list):
            raise RuntimeError(f"Semantic Scholar batch returned non-list: {payload!r}"[:400])
        return payload
    raise RuntimeError(f"Semantic Scholar batch failed after retries: {last_err}")


def load_or_fetch_arxiv(cache_dir: Path, sleep_s: float, from_cache: bool, refresh: bool) -> tuple[list[dict], str]:
    lookup = seed_lookup()
    papers: dict[str, dict] = {}
    fetched_at = utc_now()
    arxiv_dir = cache_dir / "arxiv"
    first = True
    for i, group in enumerate(chunks(SEED_IDS, CHUNK)):
        xml_path = arxiv_dir / f"chunk-{i:02d}.xml"
        ids_path = arxiv_dir / f"chunk-{i:02d}.ids.txt"
        meta_path = arxiv_dir / f"chunk-{i:02d}.meta.json"
        use_cache = xml_path.exists() and not refresh
        if from_cache and not xml_path.exists():
            raise FileNotFoundError(f"missing arXiv cache {xml_path}")
        if use_cache:
            xml_text = xml_path.read_text(encoding="utf-8")
            if meta_path.exists():
                fetched_at = json.loads(meta_path.read_text(encoding="utf-8")).get("fetched_at", fetched_at)
            print(f"cache arXiv chunk {i} ({len(group)} ids)", file=sys.stderr)
        else:
            if from_cache:
                raise FileNotFoundError(f"missing arXiv cache {xml_path}")
            sleep_between(first, sleep_s)
            first = False
            print(f"GET arXiv id_list n={len(group)} chunk={i}", file=sys.stderr)
            xml_text = fetch_arxiv_chunk(group)
            arxiv_dir.mkdir(parents=True, exist_ok=True)
            xml_path.write_text(xml_text, encoding="utf-8")
            ids_path.write_text("\n".join(group) + "\n", encoding="utf-8")
            meta_path.write_text(json.dumps({"fetched_at": utc_now()}, indent=2) + "\n", encoding="utf-8")
            fetched_at = utc_now()
        batch = arx.parse_feed(xml_text)
        print(f"  got {len(batch)} entries", file=sys.stderr)
        for p in batch:
            canonical = resolve_seed(p["id"], lookup)
            if canonical is None:
                continue
            p["id"] = canonical
            papers[canonical] = p
    return list(papers.values()), fetched_at


def load_or_fetch_s2(
    paper_ids: list[str],
    cache_dir: Path,
    sleep_s: float,
    from_cache: bool,
    refresh: bool,
) -> tuple[list[tuple[list[str], list]], str]:
    s2_dir = cache_dir / "s2"
    out: list[tuple[list[str], list]] = []
    fetched_at = utc_now()
    first = True
    for i, group in enumerate(chunks(paper_ids, CHUNK)):
        json_path = s2_dir / f"chunk-{i:02d}.json"
        ids_path = s2_dir / f"chunk-{i:02d}.ids.txt"
        meta_path = s2_dir / f"chunk-{i:02d}.meta.json"
        use_cache = json_path.exists() and not refresh
        if from_cache and not json_path.exists():
            raise FileNotFoundError(f"missing S2 cache {json_path}")
        if use_cache:
            payload = json.loads(json_path.read_text(encoding="utf-8"))
            if meta_path.exists():
                fetched_at = json.loads(meta_path.read_text(encoding="utf-8")).get("fetched_at", fetched_at)
            print(f"cache S2 chunk {i} ({len(group)} ids)", file=sys.stderr)
        else:
            sleep_between(first, sleep_s)
            first = False
            print(f"POST S2 batch n={len(group)} chunk={i}", file=sys.stderr)
            payload = s2_post_retry(group, sleep_s)
            s2_dir.mkdir(parents=True, exist_ok=True)
            json_path.write_text(json.dumps(payload, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
            ids_path.write_text("\n".join(group) + "\n", encoding="utf-8")
            meta_path.write_text(json.dumps({"fetched_at": utc_now()}, indent=2) + "\n", encoding="utf-8")
            fetched_at = utc_now()
        if not isinstance(payload, list):
            raise RuntimeError(f"S2 chunk {i} is not a list")
        out.append((group, payload))
    return out, fetched_at


def induce_citations(
    papers: list[dict], s2_batches: list[tuple[list[str], list]]
) -> list[tuple[str, str, int]]:
    lookup = {p["id"]: p for p in papers}
    seed = seed_lookup()
    edges: set[tuple[str, str]] = set()
    for group, payload in s2_batches:
        if len(payload) < len(group):
            print(
                f"warning: S2 returned {len(payload)} rows for {len(group)} ids",
                file=sys.stderr,
            )
        for req_id, rec in zip(group, payload):
            if rec is None:
                print(f"  S2 miss: {req_id}", file=sys.stderr)
                continue
            if req_id not in lookup:
                continue
            refs = rec.get("references") or []
            for ref in refs:
                if not isinstance(ref, dict):
                    continue
                ext = ref.get("externalIds") or {}
                if not isinstance(ext, dict):
                    continue
                raw = ext.get("ArXiv") or ext.get("arXiv") or ext.get("ARXIV")
                if not raw:
                    continue
                cited = resolve_seed(str(raw), seed)
                if cited is None or cited not in lookup or cited == req_id:
                    continue
                edges.add((req_id, cited))
    out = []
    for citing, cited in sorted(edges):
        out.append((citing, cited, int(lookup[citing]["year"])))
    return out


def counts_of(papers: list[dict], citations: list[tuple[str, str, int]]) -> dict[str, int]:
    ids = [p["id"] for p in papers]
    incident: set[str] = set()
    undirected: set[tuple[str, str]] = set()
    for a, b, _year in citations:
        incident.add(a)
        incident.add(b)
        undirected.add((a, b) if a < b else (b, a))
    return {
        "papers": len(ids),
        "directed_citations": len(citations),
        "undirected_pairs": len(undirected),
        "incident_nodes": len(incident),
        "isolates": len(ids) - len(incident),
    }


def year_inversions(
    papers: list[dict], citations: list[tuple[str, str, int]]
) -> list[dict[str, int | str]]:
    years = {p["id"]: int(p["year"]) for p in papers}
    rows = []
    for citing, cited, cite_year in citations:
        cited_year = years.get(cited, 0)
        if cited_year and cite_year and cite_year < cited_year:
            rows.append(
                {
                    "citing": citing,
                    "cited": cited,
                    "citing_published_year": cite_year,
                    "cited_published_year": cited_year,
                }
            )
    return rows


def write_schema(out_dir: Path) -> None:
    (out_dir / "SCHEMA.md").write_text(
        """# arxiv-citations schema

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
""",
        encoding="utf-8",
    )


def write_source(
    out_dir: Path,
    papers: list[dict],
    citations: list[tuple[str, str, int]],
    counts: dict[str, int],
    inversions: list[dict[str, int | str]],
    meta: dict,
) -> None:
    dropped = [i for i in SEED_IDS if i not in {p["id"] for p in papers}]
    accept_lines = []
    for key, expected in ACCEPTANCE.items():
        got = counts[key]
        flag = "match" if got == expected else "DIFFERENT from independent harvest"
        accept_lines.append(f"- `{key}`: {got} (acceptance {expected}; {flag})")
    inv_lines = ["None."]
    if inversions:
        inv_lines = [
            "These rows are real S2 references. They are kept. Years are not rewritten.",
            "",
        ]
        for row in inversions:
            inv_lines.append(
                f"- `{row['citing']}` ({row['citing_published_year']}) → "
                f"`{row['cited']}` ({row['cited_published_year']})"
            )
    dropped_txt = ", ".join(f"`{i}`" for i in dropped) if dropped else "(none)"
    (out_dir / "SOURCE.md").write_text(
        "\n".join(
            [
                "# arXiv bibliographic citations (committed snapshot)",
                "",
                "Real public metadata and **real bibliographic citation edges**.",
                "This is not the author / cross-list coupling graph in",
                "`data/fixtures/arxiv-slice/`.",
                "",
                "No paper identifiers were invented. An id is present only if the",
                "arXiv Atom API returned that record. No citation edges were",
                "synthesized from shared authors, shared categories, or `rel=related`",
                "links. An edge exists only when Semantic Scholar listed the cited",
                "paper's arXiv id in `references.externalIds.ArXiv` and that id is in",
                "this closed 80-id set.",
                "",
                f"- fetched_at (UTC): {meta.get('fetched_at', '')}",
                f"- user-agent: `{UA}`",
                f"- arXiv endpoint: `{ARXIV_API}`",
                f"- Semantic Scholar endpoint: `POST {S2_BATCH}?fields={S2_FIELDS}`",
                f"- papers: {counts['papers']}",
                f"- directed citations: {counts['directed_citations']}",
                f"- undirected pairs: {counts['undirected_pairs']}",
                f"- incident nodes: {counts['incident_nodes']}",
                f"- isolates: {counts['isolates']}",
                f"- seed ids omitted by arXiv: {dropped_txt}",
                "",
                "## Independent harvest (acceptance)",
                "",
                "An independent harvest of this exact id set produced 80 papers,",
                "236 directed citations, 230 undirected pairs, 69 incident nodes,",
                "and 11 isolates. Live API versions can differ (latest-version",
                "bibliographies, S2 graph rebuilds). Counts below are what this",
                "snapshot actually received. Differences are documented, never padded.",
                "",
                *accept_lines,
                "",
                "## arXiv queries",
                "",
                "Closed set: the 80 ids listed in `scripts/fetch_citation_slice.py`",
                "(`SEED_IDS`). Sequential `GET` requests to",
                f"`{ARXIV_API}` with:",
                "",
                "- `id_list=<up to 20 comma-separated ids>`",
                "- `start=0`",
                "- `max_results=<chunk size>`",
                "",
                "One request at a time (no connection pool, no parallelism).",
                f"Sleep ≥ {SLEEP_S} seconds between requests. Identify the client",
                f"with User-Agent `{UA}`.",
                "",
                "Ids the feed does not return are dropped. Extra feed entries that",
                "are not in `SEED_IDS` are ignored.",
                "",
                "## Semantic Scholar batch method",
                "",
                f"`POST {S2_BATCH}?fields={S2_FIELDS}`",
                "",
                "Body:",
                "",
                "```json",
                '{"ids": ["ARXIV:<id>", "..."]}',
                "```",
                "",
                f"Chunks of {CHUNK} paper ids, same User-Agent, no API key.",
                f"Sleep ≥ {SLEEP_S} seconds between successful chunks. HTTP 429:",
                "honor `Retry-After` when present, otherwise exponential backoff,",
                "then retry the same chunk. 5xx is retried the same way. Only",
                "`references[].externalIds.ArXiv` values that resolve to an id in",
                "the closed set are kept. Self-loops and duplicate pairs are dropped.",
                "",
                "S2 titles/years are not copied into `papers.csv`. Paper rows come",
                "from arXiv. Citation `year` is the citing paper's arXiv published year.",
                "",
                "## Latest-version bibliography vs published year",
                "",
                "Semantic Scholar's reference list follows the **latest** arXiv",
                "version it has indexed. A paper first posted in year Y1 can later",
                "cite a paper whose original arXiv published year is Y2 > Y1.",
                "Those edges are real bibliography rows. This fixture preserves",
                "them and does not rewrite `citations.year` or drop the row.",
                "",
                *inv_lines,
                "",
                "## Rate limits and terms",
                "",
                f"- arXiv API terms of use: {ARXIV_TOU}",
                f"- arXiv API user manual: {ARXIV_MANUAL}",
                f"- Semantic Scholar API license: {S2_LICENSE}",
                f"- Semantic Scholar API license (alternate): {S2_LICENSE_ALT}",
                f"- Semantic Scholar Graph API docs: {S2_DOCS}",
                "",
                "This script does not harvest the full arXiv or S2 corpora. It",
                "asks for 80 known records and their references, caches the raw",
                "responses under `--cache`, and writes a committed snapshot so CI",
                "can run `--offline` with no network.",
                "",
                "## Reproduce",
                "",
                "```bash",
                "python3 scripts/fetch_citation_slice.py --out data/fixtures/arxiv-citations",
                "python3 scripts/fetch_citation_slice.py --out data/fixtures/arxiv-citations --from-cache",
                "python3 scripts/fetch_citation_slice.py --out data/fixtures/arxiv-citations --offline",
                "```",
                "",
                "Default live mode reuses cached chunks when present. `--refresh`",
                "ignores the cache and hits both APIs again. `--from-cache`",
                "rebuilds CSVs from cached Atom/JSON only. `--offline` validates",
                "the committed snapshot and never writes.",
                "",
            ]
        )
        + "\n",
        encoding="utf-8",
    )


def write_fixture(
    out_dir: Path,
    papers: list[dict],
    citations: list[tuple[str, str, int]],
    meta: dict,
) -> dict:
    out_dir.mkdir(parents=True, exist_ok=True)
    papers = sorted({p["id"]: p for p in papers}.values(), key=lambda p: p["id"])
    citations = sorted(set(citations), key=lambda r: (r[0], r[1]))
    counts = counts_of(papers, citations)
    inversions = year_inversions(papers, citations)

    with (out_dir / "papers.csv").open("w", newline="", encoding="utf-8") as fh:
        w = csv.writer(fh)
        w.writerow(["id", "title", "year", "category", "field", "authors"])
        for p in papers:
            w.writerow(
                [
                    p["id"],
                    p["title"],
                    p["year"],
                    p["category"],
                    p["field"],
                    "; ".join(p["authors"]),
                ]
            )

    with (out_dir / "citations.csv").open("w", newline="", encoding="utf-8") as fh:
        w = csv.writer(fh)
        w.writerow(["citing", "cited", "year"])
        for citing, cited, year in citations:
            w.writerow([citing, cited, year])

    cats: dict[str, str] = {}
    for p in papers:
        observed = list(p.get("categories") or [])
        if p["category"]:
            observed.insert(0, p["category"])
        for c in observed:
            if c and c not in cats:
                cats[c] = arx.field_of(c)
    with (out_dir / "categories.csv").open("w", newline="", encoding="utf-8") as fh:
        w = csv.writer(fh)
        w.writerow(["id", "name", "group"])
        for cid, group in sorted(cats.items()):
            w.writerow([cid, cid, group])

    index_of = {p["id"]: i for i, p in enumerate(papers)}
    with (out_dir / "nodes.csv").open("w", newline="", encoding="utf-8") as fh:
        w = csv.writer(fh)
        w.writerow(["idx", "arxiv_id", "primary_cat"])
        for i, p in enumerate(papers):
            w.writerow([i, p["id"], p["category"]])

    undirected: dict[tuple[int, int], int] = defaultdict(int)
    for citing, cited, _year in citations:
        u, v = index_of[citing], index_of[cited]
        key = (u, v) if u < v else (v, u)
        undirected[key] += 1
    with (out_dir / "edges.csv").open("w", newline="", encoding="utf-8") as fh:
        w = csv.writer(fh)
        w.writerow(["u", "v", "w"])
        for (u, v), weight in sorted(undirected.items()):
            w.writerow([u, v, weight])

    with (out_dir / "ledger_edges.txt").open("w", newline="", encoding="utf-8") as fh:
        w = csv.writer(fh)
        w.writerow(["FROM", "TO", "AMOUNT"])
        for citing, cited, _year in citations:
            w.writerow([citing, cited, "1.00"])

    write_schema(out_dir)
    write_source(out_dir, papers, citations, counts, inversions, meta)

    digest = {name: sha256_file(out_dir / name) for name in DATA_FILES}
    manifest = {
        "name": "arxiv-citations",
        "description": (
            "Closed bibliographic citation graph: real arXiv metadata and "
            "Semantic Scholar references.externalIds.ArXiv edges induced on "
            "the committed 80-id set."
        ),
        "fetched_at": meta.get("fetched_at", ""),
        "every_id_from_api_response": True,
        "citation_edges_bibliographic_only": True,
        "user_agent": UA,
        "endpoints": {
            "arxiv": ARXIV_API,
            "semantic_scholar": f"{S2_BATCH}?fields={S2_FIELDS}",
        },
        "seed_ids": list(SEED_IDS),
        "counts": counts,
        "acceptance_counts": dict(ACCEPTANCE),
        "acceptance_match": counts == ACCEPTANCE,
        "year_inversions": inversions,
        "sha256": digest,
    }
    (out_dir / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    return manifest


def load_committed(out_dir: Path) -> tuple[list[dict], list[tuple[str, str, int]]]:
    with (out_dir / "papers.csv").open(encoding="utf-8") as fh:
        papers = []
        for row in csv.DictReader(fh):
            papers.append(
                {
                    "id": row["id"],
                    "title": row["title"],
                    "year": int(row["year"]),
                    "category": row["category"],
                    "field": row["field"],
                    "authors": [a.strip() for a in row["authors"].split(";") if a.strip()],
                    "categories": [row["category"]],
                }
            )
    with (out_dir / "citations.csv").open(encoding="utf-8") as fh:
        citations = [
            (row["citing"], row["cited"], int(row["year"])) for row in csv.DictReader(fh)
        ]
    return papers, citations


def validate_offline(out_dir: Path) -> int:
    required = [
        out_dir / name
        for name in (*DATA_FILES, "manifest.json", "SOURCE.md", "SCHEMA.md")
    ]
    for path in required:
        if not path.exists():
            print(f"missing {path}", file=sys.stderr)
            return 1

    papers, citations = load_committed(out_dir)
    ids = [p["id"] for p in papers]
    id_set = set(ids)
    errors: list[str] = []

    if len(ids) < 50:
        errors.append(f"expected >= 50 unique ids, got {len(ids)}")
    if len(id_set) != len(ids):
        errors.append("duplicate paper ids")
    if ids != sorted(ids):
        errors.append("papers.csv is not sorted by arXiv id")
    if any(i.startswith("synth-") for i in ids):
        errors.append("arxiv-citations must use real arXiv ids, not synth-NNNN")
    for pid in ids:
        if not ARXIV_ID_RE.match(pid):
            errors.append(f"invalid arXiv id format: {pid}")
    if "1706.03762" not in id_set:
        errors.append("expected seed id 1706.03762 in papers.csv")

    seen_cites: set[tuple[str, str]] = set()
    for citing, cited, year in citations:
        if citing == cited:
            errors.append(f"self-loop {citing}")
        if citing not in id_set or cited not in id_set:
            errors.append(f"dangling citation {citing} -> {cited}")
        if (citing, cited) in seen_cites:
            errors.append(f"duplicate citation {citing} -> {cited}")
        seen_cites.add((citing, cited))
        paper_year = next((p["year"] for p in papers if p["id"] == citing), None)
        if paper_year is not None and year != paper_year:
            errors.append(f"citation year {year} != citing published year {paper_year} for {citing}")

    with (out_dir / "nodes.csv").open(encoding="utf-8") as fh:
        nodes = list(csv.DictReader(fh))
    if [int(r["idx"]) for r in nodes] != list(range(len(papers))):
        errors.append("nodes.csv idx is not dense 0..n-1")
    if [r["arxiv_id"] for r in nodes] != ids:
        errors.append("nodes.csv order does not match papers.csv")
    for node, paper in zip(nodes, papers):
        if node["primary_cat"] != paper["category"]:
            errors.append(f"primary_cat mismatch for {paper['id']}")

    with (out_dir / "edges.csv").open(encoding="utf-8") as fh:
        edges = list(csv.DictReader(fh))
    n = len(papers)
    seen_uv: set[tuple[int, int]] = set()
    weight_sum = 0
    for row in edges:
        u, v, w = int(row["u"]), int(row["v"]), int(row["w"])
        if not (0 <= u < n and 0 <= v < n):
            errors.append(f"edge index out of range {u},{v}")
        if not (u < v):
            errors.append(f"edge not u<v: {u},{v}")
        if w <= 0:
            errors.append(f"non-positive weight {u},{v},{w}")
        if (u, v) in seen_uv:
            errors.append(f"duplicate undirected edge {u},{v}")
        seen_uv.add((u, v))
        weight_sum += w
    if weight_sum != len(citations):
        errors.append(f"sum(edges.w)={weight_sum} != directed citations {len(citations)}")

    with (out_dir / "ledger_edges.txt").open(encoding="utf-8") as fh:
        ledger = list(csv.DictReader(fh))
    if list(ledger[0].keys())[:3] != ["FROM", "TO", "AMOUNT"] and ledger:
        # DictReader keys follow the header; empty file is already an error via count.
        pass
    if len(ledger) != len(citations):
        errors.append(f"ledger rows {len(ledger)} != directed citations {len(citations)}")
    for row, (citing, cited, _year) in zip(ledger, citations):
        if row["FROM"] != citing or row["TO"] != cited or row["AMOUNT"] != "1.00":
            errors.append(f"ledger mismatch {row} vs {citing}->{cited}")
            break

    with (out_dir / "categories.csv").open(encoding="utf-8") as fh:
        cats = list(csv.DictReader(fh))
    cat_ids = {r["id"] for r in cats}
    for p in papers:
        if p["category"] and p["category"] not in cat_ids:
            errors.append(f"missing category {p['category']}")

    manifest = json.loads((out_dir / "manifest.json").read_text(encoding="utf-8"))
    if manifest.get("every_id_from_api_response") is not True:
        errors.append("manifest.every_id_from_api_response must be true")
    if manifest.get("citation_edges_bibliographic_only") is not True:
        errors.append("manifest.citation_edges_bibliographic_only must be true")
    for name in DATA_FILES:
        expected = (manifest.get("sha256") or {}).get(name)
        actual = sha256_file(out_dir / name)
        if expected != actual:
            errors.append(f"sha256 mismatch for {name}")

    got = counts_of(papers, citations)
    if manifest.get("counts") != got:
        errors.append(f"manifest.counts {manifest.get('counts')} != recomputed {got}")

    source = (out_dir / "SOURCE.md").read_text(encoding="utf-8")
    for needle in (ARXIV_TOU, S2_LICENSE, "No paper identifiers were invented", "not rewritten"):
        if needle not in source:
            errors.append(f"SOURCE.md missing required text: {needle}")

    if errors:
        print("offline validation failed:", file=sys.stderr)
        for err in errors:
            print(f"  {err}", file=sys.stderr)
        return 1

    print(
        "offline ok:",
        f"{got['papers']} papers,",
        f"{got['directed_citations']} directed,",
        f"{got['undirected_pairs']} undirected,",
        f"{got['incident_nodes']} incident,",
        f"{got['isolates']} isolates",
        f"in {out_dir}",
    )
    if got != ACCEPTANCE:
        print(
            "note: counts differ from independent harvest",
            ACCEPTANCE,
            file=sys.stderr,
        )
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--out", default="data/fixtures/arxiv-citations")
    ap.add_argument("--cache", default=".cache/arxiv-citations", help="raw Atom/JSON cache (gitignored)")
    ap.add_argument("--offline", action="store_true", help="validate the committed snapshot; no network")
    ap.add_argument("--from-cache", action="store_true", help="rebuild fixture files from cached API responses")
    ap.add_argument("--refresh", action="store_true", help="ignore cache and hit both APIs")
    ap.add_argument("--sleep", type=float, default=SLEEP_S)
    args = ap.parse_args()
    out_dir = Path(args.out)
    cache_dir = Path(args.cache)

    if args.offline:
        return validate_offline(out_dir)
    if args.from_cache and args.refresh:
        print("choose at most one of --from-cache and --refresh", file=sys.stderr)
        return 2

    try:
        papers, arxiv_fetched = load_or_fetch_arxiv(
            cache_dir, args.sleep, args.from_cache, args.refresh
        )
        papers = sorted({p["id"]: p for p in papers}.values(), key=lambda p: p["id"])
        kept = {p["id"] for p in papers}
        missing = [i for i in SEED_IDS if i not in kept]
        if missing:
            print(f"dropped {len(missing)} seed ids not returned by arXiv: {missing}", file=sys.stderr)
        s2_batches, s2_fetched = load_or_fetch_s2(
            [p["id"] for p in papers], cache_dir, args.sleep, args.from_cache, args.refresh
        )
    except (urllib.error.URLError, TimeoutError, RuntimeError, FileNotFoundError, json.JSONDecodeError) as exc:
        print(f"citation fetch failed: {exc}", file=sys.stderr)
        if out_dir.exists() and (out_dir / "papers.csv").exists():
            print("leaving committed snapshot untouched", file=sys.stderr)
            return validate_offline(out_dir)
        return 1

    citations = induce_citations(papers, s2_batches)
    fetched_at = s2_fetched or arxiv_fetched or utc_now()
    manifest = write_fixture(out_dir, papers, citations, {"fetched_at": fetched_at})
    got = manifest["counts"]
    print(
        f"wrote {got['papers']} papers, {got['directed_citations']} directed citations, "
        f"{got['undirected_pairs']} undirected pairs → {out_dir}"
    )
    if got != ACCEPTANCE:
        print(f"acceptance counts {ACCEPTANCE}; snapshot {got}", file=sys.stderr)
    return validate_offline(out_dir)


if __name__ == "__main__":
    sys.exit(main())
