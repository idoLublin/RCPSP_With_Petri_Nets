#ifndef ROOT_BOUNDS_H
#define ROOT_BOUNDS_H
// ─────────────────────────────────────────────────────────────────────────────
// ROOT DEADLINE BOUNDS — what CP / time-indexed MIP / destructive-bound B&B have and our forward A* lacks:
// reasoning with a DEADLINE D, which gives every activity a latest start (D - tail) as well as an earliest start.
//
// RCPSP_ROOT_DLB=1  (default 0)  destructive lower bound at the root: the smallest D that is not refuted by
//     precedence + time-table (compulsory-part) propagation on the windows [ES, D - tail], an energetic-reasoning
//     check on intervals [t1, t2), and (RCPSP_ROOT_DLB_SHAVE=1, default) one level of shaving: an activity whose
//     earliest (latest) start makes propagation fail cannot start there, so its window shrinks. A refuted D admits
//     no schedule, so every value returned is <= C*. Budget RCPSP_ROOT_DLB_MS (default 5000); on the budget the
//     bound is the last D not yet refuted (still sound).
// RCPSP_ROOT_SEED=1 (default 0)  a first incumbent: multi-start serial SGS (latest-finish priority, then
//     randomly perturbed) + double justification (right/left shift), RCPSP_ROOT_SEED_ITERS runs (default 60).
//     Every seed schedule is checked for precedence and resource feasibility before it is used.
// RCPSP_ROOT_PROBE=1 (default 0)  deadline probe: a depth-first search for a schedule that ends by D, the way CP
//     solvers search. Node = the same propagation as the DLB; branching = schedule-or-postpone ("SetTimes",
//     Le Pape et al. 1994): the open activity with the smallest earliest start (then smallest latest start) is
//     either started at its earliest start, or postponed until propagation moves its earliest start. Restarts with
//     Luby node limits and random tie-breaking. First D = root LB (a hit closes the instance), then D = incumbent-1
//     downwards (each hit is a better incumbent). PRIMAL ONLY: an exhausted probe never raises the LB, so the
//     pruning rules only have to be good, not complete. Every schedule is checked like the seed. Budget
//     RCPSP_ROOT_PROBE_MS (default 10000), 60% for D = LB.
// RCPSP_ROOT_LNS=1|2|3 (default 0)  large neighbourhood search on the incumbent: free a block of the best schedule and
//     repair it with the deadline probe on a partial-order neighbourhood (1), with SGS re-insertion (2), or both (3).
//     Primal only, every schedule checked; budget RCPSP_ROOT_LNS_MS (default 2000). Needs RCPSP_ROOT_SEED=1.
// If seed <= LB the seed is optimal: the instance is closed at the root. Otherwise HBAE starts with U = seed and
// stops as soon as U <= max(LB, min f_F, min f_B) (sound: LB <= C* <= U).
// Offline prototype (Python, 2026-09-13) on the j60/j90 instances only CP proves: propagation cut the root gap
// j90 9.0 -> 5.6; shaving on top closed it on 17 of 31 instances with gap <= 10.
// ─────────────────────────────────────────────────────────────────────────────
#include <vector>
#include <random>
#include <chrono>
#include <algorithm>
#include <cstdlib>
#include <climits>

