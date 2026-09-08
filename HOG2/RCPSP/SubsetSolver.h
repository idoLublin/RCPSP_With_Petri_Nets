#ifndef SUBSET_SOLVER_H
#define SUBSET_SOLVER_H

#include <vector>
#include <queue>
#include <unordered_set>
#include <unordered_map>
#include <algorithm>
#include <climits>
#include <cstdint>
#include "Globals.h"   // g_subset_dom (DR5 cutset dominance toggle)

// ─────────────────────────────────────────────────────────────────────────────
// Mini EXACT RCPSP solver for a small activity subset, returning a PROVEN lower
// bound on the minimum makespan (in the same time frame as `release`).
//
// Used as the per-node CBS "MDA + downstream" look-ahead: solving the subset
// captures how resolving the conflict cascades through its successors — the delay
// propagation the local (cardinal/MDA) bound misses.
//
// Search: branch over activity orderings; each activity is placed at its EARLIEST
// precedence-AND-resource-feasible start. That enumerates exactly the active
// schedules, and the RCPSP optimum is always an active schedule, so an A* ordered
// by an admissible CPM completion bound is optimal.
//
// ONE budget: `expandCap` = max node expansions. A* expands in non-decreasing f,
// so when the cap is hit the current node's f is the smallest f still unexpanded
// (min-f-on-OPEN) — a valid lower bound. The result carries `lb`, the `expands`
// actually used, and `capped` (true if we stopped on the cap rather than solving
// to optimum) so the caller can CALIBRATE the cap (cap-hit rate + avg expands).
//
// Activities are LOCAL indices 0..n-1. preds[i] holds i's local predecessors
// (transitive closure among the subset is the caller's job). All times share the
// `release` frame, so the bound is directly comparable to start_times. Pure — no
// globals — and non-recursive (its own guidance is a plain CPM bound; it never
// re-enters CBS/the subset heuristic), so it can be unit-tested in isolation.
// ─────────────────────────────────────────────────────────────────────────────
struct SubsetInstance {
    int n = 0;                              // activities
    int R = 0;                              // resources
    std::vector<int> dur;                   // [n]
    std::vector<int> release;               // [n] earliest start (from parent start_times)
    std::vector<int> cap;                   // [R]
    std::vector<std::vector<int>> demand;   // [n][R]
    std::vector<std::vector<int>> preds;    // [n] -> local predecessor indices
};

struct SubsetResult {
    long lb      = 0;       // proven lower bound on the subset's min makespan
    long expands = 0;       // node expansions actually performed
    bool capped  = false;   // true => stopped on expandCap (lb is a partial LB, not the optimum)
};

