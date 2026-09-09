# Spectral bridges in a temporal citation graph

## The question

Science is not a pile of papers. It is a **flow of credit** across a growing
directed graph whose vertices are documents and whose edges are citations.
Communities — arXiv categories, fields, invisible colleges — appear as regions
where that flow is dense. Interdisciplinarity is not “a paper with two
keywords.” It is the existence of a thin cut: a handful of documents whose
removal would pull two literatures apart.

**Problem (combinatorial).**
Let \(G_t = (V, E_t)\) be an undirected projection of a citation graph at time
\(t\), with a field partition \(V = \bigsqcup_{c \in C} V_c\). For a pair of
fields \(A, B \in C\) and a budget \(k\), find a vertex set \(S \subset V\),
\(|S| \le k\), maximizing the **drop in algebraic connectivity**

\[
\delta\lambda_2(A,B; S)
  \;=\;
  \lambda_2\!\big(L_{\mathrm{sym}}(G_t[V_A \cup V_B])\big)
  \;-\;
  \lambda_2\!\big(L_{\mathrm{sym}}(G_t[V_A \cup V_B] - S)\big).
\]

In words: delete the \(k\) papers that most *drop* the Fiedler gap of the
two-field graph — the papers that were holding \(A\) and \(B\) together.
(v1 wrote the difference the other way around while saying “maximize”;
the English and the combinatorics agree on a drop. Exported `delta_lambda2`
is \(\delta\lambda_2\).)

This is a spectral interdiction / most-vital-nodes problem. The discrete
version (most-vital-edges for algebraic connectivity, vertex separators of
minimum order that raise a gap above a threshold) is NP-hard by reduction
from classical cut and interdiction problems (Watanabe–Fiedler, Mosk-Aoyama,
and the spectral-interdiction line of work). For \(k=1\) we now **solve it
exactly on a shortlist**: rebuild \(L_{\mathrm{sym}}\) without \(v\),
recompute \(\lambda_2\), report \(\delta\lambda_2(v)\). For \(k>1\) we still
do not pretend to have an FPTAS.

## Graph and Laplacian

Citations are directed. Spectral geometry wants a symmetric operator, so the
core builds the undirected multigraph \(A = A_{\rightarrow} + A_{\rightarrow}^\top\)
(self-loops dropped). Let \(d_i = \sum_j A_{ij}\) and \(D = \mathrm{diag}(d)\).
The **symmetric normalized Laplacian** is

\[
L_{\mathrm{sym}}
  \;=\;
  I - D^{-1/2} A D^{-1/2},
\]

with the convention that an isolated vertex contributes a zero row (it is a
trivial kernel mode, not a community). \(L_{\mathrm{sym}}\) is real symmetric
and positive semidefinite; its spectrum lives in \([0, 2]\).

The Rayleigh quotient

\[
R(x)
  \;=\;
  \frac{x^\top L_{\mathrm{sym}} x}{x^\top x}
  \;=\;
  \frac{1}{2\|x\|^2}
  \sum_{i,j} A_{ij}
  \left(
    \frac{x_i}{\sqrt{d_i}} - \frac{x_j}{\sqrt{d_j}}
  \right)^2
\]

is the Dirichlet energy of the rescaled potential \(D^{-1/2}x\). The smallest
eigenvalue is \(\lambda_0 = 0\), with eigenvector \(D^{1/2}\mathbf{1}\) on each
connected component. The next eigenvalue \(\lambda_2\) (we keep the classical
Fiedler numbering, writing \(\lambda_2\) for the first nontrivial value even
when the kernel has multiplicity one) is the **algebraic connectivity**. It is
small precisely when a sparse cut exists. If the graph is disconnected, the
kernel has multiplicity \(\ge 2\) and \(\lambda_2 = 0\).

The associated unit eigenvector \(\varphi\) — the **Fiedler vector** — is a
harmonic coordinate on the graph. Its sign partitions \(V\) into the two sides
of the sparsest normalized cut (Cheeger); its magnitude is small on the
vertices that sit on the cut.

