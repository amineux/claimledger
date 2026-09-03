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
\(|S| \le k\), maximizing the **algebraic-connectivity gap** created by the
deletion

\[
\Delta_2(A,B; S)
  \;=\;
  \lambda_2\!\big(L_{\mathrm{sym}}(G_t[V_A \cup V_B] - S)\big)
  \;-\;
  \lambda_2\!\big(L_{\mathrm{sym}}(G_t[V_A \cup V_B])\big).
\]

In words: delete the \(k\) papers that most increase the Fiedler gap between
\(A\) and \(B\) — the papers that were holding the two fields together.

This is a spectral interdiction / most-vital-nodes problem. The discrete
version (most-vital-edges for algebraic connectivity, vertex separators of
minimum order that raise \(\lambda_2\) above a threshold) is NP-hard by
reduction from classical cut and interdiction problems
(see Watanabe–Fiedler, Mosk-Aoyama, and the spectral-interdiction line of
work). We do not pretend to solve it exactly. We ship a **spectral heuristic
with a closed-form score, a planted-cut test, and a COBOL audit trail**.

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
small precisely when a sparse cut exists.

The associated unit eigenvector \(\varphi\) — the **Fiedler vector** — is a
harmonic coordinate on the graph. Its sign partitions \(V\) into the two sides
of the sparsest normalized cut (Cheeger); its magnitude is small on the
vertices that sit on the cut.

Higher eigenvectors \(\varphi^{(2)}, \dots, \varphi^{(k)}\) embed each paper
as a point in \(\mathbb{R}^{k}\). This is the Laplacian-eigenmaps / spectral
clustering construction of Belkin–Niyogi and Ng–Jordan–Weiss. ClaimLedger
stores both the raw coordinates and the row-normalized points
\(u_i = x_i / \|x_i\|\) used for \(k\)-means.

## Temporal caveat

\(G_t\) is a DAG in citation time: an edge \(u \to v\) exists only when
\(\mathrm{year}(u) \ge \mathrm{year}(v)\). The undirected projection forgets
that arrow, which is the right thing for *geometry* and the wrong thing for
*accounting*. The COBOL ledger keeps the arrow (see below). A stricter
temporal analysis would compute a sequence \(\lambda_2(G_t)\) and watch
bridges appear and disappear; the shipped pipeline evaluates a single
snapshot, which is already enough to make the Fiedler picture and the
planted-cut test honest.

## The heuristic (what we actually compute)

Exact maximization of \(\Delta_2(A,B;S)\) requires a Laplacian eigensolve per
candidate set. For \(n \approx 10^3\) and \(k > 1\) that is already a
combinatorial explosion. The first-order (leave-one-out) change in the
Rayleigh quotient when vertex \(v\) and its incident edges are deleted is
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

The shipped score is

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
are reported as the pair \((A,B)\) whose gap the ranking is most responsible
for.

This is a polynomial-time heuristic: one sparse eigensolve plus a linear
scan. It is *not* an FPTAS. It *is* exact on the planted fixture
(two cliques, one liaison): the liaison is rank 1, which the unit test
asserts.

## Complexity notes

| object | cost |
|--------|------|
| CSR build | \(O(n + m)\) |
| \(L_{\mathrm{sym}}\) assemble | \(O(n + m)\) |
| Lanczos, \(s\) steps, full reorth. | \(O(s\, m + s^2 n + s^3)\)  (Jacobi on the \(s \times s\) Ritz matrix) |
| \(k\)-means, \(t\) iters | \(O(t\, n\, k\, d)\) |
| Bridge scan | \(O(n + m)\) |
| Exact \(\arg\max_{|S|\le k} \Delta_2\) | exponential in \(k\); NP-hard |

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
exposes both.

The COBOL programs in `cobol/` are a second, independent implementation of
these posting rules. When `cobc` is absent, `scripts/verify_ledger.py` and
the C++ `verify_journal` assert the same invariants against the same
fixed-width / CSV files.

## Reproducible fixtures

- `data/fixtures/tiny/` — 10 nodes, two cliques, liaison `synth-0007`.
- `data/fixtures/{papers,citations,categories}.csv` — a 10-block stochastic
  block model with planted distant liaisons, generated by
  `scripts/generate_fixtures.py` (seed `20260903`). All ids are `synth-NNNN`.

Run `cmake --build` and `ctest`, then `./build/claimledger --data data/fixtures --out out --docs docs/data`.
The atlas in `docs/` is a pure static read of those JSON artifacts.
