#include "claimledger/lanczos.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <random>
#include <stdexcept>

namespace claimledger {
namespace {

constexpr double kTiny = 1e-14;

double dot(std::span<const double> a, std::span<const double> b) {
    double s = 0.0;
    for (std::size_t i = 0; i < a.size(); ++i) {
        s += a[i] * b[i];
    }
    return s;
}

double nrm2(std::span<const double> a) {
    return std::sqrt(dot(a, a));
}

void axpy(double alpha, std::span<const double> x, std::span<double> y) {
    for (std::size_t i = 0; i < x.size(); ++i) {
        y[i] += alpha * x[i];
    }
}

void scale(std::span<double> x, double s) {
    for (double& v : x) {
        v *= s;
    }
}

void fix_sign(std::span<double> v) {
    auto it = std::max_element(v.begin(), v.end(), [](double a, double b) {
        return std::abs(a) < std::abs(b);
    });
    if (it != v.end() && *it < 0.0) {
        scale(v, -1.0);
    }
}

}  // namespace

void jacobi_symmetric(std::vector<double>& A, int n, std::vector<double>& evals,
                      std::vector<double>& evecs, double tol, int max_sweeps) {
    evals.assign(static_cast<std::size_t>(n), 0.0);
    evecs.assign(static_cast<std::size_t>(n * n), 0.0);
    for (int i = 0; i < n; ++i) {
        evecs[static_cast<std::size_t>(i * n + i)] = 1.0;
    }

    auto at = [&](int i, int j) -> double& { return A[static_cast<std::size_t>(i * n + j)]; };

    for (int sweep = 0; sweep < max_sweeps; ++sweep) {
        double off = 0.0;
        for (int i = 0; i < n; ++i) {
            for (int j = i + 1; j < n; ++j) {
                off += at(i, j) * at(i, j);
            }
        }
        if (std::sqrt(2.0 * off) < tol * static_cast<double>(n)) {
            break;
        }
        for (int p = 0; p < n; ++p) {
            for (int q = p + 1; q < n; ++q) {
                const double apq = at(p, q);
                if (std::abs(apq) < tol) {
                    continue;
                }
                const double app = at(p, p);
                const double aqq = at(q, q);
                const double tau = (aqq - app) / (2.0 * apq);
                const double t = (tau >= 0.0 ? 1.0 : -1.0) / (std::abs(tau) + std::sqrt(1.0 + tau * tau));
                const double c = 1.0 / std::sqrt(1.0 + t * t);
                const double s = t * c;

                at(p, p) = app - t * apq;
                at(q, q) = aqq + t * apq;
                at(p, q) = 0.0;
                at(q, p) = 0.0;

                for (int k = 0; k < n; ++k) {
                    if (k == p || k == q) {
                        continue;
                    }
                    const double akp = at(k, p);
                    const double akq = at(k, q);
                    at(k, p) = c * akp - s * akq;
                    at(p, k) = at(k, p);
                    at(k, q) = s * akp + c * akq;
                    at(q, k) = at(k, q);
                }
                for (int k = 0; k < n; ++k) {
                    const double vkp = evecs[static_cast<std::size_t>(k * n + p)];
                    const double vkq = evecs[static_cast<std::size_t>(k * n + q)];
                    evecs[static_cast<std::size_t>(k * n + p)] = c * vkp - s * vkq;
                    evecs[static_cast<std::size_t>(k * n + q)] = s * vkp + c * vkq;
                }
            }
        }
    }
    for (int i = 0; i < n; ++i) {
        evals[static_cast<std::size_t>(i)] = A[static_cast<std::size_t>(i * n + i)];
    }
}

LanczosResult lanczos_smallest(const SparseMatrix& A, const LanczosOptions& opt) {
    if (A.n <= 0) {
        throw std::runtime_error("lanczos: empty matrix");
    }
    const int n = A.n;
    const int want = std::max(1, std::min(opt.k, n));
    int m = opt.max_steps;
    if (m <= 0) {
        m = std::min(n, std::max(4 * want + 16, 64));
    }
    m = std::min(n, std::max(m, want + 2));

    std::mt19937 rng(opt.seed);
    std::normal_distribution<double> nd(0.0, 1.0);

    std::vector<double> q0(static_cast<std::size_t>(n), 0.0);
    std::vector<double> q1(static_cast<std::size_t>(n), 0.0);
    for (double& v : q1) {
        v = nd(rng);
    }
    double nq = nrm2(q1);
    if (nq < kTiny) {
        q1[0] = 1.0;
        nq = 1.0;
    }
    scale(q1, 1.0 / nq);

    // Q stored column-major: Q[:, j] at offset j*n
    std::vector<double> Q(static_cast<std::size_t>(n) * static_cast<std::size_t>(m), 0.0);
    std::vector<double> alpha(static_cast<std::size_t>(m), 0.0);
    std::vector<double> beta(static_cast<std::size_t>(m), 0.0);

    std::copy(q1.begin(), q1.end(), Q.begin());

    std::vector<double> w(static_cast<std::size_t>(n), 0.0);
    int built = 0;
    double beta_prev = 0.0;

    for (int j = 0; j < m; ++j) {
        std::span<const double> qj(Q.data() + static_cast<std::size_t>(j) * static_cast<std::size_t>(n),
                                   static_cast<std::size_t>(n));
        A.multiply(qj, w);
        if (j > 0) {
            std::span<const double> qjm(Q.data() + static_cast<std::size_t>(j - 1) * static_cast<std::size_t>(n),
                                        static_cast<std::size_t>(n));
            axpy(-beta_prev, qjm, w);
        }
        const double aj = dot(w, qj);
        alpha[static_cast<std::size_t>(j)] = aj;
        axpy(-aj, qj, w);

        // Full reorthogonalization against all previous Lanczos vectors.
        for (int i = 0; i <= j; ++i) {
            std::span<const double> qi(Q.data() + static_cast<std::size_t>(i) * static_cast<std::size_t>(n),
                                       static_cast<std::size_t>(n));
            const double coeff = dot(w, qi);
            axpy(-coeff, qi, w);
        }
        // Second pass (Daniel–Gragg–Kaufman–Stewart) for stubborn cancellation.
        for (int i = 0; i <= j; ++i) {
            std::span<const double> qi(Q.data() + static_cast<std::size_t>(i) * static_cast<std::size_t>(n),
                                       static_cast<std::size_t>(n));
            const double coeff = dot(w, qi);
            axpy(-coeff, qi, w);
        }

        const double bj = nrm2(w);
        built = j + 1;
        if (bj >= 1e-12) {
            beta[static_cast<std::size_t>(j)] = bj;
            if (j + 1 < m) {
                scale(w, 1.0 / bj);
                std::copy(w.begin(), w.end(),
                          Q.begin() + static_cast<std::size_t>(j + 1) * static_cast<std::size_t>(n));
            }
            beta_prev = bj;
            continue;
        }

        // Breakdown: an invariant subspace was found. Start a fresh
        // direction orthogonal to Q so we still see repeated eigenvalues
        // (e.g. the two 0.5 modes of C_6).
        beta[static_cast<std::size_t>(j)] = 0.0;
        beta_prev = 0.0;
        if (j + 1 >= m) {
            break;
        }
        bool spawned = false;
        for (int attempt = 0; attempt < 8 && !spawned; ++attempt) {
            for (double& v : w) {
                v = nd(rng);
            }
            for (int i = 0; i <= j; ++i) {
                std::span<const double> qi(Q.data() + static_cast<std::size_t>(i) * static_cast<std::size_t>(n),
                                           static_cast<std::size_t>(n));
                axpy(-dot(w, qi), qi, w);
            }
            const double nw = nrm2(w);
            if (nw > 1e-12) {
                scale(w, 1.0 / nw);
                std::copy(w.begin(), w.end(),
                          Q.begin() + static_cast<std::size_t>(j + 1) * static_cast<std::size_t>(n));
                spawned = true;
            }
        }
        if (!spawned) {
            break;
        }
    }

    // Build dense T (tridiagonal) and solve the Rayleigh–Ritz problem.
    std::vector<double> T(static_cast<std::size_t>(built * built), 0.0);
    for (int i = 0; i < built; ++i) {
        T[static_cast<std::size_t>(i * built + i)] = alpha[static_cast<std::size_t>(i)];
        if (i + 1 < built) {
            const double b = beta[static_cast<std::size_t>(i)];
            T[static_cast<std::size_t>(i * built + (i + 1))] = b;
            T[static_cast<std::size_t>((i + 1) * built + i)] = b;
        }
    }
    std::vector<double> evals;
    std::vector<double> evecs;
    jacobi_symmetric(T, built, evals, evecs);

    std::vector<int> order(static_cast<std::size_t>(built));
    std::iota(order.begin(), order.end(), 0);
    std::stable_sort(order.begin(), order.end(), [&](int a, int b) {
        return evals[static_cast<std::size_t>(a)] < evals[static_cast<std::size_t>(b)];
    });

    LanczosResult result;
    result.steps = built;
    result.max_residual = 0.0;

    std::vector<double> Ax(static_cast<std::size_t>(n), 0.0);
    for (int oi = 0; oi < built; ++oi) {
        const int col = order[static_cast<std::size_t>(oi)];
        const double lam = evals[static_cast<std::size_t>(col)];
        if (opt.skip_trivial && std::abs(lam) < 1e-8) {
            continue;
        }
        EigenPair pair;
        pair.value = lam;
        pair.vector.assign(static_cast<std::size_t>(n), 0.0);
        for (int j = 0; j < built; ++j) {
            const double yj = evecs[static_cast<std::size_t>(j * built + col)];
            const double* qcol = Q.data() + static_cast<std::size_t>(j) * static_cast<std::size_t>(n);
            for (int i = 0; i < n; ++i) {
                pair.vector[static_cast<std::size_t>(i)] += yj * qcol[i];
            }
        }
        const double vn = nrm2(pair.vector);
        if (vn > kTiny) {
            scale(pair.vector, 1.0 / vn);
        }
        fix_sign(pair.vector);
        A.multiply(pair.vector, Ax);
        double r2 = 0.0;
        for (int i = 0; i < n; ++i) {
            const double ri = Ax[static_cast<std::size_t>(i)] - lam * pair.vector[static_cast<std::size_t>(i)];
            r2 += ri * ri;
        }
        const double resid = std::sqrt(r2);
        result.max_residual = std::max(result.max_residual, resid);
        if (resid < opt.residual_tol * (1.0 + std::abs(lam))) {
            ++result.converged;
        }
        result.pairs.push_back(std::move(pair));
        if (static_cast<int>(result.pairs.size()) >= want) {
            break;
        }
    }
    return result;
}

}  // namespace claimledger