Higher eigenvectors \(\varphi^{(2)}, \dots, \varphi^{(k)}\) embed each paper
as a point in \(\mathbb{R}^{k}\). This is the Laplacian-eigenmaps / spectral
clustering construction of Belkin–Niyogi and Ng–Jordan–Weiss. ClaimLedger
stores both the raw coordinates and the row-normalized points
\(u_i = x_i / \|x_i\|\) used for \(k\)-means. The atlas uses the first two
as a Fiedler plane and the first three (the third also exported as `z`)
for the orbiting scatter.

## Temporal slices

\(G_t\) is a DAG in citation time: an edge \(u \to v\) exists only when
\(\mathrm{year}(u) \ge \mathrm{year}(v)\). The undirected projection forgets
that arrow, which is the right thing for *geometry* and the wrong thing for
*accounting*. The COBOL ledger keeps the arrow.

v2 evaluates the sequence. For each distinct paper year \(t\),
`claimledger timeline` (and `run` by default) builds the cumulative graph
on papers with year \(\le t\) and citations with citing year \(\le t\),
then records \(\{t, n, m, \lambda_2, \text{top heuristic bridge}\}\). That
is one eigensolve per year — a decade of the 928-node corpus is cheap.
Leave-one-out is *not* repeated on every slice; the expensive Δλ₂ lives
on the final snapshot. The atlas slider restyles nodes that have not yet
appeared and updates the stats strip from `timeline.json`.

## The heuristic (pre-filter)

Exact maximization of \(\delta\lambda_2(A,B;S)\) for \(|S|>1\) is still
combinatorial. The first-order (leave-one-out) change in the Rayleigh
quotient when vertex \(v\) and its incident edges are deleted is
proportional to the **cross-field Dirichlet energy** of \(v\):

\[
\mathcal{R}_{\mathrm{cross}}(v)
  \;=\;
  \frac12
  \sum_{u \sim v \atop c(u) \ne c(v)}
  A_{vu}
  \left(
    \frac{\varphi_v}{\sqrt{d_v}} - \frac{\varphi_u}{\sqrt{d_u}}
  \right)^2.
\]

A paper that is cited across a deep Fiedler gap contributes a lot; a paper
whose neighbors all live on the same side contributes almost nothing.

Two more observables stop the score from collapsing onto high-degree hubs
that happen to have one foreign neighbor:

1. **Participation entropy.** Let \(h_v(c)\) be the number of neighbors of
   \(v\) in field \(c\) (plus a self-count so isolates are well-defined).
   \[
   H(v) = -\sum_c p_v(c)\log p_v(c),
   \qquad
   p_v(c) = h_v(c)\big/\textstyle\sum_{c'} h_v(c').
   \]
   Shannon entropy is zero on a monogamous paper and large on a true liaison.

2. **Cut proximity.** \(1 / (1 + 8|\varphi_v|)\). Bridges of the global cut
   sit near \(\varphi = 0\).

The shipped pre-filter score is

\[
\mathrm{bridge}(v)
  \;=\;
  \mathcal{R}_{\mathrm{cross}}(v)
  \,(1 + H(v))
  \,(0.35 + 0.65\, f_{\mathrm{cross}}(v))
  \,(0.25 + 0.75\, \pi_{\mathrm{cut}}(v)),
\]

where \(f_{\mathrm{cross}}\) is the fraction of neighbors in a foreign field.
The two fields with the most negative / most positive mean Fiedler coordinate
are the pair \((A,B)\) on which leave-one-out is evaluated.

## Leave-one-out (what is new)

For each candidate \(v\) in the shortlist:

1. Restrict to \(G[V_A \cup V_B]\) (the Fiedler pair). Vertices outside the
   pair cannot interdict \((A,B)\); their \(\delta\lambda_2\) is 0.
2. Assemble \(L_{\mathrm{sym}}\) of that graph **without** \(v\) by a
   **degree deflation**: subtract the deleted adjacency from neighbor
   degrees and remap the remaining \(n-1\) indices. This is *not* a Kron
   / Schur complement — Kron reduction would add a clique among the
   neighbors of \(v\), which is a different graph. The deflated assembly
   is checked, vertex-by-vertex on the tiny fixture, against a full
   `Graph::without_vertex` rebuild.
3. Recompute \(\lambda_2\) with the same Lanczos eigensolver.
   Disconnected remainders have \(\lambda_2 = 0\) (kernel multiplicity
   \(\ge 2\); values \(< 10^{-8}\) snap to zero).
