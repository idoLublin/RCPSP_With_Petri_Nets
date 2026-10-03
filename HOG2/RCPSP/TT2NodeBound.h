#ifndef TT2_NODE_BOUND_H
#define TT2_NODE_BOUND_H
// ─────────────────────────────────────────────────────────────────────────────
// RCPSP_TT2_TT=1 — per-node deadline reasoning for the Petri-net search (the CBS RCPSP_CBS_TT mechanism,
// ported to TT2/HBAE). At a node we know: the unfinished activities, the activities in progress with their
// REMAINING time, and the elapsed time g. The remaining problem is therefore a smaller RCPSP starting now:
//
//   duration   d[i] = remaining time            (in progress)      |  p[i]  (not started)
//   earliest   es[i] = 0                        (in progress)      |  max over unfinished predecessors q of es[q] + d[q]
//   latest     ls[i] = D - tail[i]              with tail[i] = longest path i -> sink (remaining: tail[i] - p[i] + d[i])
//
// Ask: can the remaining work finish within D? Compulsory parts [ls, es+d) that overload a resource, or a window
// that empties, prove it cannot — so the remaining makespan is > D and h can be raised to D+1. Every raise is a
// proof, so h stays admissible; failure to refute leaves h untouched.
//
// Cost control: RCPSP_TT2_TT_STEPS deadlines per call (default 16 - the loop stops at the first deadline it
// cannot refute, so extra steps cost nothing unless they succeed; measured saturation at 16 on j30), the cheap compulsory-part test first, the full
// window tightening only under RCPSP_TT2_TT_FULL (default 1), and only for nodes with at least
// RCPSP_TT2_TT_MINACT unfinished activities (default 2). Optional RS gate: RCPSP_TT2_TT_MAXRS (default 1.0 = off).
// ─────────────────────────────────────────────────────────────────────────────
#include <vector>
#include <algorithm>
#include <chrono>
#include "RootBounds.h"

