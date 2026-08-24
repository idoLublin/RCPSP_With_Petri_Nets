#ifndef THETA_TREE_H
#define THETA_TREE_H

#include <vector>
#include <algorithm>
#include <climits>
#include <utility>

// ─────────────────────────────────────────────────────────────────────────────
// Vilím Θ-tree Earliest-Completion-Time (ECT) bound for a cumulative resource.
//
// Given a set of activities that must all run on one resource of capacity C,
// each with an earliest-start bound est_i and an energy (= duration_i * demand_i,
// > 0), this returns
//
//     ceil( Env(root) / C ),   Env(root) = max_{Ω ⊆ leaves} ( C·min_{i∈Ω} est_i
//                                                              + Σ_{i∈Ω} energy_i )
//
// which is a valid lower bound on the time by which every given activity can
// possibly finish on the resource — respecting BOTH each activity's est and the
// shared capacity C (strictly tighter than the plain Σenergy/C energy bound,
// which is the special case Ω = all with a common est of 0).
//
// Construction (Vilím 2008, "Filtering algorithms for the unary resource
// constraint"): sort leaves ascending by est, pad up to a power of two with
// empty leaves (energy 0, envelope -∞), then combine bottom-up with
//     energy(parent)   = energy(L) + energy(R)
//     envelope(parent) = max( envelope(R), envelope(L) + energy(R) )
// where L is the earlier-est (left) child and R the later-est (right) child.
//
// Pure: depends on no globals, so it can be unit-tested in isolation.
// leaves are (est, energy) pairs. C is the capacity (> 0).
// ─────────────────────────────────────────────────────────────────────────────
inline long thetaTreeECT(const std::vector<std::pair<long, long>>& leaves, long C) {
    if (leaves.empty() || C <= 0) return 0;

    std::vector<std::pair<long, long>> s(leaves);          // (est, energy)
    std::sort(s.begin(), s.end(),
              [](const std::pair<long, long>& a, const std::pair<long, long>& b) {
                  return a.first < b.first;                 // ascending est
              });

    const int n = static_cast<int>(s.size());
    int m = 1;
    while (m < n) m <<= 1;                                  // next power of two

    const long NEG = LONG_MIN / 4;                          // empty-leaf envelope = -∞
    std::vector<long> energy(2 * m, 0);
    std::vector<long> env(2 * m, NEG);

    // Leaves occupy [m, m+n); the ascending sort puts real leaves left, empties right.
    for (int i = 0; i < n; ++i) {
        energy[m + i] = s[i].second;
        env[m + i]    = s[i].first * C + s[i].second;
    }

    for (int i = m - 1; i >= 1; --i) {
        energy[i] = energy[2 * i] + energy[2 * i + 1];
        const long envR     = env[2 * i + 1];
        const long envLplus = (env[2 * i] <= NEG / 2) ? NEG : env[2 * i] + energy[2 * i + 1];
        env[i] = std::max(envR, envLplus);
    }

    const long envRoot = env[1];
    if (envRoot <= NEG / 2) return 0;                       // all leaves empty
    return (envRoot + C - 1) / C;                           // ceil(envRoot / C)
}

#endif // THETA_TREE_H