4. \(\delta\lambda_2(v) = \lambda_2(G[A\cup B]) - \lambda_2(G[A\cup B]-v)\).

Default policy:

| corpus | shortlist | LOO |
|--------|-----------|-----|
| tiny (n = 10) | all vertices | all |
| 928-node fixture | heuristic top 64 | those 64 |
| `--no-loo` | — | skip; emit the heuristic ranking |

The emitted ranking is \(\delta\lambda_2\) descending, heuristic score as
tie-break. On the 10-node planted-cut fixture the liaison `synth-0007`
has the unique largest Δλ₂ (it is the only cut vertex of \(G[\mathrm{cs}\cup\mathrm{qbio}]\)).

## Complexity notes

| object | cost |
|--------|------|
| CSR build | \(O(n + m)\) |
| \(L_{\mathrm{sym}}\) assemble | \(O(n + m)\) |
| Lanczos, \(s\) steps, full reorth. | \(O(s\, m + s^2 n + s^3)\)  (Jacobi on the \(s \times s\) Ritz matrix) |
| \(k\)-means, \(t\) iters | \(O(t\, n\, k\, d)\) |
| Bridge scan (heuristic) | \(O(n + m)\) |
| Degree-deflated \(L_{\mathrm{sym}}\) except \(v\) | \(O(n + m)\) |
| Leave-one-out, \(P\) candidates | \(P \cdot O(s\, m + s^2 n + s^3)\) with \(P=\min(n,64)\) |
| Timeline, \(T\) distinct years | \(T\) eigensolves + \(T\) heuristic scans |
| Exact \(\arg\max_{|S|\le k} \delta\lambda_2\) for \(k>1\) | exponential in \(k\); NP-hard |

The expensive term on the 928-node corpus is \(64\) small Lanczos runs, not
the embedding. \(n \approx 10^3\), \(s = 64\), \(m \sim 10^4\) is laptop-seconds.

The Lanczos implementation is self-contained (no Eigen). It builds a Krylov
basis with double reorthogonalization (Daniel–Gragg–Kaufman–Stewart), solves
the projected tridiagonal problem by Jacobi rotations, and returns residual
norms \(\|Lq - \lambda q\|\). For the fixture graphs, \(s = \min(n, \max(4k+16, 64))\)
recovers \(\lambda_2\) of \(C_6\) and \(K_5\) to \(10^{-3}\) or better
(see `tests/test_lanczos.cpp`).

## Accounting of the same edges

Every directed citation is also a journal entry:

```
DR  citing paper     1.00   intellectual debt
CR  cited  paper     1.00   intellectual capital
```

The trial balance of a closed ledger is identically zero: science does not
create or destroy credit, it transfers it. A spectral bridge with a large
*credit* balance is a paper the foreign field keeps citing; a bridge with a
large *debit* balance is a paper that imported the foreign field. The atlas
exposes both, plus a DR/CR bar against field peers.

The COBOL programs in `cobol/` are a second, independent implementation of
these posting rules. When `cobc` is absent, `scripts/verify_ledger.py` and
the C++ `verify_journal` assert the same invariants against the same
fixed-width / CSV files.

## Reproducible fixtures

- `data/fixtures/tiny/` — 10 nodes, two cliques, liaison `synth-0007`.
- `data/fixtures/{papers,citations,categories}.csv` — a 10-block stochastic
  block model with planted distant liaisons, generated by
  `scripts/generate_fixtures.py` (seed `20260903`). All ids are `synth-NNNN`.
- `data/fixtures/arxiv-citations/` — 80 real arXiv records and 236
  directed bibliographic citations committed from
  `scripts/fetch_citation_slice.py`. Edges are Semantic Scholar
  `references.externalIds.ArXiv` induced on that closed set, not
  author/category coupling. CI never hits the network (`--offline`).
- `data/fixtures/arxiv-slice/` — 42 real arXiv records (including
  `1706.03762`) committed from `scripts/fetch_arxiv_slice.py`. Edges are
  shared-author / cross-list **coupling**, labeled in SCHEMA.md. CI never
  hits the network (`--offline`).

Run `cmake --build` and `ctest`, then `./build/claimledger --data data/fixtures --out out --docs docs/data`.
The atlas in `docs/` is a pure static read of those JSON artifacts.
