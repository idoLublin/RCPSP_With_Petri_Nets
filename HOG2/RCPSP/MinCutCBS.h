#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// Pragmatic min-cut / energetic resource lower bound for a CBS node
// (RCPSP_CBS_MINCUT=1). This is the buildable, per-node-affordable distillation of
// the resource min-cut idea (cf. Möhring, Schulz, Stork & Uetz, "Solving Project
// Scheduling Problems by Minimum Cut Computations", Mgmt. Sci. 49(3), 2003 — whose
// FULL method is a heavy Lagrangian subgradient over a time-indexed ILP; here we take
// only the admissible resource-feasibility min-cut and use it as a bound floor).
//
// Idea: relax to a PREEMPTIVE, divisible-energy problem over each activity's time
// window [ES(i), LF_C(i)]. For resource k, activity i must place r_ik * d_i units of
// "energy" somewhere in its window; each elementary time interval offers R_k * width
// capacity. A candidate makespan C is feasible iff, for every resource, a max-flow
// saturates the total energy (max-flow = min-cut). Binary-search the smallest feasible
// C; that is an admissible ABSOLUTE makespan lower bound.
//
// Admissible: any REAL schedule with makespan <= C induces a feasible flow (place each
// activity's energy in the intervals it actually occupies; capacities hold because the
// real schedule respects R_k). So flow-infeasible(C) => optimum > C. The smallest
// flow-feasible C is therefore <= optimum. ES(i) = start_times[i] is an admissible
// earliest start (CBS only pushes starts forward); LF_C(i) = C - reach[i] + d_i comes
// from the static precedence tail. Dominates the workload bound (its single-interval
// special case). Bound-only: it can only tighten h, never change which goals are legal.
// ─────────────────────────────────────────────────────────────────────────────
#include <vector>
#include <algorithm>
#include <climits>
#include "Globals.h"

namespace mincut_detail {
// Compact Dinic max-flow (long capacities).
struct Dinic {
    struct E { int to, rev; long cap; };
    std::vector<std::vector<E>> g;
    std::vector<int> level, it;
    int n = 0;
    void init(int n_) { n = n_; g.assign(n, {}); }
    void add(int u, int v, long c) {
        g[u].push_back({v, (int)g[v].size(), c});
        g[v].push_back({u, (int)g[u].size() - 1, 0});
    }
    bool bfs(int s, int t) {
        level.assign(n, -1);
        std::vector<int> q; q.reserve(n); level[s] = 0; q.push_back(s);
        for (size_t h = 0; h < q.size(); ++h) {
            int u = q[h];
            for (auto& e : g[u]) if (e.cap > 0 && level[e.to] < 0) { level[e.to] = level[u] + 1; q.push_back(e.to); }
        }
        return level[t] >= 0;
    }
    long dfs(int u, int t, long f) {
        if (u == t) return f;
        for (int& i = it[u]; i < (int)g[u].size(); ++i) {
            E& e = g[u][i];
            if (e.cap > 0 && level[e.to] == level[u] + 1) {
                long d = dfs(e.to, t, std::min(f, e.cap));
                if (d > 0) { e.cap -= d; g[e.to][e.rev].cap += d; return d; }
            }
        }
        return 0;
    }
    long maxflow(int s, int t) {
        long flow = 0;
        while (bfs(s, t)) { it.assign(n, 0); long f; while ((f = dfs(s, t, LLONG_MAX / 4)) > 0) flow += f; }
        return flow;
    }
};
} // namespace mincut_detail

template<short N>
inline double minCutMakespanBound_CBS(const RCPSPState_CBS<N>& s) {
    const int n = (int)RCPSPex.activities.size();
    if (n == 0) return 0.0;

    // reach[i] = longest duration-sum path from start(i) to project completion,
    // INCLUDING d_i (static precedence tail). LS_C(i) = C - reach[i].
    std::vector<int> reach(n, -1);
    {
        std::vector<int> stack;
        for (int i0 = 0; i0 < n; ++i0) {
            if (reach[i0] >= 0) continue;
            stack.push_back(i0);
            while (!stack.empty()) {
                int u = stack.back();
                if (reach[u] >= 0) { stack.pop_back(); continue; }
                bool ready = true; int best = 0;
                for (short succ1 : RCPSPex.dependencies[u]) {
                    int v = (int)succ1 - 1;
                    if (v < 0 || v >= n) continue;
                    if (reach[v] < 0) { ready = false; stack.push_back(v); }
                    else if (reach[v] > best) best = reach[v];
                }
                if (ready) { reach[u] = (int)RCPSPex.activities[u].duration + best; stack.pop_back(); }
            }
        }
    }

    int lo = s.start_times[g_sink_id];            // current relaxed makespan = valid LB
    if (lo < 0) lo = 0;
    int sumd = 0, maxes = 0;
    for (int i = 0; i < n; ++i) {
        int d = (int)RCPSPex.activities[i].duration; if (d > 0) sumd += d;
        if (s.start_times[i] > maxes) maxes = s.start_times[i];
    }
    int hi = maxes + sumd + 1;                    // always window-feasible
    if (hi < lo) hi = lo;

    // Flow-feasibility of makespan C (preemptive energetic relaxation, per resource).
    auto feasible = [&](int C) -> bool {
        for (const auto& [resName, capacity] : RCPSPex.resources) {
            if (capacity <= 0) continue;
            struct Act { int es, lf; long dem, energy; };
            std::vector<Act> acts;
            std::vector<int> bp;
            long totalEnergy = 0;
            for (int i = 0; i < n; ++i) {
                const auto& a = RCPSPex.activities[i];
                if (a.duration <= 0) continue;
                auto it = a.resource_demands.find(resName);
                if (it == a.resource_demands.end() || it->second <= 0) continue;
                int es  = s.start_times[i];
                int lsc = C - reach[i];
                if (lsc < es) return false;         // window empty at C -> infeasible
                int lf  = lsc + (int)a.duration;
                long dem = (long)it->second;
                long en  = dem * (long)a.duration;
                acts.push_back({es, lf, dem, en});
                totalEnergy += en;
                bp.push_back(es); bp.push_back(lf);
            }
            if (acts.empty()) continue;
            std::sort(bp.begin(), bp.end());
            bp.erase(std::unique(bp.begin(), bp.end()), bp.end());
            const int L = (int)bp.size() - 1;
            if (L <= 0) return false;
            const int m = (int)acts.size();
            const int S = 0, T = m + L + 1;
            mincut_detail::Dinic dn; dn.init(m + L + 2);
            for (int k = 0; k < m; ++k) dn.add(S, 1 + k, acts[k].energy);
            for (int l = 0; l < L; ++l) dn.add(m + 1 + l, T, (long)capacity * (long)(bp[l + 1] - bp[l]));
            for (int k = 0; k < m; ++k)
                for (int l = 0; l < L; ++l)
                    if (bp[l] >= acts[k].es && bp[l + 1] <= acts[k].lf)
                        dn.add(1 + k, m + 1 + l, acts[k].dem * (long)(bp[l + 1] - bp[l]));
            if (dn.maxflow(S, T) < totalEnergy) return false;
        }
        return true;
    };

    if (feasible(lo)) return (double)lo;           // no improvement over current makespan
    if (!feasible(hi)) return (double)hi;          // even hi infeasible: return hi (safe, > lo)
    int L2 = lo, R2 = hi;                          // smallest feasible C in (lo, hi]
    while (L2 + 1 < R2) { int mid = L2 + (R2 - L2) / 2; if (feasible(mid)) R2 = mid; else L2 = mid; }
    return (double)R2;
}
