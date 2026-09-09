#!/usr/bin/env python3
"""Fetch a small public arXiv metadata slice for ClaimLedger.

This is NOT a citation dump. The committed `citations.csv` is an
author-coupling / cross-list category-coupling graph derived from the
Atom feed. See data/SCHEMA.md and data/fixtures/arxiv-slice/SOURCE.md.
For a real bibliography graph see scripts/fetch_citation_slice.py and
data/fixtures/arxiv-citations/.

arXiv API terms of use: https://info.arxiv.org/help/api/tou.html
- Identify the User-Agent.
- Sleep at least 3 seconds between requests.
- Do not harvest the full corpus. This script asks for a few dozen records.

Usage:
  python3 scripts/fetch_arxiv_slice.py --out data/fixtures/arxiv-slice
  python3 scripts/fetch_arxiv_slice.py --out data/fixtures/arxiv-slice --offline
"""

from __future__ import annotations

import argparse
import csv
import re
import sys
import time
import urllib.error
import urllib.parse
import urllib.request
import xml.etree.ElementTree as ET
from collections import defaultdict
from datetime import datetime, timezone
from pathlib import Path

ATOM = "http://www.w3.org/2005/Atom"
ARXIV = "http://arxiv.org/schemas/atom"
API = "https://export.arxiv.org/api/query"
UA = "ClaimLedger/0.2 (https://github.com/amineux/claimledger; research fixture)"
SLEEP_S = 3.1

# Real public ids fetched via id_list (not invented). Included only if the
# API returns them. 1706.03762 is the well-known transformer paper.
SEED_IDS = [
    "1706.03762",
    "1412.6980",
    "1502.03167",
    "1606.09470",
    "1706.02515",
    "1509.06461",
    "0907.1815",
    "1802.05957",
]

QUERIES = [
    ("id_list", ",".join(SEED_IDS), 8),
    ("search_query", "cat:cs.LG AND cat:stat.ML", 18),
    ("search_query", "cat:math.ST", 16),
]

FIELD_OF = {
    "cs": "cs",
    "stat": "stat",
    "math": "math",
    "physics": "physics",
    "q-bio": "qbio",
    "q-fin": "qfin",
    "eess": "eess",
    "econ": "econ",
    "astro-ph": "physics",
    "cond-mat": "physics",
    "gr-qc": "physics",
    "hep-ex": "physics",
    "hep-lat": "physics",
    "hep-ph": "physics",
    "hep-th": "physics",
    "nlin": "physics",
    "nucl-ex": "physics",
    "nucl-th": "physics",
    "quant-ph": "physics",
}


def atom_text(el, tag: str) -> str:
    node = el.find(f"{{{ATOM}}}{tag}")
    return (node.text or "").strip() if node is not None else ""


def arxiv_id_from_url(url: str) -> str:
    url = url.strip()
    url = re.sub(r"^https?://arxiv\.org/abs/", "", url)
    url = re.sub(r"^http://arxiv\.org/abs/", "", url)
    url = url.split("/")[-1] if url.startswith("http") else url
    url = re.sub(r"v\d+$", "", url)
    return url


def field_of(category: str) -> str:
    if not category:
        return "other"
    if category in FIELD_OF:
        return FIELD_OF[category]
    head = category.split(".")[0]
    return FIELD_OF.get(head, head)


def normalize_author(name: str) -> str:
    name = re.sub(r"\s+", " ", name).strip().lower()
    parts = name.split()
    if len(parts) >= 2:
        return f"{parts[-1]}|{parts[0][0]}"
    return name


def fetch(params: dict[str, str]) -> str:
    q = urllib.parse.urlencode(params)
    req = urllib.request.Request(f"{API}?{q}", headers={"User-Agent": UA})
    with urllib.request.urlopen(req, timeout=60) as resp:
        return resp.read().decode("utf-8")