namespace tt2nb {

inline bool  On()      { static const bool  v = rootb::envOn("RCPSP_TT2_TT", false); return v; }
inline long  Steps()   { static const long  v = rootb::envNum("RCPSP_TT2_TT_STEPS", 16); return v; }
inline bool  Full()    { static const bool  v = rootb::envOn("RCPSP_TT2_TT_FULL", true); return v; }
inline long  MinAct()  { static const long  v = rootb::envNum("RCPSP_TT2_TT_MINACT", 2); return v; }
inline double MaxRS()  { static const double v = (double)rootb::envNum("RCPSP_TT2_TT_MAXRS_X100", 100) / 100.0; return v; }
// RCPSP_TT2_TT_ENERGY=1: add energetic reasoning to the refutation. Unlike the time-table it needs no compulsory
// part - it counts the work each activity MUST do inside an interval even when its position is still free -
// which is the case that tight-resource (RS=0.2) instances are made of.
inline bool  EnergyOn() { static const bool v = rootb::envOn("RCPSP_TT2_TT_ENERGY", false); return v; }
// RCPSP_TT2_WIN=1: with the shared root windows, an unfinished activity whose EARLIEST possible start
// (now + its residual earliest start) is already past winLs cannot fit a schedule <= winD, so h rises to
// winD + 1 - g. TT2 keeps no absolute starts, so only this late-side form exists - which is the sound one.
inline bool  WinOn()    { static const bool v = rootb::envOn("RCPSP_TT2_WIN", false); return v; }
// RCPSP_TT2_TT_SER=1: suffix energetic reasoning. For every candidate start t the residual activities must
// still do min(D-t, d_i, es_i + d_i - t) units of work inside [t, D] - whatever their position - so more than
// cap_k * (D - t) of it refutes D. Unlike the time table this needs no compulsory part, and unlike full ER it
// fixes t2 = D (the only endpoint a DEADLINE refutation cares about), so it is O(n^2 R) not O(n^3 R).
inline bool  SerOn()    { static const bool v = rootb::envOn("RCPSP_TT2_TT_SER", false); return v; }
// RCPSP_TT2_WINLS=1: also hand the root windows to the per-node deadline refutation. Any completion of this
// node with residual makespan <= D ends at gNow + D; when that is <= winD the whole schedule obeys the root
// windows, so a not-yet-started activity cannot start later than winLs - gNow. Strictly tighter ls at no cost.
inline bool  WinLsOn()  { static const bool v = rootb::envOn("RCPSP_TT2_WINLS", false); return v; }
inline bool& winTighten() { static thread_local bool v = false; return v; }   // windows valid for this node
inline int&  winG()       { static thread_local int  v = -1;    return v; }   // absolute time at this node

struct Stats { long calls = 0, raised = 0, steps = 0, tt0Fail = 0, propFail = 0; double sec = 0.0; long raiseSum = 0; long winPruned = 0, serFail = 0; };
inline Stats& st() { static Stats s; return s; }

// HBAE alternates between the forward and the reversed net within one solve, so keep a SMALL CACHE of built
// instances keyed by fingerprint instead of rebuilding on every side switch.
struct Ctx { rootb::Inst I; bool built = false; uint64_t fp = 0; };
inline Ctx& ctx() { static Ctx c; return c; }
inline std::vector<std::pair<uint64_t, rootb::Inst>>& cache() { static std::vector<std::pair<uint64_t, rootb::Inst>> c; return c; }
inline void reset() { ctx().built = false; ctx().fp = 0; cache().clear(); st() = Stats{}; }
// RCPSPex is swapped for the reversed instance and inside sub-solves, so a cached Inst can belong to a DIFFERENT
// problem - which would make the bound inadmissible. Fingerprint the loaded instance and rebuild when it changes.
inline uint64_t fingerprint() { return (uint64_t)rootb::instFp(); }
#if 0
inline uint64_t fingerprintOld() {
    uint64_t h = 1469598103934665603ULL;
    auto mix = [&h](uint64_t v) { h = (h ^ v) * 1099511628211ULL; };
    mix(RCPSPex.activities.size()); mix(RCPSPex.resources.size());
    for (size_t i = 0; i < RCPSPex.activities.size(); ++i) {
        mix((uint64_t)RCPSPex.activities[i].duration);
        mix((uint64_t)RCPSPex.backword_dependencies[i].size());
    }
    for (const auto& r : RCPSPex.resources) mix((uint64_t)r.second);
    return h;
}
#endif
inline void build() {                       // fingerprint -> Inst, at most 4 kept (forward + reversed + slack)
    const uint64_t fp = fingerprint();
    for (auto& e : cache()) if (e.first == fp) { ctx().I = e.second; ctx().built = true; ctx().fp = fp; return; }
    rootb::Inst I = rootb::build();
    if (cache().size() >= 4) cache().erase(cache().begin());
    cache().emplace_back(fp, I);
    ctx().I = std::move(I); ctx().built = true; ctx().fp = fp;
}

// scratch, reused across calls
struct Buf {
    std::vector<int> d, es, ls, order;
    std::vector<char> live, started;
    std::vector<std::vector<int>> prof;
};
inline Buf& buf() { static thread_local Buf b; return b; }

// true = D is refuted: the remaining work cannot finish within D
inline bool refuted(int D, int nlive) {
    const rootb::Inst& I = ctx().I;
    Buf& b = buf();
    if (D < 0) return true;
    const int H = D + 2;
    // the root windows hold for schedules of length <= winD; this node's completions end at winG + D
    const bool wt = winTighten() && winG() >= 0 && winG() + D <= rootb::info().winD;
    const std::vector<int>& wls = rootb::info().winLs;
    // ── windows ────────────────────────────────────────────────────────────
    for (int i = 0; i < I.n; ++i) {
        if (!b.live[i]) continue;
        b.ls[i] = D - (I.tail[i] - I.p[i] + b.d[i]);
        if (wt && !b.started[i]) b.ls[i] = std::min(b.ls[i], wls[i] - winG());   // root windows, shifted to now
        if (b.es[i] > b.ls[i]) return true;
    }
    // ── compulsory parts (cheap test) ──────────────────────────────────────
    auto& prof = b.prof;
    if ((int)prof.size() < I.R) prof.resize(I.R);
    for (int k = 0; k < I.R; ++k) { if ((int)prof[k].size() < H) prof[k].resize(H); std::fill(prof[k].begin(), prof[k].begin() + H, 0); }
    for (int i = 0; i < I.n; ++i) {
        if (!b.live[i] || b.d[i] <= 0) continue;
        const int a = b.ls[i], e = b.es[i] + b.d[i];
        if (a < e) for (int k : I.resOf[i]) for (int t = a; t < e && t <= D; ++t) prof[k][t] += I.dem[k][i];
    }
    for (int k = 0; k < I.R; ++k)
        for (int t = 0; t <= D; ++t)
            if (prof[k][t] > I.cap[k]) { ++st().tt0Fail; return true; }
    // ── suffix energetic reasoning (cheap ER: t2 = D) ─────────────────────
    if (SerOn()) {
        b.order.clear();
        for (int i = 0; i < I.n; ++i) if (b.live[i] && b.d[i] > 0) b.order.push_back(b.es[i]);
        std::sort(b.order.begin(), b.order.end());
        b.order.erase(std::unique(b.order.begin(), b.order.end()), b.order.end());
        for (int t1 : b.order) {
            const int L = D - t1;
            if (L <= 0) continue;
            for (int k = 0; k < I.R; ++k) {
                long W = 0; const long capL = (long)I.cap[k] * L;
                for (int i = 0; i < I.n; ++i) {
                    if (!b.live[i] || b.d[i] <= 0) continue;
                    const int dem = I.dem[k][i];
                    if (dem == 0) continue;
                    const int ov = std::min(std::min(L, b.d[i]), b.es[i] + b.d[i] - t1);
                    if (ov > 0) { W += (long)dem * ov; if (W > capL) { ++st().serFail; return true; } }
                }
            }
        }
        b.order.clear();
        for (int idx = 0; idx < I.n; ++idx) { const int i = I.topo[idx]; if (b.live[i]) b.order.push_back(i); }
    }
    // ── energetic reasoning over the interval endpoints of the live activities ──
    if (EnergyOn()) {
        Buf& bb = b;
        bb.order.clear();                                   // reuse as the endpoint list
        for (int i = 0; i < I.n; ++i) if (bb.live[i] && bb.d[i] > 0) { bb.order.push_back(bb.es[i]); bb.order.push_back(bb.ls[i]); }
        bb.order.push_back(0); bb.order.push_back(D);
        std::sort(bb.order.begin(), bb.order.end());
        bb.order.erase(std::unique(bb.order.begin(), bb.order.end()), bb.order.end());
        const int M = (int)bb.order.size();
        for (int x = 0; x < M; ++x) {
            for (int y = x + 1; y < M; ++y) {
                const int t1 = bb.order[x], t2 = bb.order[y];
                const int L = t2 - t1;
                if (L <= 0) continue;
                for (int k = 0; k < I.R; ++k) {
                    long W = 0; const long capL = (long)I.cap[k] * L;
                    for (int i = 0; i < I.n; ++i) {
                        if (!bb.live[i] || bb.d[i] <= 0) continue;
                        const int dem = I.dem[k][i];
                        if (dem == 0) continue;
                        const int ov = std::min(std::min(L, bb.d[i]),
                                                std::min(bb.es[i] + bb.d[i] - t1, t2 - bb.ls[i]));
                        if (ov > 0) { W += (long)dem * ov; if (W > capL) { ++st().propFail; return true; } }
                    }
                }
            }
        }
        bb.order.clear();
        for (int idx = 0; idx < I.n; ++idx) { const int i = I.topo[idx]; if (bb.live[i]) bb.order.push_back(i); }
    }
    if (!Full()) return false;
    // ── window tightening against the forced demand ────────────────────────
    for (int pass = 0; pass < 3; ++pass) {
        bool changed = false;
        for (int i = 0; i < I.n; ++i) {
            if (!b.live[i] || b.d[i] <= 0 || I.resOf[i].empty()) continue;
            const int a = b.ls[i], e = b.es[i] + b.d[i];
            int t = b.es[i];
            while (t <= b.ls[i]) {                       // push es past any placement covering a blocked point
                int last = -1;
                for (int u = t; u < t + b.d[i] && u <= D; ++u) {
                    bool bad = false;
                    for (int k : I.resOf[i]) {
                        const int other = prof[k][u] - ((a < e && u >= a && u < e) ? I.dem[k][i] : 0);
                        if (other + I.dem[k][i] > I.cap[k]) { bad = true; break; }
                    }
                    if (bad) last = u;
                }
                if (last >= 0) t = last + 1; else break;
            }
            if (t > b.ls[i]) { ++st().propFail; return true; }
            if (t > b.es[i]) { b.es[i] = t; changed = true; }
        }
        if (!changed) break;
        // successors inherit the pushed earliest starts
        for (int idx = 0; idx < (int)b.order.size(); ++idx) {
            const int i = b.order[idx];
            for (int s : I.succ[i]) if (b.live[s]) b.es[s] = std::max(b.es[s], b.es[i] + b.d[i]);
        }
        for (int i = 0; i < I.n; ++i) if (b.live[i] && b.es[i] > b.ls[i]) { ++st().propFail; return true; }
    }
    return false;
}

// h0 = the admissible heuristic we already have (remaining makespan). Returns a value >= h0, still admissible.
// unfinished: 1-based activity ids; active: (1-based id, remaining time) for the ones in progress.
template <class VecU, class VecA>
inline int raise(const VecU& unfinished, const VecA& active, int h0, int gNow = -1) {
    if ((!On() && !WinOn()) || g_in_subsolve) return h0;   // sub-solves use a relaxed instance: never bound with it
    if (!ctx().built || ctx().fp != fingerprint()) build();
    const rootb::Inst& I = ctx().I;
    if (I.n <= 0) return h0;
    if ((long)unfinished.size() < MinAct()) return h0;
    const auto t0 = std::chrono::steady_clock::now();
    ++st().calls;
    Buf& b = buf();
    b.d.assign(I.n, 0); b.es.assign(I.n, 0); b.ls.assign(I.n, 0); b.live.assign(I.n, 0);
    int nlive = 0;
    for (const auto& u : unfinished) {
        const int i = (int)u - 1;
        if (i < 0 || i >= I.n) continue;
        b.live[i] = 1; b.d[i] = I.p[i]; ++nlive;
    }
    b.started.assign(I.n, 0);
    for (const auto& a : active) {
        const int i = (int)a.first - 1;
        if (i < 0 || i >= I.n || !b.live[i]) continue;
        b.d[i] = std::max(0, (int)a.second);            // only the remaining time is still to be done
        b.started[i] = 1;                               // in progress: it STARTED in the past, not at `now`
    }
    // earliest starts: topological forward pass over the live sub-DAG (in-progress ones start now)
    b.order.clear();
    for (int idx = 0; idx < I.n; ++idx) {
        const int i = I.topo[idx];
        if (!b.live[i]) continue;
        b.order.push_back(i);
        for (int q : I.preds[i]) if (b.live[q]) b.es[i] = std::max(b.es[i], b.es[q] + b.d[q]);
    }
    for (const auto& a : active) {                       // in progress => already running, cannot be pushed
        const int i = (int)a.first - 1;
        if (i >= 0 && i < I.n && b.live[i]) b.es[i] = 0;
    }
    const bool winAvail = gNow >= 0 && !rootb::info().winLs.empty()
                          && rootb::info().winFp == (unsigned long long)ctx().fp;  // not the reversed net / a sub-solve
    winTighten() = WinLsOn() && winAvail; winG() = gNow;
    if (WinOn() && winAvail) {                                      // late-side window test
        const auto& wls = rootb::info().winLs;
        for (int i = 0; i < I.n && i < (int)wls.size(); ++i)
            if (b.live[i] && !b.started[i] && b.d[i] > 0 && gNow + b.es[i] > wls[i]) {   // not-yet-started only
                ++st().winPruned;
                const int hw = rootb::info().winD + 1 - gNow;
                st().sec += std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
                return std::max(h0, hw);
            }
    }
    if (!On()) { st().sec += std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count(); return h0; }
    int h = h0;
    for (long s = 0; s < Steps(); ++s) {
        const std::vector<int> es0 = b.es;               // refuted() tightens es; restore for the next deadline
        ++st().steps;
        if (refuted(h, nlive)) { ++h; b.es = es0; } else { b.es = es0; break; }
    }
    st().sec += std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    if (h > h0) { ++st().raised; st().raiseSum += (h - h0); }
    return h;
}

}  // namespace tt2nb
#endif