// ub: an incumbent the CALLER wants to beat. As soon as the sub-search proves its
// optimum >= ub (min-f-on-OPEN reaches ub), we stop and return that LB — enough for
// the caller to prune, without solving to the exact optimum. LONG_MAX => no UB.
// NOTE: sentinel is LONG_MAX (fits the `long` type), NOT LLONG_MAX — on LLP64 (Windows,
// long=32-bit) LLONG_MAX truncates to -1, so `cur.f >= ub` fires at the root and the
// solver returns the trivial bound with 0 expands. LONG_MAX is safe on LP64 and LLP64.
inline SubsetResult subsetRcpspLB(const SubsetInstance& P, long expandCap, long ub = LONG_MAX) {
    const int n = P.n, R = P.R;
    if (n <= 0) return {0, 0, false};

    // Successors + a topological order (Kahn) over the subset DAG.
    std::vector<std::vector<int>> succ(n);
    std::vector<int> indeg(n, 0);
    for (int i = 0; i < n; ++i)
        for (int p : P.preds[i]) { succ[p].push_back(i); indeg[i]++; }
    std::vector<int> topo;
    { std::vector<int> q, d = indeg;
      for (int i = 0; i < n; ++i) if (d[i] == 0) q.push_back(i);
      for (size_t h = 0; h < q.size(); ++h)
          for (int v : succ[q[h]]) if (--d[v] == 0) q.push_back(v);
      topo = q; }

    // tail[i] = longest precedence path of durations from i's START to project end.
    std::vector<int> tail(n, 0);
    for (int idx = (int)topo.size() - 1; idx >= 0; --idx) {
        int i = topo[idx], m = 0;
        for (int s : succ[i]) m = std::max(m, tail[s]);
        tail[i] = P.dur[i] + m;
    }

    // RC (resource/energetic) term the main solvers always have as max(CP,RC) but the
    // sub-solver was missing: for each resource, ceil(total energy / capacity) is a valid
    // makespan LB. Constant over states (all activities must run), so it floors the CPM
    // bound — pays off together with UB pruning (raises f to cross the incumbent sooner).
    long rcLB = 0;
    for (int r = 0; r < R; ++r) {
        if (P.cap[r] <= 0) continue;
        long e = 0; for (int i = 0; i < n; ++i) e += (long)P.demand[i][r] * (long)P.dur[i];
        long lb = (e + P.cap[r] - 1) / P.cap[r];
        if (lb > rcLB) rcLB = lb;
    }

    // Admissible ABSOLUTE makespan lower bound for a partial schedule: max( CPM forward
    // pass (release+precedence, resource-blind) + tail , RC energetic bound ).
    auto makespanLB = [&](const std::vector<short>& start) -> int {
        std::vector<int> est(n, 0);
        for (int i : topo) {
            int e = P.release[i];
            for (int p : P.preds[i]) e = std::max(e, est[p] + P.dur[p]);
            if (start[i] >= 0) e = start[i];
            est[i] = e;
        }
        long lb = rcLB;
        for (int i = 0; i < n; ++i) lb = std::max(lb, (long)est[i] + (long)tail[i]);
        return (int)lb;
    };

    // Earliest precedence-and-resource-feasible start for activity a in `start`.
    auto earliestFeasible = [&](int a, const std::vector<short>& start) -> int {
        int t = P.release[a];
        for (int p : P.preds[a]) t = std::max(t, (int)start[p] + P.dur[p]);
        while (true) {
            const int end = t + P.dur[a];
            bool ok = true;
            int nextDrop = INT_MAX;
            std::vector<int> checkT; checkT.push_back(t);   // peak is at t or a scheduled start
            for (int j = 0; j < n; ++j) if (start[j] >= 0) {
                int sj = start[j], fj = start[j] + P.dur[j];
                if (sj > t && sj < end) checkT.push_back(sj);
                if (fj > t) nextDrop = std::min(nextDrop, fj);
            }
            for (int r = 0; r < R && ok; ++r) {
                if (P.demand[a][r] == 0) continue;
                for (int tau : checkT) {
                    int use = 0;
                    for (int j = 0; j < n; ++j)
                        if (start[j] >= 0 && start[j] <= tau && tau < start[j] + P.dur[j])
                            use += P.demand[j][r];
                    if (use + P.demand[a][r] > P.cap[r]) { ok = false; break; }
                }
            }
            if (ok) return t;
            t = (nextDrop == INT_MAX) ? t + 1 : nextDrop;
        }
    };

    struct Node { std::vector<short> start; int f; int depth; };
    struct Cmp { bool operator()(const Node& a, const Node& b) const { return a.f > b.f; } };
    std::priority_queue<Node, std::vector<Node>, Cmp> open;
    // Exact-state dedup (fallback when dominance is off).
    std::unordered_set<uint64_t> seen;
    auto hashState = [&](const std::vector<short>& start) -> uint64_t {
        uint64_t h = 1469598103934665603ULL;
        for (int i = 0; i < n; ++i) { h ^= (uint16_t)start[i]; h *= 1099511628211ULL; }
        return h;
    };
    // DR5 cutset dominance: per scheduled-SET, keep only pointwise-start non-dominated
    // partials (a skyline). Sound: same set + all starts <= => every completion carries
    // over with makespan <=, and f=makespanLB is monotone so the dominator pops no later.
    std::unordered_map<uint64_t, std::vector<std::vector<short>>> domStore;
    auto setKey = [&](const std::vector<short>& s) -> uint64_t {
        uint64_t h = 1469598103934665603ULL;
        for (int i = 0; i < n; ++i) if (s[i] >= 0) { h ^= (uint64_t)(unsigned)(i + 1); h *= 1099511628211ULL; }
        return h;
    };
    // returns true if ns should be explored; false if dominated/duplicate (skip).
    auto admit = [&](const std::vector<short>& ns) -> bool {
        if (!g_subset_dom) return seen.insert(hashState(ns)).second;
        auto& lst = domStore[setKey(ns)];
        for (size_t i = 0; i < lst.size(); ) {
            const auto& v = lst[i];
            bool sameset = true, vLEns = true, nsLEv = true;
            for (int k = 0; k < n; ++k) {
                bool vi = v[k] >= 0, ni = ns[k] >= 0;
                if (vi != ni) { sameset = false; break; }
                if (vi) { if (v[k] > ns[k]) vLEns = false; if (ns[k] > v[k]) nsLEv = false; }
            }
            if (sameset && vLEns) return false;                  // v dominates ns => prune ns
            if (sameset && nsLEv) { lst[i] = lst.back(); lst.pop_back(); continue; } // ns dominates v => drop v
            ++i;
        }
        lst.push_back(ns);
        return true;
    };

    open.push({std::vector<short>(n, -1), makespanLB(std::vector<short>(n, -1)), 0});

    long expands = 0;
    while (!open.empty()) {
        Node cur = open.top(); open.pop();
        if (cur.depth == n) return {cur.f, expands, false};          // first goal popped = optimum
        if ((long)cur.f >= ub) return {cur.f, expands, true};        // proven optimum >= ub => enough to prune
        if (expandCap > 0 && expands >= expandCap) return {cur.f, expands, true};  // cap<=0 => UNCAPPED (run to optimum)
        if ((expands & 1023) == 0 && heur_deadline_hit()) return {cur.f, expands, true};  // parent 300s budget spent: cur.f is min-f-on-OPEN <= opt, a valid partial LB
        ++expands;

        for (int a = 0; a < n; ++a) {
            if (cur.start[a] >= 0) continue;
            bool ready = true;
            for (int p : P.preds[a]) if (cur.start[p] < 0) { ready = false; break; }
            if (!ready) continue;
            std::vector<short> ns = cur.start;
            ns[a] = (short)earliestFeasible(a, cur.start);           // DR4: earliest feasible => left-justified
            if (!admit(ns)) continue;                                // DR5 cutset dominance (or exact dedup if off)
            int f = makespanLB(ns);                                  // compute f BEFORE pushing
            open.push({std::move(ns), f, cur.depth + 1});
        }
    }
    return {0, expands, false}; // unreachable for a feasible instance
}

#endif // SUBSET_SOLVER_H