def parse_feed(xml_text: str) -> list[dict]:
    root = ET.fromstring(xml_text)
    papers = []
    for entry in root.findall(f"{{{ATOM}}}entry"):
        raw_id = atom_text(entry, "id")
        pid = arxiv_id_from_url(raw_id)
        if not pid:
            continue
        title = re.sub(r"\s+", " ", atom_text(entry, "title"))
        published = atom_text(entry, "published")
        year = 0
        if published:
            try:
                year = int(published[:4])
            except ValueError:
                year = 0
        authors = []
        for au in entry.findall(f"{{{ATOM}}}author"):
            nm = atom_text(au, "name")
            if nm:
                authors.append(nm)
        cats = []
        for c in entry.findall(f"{{{ATOM}}}category"):
            term = c.attrib.get("term", "")
            if term:
                cats.append(term)
        primary = ""
        prim = entry.find(f"{{{ARXIV}}}primary_category")
        if prim is not None:
            primary = prim.attrib.get("term", "")
        if not primary and cats:
            primary = cats[0]
        related = []
        for link in entry.findall(f"{{{ATOM}}}link"):
            if link.attrib.get("rel") == "related":
                href = link.attrib.get("href", "")
                rid = arxiv_id_from_url(href)
                if rid and rid != pid:
                    related.append(rid)
        papers.append(
            {
                "id": pid,
                "title": title,
                "year": year,
                "category": primary,
                "field": field_of(primary),
                "authors": authors,
                "categories": cats,
                "related": related,
            }
        )
    return papers


def coupling_edges(papers: list[dict]) -> list[tuple[str, str, int, str]]:
    by_id = {p["id"]: p for p in papers}
    edges: dict[tuple[str, str], tuple[int, str]] = {}

    def add(a: str, b: str, year: int, kind: str) -> None:
        if a == b or a not in by_id or b not in by_id:
            return
        key = (a, b) if a < b else (b, a)
        prev = edges.get(key)
        if prev is None or kind == "related":
            edges[key] = (year, kind)

    authors: dict[str, list[str]] = defaultdict(list)
    for p in papers:
        for au in p["authors"]:
            authors[normalize_author(au)].append(p["id"])
    for ids in authors.values():
        uniq = sorted(set(ids))
        for i, a in enumerate(uniq):
            for b in uniq[i + 1 :]:
                year = max(by_id[a]["year"], by_id[b]["year"])
                add(a, b, year, "shared-author")

    for i, a in enumerate(papers):
        sa = set(a["categories"])
        for b in papers[i + 1 :]:
            if a["category"] == b["category"]:
                continue
            shared = sa.intersection(b["categories"])
            # Couple only across different primaries that actually share a
            # category. Same-query cliques (every cs.LG+stat.ML paper sharing
            # those two labels) are not edges — that would be a complete graph
            # we refuse to dress up as citations.
            if shared:
                year = max(a["year"], b["year"])
                add(a["id"], b["id"], year, "cross-list")

    for p in papers:
        for rid in p["related"]:
            if rid in by_id:
                add(p["id"], rid, max(p["year"], by_id[rid]["year"]), "related")

    out = []
    for (a, b), (year, kind) in sorted(edges.items()):
        out.append((a, b, year, kind))
    return out