namespace rootb {

inline bool envOn(const char* k, bool d) { const char* e = std::getenv(k); return e ? (std::atoi(e) != 0) : d; }
inline long envNum(const char* k, long d) { const char* e = std::getenv(k); return e ? std::atol(e) : d; }
inline bool DlbOn()    { static const bool v = envOn("RCPSP_ROOT_DLB", false); return v; }
inline bool SeedOn()   { static const bool v = envOn("RCPSP_ROOT_SEED", false); return v; }
inline bool ProbeOn()  { static const bool v = envOn("RCPSP_ROOT_PROBE", false); return v; }
inline bool ShaveOn()  { static const bool v = envOn("RCPSP_ROOT_DLB_SHAVE", true); return v; }
inline long BudgetMs() { static const long v = envNum("RCPSP_ROOT_DLB_MS", 5000); return v; }
inline long SeedIters(){ static const long v = envNum("RCPSP_ROOT_SEED_ITERS", 60); return v; }
inline long ProbeMs()  { static const long v = envNum("RCPSP_ROOT_PROBE_MS", 10000); return v; }
// RCPSP_ROOT_PAIR=1: root preprocessing - transitive reduction of the precedence graph + pairwise (disjunctive)
// reasoning. Two activities that cannot overlap on a resource must be ordered; when one order cannot fit under the
// deadline, the other one is forced, which tightens the windows the way CP disjunctive propagation does.
inline bool PairOn()   { static const bool v = envOn("RCPSP_ROOT_PAIR", false); return v; }
// RCPSP_ROOT_LNS=1|2|3 (default 0): large neighbourhood search on the incumbent after the seed and the probe
// (1 = CP repair, 2 = SGS repair, 3 = both), budget RCPSP_ROOT_LNS_MS (default 2000), CP repair node limit
// RCPSP_ROOT_LNS_NODES (default 300). Stops as soon as the incumbent reaches the root LB.
inline long LnsMode()  { static const long v = envNum("RCPSP_ROOT_LNS", 0); return v; }
inline bool LnsOn()    { return LnsMode() > 0; }
inline long LnsMs()    { static const long v = envNum("RCPSP_ROOT_LNS_MS", 2000); return v; }
inline long LnsNodes() { static const long v = envNum("RCPSP_ROOT_LNS_NODES", 300); return v; }
// RCPSP_ROOT_WIN=1: after the seed, propagate ONCE at D = seed and keep the windows. Every schedule at least
// as short as the seed has start_i in [winEs, winLs]. The searches only use the LATE side: starts never
// decrease down the tree, so a start past winLs is inherited by every descendant and that subtree is dead.
// (The early side would have to be carried in the state as release times - not worth it, measured 0.)
inline bool WinOn()    { static const bool v = envOn("RCPSP_ROOT_WIN", false); return v; }

// fingerprint of the instance currently in RCPSPex (see TT2NodeBound: the same hash, one definition).
inline unsigned long long instFp() {
    unsigned long long h = 1469598103934665603ULL;
    auto mix = [&h](unsigned long long v) { h = (h ^ v) * 1099511628211ULL; };
    mix(RCPSPex.activities.size()); mix(RCPSPex.resources.size());
    for (size_t i = 0; i < RCPSPex.activities.size(); ++i) {
        mix((unsigned long long)RCPSPex.activities[i].duration);
        mix((unsigned long long)RCPSPex.backword_dependencies[i].size());
    }
    for (const auto& r : RCPSPex.resources) mix((unsigned long long)r.second);
    return h;
}

struct Info {
    int lbPlain = -1;        // max(CPM, energy) — what the A* heuristic gives at the root
    int lb = -1;             // destructive bound (-1 = not computed)
    int seed = -1;           // best incumbent makespan: SGS seed, improved by the probe (-1 = none)
    double lbSec = 0.0, seedSec = 0.0;
    bool budgetHit = false;  // the DLB loop stopped on its time budget
    bool closed = false;     // seed <= lb: optimal at the root
    int probeSeedIn = -1;    // incumbent before the probe
    int probeBest = -1;      // best makespan the probe found (-1 = none)
    long probeNodes = 0;
    int probeExhausted = 0;  // deadlines whose (pruned) probe tree was fully explored without a schedule
    double probeSec = 0.0;
    int lnsSeedIn = -1;      // incumbent before the LNS
    int lnsBest = -1;        // makespan the LNS improved the incumbent to (-1 = no improvement)
    long lnsIters = 0, lnsImpCp = 0, lnsImpSgs = 0;   // iterations; strict improvements by CP / SGS repair
    double lnsSec = 0.0;
    std::vector<int> seedStart;   // start time per activity (0-based index), for printing
    int winD = -1;                // RCPSP_ROOT_WIN: the deadline the windows were propagated at (= seed)
    unsigned long long winFp = 0; // fingerprint of the instance the windows belong to (0 = none)
    std::vector<int> winEs, winLs;
};
inline Info& info() { static Info i; return i; }
inline void reset() { info() = Info{}; }

struct Inst {
    int n = 0, R = 0, cpm = 0, energy = 0, horizon = 0, maxp = 0;
    std::vector<int> p, cap, topo, es0, tail;
    std::vector<std::vector<int>> dem, preds, succ, resOf;   // dem[k][i]; resOf[i] = resources with demand
    std::vector<std::vector<char>> inc;   // RCPSP_ROOT_PAIR: inc[i][j] = i and j are unordered and cannot overlap
};

inline Inst build() {
    Inst I;
    I.n = (int)RCPSPex.activities.size();
    I.R = (int)RCPSPex.resources.size();
    I.p.resize(I.n); I.cap.resize(I.R);
    I.dem.assign(I.R, std::vector<int>(I.n, 0));
    I.resOf.assign(I.n, {});
    for (int k = 0; k < I.R; ++k) I.cap[k] = RCPSPex.resources[k].second;
    for (int i = 0; i < I.n; ++i) {
        I.p[i] = RCPSPex.activities[i].duration;
        I.maxp = std::max(I.maxp, I.p[i]);
        for (const auto& d : RCPSPex.activities[i].resource_demands)
            for (int k = 0; k < I.R; ++k)
                if (RCPSPex.resources[k].first == d.first) I.dem[k][i] += d.second;
        for (int k = 0; k < I.R; ++k) if (I.dem[k][i] > 0) I.resOf[i].push_back(k);
    }
    I.preds.assign(I.n, {}); I.succ.assign(I.n, {});
    for (int i = 0; i < I.n; ++i)
        for (short q : RCPSPex.backword_dependencies[i]) { I.preds[i].push_back(q - 1); I.succ[q - 1].push_back(i); }
    // topological order (works for the reversed instance too, where ids are not topological)
    std::vector<int> indeg(I.n, 0);
    for (int i = 0; i < I.n; ++i) indeg[i] = (int)I.preds[i].size();
    for (int i = 0; i < I.n; ++i) if (indeg[i] == 0) I.topo.push_back(i);
    for (size_t h = 0; h < I.topo.size(); ++h)
        for (int s : I.succ[I.topo[h]]) if (--indeg[s] == 0) I.topo.push_back(s);
    I.es0.assign(I.n, 0); I.tail.assign(I.n, 0);
    for (int i : I.topo) for (int q : I.preds[i]) I.es0[i] = std::max(I.es0[i], I.es0[q] + I.p[q]);
    for (int h = I.n - 1; h >= 0; --h) {
        const int i = I.topo[h]; int m = 0;
        for (int s : I.succ[i]) m = std::max(m, I.tail[s]);
        I.tail[i] = I.p[i] + m;
    }
    for (int i = 0; i < I.n; ++i) { I.cpm = std::max(I.cpm, I.es0[i] + I.p[i]); I.horizon += std::max(1, I.p[i]); }
    for (int k = 0; k < I.R; ++k) {
        long e = 0; for (int i = 0; i < I.n; ++i) e += (long)I.p[i] * I.dem[k][i];
        if (I.cap[k] > 0) I.energy = std::max(I.energy, (int)((e + I.cap[k] - 1) / I.cap[k]));
    }
    if (PairOn()) {
        const int W = (I.n + 63) / 64;
        std::vector<std::vector<uint64_t>> reach(I.n, std::vector<uint64_t>(W, 0));
        for (int h = I.n - 1; h >= 0; --h) {
            const int i = I.topo[h];
            for (int s : I.succ[i]) { reach[i][s >> 6] |= 1ull << (s & 63); for (int w = 0; w < W; ++w) reach[i][w] |= reach[s][w]; }
        }
        auto reaches = [&](int a, int b) { return ((reach[a][b >> 6] >> (b & 63)) & 1ull) != 0; };
        for (int i = 0; i < I.n; ++i) {            // transitive reduction: drop i->j if another successor reaches j
            std::vector<int> keep;
            for (int j : I.succ[i]) {
                bool red = false;
                for (int s : I.succ[i]) if (s != j && reaches(s, j)) { red = true; break; }
                if (!red) keep.push_back(j);
            }
            I.succ[i] = keep;
        }
        I.preds.assign(I.n, {});
        for (int i = 0; i < I.n; ++i) for (int s : I.succ[i]) I.preds[s].push_back(i);
        I.inc.assign(I.n, std::vector<char>(I.n, 0));
        for (int i = 0; i < I.n; ++i) {
            if (I.p[i] == 0) continue;
            for (int j = i + 1; j < I.n; ++j) {
                if (I.p[j] == 0 || reaches(i, j) || reaches(j, i)) continue;
                bool bad = false;
                for (int k = 0; k < I.R && !bad; ++k) bad = (I.dem[k][i] + I.dem[k][j] > I.cap[k]);
                if (bad) { I.inc[i][j] = 1; I.inc[j][i] = 1; }
            }
        }
    }
    return I;
}

// precedence + time-table propagation on [es, ls] for deadline D; false = some window emptied (D refuted)
// RCPSP_PROP_ITERS: cap on fixpoint rounds (default 60 = the original). A lower cap stops early with windows
// that are still valid, only looser, so every caller stays sound. Counters are per thread, never reset here.
inline long PropIters() { static const long v = envNum("RCPSP_PROP_ITERS", 60); return v; }
inline long& propCalls()  { static thread_local long v = 0; return v; }
inline long& propRounds() { static thread_local long v = 0; return v; }
// RCPSP_PROP_FAST (default 0): faster code for the same results - here the time-table step, and in RCPSPState.cpp
// the earliest-conflict scan (compute_first_conflict). 1 = on, 2 = run the original and the fast code on every
// call and abort on any difference (debug). RCPSPState.cpp also uses it for the linear search in compute_mda_cost
// and for the sorts of the full conflict scan in compute_h_and_RVS. The fast step computes exactly the same windows: a time unit t blocks activity
// i on resource k iff prof[k][t] > cap[k] - dem[k][i] and t lies outside i's own compulsory part (inside it the
// test would read prof[k][t] > cap[k], which the overload check has just excluded). So (a) a resource whose
// largest profile value is <= cap - dem cannot block i anywhere, (b) an activity with no such resource keeps its
// window, and (c) for the others "blocked" is evaluated only at the time units the two scans below actually read
// (usually one duration at the earliest start and one at the latest), and remembered, instead of for the whole
// window. Every value read is the value the original would have stored, so the scans behave identically.
inline long PropFast() { static const long v = envNum("RCPSP_PROP_FAST", 0); return v; }
inline bool propagateImpl(const Inst& I, int D, std::vector<int>& es, std::vector<int>& ls, const bool fast, const bool count) {
    static thread_local std::vector<std::vector<int>> prof;
    static thread_local std::vector<char> bad;
    static thread_local std::vector<int> maxprof;               // fast: per resource, largest profile value
    const int H = D + 2;
    if ((int)prof.size() < I.R) prof.resize(I.R);
    for (int k = 0; k < I.R; ++k) if ((int)prof[k].size() < H) prof[k].resize(H);
    if ((int)bad.size() < H) bad.resize(H);
    if (fast && (int)maxprof.size() < I.R) maxprof.resize(I.R);
    if (count) ++propCalls();
    const int maxIter = (int)PropIters();
    for (int iter = 0; iter < maxIter; ++iter) {
        if (count) ++propRounds();
        bool changed = false;
        for (int i : I.topo) {
            int v = es[i];
            for (int q : I.preds[i]) v = std::max(v, es[q] + I.p[q]);
            if (v > es[i]) { es[i] = v; changed = true; }
        }
        for (int h = I.n - 1; h >= 0; --h) {
            const int i = I.topo[h];
            int v = std::min(ls[i], D - I.p[i]);
            for (int s : I.succ[i]) v = std::min(v, ls[s] - I.p[i]);
            if (v < ls[i]) { ls[i] = v; changed = true; }
        }
        for (int i = 0; i < I.n; ++i) if (es[i] > ls[i] || es[i] < 0) return false;
        if (PairOn() && !I.inc.empty()) {          // force the only order of an incompatible pair that fits by D
            for (int i = 0; i < I.n; ++i) {
                for (int j = 0; j < I.n; ++j) {
                    if (!I.inc[i][j]) continue;
                    const bool jFirstBad = (es[j] + I.p[j] + I.tail[i] > D) || (es[j] + I.p[j] > ls[i]);
                    const bool iFirstBad = (es[i] + I.p[i] + I.tail[j] > D) || (es[i] + I.p[i] > ls[j]);
                    if (jFirstBad && iFirstBad) return false;
                    if (jFirstBad) {
                        if (es[i] + I.p[i] > es[j]) { es[j] = es[i] + I.p[i]; changed = true; }
                        if (ls[j] - I.p[i] < ls[i]) { ls[i] = ls[j] - I.p[i]; changed = true; }
                        if (es[i] > ls[i] || es[j] > ls[j]) return false;
                    }
                }
            }
        }
        for (int k = 0; k < I.R; ++k) std::fill(prof[k].begin(), prof[k].begin() + H, 0);
        int cpLo = H, cpHi = 0;                      // fast: the time units that hold any compulsory part lie in [cpLo, cpHi)
        for (int i = 0; i < I.n; ++i) {
            if (I.p[i] == 0) continue;
            const int a = ls[i], b = es[i] + I.p[i];
            if (a < b) {
                for (int k : I.resOf[i]) for (int t = a; t < b; ++t) prof[k][t] += I.dem[k][i];
                if (!I.resOf[i].empty()) { if (a < cpLo) cpLo = a; if (b > cpHi) cpHi = b; }
            }
        }
        if (!fast) {
            for (int k = 0; k < I.R; ++k) for (int t = 0; t <= D; ++t) if (prof[k][t] > I.cap[k]) return false;
        } else {                                    // the same overload test, and the largest value on the way
            for (int k = 0; k < I.R; ++k) {
                const int* pk = prof[k].data(); const int c = I.cap[k]; int mx = 0;
                for (int t = cpLo; t < cpHi; ++t) { const int v = pk[t]; if (v > c) return false; if (v > mx) mx = v; }   // 0 elsewhere
                maxprof[k] = mx;
            }
        }
        for (int i = 0; i < I.n; ++i) {
            if (I.p[i] == 0 || I.resOf[i].empty()) continue;
            const int a = ls[i], b = es[i] + I.p[i];
            const int lo = es[i], hi = ls[i] + I.p[i];
            bool dense = !fast;
            const int* blkProf[8]; int thr[8], nb = 0;
            if (fast) {
                for (int k : I.resOf[i]) {
                    const int th = I.cap[k] - I.dem[k][i];
                    if (th < 0 || nb == 8) { dense = true; break; }      // demand above capacity / too many resources: original test
                    if (maxprof[k] > th) { blkProf[nb] = prof[k].data(); thr[nb] = th; ++nb; }
                }
                if (!dense) {
                    if (nb == 0) continue;                               // nothing can block it: its window stays as it is
                    std::fill(bad.begin() + lo, bad.begin() + hi, (char)2);   // 2 = not evaluated yet
                }
            }
            // blocked(u), as the original stores it: some resource has prof > cap - dem at u, outside i's own compulsory part
            auto isBad = [&](int u) -> bool {
                char& c = bad[u];
                if (c == 2) {
                    c = 0;
                    if (u < a || u >= b) for (int q = 0; q < nb; ++q) if (blkProf[q][u] > thr[q]) { c = 1; break; }
                }
                return c != 0;
            };
            if (dense) {
                for (int t = lo; t < hi; ++t) {
                    bool bd = false;
                    for (int k : I.resOf[i]) {
                        int other = prof[k][t] - ((a < b && t >= a && t < b) ? I.dem[k][i] : 0);
                        if (other + I.dem[k][i] > I.cap[k]) { bd = true; break; }
                    }
                    bad[t] = bd;
                }
            }
            int t = es[i];
            while (t <= ls[i]) {
                int last = -1;
                for (int u = t; u < t + I.p[i]; ++u) if (isBad(u)) last = u;
                if (last >= 0) t = last + 1; else break;
            }
            if (t > ls[i]) return false;
            if (t > es[i]) { es[i] = t; changed = true; }
            t = ls[i];
            while (t >= es[i]) {
                int first = -1;
                for (int u = t; u < t + I.p[i]; ++u) if (isBad(u)) { first = u; break; }
                if (first >= 0) t = first - I.p[i]; else break;
            }
            if (t < es[i]) return false;
            if (t < ls[i]) { ls[i] = t; changed = true; }
        }
        if (!changed) break;
    }
    return true;
}
// RCPSP_PROP_INC (default 0; acts only with RCPSP_PROP_FAST=1): the same fixpoint, computed incrementally.
// Inside one call the windows only shrink (es grows, ls falls), so a compulsory part [ls, es + p) only grows. Hence
//  (a) the profile is not rebuilt in every round: the time units that became compulsory since the last round are
//      added to it, which gives the values a rebuild would give;
//  (b) the overload test and the per-resource maximum look only at those units (the others were tested before and
//      have not changed);
//  (c) the time-table step is repeated for an activity only if its window changed since the step was last done for
//      it, or the profile of one of its resources changed inside its window in this round. Otherwise the step would
//      read the same values and leave the window as it is: its earliest and latest start were found free of blocked
//      time units, and "blocked" depends only on the profile inside the window and on the activity's own compulsory
//      part. (If the step moved the window and the compulsory part grew, the profile changes inside the window in
//      the next round and the step is repeated.)
// Windows, result and number of rounds are those of RCPSP_PROP_FAST=1. 1 = on, 2 = also run the RCPSP_PROP_FAST=1
// code on every call and abort on any difference. Not used with RCPSP_ROOT_PAIR.
inline long PropInc() { static const long v = envNum("RCPSP_PROP_INC", 0); return v; }
inline bool propagateInc(const Inst& I, int D, std::vector<int>& es, std::vector<int>& ls, const bool count) {
    static thread_local std::vector<std::vector<int>> prof;
    static thread_local std::vector<char> bad;
    static thread_local std::vector<int> maxprof, chgLo, chgHi, fEs, fLs, cpA, cpB;
    const int H = D + 2, n = I.n, R = I.R;
    if ((int)prof.size() < R) prof.resize(R);
    for (int k = 0; k < R; ++k) { if ((int)prof[k].size() < H) prof[k].resize(H); std::fill(prof[k].begin(), prof[k].begin() + H, 0); }
    if ((int)bad.size() < H) bad.resize(H);
    maxprof.assign(R, 0); chgLo.assign(R, H); chgHi.assign(R, 0);
    fEs.assign(n, INT_MIN); fLs.assign(n, INT_MIN);    // window of each activity right after its last time-table step
    cpA.assign(n, 0); cpB.assign(n, 0);                // compulsory part of each activity that is in the profile (empty if cpA >= cpB)
    if (count) ++propCalls();
    const int maxIter = (int)PropIters();
    for (int iter = 0; iter < maxIter; ++iter) {
        if (count) ++propRounds();
        bool changed = false;
        for (int i : I.topo) {
            int v = es[i];
            for (int q : I.preds[i]) v = std::max(v, es[q] + I.p[q]);
            if (v > es[i]) { es[i] = v; changed = true; }
        }
        for (int h = n - 1; h >= 0; --h) {
            const int i = I.topo[h];
            int v = std::min(ls[i], D - I.p[i]);
            for (int s : I.succ[i]) v = std::min(v, ls[s] - I.p[i]);
            if (v < ls[i]) { ls[i] = v; changed = true; }
        }
        for (int i = 0; i < n; ++i) if (es[i] > ls[i] || es[i] < 0) return false;
        for (int k = 0; k < R; ++k) { chgLo[k] = H; chgHi[k] = 0; }
        for (int i = 0; i < n; ++i) {                  // (a) add the new compulsory time units
            if (I.p[i] == 0) continue;
            const int a = ls[i], b = es[i] + I.p[i];
            if (a >= b) continue;
            const int oa = cpA[i], ob = cpB[i];
            const bool had = oa < ob;
            if (had && a == oa && b == ob) continue;
            for (int k : I.resOf[i]) {
                const int d = I.dem[k][i]; int* pk = prof[k].data();
                if (!had) { for (int t = a; t < b; ++t) pk[t] += d; }
                else { for (int t = a; t < oa; ++t) pk[t] += d; for (int t = ob; t < b; ++t) pk[t] += d; }
                const int lo = had ? (a < oa ? a : ob) : a, hi = had ? (ob < b ? b : oa) : b;
                if (lo < chgLo[k]) chgLo[k] = lo;
                if (hi > chgHi[k]) chgHi[k] = hi;
            }
            cpA[i] = a; cpB[i] = b;
        }
        for (int k = 0; k < R; ++k) {                  // (b) overload test and maximum on the changed units
            const int* pk = prof[k].data(); const int c = I.cap[k]; int mx = maxprof[k];
            for (int t = chgLo[k]; t < chgHi[k]; ++t) { const int v = pk[t]; if (v > c) return false; if (v > mx) mx = v; }
            maxprof[k] = mx;
        }
        for (int i = 0; i < n; ++i) {
            if (I.p[i] == 0 || I.resOf[i].empty()) continue;
            const int a = ls[i], b = es[i] + I.p[i];
            const int lo = es[i], hi = ls[i] + I.p[i];
            if (es[i] == fEs[i] && ls[i] == fLs[i]) {  // (c) same window as after its last step: did the profile change inside it?
                bool touched = false;
                for (int k : I.resOf[i]) if (chgLo[k] < hi && chgHi[k] > lo) { touched = true; break; }
                if (!touched) continue;
            }
            bool dense = false;
            const int* blkProf[8]; int thr[8], nb = 0;
            for (int k : I.resOf[i]) {
                const int th = I.cap[k] - I.dem[k][i];
                if (th < 0 || nb == 8) { dense = true; break; }
                if (maxprof[k] > th) { blkProf[nb] = prof[k].data(); thr[nb] = th; ++nb; }
            }
            if (!dense) {
                if (nb == 0) { fEs[i] = es[i]; fLs[i] = ls[i]; continue; }
                std::fill(bad.begin() + lo, bad.begin() + hi, (char)2);
            }
            auto isBad = [&](int u) -> bool {
                char& c = bad[u];
                if (c == 2) {
                    c = 0;
                    if (u < a || u >= b) for (int q = 0; q < nb; ++q) if (blkProf[q][u] > thr[q]) { c = 1; break; }
                }
                return c != 0;
            };
            if (dense) {
                for (int t = lo; t < hi; ++t) {
                    bool bd = false;
                    for (int k : I.resOf[i]) {
                        int other = prof[k][t] - ((a < b && t >= a && t < b) ? I.dem[k][i] : 0);
                        if (other + I.dem[k][i] > I.cap[k]) { bd = true; break; }
                    }
                    bad[t] = bd;
                }
            }
            int t = es[i];
            while (t <= ls[i]) {
                int last = -1;                         // the last blocked unit of [t, t + p): scan from the right
                for (int u = t + I.p[i] - 1; u >= t; --u) if (isBad(u)) { last = u; break; }
                if (last >= 0) t = last + 1; else break;
            }
            if (t > ls[i]) return false;
            if (t > es[i]) { es[i] = t; changed = true; }
            t = ls[i];
            while (t >= es[i]) {
                int first = -1;
                for (int u = t; u < t + I.p[i]; ++u) if (isBad(u)) { first = u; break; }
                if (first >= 0) t = first - I.p[i]; else break;
            }
            if (t < es[i]) return false;
            if (t < ls[i]) { ls[i] = t; changed = true; }
            fEs[i] = es[i]; fLs[i] = ls[i];
        }
        if (!changed) break;
    }
    return true;
}
inline bool propagate(const Inst& I, int D, std::vector<int>& es, std::vector<int>& ls) {
    const long mode = PropFast();
    if (mode == 0) return propagateImpl(I, D, es, ls, false, true);
    if (mode == 1) {
        const long inc = (PairOn() && !I.inc.empty()) ? 0 : PropInc();
        if (inc == 0) return propagateImpl(I, D, es, ls, true, true);
        if (inc == 1) return propagateInc(I, D, es, ls, true);
        static thread_local std::vector<int> e3, l3;            // 2: differential check against the RCPSP_PROP_FAST=1 code
        e3 = es; l3 = ls;
        const bool r1 = propagateImpl(I, D, es, ls, true, true);
        const bool r3 = propagateInc(I, D, e3, l3, false);
        if (r1 != r3 || es != e3 || ls != l3) {
            std::cerr << "RCPSP_PROP_INC MISMATCH at D=" << D << ": fast " << r1 << " incremental " << r3 << std::endl;
            for (int i = 0; i < I.n; ++i) if (es[i] != e3[i] || ls[i] != l3[i])
                std::cerr << "  act " << i << ": window [" << es[i] << "," << ls[i] << "] vs incremental [" << e3[i] << "," << l3[i] << "]" << std::endl;
            std::abort();
        }
        return r1;
    }
    static thread_local std::vector<int> e2, l2;                // 2: differential check
    e2 = es; l2 = ls;
    const bool r1 = propagateImpl(I, D, es, ls, false, true);
    const bool r2 = propagateImpl(I, D, e2, l2, true, false);
    if (r1 != r2 || es != e2 || ls != l2) {
        std::cerr << "RCPSP_PROP_FAST MISMATCH at D=" << D << ": original " << r1 << " fast " << r2 << std::endl;
        for (int i = 0; i < I.n; ++i) if (es[i] != e2[i] || ls[i] != l2[i])
            std::cerr << "  act " << i << ": window [" << es[i] << "," << ls[i] << "] vs fast [" << e2[i] << "," << l2[i] << "]" << std::endl;
        std::abort();
    }
    return r1;
}

// energetic-reasoning feasibility check on intervals [t1, t2); false = overload (D refuted)
inline bool energeticOk(const Inst& I, const std::vector<int>& es, const std::vector<int>& ls) {
    std::vector<int> T1, T2;
    for (int i = 0; i < I.n; ++i) if (I.p[i] > 0) { T1.push_back(es[i]); T1.push_back(ls[i]); T2.push_back(es[i] + I.p[i]); T2.push_back(ls[i] + I.p[i]); }
    std::sort(T1.begin(), T1.end()); T1.erase(std::unique(T1.begin(), T1.end()), T1.end());
    std::sort(T2.begin(), T2.end()); T2.erase(std::unique(T2.begin(), T2.end()), T2.end());
    for (int t1 : T1)
        for (int t2 : T2) {
            if (t2 <= t1) continue;
            const int L = t2 - t1;
            for (int k = 0; k < I.R; ++k) {
                long W = 0; const long capL = (long)I.cap[k] * L;
                for (int i = 0; i < I.n; ++i) {
                    const int d = I.dem[k][i];
                    if (d == 0 || I.p[i] == 0) continue;
                    int ov = std::min({L, I.p[i], es[i] + I.p[i] - t1, t2 - ls[i]});
                    if (ov > 0) { W += (long)d * ov; if (W > capL) return false; }
                }
            }
        }
    return true;
}

// 1 = D refuted, 0 = not refuted, -1 = budget hit (unknown)
inline int refuted(const Inst& I, int D, std::chrono::steady_clock::time_point until) {
    std::vector<int> es = I.es0, ls(I.n);
    for (int i = 0; i < I.n; ++i) ls[i] = D - I.tail[i];
    if (!propagate(I, D, es, ls)) return 1;
    if (!energeticOk(I, es, ls)) return 1;
    if (!ShaveOn()) return 0;
    bool changed = true;
    while (changed) {
        changed = false;
        for (int i = 0; i < I.n; ++i) {
            if (I.p[i] == 0) continue;
            if (std::chrono::steady_clock::now() > until) return -1;
            while (es[i] < ls[i]) {                      // can i start at its earliest start?
                std::vector<int> e2 = es, l2 = ls; l2[i] = es[i];
                if (!propagate(I, D, e2, l2)) { ++es[i]; changed = true; } else break;
            }
            while (ls[i] > es[i]) {                      // can i start at its latest start?
                std::vector<int> e2 = es, l2 = ls; e2[i] = ls[i];
                if (!propagate(I, D, e2, l2)) { --ls[i]; changed = true; } else break;
            }
            if (changed && !propagate(I, D, es, ls)) return 1;
        }
    }
    return 0;
}

// ── primal seed: serial SGS on a priority key + double justification ──
inline int sgs(const Inst& I, const std::vector<std::vector<int>>& pr, const std::vector<double>& key, std::vector<int>& start) {
    const int H = I.horizon + I.maxp + 2;
    static thread_local std::vector<std::vector<int>> use;
    if ((int)use.size() < I.R) use.resize(I.R);
    for (int k = 0; k < I.R; ++k) { if ((int)use[k].size() < H) use[k].resize(H); std::fill(use[k].begin(), use[k].begin() + H, 0); }
    start.assign(I.n, -1);
    std::vector<int> left(I.n); for (int i = 0; i < I.n; ++i) left[i] = (int)pr[i].size();
    std::vector<std::vector<int>> nx(I.n);
    for (int i = 0; i < I.n; ++i) for (int q : pr[i]) nx[q].push_back(i);
    std::vector<int> elig; for (int i = 0; i < I.n; ++i) if (left[i] == 0) elig.push_back(i);
    int mk = 0;
    for (int placed = 0; placed < I.n; ++placed) {
        if (elig.empty()) return INT_MAX;
        size_t bi = 0;
        for (size_t h = 1; h < elig.size(); ++h)
            if (key[elig[h]] < key[elig[bi]] || (key[elig[h]] == key[elig[bi]] && elig[h] < elig[bi])) bi = h;
        const int i = elig[bi]; elig.erase(elig.begin() + bi);
        int t = 0; for (int q : pr[i]) t = std::max(t, start[q] + I.p[q]);
        if (I.p[i] > 0 && !I.resOf[i].empty()) {
            while (true) {
                if (t + I.p[i] >= H) return INT_MAX;
                int clash = -1;
                for (int k : I.resOf[i]) {
                    for (int u = t; u < t + I.p[i]; ++u) if (use[k][u] + I.dem[k][i] > I.cap[k]) { clash = std::max(clash, u); }
                }
                if (clash < 0) break;
                t = clash + 1;
            }
            for (int k : I.resOf[i]) for (int u = t; u < t + I.p[i]; ++u) use[k][u] += I.dem[k][i];
        }
        start[i] = t; mk = std::max(mk, t + I.p[i]);
        for (int s : nx[i]) if (--left[s] == 0) elig.push_back(s);
    }
    return mk;
}

inline bool feasible(const Inst& I, const std::vector<int>& start, int mk) {
    for (int i = 0; i < I.n; ++i) {
        if (start[i] < 0 || start[i] + I.p[i] > mk) return false;
        for (int q : I.preds[i]) if (start[q] + I.p[q] > start[i]) return false;
    }
    for (int k = 0; k < I.R; ++k) {
        std::vector<int> u(mk + 1, 0);
        for (int i = 0; i < I.n; ++i) if (I.dem[k][i]) for (int t = start[i]; t < start[i] + I.p[i]; ++t) { u[t] += I.dem[k][i]; if (u[t] > I.cap[k]) return false; }
    }
    return true;
}

inline int justify(const Inst& I, std::vector<int>& start, int mk) {
    for (int round = 0; round < 10; ++round) {
        std::vector<double> kr(I.n); for (int i = 0; i < I.n; ++i) kr[i] = -(double)(start[i] + I.p[i]);
        std::vector<int> rev; const int mr = sgs(I, I.succ, kr, rev);       // right shift = SGS on the reversed network
        if (mr == INT_MAX) break;
        std::vector<double> kl(I.n); for (int i = 0; i < I.n; ++i) kl[i] = (double)(mr - rev[i] - I.p[i]);
        std::vector<int> lft; const int ml = sgs(I, I.preds, kl, lft);      // left shift in that order
        if (ml < mk) { start = lft; mk = ml; } else break;
    }
    return mk;
}

// target >= 0: stop as soon as a seed reaches it (a seed equal to the root LB is optimal, so more seeds cannot help)
inline int seedUB(const Inst& I, std::vector<int>& best, int target = -1) {
    std::vector<double> lf(I.n, 0.0);
    for (int h = I.n - 1; h >= 0; --h) {
        const int i = I.topo[h]; double m = (double)I.cpm;
        for (int s : I.succ[i]) m = std::min(m, lf[s] - I.p[s]);
        lf[i] = m;
    }
    std::mt19937 rng(12345);
    std::uniform_real_distribution<double> jit(0.85, 1.15);
    int bestMk = INT_MAX;
    const long iters = std::max(1L, SeedIters());
    for (long it = 0; it < iters; ++it) {
        std::vector<double> key = lf;
        if (it > 0) for (double& v : key) v *= jit(rng);
        std::vector<int> st; int mk = sgs(I, I.preds, key, st);
        if (mk == INT_MAX) continue;
        mk = justify(I, st, mk);
        if (mk < bestMk && feasible(I, st, mk)) { bestMk = mk; best = st; }
        if (target >= 0 && bestMk <= target) break;
    }
    return bestMk == INT_MAX ? -1 : bestMk;
}

// ── deadline probe (RCPSP_ROOT_PROBE): schedule-or-postpone DFS under deadline D, with restarts ──
inline long luby(long i) {                       // 1 1 2 1 1 2 4 1 1 2 ...  (i >= 1)
    for (long k = 1; k < 62; ++k) {
        const long m = (1L << k) - 1;
        if (i == m) return 1L << (k - 1);
        if (i < m) return luby(i - (1L << (k - 1)) + 1);
    }
    return 1;
}

struct Probe {
    const Inst& I; const int D;
    const std::chrono::steady_clock::time_point until;
    long nodes = 0, limit = 0;
    bool stop = false, randomTies = false;
    std::mt19937 rng;
    std::vector<int> sol;
    Probe(const Inst& i, int d, std::chrono::steady_clock::time_point u) : I(i), D(d), until(u), rng(7919u + (unsigned)d) {}

