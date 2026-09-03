#!/usr/bin/env python3
"""Deterministic stochastic-block citation corpus for ClaimLedger.

IDs are always synth-NNNN. Titles, authors, and arXiv-like categories are
synthetic — they deliberately do not impersonate real papers.
"""

from __future__ import annotations

import argparse
import csv
import math
import random
from dataclasses import dataclass
from pathlib import Path

CATEGORIES = [
    ("cs.LG", "Machine Learning", "cs"),
    ("cs.AI", "Artificial Intelligence", "cs"),
    ("cs.CL", "Computation and Language", "cs"),
    ("stat.ML", "Machine Learning (Statistics)", "stat"),
    ("math.ST", "Statistics Theory", "math"),
    ("math.PR", "Probability", "math"),
    ("physics.comp-ph", "Computational Physics", "physics"),
    ("cond-mat.dis-nn", "Disordered Systems and Neural Networks", "physics"),
    ("q-bio.QM", "Quantitative Methods", "qbio"),
    ("q-bio.NC", "Neurons and Cognition", "qbio"),
]

# Adjacent fields that share a thin citation membrane (not bridges).
ADJACENT = {
    ("cs", "stat"),
    ("stat", "cs"),
    ("stat", "math"),
    ("math", "stat"),
    ("cs", "qbio"),
    ("qbio", "cs"),
    ("physics", "math"),
    ("math", "physics"),
    ("physics", "qbio"),
    ("qbio", "physics"),
}

ADJ = [
    "synthetic",
    "spectral",
    "variational",
    "measure-theoretic",
    "high-dimensional",
    "non-asymptotic",
    "coarse-grained",
    "equivariant",
    "dissipative",
    "information-limited",
]
NOUNS = [
    "oracles",
    "kernels",
    "ensembles",
    "lattices",
    "flows",
    "currents",
    "partitions",
    "sheaves",
    "credit",
    "traces",
    "occupancy",
    "resolvents",
]
TOPICS = [
    "generalization",
    "mixing",
    "transport",
    "alignment",
    "inference",
    "renormalization",
    "sparsity",
    "stability",
    "identifiability",
    "synchrony",
]
AUTHORS = [
    "Ada Lovelace",
    "Alan Turing",
    "Emmy Noether",
    "Grace Hopper",
    "Sofia Kovalevskaya",
    "Katherine Johnson",
    "Maryam Mirzakhani",
    "Dorothy Vaughan",
    "Andrey Kolmogorov",
    "Harald Cramér",
    "Kiyosi Itô",
    "Shiing-Shen Chern",
    "Chien-Shiung Wu",
    "Lise Meitner",
    "Subrahmanyan Chandrasekhar",
    "Claude Shannon",
    "Lotfi Zadeh",
    "Benoît Mandelbrot",
    "Frances Allen",
    "Barbara Liskov",
]


@dataclass
class Paper:
    id: str
    title: str
    year: int
    category: str
    field: str
    authors: str
    block: int
    is_bridge: bool = False
    bridge_blocks: tuple[int, ...] = ()


def title_for(rng: random.Random, cat: str, bridge: bool) -> str:
    adj = rng.choice(ADJ)
    noun = rng.choice(NOUNS)
    topic = rng.choice(TOPICS)
    if bridge:
        return f"Synthetic liaison: {adj} {noun} for cross-field {topic} ({cat})"
    return f"Synthetic {adj} {noun} and {topic} ({cat})"


def authors_for(rng: random.Random) -> str:
    k = 1 + rng.randrange(3)
    return "; ".join(rng.sample(AUTHORS, k=k))


