#ifndef SUBSET_SOLVER_H
#define SUBSET_SOLVER_H

#include <vector>
#include <queue>
#include <unordered_set>
#include <algorithm>
#include <climits>
#include <cstdint>

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

inline SubsetResult subsetRcpspLB(const SubsetInstance& P, long expandCap) {
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

    // Admissible ABSOLUTE makespan lower bound for a partial schedule: CPM forward
    // pass (release + precedence, resources ignored => underestimate) plus tail.
    auto makespanLB = [&](const std::vector<short>& start) -> int {
        std::vector<int> est(n, 0);
        for (int i : topo) {
            int e = P.release[i];
            for (int p : P.preds[i]) e = std::max(e, est[p] + P.dur[p]);
            if (start[i] >= 0) e = start[i];
            est[i] = e;
        }
        int lb = 0;
        for (int i = 0; i < n; ++i) lb = std::max(lb, est[i] + tail[i]);
        return lb;
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
    std::unordered_set<uint64_t> seen;
    auto hashState = [&](const std::vector<short>& start) -> uint64_t {
        uint64_t h = 1469598103934665603ULL;
        for (int i = 0; i < n; ++i) { h ^= (uint16_t)start[i]; h *= 1099511628211ULL; }
        return h;
    };

    open.push({std::vector<short>(n, -1), makespanLB(std::vector<short>(n, -1)), 0});

    long expands = 0;
    while (!open.empty()) {
        Node cur = open.top(); open.pop();
        if (cur.depth == n) return {cur.f, expands, false};          // first goal popped = optimum
        if (expands >= expandCap) return {cur.f, expands, true};     // cap => min-f-on-OPEN LB
        ++expands;

        for (int a = 0; a < n; ++a) {
            if (cur.start[a] >= 0) continue;
            bool ready = true;
            for (int p : P.preds[a]) if (cur.start[p] < 0) { ready = false; break; }
            if (!ready) continue;
            std::vector<short> ns = cur.start;
            ns[a] = (short)earliestFeasible(a, cur.start);
            if (!seen.insert(hashState(ns)).second) continue;
            int f = makespanLB(ns);                                  // compute f BEFORE pushing
            open.push({std::move(ns), f, cur.depth + 1});
        }
    }
    return {0, expands, false}; // unreachable for a feasible instance
}

#endif // SUBSET_SOLVER_H