def write_slice(out_dir: Path, papers: list[dict], edges: list[tuple[str, str, int, str]], meta: dict) -> None:
    out_dir.mkdir(parents=True, exist_ok=True)
    papers = sorted({p["id"]: p for p in papers}.values(), key=lambda p: p["id"])
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
        for a, b, year, _kind in edges:
            w.writerow([a, b, year])
    cats = {}
    for p in papers:
        for c in p["categories"] or [p["category"]]:
            if c and c not in cats:
                cats[c] = field_of(c)
    with (out_dir / "categories.csv").open("w", newline="", encoding="utf-8") as fh:
        w = csv.writer(fh)
        w.writerow(["id", "name", "group"])
        for cid, group in sorted(cats.items()):
            w.writerow([cid, cid, group])
    kinds = defaultdict(int)
    for *_, kind in edges:
        kinds[kind] += 1
    source = out_dir / "SOURCE.md"
    source.write_text(
        "\n".join(
            [
                "# arXiv slice (public metadata, committed snapshot)",
                "",
                "Fetched via the [arXiv API](https://info.arxiv.org/help/api/user-manual.html).",
                "Terms: https://info.arxiv.org/help/api/tou.html",
                "",
                f"- fetched_at (UTC): {meta.get('fetched_at', '')}",
                f"- user-agent: `{UA}`",
                f"- papers: {len(papers)}",
                f"- coupling edges: {len(edges)} ({dict(kinds)})",
                "",
                "## Queries",
                "",
                *[f"- `{q[0]}={q[1]}` (max_results={q[2]})" for q in QUERIES],
                "",
                "## Edge rule (not citations)",
                "",
                "`citations.csv` keeps the pipeline column names (`citing,cited,year`)",
                "but the rows are **not** bibliographic citations. An undirected edge",
                "exists when any of the following hold:",
                "",
                "1. **shared-author** — both papers list the same normalized author",
                "   (`lastname|first-initial`).",
                "2. **cross-list** — different primary categories with nonempty category overlap.",
                "3. **related** — the Atom feed supplied a `rel=related` link.",
                "",
                "Each pair is written once, lexicographic order, `year = max(year_a, year_b)`.",
                "The COBOL ledger will still post DR/CR on these rows; read that as",
                "coupling mass, not a real citation of record.",
                "",
                "Do not treat this fixture as a citation graph. The default demo",
                "corpus remains `data/fixtures/` (`synth-NNNN`).",
                "",
            ]
        )
        + "\n",
        encoding="utf-8",
    )


def validate_offline(out_dir: Path) -> int:
    papers = out_dir / "papers.csv"
    cites = out_dir / "citations.csv"
    cats = out_dir / "categories.csv"
    src = out_dir / "SOURCE.md"
    for p in (papers, cites, cats, src):
        if not p.exists():
            print(f"missing {p}", file=sys.stderr)
            return 1
    with papers.open(encoding="utf-8") as fh:
        rows = list(csv.DictReader(fh))
    if len(rows) < 8:
        print(f"expected a few dozen (or at least 8) papers, got {len(rows)}", file=sys.stderr)
        return 1
    ids = [r["id"] for r in rows]
    if any(i.startswith("synth-") for i in ids):
        print("arxiv-slice must use real arXiv ids, not synth-NNNN", file=sys.stderr)
        return 1
    if "1706.03762" not in ids:
        print("warning: 1706.03762 not in snapshot (ok if the API omitted it)", file=sys.stderr)
    print(f"offline ok: {len(rows)} papers in {out_dir}")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--out", default="data/fixtures/arxiv-slice")
    ap.add_argument("--offline", action="store_true", help="validate the committed snapshot")
    ap.add_argument("--sleep", type=float, default=SLEEP_S)
    args = ap.parse_args()
    out_dir = Path(args.out)
    if args.offline:
        return validate_offline(out_dir)

    papers: list[dict] = []
    for i, (kind, value, nmax) in enumerate(QUERIES):
        if i:
            time.sleep(args.sleep)
        params = {"start": "0", "max_results": str(nmax)}
        params[kind] = value
        print(f"GET {kind}={value} max={nmax}", file=sys.stderr)
        try:
            xml_text = fetch(params)
        except (urllib.error.URLError, TimeoutError) as exc:
            print(f"arxiv API failed: {exc}", file=sys.stderr)
            if out_dir.exists() and (out_dir / "papers.csv").exists():
                print("leaving committed snapshot untouched", file=sys.stderr)
                return validate_offline(out_dir)
            return 1
        batch = parse_feed(xml_text)
        print(f"  got {len(batch)} entries", file=sys.stderr)
        papers.extend(batch)

    # de-dupe
    uniq = {}
    for p in papers:
        uniq[p["id"]] = p
    papers = list(uniq.values())
    edges = coupling_edges(papers)
    write_slice(
        out_dir,
        papers,
        edges,
        {"fetched_at": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")},
    )
    print(f"wrote {len(papers)} papers, {len(edges)} coupling edges → {out_dir}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