def write_csv(path: Path, header: list[str], rows: list[list[object]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="", encoding="utf-8") as f:
        w = csv.writer(f, lineterminator="\n")
        w.writerow(header)
        w.writerows(rows)


def generate(n_per_block: int, n_bridges: int, seed: int) -> tuple[list[Paper], list[tuple[str, str, int]]]:
    rng = random.Random(seed)
    papers: list[Paper] = []
    pid = 1
    blocks: list[list[int]] = [[] for _ in CATEGORIES]

    for b, (cat, _name, field) in enumerate(CATEGORIES):
        for _ in range(n_per_block):
            ident = f"synth-{pid:04d}"
            year = rng.randint(2016, 2024)
            papers.append(
                Paper(
                    id=ident,
                    title=title_for(rng, cat, False),
                    year=year,
                    category=cat,
                    field=field,
                    authors=authors_for(rng),
                    block=b,
                )
            )
            blocks[b].append(len(papers) - 1)
            pid += 1

    # Planted liaisons: each belongs to a home block but cites two distant fields.
    distant_pairs = [
        (0, 8),  # cs.LG ↔ q-bio.QM
        (1, 9),  # cs.AI ↔ q-bio.NC
        (2, 6),  # cs.CL ↔ physics.comp-ph
        (3, 7),  # stat.ML ↔ cond-mat
        (4, 9),  # math.ST ↔ q-bio.NC
        (5, 8),  # math.PR ↔ q-bio.QM
        (0, 6),  # cs.LG ↔ physics
        (1, 7),
    ]
    for i in range(n_bridges):
        a, b = distant_pairs[i % len(distant_pairs)]
        home = a if i % 2 == 0 else b
        cat, _name, field = CATEGORIES[home]
        ident = f"synth-{pid:04d}"
        year = rng.randint(2019, 2025)
        papers.append(
            Paper(
                id=ident,
                title=title_for(rng, cat, True),
                year=year,
                category=cat,
                field=field,
                authors=authors_for(rng),
                block=home,
                is_bridge=True,
                bridge_blocks=(a, b),
            )
        )
        blocks[home].append(len(papers) - 1)
        pid += 1

    # Stochastic block citations, temporal DAG (citing year >= cited year).
    p_in = 0.085
    p_adj = 0.012
    p_out = 0.0016
    citations: list[tuple[str, str, int]] = []
    seen: set[tuple[int, int]] = set()

    def maybe_cite(i: int, j: int, p: float) -> None:
        if i == j or (i, j) in seen:
            return
        src, dst = papers[i], papers[j]
        if src.year < dst.year:
            return
        if rng.random() > p:
            return
        seen.add((i, j))
        citations.append((src.id, dst.id, src.year))

    n = len(papers)
    for i in range(n):
        for j in range(n):
            if i == j:
                continue
            pi, pj = papers[i], papers[j]
            if pi.block == pj.block:
                p = p_in
            elif (pi.field, pj.field) in ADJACENT:
                p = p_adj
            else:
                p = p_out
            if pi.is_bridge and pj.block in pi.bridge_blocks:
                p = 0.22
            if pj.is_bridge and pi.block in pj.bridge_blocks:
                p = max(p, 0.10)
            maybe_cite(i, j, p)

    # Preferential extra intra-block edges so degrees are heavy-tailed.
    for b, members in enumerate(blocks):
        if len(members) < 4:
            continue
        for _ in range(len(members) * 2):
            i = rng.choice(members)
            j = rng.choice(members)
            maybe_cite(i, j, 0.55)

    # Ensure connectivity: attach each isolated paper to a same-block neighbor.
    cited_or_citing = {u for u, _, _ in citations} | {v for _, v, _ in citations}
    id_to_idx = {p.id: k for k, p in enumerate(papers)}
    for p in papers:
        if p.id in cited_or_citing:
            continue
        mates = [papers[k] for k in blocks[p.block] if papers[k].id != p.id]
        if not mates:
            continue
        other = rng.choice(mates)
        if p.year >= other.year:
            citations.append((p.id, other.id, p.year))
        else:
            citations.append((other.id, p.id, other.year))

    citations.sort()
    return papers, citations


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", type=Path, default=Path("data/fixtures"))
    ap.add_argument("--per-block", type=int, default=90)
    ap.add_argument("--bridges", type=int, default=28)
    ap.add_argument("--seed", type=int, default=20260903)
    args = ap.parse_args()

    papers, citations = generate(args.per_block, args.bridges, args.seed)
    write_csv(
        args.out / "categories.csv",
        ["id", "name", "group"],
        [[c, n, g] for c, n, g in CATEGORIES],
    )
    write_csv(
        args.out / "papers.csv",
        ["id", "title", "year", "category", "field", "authors"],
        [[p.id, p.title, p.year, p.category, p.field, p.authors] for p in papers],
    )
    write_csv(
        args.out / "citations.csv",
        ["citing", "cited", "year"],
        [list(c) for c in citations],
    )
    n_bridge = sum(1 for p in papers if p.is_bridge)
    print(
        f"wrote {len(papers)} papers ({n_bridge} planted liaisons), "
        f"{len(citations)} citations → {args.out}"
    )


if __name__ == "__main__":
    main()