    // es == ls means started (fixed). post[i] = earliest start when i was postponed (-1 = not postponed);
    // a postponed activity becomes selectable again once propagation has moved its earliest start.
    bool dfs(std::vector<int> es, std::vector<int> ls, std::vector<int> post, int depth) {
        if (stop) return false;
        if (++nodes > limit || ((nodes & 127) == 0 && std::chrono::steady_clock::now() > until)) { stop = true; return false; }
        if (depth > 8 * I.n) return false;
        if (!propagate(I, D, es, ls)) return false;
        int sel = -1; bool open = false;
        for (int i = 0; i < I.n; ++i) {
            if (es[i] == ls[i]) continue;
            open = true;
            if (post[i] >= 0 && es[i] > post[i]) post[i] = -1;
            if (post[i] >= 0) continue;
            if (sel < 0 || es[i] < es[sel] || (es[i] == es[sel] && (ls[i] < ls[sel] || (ls[i] == ls[sel] && randomTies && (rng() & 1U)))))
                sel = i;
        }
        if (!open) { sol = es; return true; }
        if (sel < 0) return false;                                   // only postponed activities are left
        for (int i = 0; i < I.n; ++i)                                // later starts are >= es[sel]: a postponed window that closes before it is dead
            if (es[i] != ls[i] && post[i] >= 0 && ls[i] < es[sel]) return false;
        {   // start sel at its earliest start
            std::vector<int> l2 = ls; l2[sel] = es[sel];
            if (dfs(es, std::move(l2), post, depth + 1)) return true;
            if (stop) return false;
        }
        if (I.p[sel] == 0 || I.resOf[sel].empty()) return false;     // starting a resource-free activity early is never worse
        post[sel] = es[sel];
        return dfs(std::move(es), std::move(ls), std::move(post), depth + 1);
    }
};

// true = a verified schedule with makespan <= D in sol/mk; exhausted = the whole (pruned) tree failed
inline bool probeDeadline(const Inst& I, int D, std::chrono::steady_clock::time_point until,
                          std::vector<int>& sol, int& mk, long& nodes, bool& exhausted) {
    exhausted = false;
    std::vector<int> es = I.es0, ls(I.n);
    for (int i = 0; i < I.n; ++i) ls[i] = D - I.tail[i];
    if (!propagate(I, D, es, ls)) { exhausted = true; return false; }
    Probe P(I, D, until);
    for (long r = 1; std::chrono::steady_clock::now() < until; ++r) {
        P.nodes = 0; P.stop = false; P.limit = 256 * luby(r); P.randomTies = (r > 1);
        const bool ok = P.dfs(es, ls, std::vector<int>(I.n, -1), 0);
        nodes += P.nodes;
        if (ok) {
            int m = 0; for (int i = 0; i < I.n; ++i) m = std::max(m, P.sol[i] + I.p[i]);
            if (m <= D && feasible(I, P.sol, m)) { sol = P.sol; mk = m; return true; }
            return false;                                            // cannot happen with a correct propagator
        }
        if (!P.stop) { exhausted = true; return false; }
    }
    return false;
}

inline void runProbe(const Inst& I, Info& in) {
    const auto t0 = std::chrono::steady_clock::now();
    const auto end = t0 + std::chrono::milliseconds(ProbeMs());
    const int L = std::max(in.lb, in.lbPlain);
    in.probeSeedIn = in.seed;
    auto take = [&](const std::vector<int>& s, int m) {
        if (in.seed < 0 || m < in.seed) { in.seed = m; in.seedStart = s; }
        if (in.probeBest < 0 || m < in.probeBest) in.probeBest = m;
    };
    std::vector<int> sol; int mk = 0; bool ex = false;
    if (!(in.seed > 0 && in.seed <= L)) {
        // 1) D = L: a schedule here is optimal
        const auto u1 = t0 + std::chrono::milliseconds(ProbeMs() * 6 / 10);
        if (probeDeadline(I, L, in.seed > 0 ? u1 : end, sol, mk, in.probeNodes, ex)) take(sol, mk);
        else if (ex) ++in.probeExhausted;
        // 2) better incumbents: D = incumbent - 1 downwards (with no incumbent: D = L + 1 upwards), half the rest each
        int D = in.seed > 0 ? in.seed - 1 : L + 1;
        while (!(in.seed > 0 && in.seed <= L) && D > L && D < I.horizon) {
            const auto now = std::chrono::steady_clock::now();
            if (now >= end - std::chrono::milliseconds(20)) break;
            const auto u = now + (end - now) / 2;
            if (probeDeadline(I, D, u, sol, mk, in.probeNodes, ex)) { take(sol, mk); D = mk - 1; }
            else { if (ex) ++in.probeExhausted; if (in.seed > 0) break; ++D; }
        }
    }
    in.probeSec = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
}

// ── large neighbourhood search on the incumbent (RCPSP_ROOT_LNS): free part of the best schedule, keep the rest ──
// Mode 1 (CP repair, Godard/Laborie/Nuijten 2005): the frozen activities keep their sequencing on every resource as
// precedence arcs (a partial-order schedule: they may still shift left), and the deadline probe looks for a schedule
// of that constrained instance with makespan <= U-1 under a node limit. The neighbourhood grows when the probe proves
// it empty and shrinks when the probe runs out of nodes. Mode 2 (SGS repair): serial SGS with the incumbent's start
// times as priority (which never starts a frozen activity later) and random keys for the freed block, then double
// justification; equal makespans are accepted so the walk can cross plateaus. Mode 3: both (one CP step per 10).
// PRIMAL ONLY: every schedule is checked against the original instance before it replaces the incumbent.
inline bool deriveOrder(Inst& J) {                  // topo / es0 / tail of J from its (extended) precedence arcs
    std::vector<int> indeg(J.n, 0);
    J.topo.clear();
    for (int i = 0; i < J.n; ++i) indeg[i] = (int)J.preds[i].size();
    for (int i = 0; i < J.n; ++i) if (indeg[i] == 0) J.topo.push_back(i);
    for (size_t h = 0; h < J.topo.size(); ++h)
        for (int s : J.succ[J.topo[h]]) if (--indeg[s] == 0) J.topo.push_back(s);
    if ((int)J.topo.size() != J.n) return false;
    J.es0.assign(J.n, 0); J.tail.assign(J.n, 0);
    for (int i : J.topo) for (int q : J.preds[i]) J.es0[i] = std::max(J.es0[i], J.es0[q] + J.p[q]);
    for (int h = J.n - 1; h >= 0; --h) {
        const int i = J.topo[h]; int m = 0;
        for (int s : J.succ[i]) m = std::max(m, J.tail[s]);
        J.tail[i] = J.p[i] + m;
    }
    return true;
}

// J = I plus, per resource, frozen i -> frozen j whenever j starts after i ends in S (first layer only: an arc that
// follows through a frozen k on the same resource is implied). Frozen activities that did not overlap in S can then
// never overlap, and any set that overlaps pairwise had a common point in S, so the frozen part stays resource-feasible.
inline void posArcs(const Inst& I, const std::vector<int>& S, const std::vector<char>& freed, Inst& J) {
    J.preds = I.preds; J.succ = I.succ;
    static thread_local std::vector<int> F;
    static thread_local std::vector<char> mark;
    mark.assign((size_t)I.n * I.n, 0);
    for (int k = 0; k < I.R; ++k) {
        F.clear();
        for (int i = 0; i < I.n; ++i) if (!freed[i] && I.p[i] > 0 && I.dem[k][i] > 0) F.push_back(i);
        std::sort(F.begin(), F.end(), [&](int a, int b) { return S[a] < S[b] || (S[a] == S[b] && a < b); });
        for (size_t a = 0; a < F.size(); ++a) {
            const int i = F[a], e = S[i] + I.p[i];
            int m = INT_MAX;
            for (size_t b = a + 1; b < F.size(); ++b) {
                const int j = F[b];
                if (S[j] < e) continue;
                if (S[j] >= m) break;
                m = std::min(m, S[j] + I.p[j]);
                char& seen = mark[(size_t)i * I.n + j];
                if (!seen) { seen = 1; J.succ[i].push_back(j); J.preds[j].push_back(i); }
            }
        }
    }
}

// kk activities (with duration) to free: a block consecutive in start order, the users of one resource nearest to a
// random time, or a random subset
inline void pickNeighbourhood(const Inst& I, const std::vector<int>& S, int kk, std::mt19937& rng, std::vector<char>& freed) {
    freed.assign(I.n, 0);
    std::vector<int> act;
    for (int i = 0; i < I.n; ++i) if (I.p[i] > 0) act.push_back(i);
    if (act.empty()) return;
    kk = std::max(1, std::min(kk, (int)act.size()));
    const unsigned type = rng() % 3;
    if (type == 0) {
        std::sort(act.begin(), act.end(), [&](int a, int b) { return S[a] < S[b] || (S[a] == S[b] && a < b); });
        const int s = (int)(rng() % (act.size() - kk + 1));
        for (int h = s; h < s + kk; ++h) freed[act[h]] = 1;
        return;
    }
    if (type == 1 && I.R > 0) {
        const int k = (int)(rng() % I.R);
        int smax = 0; for (int i : act) smax = std::max(smax, S[i]);
        const int t = (int)(rng() % (unsigned)(smax + 1));
        std::vector<int> users;
        for (int i : act) if (I.dem[k][i] > 0) users.push_back(i);
        std::sort(users.begin(), users.end(), [&](int a, int b) {
            const int da = std::abs(S[a] - t), db = std::abs(S[b] - t); return da < db || (da == db && a < b); });
        int got = 0;
        for (int i : users) { if (got == kk) break; freed[i] = 1; ++got; }
        if (got == kk) return;
        std::shuffle(act.begin(), act.end(), rng);
        for (int i : act) { if (got == kk) break; if (!freed[i]) { freed[i] = 1; ++got; } }
        return;
    }
    std::shuffle(act.begin(), act.end(), rng);
    for (int h = 0; h < kk; ++h) freed[act[h]] = 1;
}

// 1 = schedule with makespan <= D found (sol, mk), 0 = neighbourhood proved empty, -1 = node limit or time
inline int cpRepair(const Inst& J, int D, long limit, unsigned seed, std::chrono::steady_clock::time_point until,
                    std::vector<int>& sol, int& mk) {
    std::vector<int> es = J.es0, ls(J.n);
    for (int i = 0; i < J.n; ++i) ls[i] = D - J.tail[i];
    if (!propagate(J, D, es, ls)) return 0;
    Probe P(J, D, until);
    P.rng.seed(seed); P.limit = limit; P.randomTies = true;
    if (P.dfs(std::move(es), std::move(ls), std::vector<int>(J.n, -1), 0)) {
        int m = 0; for (int i = 0; i < J.n; ++i) m = std::max(m, P.sol[i] + J.p[i]);
        sol = P.sol; mk = m; return 1;
    }
    return P.stop ? -1 : 0;
}

inline int sgsRepair(const Inst& I, const std::vector<int>& S, const std::vector<char>& freed, std::mt19937& rng, std::vector<int>& sol) {
    int lo = INT_MAX, hi = 0;
    for (int i = 0; i < I.n; ++i) if (freed[i]) { lo = std::min(lo, S[i]); hi = std::max(hi, S[i] + I.p[i]); }
    if (lo == INT_MAX) return INT_MAX;
    std::uniform_real_distribution<double> u((double)lo, (double)std::max(lo + 1, hi));
    std::vector<double> key(I.n);
    for (int i = 0; i < I.n; ++i) key[i] = freed[i] ? u(rng) : (double)S[i];
    const int mk = sgs(I, I.preds, key, sol);
    if (mk == INT_MAX) return INT_MAX;
    return justify(I, sol, mk);
}

inline void runLns(const Inst& I, Info& in) {
    const auto t0 = std::chrono::steady_clock::now();
    const auto end = t0 + std::chrono::milliseconds(LnsMs());
    const int L = std::max(in.lb, in.lbPlain);
    in.lnsSeedIn = in.seed;
    if (in.seed > L && (int)in.seedStart.size() == I.n && feasible(I, in.seedStart, in.seed)) {
        const int mode = (int)LnsMode();
        int real = 0; for (int i = 0; i < I.n; ++i) if (I.p[i] > 0) ++real;
        std::vector<int> S = in.seedStart, sol;
        int U = in.seed, mk = 0;
        Inst J = I;
        std::mt19937 rng(424242u);
        std::vector<char> freed;
        double kcp = std::max(4.0, real / 8.0);
        long fails = 0;
        for (long it = 0; U > L; ++it) {
            if ((it & 15) == 0 && std::chrono::steady_clock::now() >= end) break;
            ++in.lnsIters;
            const bool cp = mode == 1 || (mode == 3 && it % 10 == 0);
            if (cp) {
                pickNeighbourhood(I, S, (int)kcp, rng, freed);
                posArcs(I, S, freed, J);
                if (!deriveOrder(J)) continue;                               // cannot happen: arcs follow S
                const bool plateau = fails > 0 && fails % 50 == 0;             // same makespan, new schedule
                const int D = plateau ? U : U - 1;
                const int r = cpRepair(J, D, LnsNodes(), rng(), end, sol, mk);
                if (r == 1 && mk <= D && feasible(I, sol, mk)) {
                    const int mj = justify(I, sol, mk);
                    if (mj < mk && feasible(I, sol, mj)) mk = mj;
                    if (mk < U) { ++in.lnsImpCp; fails = 0; } else ++fails;
                    S = sol; U = mk;
                } else {
                    ++fails;
                    if (r == 0) kcp = std::min((double)real, kcp * 1.1 + 0.5);    // proved empty: free more
                    else if (r < 0) kcp = std::max(2.0, kcp * 0.9);            // out of nodes: free less
                }
            } else {
                const int kk = 2 + (int)(rng() % (unsigned)std::max(1, real / 3));
                pickNeighbourhood(I, S, kk, rng, freed);
                const int m = sgsRepair(I, S, freed, rng, sol);
                if (m != INT_MAX && m <= U && feasible(I, sol, m)) {
                    if (m < U) { ++in.lnsImpSgs; fails = 0; }
                    S = sol; U = m;
                } else ++fails;
            }
        }
        if (U < in.seed) { in.seed = U; in.seedStart = S; in.lnsBest = U; }
    }
    in.lnsSec = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
}

// Computes whatever is switched on for the instance loaded in RCPSPex; returns info().closed.
inline bool computeRootBounds() {
    reset();
    if (!DlbOn() && !SeedOn() && !ProbeOn()) return false;
    Info& in = info();
    const Inst I = build();
    in.lbPlain = std::max(I.cpm, I.energy);
    // The destructive bound runs FIRST: it is cheap and it gives the seed loop a target. A seed that reaches the
    // bound is optimal, so the remaining seeds (and the probe) are skipped -- this is what keeps easy instances fast.
    if (DlbOn()) {
        const auto t0 = std::chrono::steady_clock::now();
        const auto until = t0 + std::chrono::milliseconds(BudgetMs());
        const int cap = I.horizon;
        int D = in.lbPlain;
        while (D < cap) {
            const int r = refuted(I, D, until);
            if (r < 0) { in.budgetHit = true; break; }
            if (r == 0) break;
            ++D;
        }
        in.lb = D;
        in.lbSec = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    }
    if (SeedOn()) {
        const auto t0 = std::chrono::steady_clock::now();
        in.seed = seedUB(I, in.seedStart, std::max(in.lb, in.lbPlain));
        in.seedSec = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    }
    if (ProbeOn()) runProbe(I, in);
    if (LnsOn() && SeedOn()) runLns(I, in);
    if (WinOn() && in.seed > 0) {                    // windows at D = seed: one propagation, reused all search
        std::vector<int> es = I.es0, ls(I.n);
        for (int i = 0; i < I.n; ++i) ls[i] = in.seed - I.tail[i];
        if (propagate(I, in.seed, es, ls)) {
            in.winD = in.seed; in.winEs = es; in.winLs = ls; in.winFp = instFp();
            if (envOn("RCPSP_ROOT_WIN_DIAG", false)) {       // is the window tighter than plain precedence?
                int nl = 0, sl = 0, ne = 0, se = 0;
                for (int i = 0; i < I.n; ++i) {
                    const int pl = in.seed - I.tail[i];
                    if (ls[i] < pl)      { ++nl; sl += pl - ls[i]; }
                    if (es[i] > I.es0[i]) { ++ne; se += es[i] - I.es0[i]; }
                }
                fprintf(stderr, "[win] D=%d lsTight=%d/%d sum=%d esTight=%d/%d sum=%d\n",
                        in.seed, nl, I.n, sl, ne, I.n, se);
            }
        }
    }
    in.closed = (in.seed > 0 && in.seed <= std::max(in.lb, in.lbPlain));
    return in.closed;
}

inline double seconds() { return info().lbSec + info().seedSec + info().probeSec + info().lnsSec; }

}  // namespace rootb
#endif
