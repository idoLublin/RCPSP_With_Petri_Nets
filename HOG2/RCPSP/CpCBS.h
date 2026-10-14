#ifndef CP_CBS_H
#define CP_CBS_H
// ─────────────────────────────────────────────────────────────────────────────
// CP ELEMENTS FOR CBS — the three things CP solvers have that the CBS delay search lacked.
//
// (1) HONEST INCUMBENT. When any RootBounds.h flag is on (RCPSP_ROOT_DLB / _SEED / _PROBE) the CBS driver
//     takes its incumbent from the verified root seed instead of the datasheet (RCPSP_UB), closes the
//     instance at the root when seed <= root LB, and, because the incumbent is a REAL schedule, prunes
//     children whose makespan is >= the incumbent (not only >) and nodes whose f >= incumbent at expansion.
//     The root LB also floors h.
// (2) RCPSP_CBS_TT=1  deadline time-table propagation per node. CBS only pushes start times forward, so a
//     node's start_times are earliest starts for every schedule below it. With deadline D every activity
//     also has a latest start D - tail. If the compulsory parts [D - tail_i, start_i + p_i) overload a
//     resource (or RootBounds' propagate() fails), no schedule below the node ends by D: f >= D + 1.
//     With a real incumbent U it tests D = U - 1 first (a hit makes f >= U: pruned at expansion), then
//     raises the bound from the current f for RCPSP_CBS_TT_STEPS deadlines (default 2).
//     Uses ONLY start_times + the static network, because CBS state identity is start_times alone
//     (operator== ignores added_precedences) — a bound that read added_precedences could let a pruned,
//     more constrained duplicate shadow an unconstrained one.
// (3) RCPSP_CBS_NG=1 (needs TT) nogood learning. A direct overload at (resource k, time t) is explained by
//     activities A with sum demand > cap whose compulsory parts cover t: literals start_i >= t - p_i + 1,
//     valid for every deadline D' <= min_i (t + tail_i). Any later node satisfying all literals is refuted
//     up to that deadline without propagation (lazy-clause-generation style, cf. Schutt et al. 2011;
//     Lazy CBS, Gange, Harabor & Stuckey 2019). Monotone in start_times, so consistent with state identity.
//     Store capped by RCPSP_CBS_NG_MAX (default 20000).
// All of it is skipped inside nested sub-solves (g_in_subsolve) and before buildInstance() of the instance.
// ─────────────────────────────────────────────────────────────────────────────
#include "RootBounds.h"
#include <vector>
#include <chrono>
#include <algorithm>
#include <unordered_set>
#include <climits>

namespace cpcbs {

inline bool TTOn()    { static const bool v = rootb::envOn("RCPSP_CBS_TT", false); return v; }
inline bool NGOn()    { static const bool v = rootb::envOn("RCPSP_CBS_NG", false) && TTOn(); return v; }
// RCPSP_CBS_LSPROP=1: score conflicts with the PROPAGATED latest starts (deadline D = incumbent-1,
// time-table + window tightening) instead of the precedence-only CPM latest starts. Conflict choice only:
// CBS stays exact whichever conflict it branches on, so this cannot affect correctness, only the tree.
// mode 1: deadline = incumbent-1 (loose when the incumbent is far above the bound)
// mode 2: deadline = the NODE's own bound - start at max(root LB, this node's makespan) and step up until
//         propagation survives (at most RCPSP_CBS_LSPROP_STEPS). Tightest windows consistent with the node.
inline long LsPropMode()  { static const long v = rootb::envNum("RCPSP_CBS_LSPROP", 0); return v; }
inline bool LsPropOn()    { return LsPropMode() > 0; }
inline long LsPropSteps() { static const long v = rootb::envNum("RCPSP_CBS_LSPROP_STEPS", 4); return v; }
// RCPSP_CBS_ROOTES=1: start the search from the PROPAGATED earliest starts at D = incumbent-1 instead of the
// CPM ones. Every schedule shorter than the incumbent has start_i >= es_i, so all of them remain reachable
// (CBS only ever delays); the subtrees below the propagated starts can only hold schedules >= the incumbent.
// Propagation failing at D = U-1 is itself a proof that the incumbent is optimal.
// mode 1 (safe): D = the incumbent itself - every optimal schedule still has start_i >= es_i, so the search keeps
//                 its normal meaning and reporting. mode 2: D = incumbent-1 (prove-or-improve; needs the
//                 exhaustion path to report the incumbent, which is NOT wired - left for later).
inline long RootEsMode() { static const long v = rootb::envNum("RCPSP_CBS_ROOTES", 0); return v; }
inline bool RootEsOn()   { return RootEsMode() > 0; }
// RCPSP_CBS_WIN=1: prune with the shared root windows (RCPSP_ROOT_WIN). LATE side only - CBS starts grow
// monotonically, so start_i > winLs_i means no schedule <= winD exists anywhere below. The root itself is
// never pruned: there every activity still sits at its CPM start, which is <= winLs.
inline bool WinOn()      { static const bool v = rootb::envOn("RCPSP_CBS_WIN", false); return v; }
// default 16: the loop stops at the first deadline it cannot refute, so extra steps cost nothing unless they
// succeed. Measured on the valid CBS (no root/UB), 26 j30 edge instances: steps 2 -> 2.05M expansions,
// steps 16 -> 0.92M (base without TT 9.76M). Same trap as RCPSP_TT2_TT_STEPS.
inline long TTSteps() { static const long v = rootb::envNum("RCPSP_CBS_TT_STEPS", 16); return v; }
inline long NGMax()   { static const long v = rootb::envNum("RCPSP_CBS_NG_MAX", 20000); return v; }
inline bool FloorOn() { static const bool v = rootb::envOn("RCPSP_CBS_ROOTFLOOR", true); return v; }   // root LB as h floor
inline bool TTFull()  { static const bool v = rootb::envOn("RCPSP_CBS_TT_FULL", true); return v; }     // 0 = compulsory-part overload test only
// RCPSP_CBS_MDAREF=1: after a successful propagation at D, refute D if every MDA of the branching conflict has
// a member whose forced start NewStart(M) is past its propagated latest start (see mdaRefuted below).
inline bool MdaRefOn() { static const bool v = rootb::envOn("RCPSP_CBS_MDAREF", false); return v; }
// RCPSP_CBS_TT_ER=1: energetic reasoning after a successful propagation (see refuted). Only on instances with
// RS <= RCPSP_CBS_TT_ER_MAXRS_X100 / 100 (default 100 = all).
inline bool ErOn()     { static const bool v = rootb::envOn("RCPSP_CBS_TT_ER", false); return v; }
inline double ErMaxRS(){ static const double v = rootb::envNum("RCPSP_CBS_TT_ER_MAXRS_X100", 100) / 100.0; return v; }
inline bool ErActive() { return ErOn() && (g_instance_rs < 0.0 || g_instance_rs <= ErMaxRS() + 1e-9); }
inline bool RootOn()  { return rootb::DlbOn() || rootb::SeedOn() || rootb::ProbeOn(); }

struct Stats {
    int ubSource = 0;            // 0 none, 1 datasheet (RCPSP_UB), 2 root seed
    bool realInc = false;        // incumbent is a real schedule (root seed)
    bool closedAtRoot = false;
    long ttCalls = 0, ttRaised = 0, ttPrunedAtExpand = 0;
    long ngLearned = 0, ngHits = 0;
    double ttSec = 0.0;
    long lsCalls = 0, lsFail = 0, lsTighter = 0, lsScoreDiff = 0, lsPickDiff = 0;   // RCPSP_CBS_LSPROP
    long rootEsMoved = 0; bool rootEsRefuted = false;                              // RCPSP_CBS_ROOTES
    long winPruned = 0, winChecks = 0;                                             // RCPSP_CBS_WIN
    long mdaRef = 0, mdaRefChecks = 0;                                             // RCPSP_CBS_MDAREF
    long erRefuted = 0;                                                            // RCPSP_CBS_TT_ER
    double lsSec = 0.0;
    long warmUsed = 0, warmQuick = 0, warmNoSlot = 0, warmNotLater = 0, warmUpMade = 0, warmUpUsed = 0;   // RCPSP_CBS_TT_WARM
};
inline Stats& st() { static Stats s; return s; }

struct Nogood { short dmax = 0; std::vector<std::pair<short, short>> lits; };   // refuted for D <= dmax if start[a] >= b for all lits

struct Ctx {
    rootb::Inst I;
    bool built = false;
    int rootLB = -1;
    std::vector<Nogood> ng;
    std::unordered_set<uint64_t> ngKeys;
    std::vector<std::vector<int>> prof;
    // RCPSP_CBS_TT_WARM: slot (hash of node serial and deadline) = the start times, deadline and propagated windows of a
    // node's successful propagation. A slot is used only at the same deadline and only if the node's start times are
    // all no later than the child's, so a stale or foreign slot is still a valid outer bound (never wrong, only useless).
    long wK = 0; int wN = 0;
    std::vector<unsigned> wSeq;
    std::vector<int> wD;                  // -1 = empty
    std::vector<short> wBuf;              // per slot: start, es, ls (wN each)
};
inline Ctx& ctx() { static Ctx c; return c; }

// RCPSP_CBS_TT_WARM=1: start a child's propagation from its parent's fixpoint at the same deadline (rootb::propagateInc
// warm start); 2 = also run the cold propagation and abort on any difference. Needs RCPSP_PROP_FAST=1. Same search.
// RCPSP_CBS_TT_WARM_SLOTS: number of remembered (node, deadline) pairs (a power of two, default 65536; 6 x n bytes each).
inline long WarmMode()  { static const long v = rootb::envNum("RCPSP_CBS_TT_WARM", 0); return v; }
inline long WarmSlots() { static const long v = rootb::envNum("RCPSP_CBS_TT_WARM_SLOTS", 65536); return v; }

inline void resetInstance() { st() = Stats{}; Ctx& c = ctx(); c.built = false; c.rootLB = -1; c.ng.clear(); c.ngKeys.clear(); }
// call once the real instance is loaded (after warm start / anything that swaps RCPSPex)
inline void buildInstance(int rootLB) {
    Ctx& c = ctx(); c.I = rootb::build(); c.built = true; c.rootLB = FloorOn() ? rootLB : -1;
    if (WarmMode()) {                     // new instance or mirrored project: forget every slot
        long K = 1; while (K < WarmSlots()) K <<= 1;
        c.wK = K; c.wN = c.I.n;
        c.wSeq.assign(K, 0); c.wD.assign(K, -1); c.wBuf.assign((size_t)K * 3 * c.wN, 0);
    }
}
inline bool Active() { return ctx().built && !g_in_subsolve; }
// real incumbent => prune makespan >= incumbent instead of >
inline int GeShift() { return (st().realInc && g_use_ub && !g_in_subsolve) ? 1 : 0; }

// time-table check with the compulsory parts of this node for deadline D; on overload fill `out` (if given)
template <class Arr>
inline bool tt0(const rootb::Inst& I, const Arr& start, int D, Nogood* out) {
    auto& prof = ctx().prof;
    const int H = D + 2;
    if ((int)prof.size() < I.R) prof.resize(I.R);
    for (int k = 0; k < I.R; ++k) { if ((int)prof[k].size() < H) prof[k].resize(H); std::fill(prof[k].begin(), prof[k].begin() + H, 0); }
    for (int i = 0; i < I.n; ++i) {
        if (start[i] + I.tail[i] > D) {                         // window empty (cannot happen for D >= node makespan)
            if (out) { out->lits = {{(short)i, (short)start[i]}}; out->dmax = (short)(start[i] + I.tail[i] - 1); }
            return true;
        }
        if (I.p[i] == 0 || I.resOf[i].empty()) continue;
        const int a = std::max(0, D - I.tail[i]), b = std::min(H, start[i] + I.p[i]);
        for (int k : I.resOf[i]) for (int t = a; t < b; ++t) prof[k][t] += I.dem[k][i];
    }
    for (int k = 0; k < I.R; ++k)
        for (int t = 0; t <= D; ++t) {
            if (prof[k][t] <= I.cap[k]) continue;
            if (out) {
                std::vector<int> A;
                for (int i = 0; i < I.n; ++i)
                    if (I.dem[k][i] > 0 && I.p[i] > 0 && D - I.tail[i] <= t && start[i] + I.p[i] > t) A.push_back(i);
                std::sort(A.begin(), A.end(), [&](int x, int y) { return I.dem[k][x] > I.dem[k][y]; });
                int sum = 0, dmax = INT_MAX;
                out->lits.clear();
                for (int i : A) {
                    sum += I.dem[k][i];
                    dmax = std::min(dmax, t + I.tail[i]);
                    const int bnd = t - I.p[i] + 1;
                    if (bnd > I.es0[i]) out->lits.push_back({(short)i, (short)bnd});   // otherwise true at every node
                    if (sum > I.cap[k]) break;
                }
                out->dmax = (short)std::min(dmax, (int)SHRT_MAX);
                std::sort(out->lits.begin(), out->lits.end(), [&](const std::pair<short, short>& x, const std::pair<short, short>& y) {
                    return x.second - I.es0[x.first] > y.second - I.es0[y.first]; });            // most selective first
            }
            return true;
        }
    return false;
}

inline void store(const Nogood& g) {
    Ctx& c = ctx();
    if ((long)c.ng.size() >= NGMax()) return;
    std::vector<std::pair<short, short>> key = g.lits;
    std::sort(key.begin(), key.end());
    uint64_t h = 1469598103934665603ULL ^ (uint64_t)(uint16_t)g.dmax;
    for (auto& l : key) { h = (h ^ (uint64_t)(uint16_t)l.first) * 1099511628211ULL; h = (h ^ (uint64_t)(uint16_t)l.second) * 1099511628211ULL; }
    if (!c.ngKeys.insert(h).second) return;
    c.ng.push_back(g);
    ++st().ngLearned;
}

// largest dmax among nogoods this node satisfies (-1 = none)
template <class Arr>
inline int ngBound(const Arr& start) {
    int best = -1;
    for (const Nogood& g : ctx().ng) {
        if (g.dmax <= best) continue;
        bool ok = true;
        for (const auto& l : g.lits) if (start[l.first] < l.second) { ok = false; break; }
        if (ok) best = g.dmax;
    }
    return best;
}

// windows left by the last full propagation in refuted(); valid right after refuted() returned false
inline std::vector<int>& esBuf() { static thread_local std::vector<int> v; return v; }
inline std::vector<int>& lsBuf() { static thread_local std::vector<int> v; return v; }

template <class Arr>
inline bool refuted(const Arr& start, int D) {
    const rootb::Inst& I = ctx().I;
    Nogood g;
    // RCPSP_PROP_FAST=1 without nogoods: skip the pre-check. It refutes D exactly when the first round of propagate
    // does (same windows [start, D - tail], same compulsory parts, same overload test), so the answer is unchanged.
    if (!(rootb::PropFast() == 1 && !NGOn()) && tt0(I, start, D, NGOn() ? &g : nullptr)) { if (NGOn()) store(g); return true; }
    if (!TTFull()) return false;
    // reused across calls: with RCPSP_CBS_TT_STEPS=16 this runs up to 17 times per node
    std::vector<int>& es = esBuf();
    std::vector<int>& ls = lsBuf();
    es.resize(I.n); ls.resize(I.n);
    for (int i = 0; i < I.n; ++i) { es[i] = start[i]; ls[i] = D - I.tail[i]; }
    if (!rootb::propagate(I, D, es, ls)) return true;
    if (ErActive() && !rootb::energeticOk(I, es, ls)) { ++st().erRefuted; return true; }
    return false;
}

// RCPSP_CBS_TT_WARM: the warm start applies where refuted() runs the incremental propagation with no nogood pre-check
inline bool WarmOn() {
    const Ctx& c = ctx();
    return WarmMode() && c.wK > 0 && TTFull() && rootb::PropFast() == 1 && !NGOn() && !(rootb::PairOn() && !c.I.inc.empty());
}
// RCPSP_CBS_TT_WARM_UP=1: a child refuted at its parent's deadline goes on to D + 1, D + 2, ... The parent is not refuted
// there either (refutation only spreads to smaller deadlines), so its fixpoint at those deadlines exists: computed
// once, from the start times in its slot, when the first child needs it, and every later child starts there too.
inline long WarmUp() { static const long v = rootb::envNum("RCPSP_CBS_TT_WARM_UP", 0); return v; }
inline size_t warmIdx(unsigned seq, int D) {
    return (size_t)((seq * 0x9E3779B1u) ^ ((unsigned)D * 0x85EBCA77u)) & (size_t)(ctx().wK - 1);
}
// slot (self, D): start times and the fixpoint at D (es / ls; default the buffers of the last successful propagation)
template <class Arr>
inline void warmStore(const Arr& start, int D, unsigned self,
                      const std::vector<int>& es = esBuf(), const std::vector<int>& ls = lsBuf()) {
    Ctx& c = ctx();
    const size_t s = warmIdx(self, D);
    c.wSeq[s] = self; c.wD[s] = D;
    short* b = &c.wBuf[s * 3 * c.wN];
    for (int i = 0; i < c.wN; ++i) { b[i] = (short)start[i]; b[c.wN + i] = (short)es[i]; b[2 * c.wN + i] = (short)ls[i]; }
}
inline std::vector<short>& parBuf() { static thread_local std::vector<short> v; return v; }   // the parent's start times
// the parent's slot at D if it can start this node's propagation there, else nullptr. havePar: parBuf() holds the
// parent's start times (all no later than this node's), taken from the first slot of the parent found.
template <class Arr>
inline const short* warmFind(const Arr& start, int D, unsigned parent, bool& havePar) {
    Ctx& c = ctx();
    const int n = c.wN;
    const size_t s = warmIdx(parent, D);
    if (c.wSeq[s] == parent && c.wD[s] == D) {
        const short* b = &c.wBuf[s * 3 * n];
        for (int i = 0; i < n; ++i) if (start[i] < b[i]) { ++st().warmNotLater; return nullptr; }   // also a foreign slot
        if (havePar) ++st().warmUpUsed;
        else { parBuf().assign(b, b + n); havePar = true; }
        return b;
    }
    if (!havePar || !WarmUp()) { ++st().warmNoSlot; return nullptr; }
    static thread_local std::vector<int> e, l;
    const rootb::Inst& I = c.I;
    const std::vector<short>& par = parBuf();
    e.resize(I.n); l.resize(I.n);
    for (int i = 0; i < I.n; ++i) { e[i] = par[i]; l[i] = D - I.tail[i]; }
    if (!rootb::propagate(I, D, e, l)) return nullptr;     // cannot happen: the parent survived a smaller deadline
    ++st().warmUpMade;
    warmStore(par, D, parent, e, l);
    return &c.wBuf[s * 3 * n];
}
// refuted(start, D) started from the slot b (same D, start times no later than these): same answer, same windows
template <class Arr>
inline bool refutedWarm(const Arr& start, int D, const short* b) {
    const rootb::Inst& I = ctx().I;
    const int n = ctx().wN;
    const short* pEs = b + n; const short* pLs = b + 2 * n;
    ++st().warmUsed;
    std::vector<int>& es = esBuf();
    std::vector<int>& ls = lsBuf();
    es.resize(I.n); ls.resize(I.n);
    bool empty = false;
    for (int i = 0; i < I.n; ++i) {
        es[i] = std::max((int)start[i], (int)pEs[i]); ls[i] = pLs[i];   // pLs <= D - tail already
        if (es[i] > ls[i]) empty = true;
    }
    bool ref;
    if (empty) { ++st().warmQuick; ref = true; }             // a window of the parent's fixpoint is already empty
    else {
        ref = !rootb::propagateInc(I, D, es, ls, true, pEs, pLs);
        if (!ref && ErActive() && !rootb::energeticOk(I, es, ls)) { ++st().erRefuted; ref = true; }
    }
    if (WarmMode() == 2) {                                    // the cold call must agree
        static thread_local std::vector<int> e2, l2;
        e2.resize(I.n); l2.resize(I.n);
        for (int i = 0; i < I.n; ++i) { e2[i] = start[i]; l2[i] = D - I.tail[i]; }
        bool ref2 = !rootb::propagate(I, D, e2, l2);
        if (!ref2 && ErActive() && !rootb::energeticOk(I, e2, l2)) ref2 = true;
        if (ref != ref2 || (!ref && (es != e2 || ls != l2))) {
            std::cerr << "RCPSP_CBS_TT_WARM MISMATCH at D=" << D << ": warm " << ref << " cold " << ref2 << std::endl;
            if (!ref && !ref2) for (int i = 0; i < I.n; ++i) if (es[i] != e2[i] || ls[i] != l2[i])
                std::cerr << "  act " << i << ": warm [" << es[i] << "," << ls[i] << "] cold [" << e2[i] << "," << l2[i] << "]" << std::endl;
            std::abort();
        }
    }
    return ref;
}

// Latest starts of THIS node under "beat the incumbent": propagate from its start times with D = U-1.
// false = no incumbent yet, flag off, or the node is already refuted at U-1 (the caller then keeps the
// CPM values; the node dies on its bound anyway). Used only to score conflicts.
template <class Arr>
inline bool propLatestStarts(const Arr& start, std::vector<int>& ls) {
    if (!LsPropOn() || !Active()) return false;
    const rootb::Inst& I = ctx().I;
    const int U = (g_use_ub && st().realInc && g_incumbent < SHRT_MAX) ? (int)g_incumbent : INT_MAX;
    int D0, Dmax;
    if (LsPropMode() >= 2) {
        int mk = 0;
        for (int i = 0; i < I.n; ++i) mk = std::max(mk, (int)start[i] + I.p[i]);
        D0 = std::max(ctx().rootLB, mk);
        Dmax = D0 + (int)LsPropSteps();
        if (U != INT_MAX) Dmax = std::min(Dmax, U - 1);
    } else {
        if (U == INT_MAX || U <= 1) return false;
        D0 = Dmax = U - 1;
    }
    if (Dmax < D0) return false;
    const auto t0 = std::chrono::steady_clock::now();
    static thread_local std::vector<int> es;
    bool ok = false;
    for (int D = D0; D <= Dmax && !ok; ++D) {
        ++st().lsCalls;
        es.resize(I.n); ls.assign(I.n, 0);
        for (int i = 0; i < I.n; ++i) { es[i] = (int)start[i]; ls[i] = D - I.tail[i]; }
        ok = rootb::propagate(I, D, es, ls);
        if (!ok) ++st().lsFail;
    }
    st().lsSec += std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    return ok;
}

// winD + 1 when an activity already starts past its window (=> nothing <= winD below this node), else -1
template <class Arr>
inline int winBound(const Arr& start) {
    if (!WinOn() || !Active() || rootb::info().winLs.empty()) return -1;
    ++st().winChecks;
    const auto& ls = rootb::info().winLs;
    for (int i = 0; i < (int)ls.size(); ++i)
        if ((int)start[i] > ls[i]) { ++st().winPruned; return rootb::info().winD + 1; }
    return -1;
}

// 1 = root shifted to the propagated earliest starts, 0 = not applicable, -1 = refuted at U-1 (incumbent optimal)
template <class Arr>
inline int shiftRootToPropagatedEs(Arr& start) {
    if (!RootEsOn() || !Active()) return 0;
    const int U = (g_use_ub && st().realInc && g_incumbent < SHRT_MAX) ? (int)g_incumbent : INT_MAX;
    if (U == INT_MAX || U <= 1) return 0;
    const rootb::Inst& I = ctx().I;
    const int D = (RootEsMode() >= 2) ? U - 1 : U;
    std::vector<int> es(I.n), ls(I.n);
    for (int i = 0; i < I.n; ++i) { es[i] = (int)start[i]; ls[i] = D - I.tail[i]; }
    if (!rootb::propagate(I, D, es, ls)) { st().rootEsRefuted = true; return (RootEsMode() >= 2) ? -1 : 0; }
    long moved = 0;
    for (int i = 0; i < I.n; ++i) if (es[i] > (int)start[i]) { start[i] = (short)es[i]; ++moved; }
    st().rootEsMoved = moved;
    return moved > 0 ? 1 : 0;
}

// admissible absolute makespan bound for a CBS node with conflicts, given its current bound B = g + h
// true = no child of the branching conflict (G, mdas) can meet the deadline whose propagation just succeeded
template <class Arr>
inline bool mdaRefuted(const Arr& start, const std::vector<short>& G, const std::vector<MDA>& mdas) {
    const rootb::Inst& I = ctx().I;
    const std::vector<int>& ls = lsBuf();
    if ((int)ls.size() < I.n) return false;
    ++st().mdaRefChecks;
    for (const MDA& m : mdas) {
        int ns = INT_MAX;                                   // NewStart(M): earliest finish in G \ M
        for (short b : G) {
            if (std::find(m.activities.begin(), m.activities.end(), b) != m.activities.end()) continue;
            if (b < 0 || b >= I.n) return false;
            ns = std::min(ns, (int)start[b] + I.p[b]);
        }
        if (ns == INT_MAX) return false;                    // M = G: nothing to claim
        bool dead = false;
        for (short a : m.activities) {
            if (a < 0 || a >= I.n) return false;
            if (ns > ls[a]) { dead = true; break; }
        }
        if (!dead) return false;                            // this child can still make the deadline
    }
    return !mdas.empty();
}

// self / parent: the node's serial and its parent's (RCPSP_CBS_TT_WARM)
template <class Arr>
inline int nodeBound(const Arr& start, int B,
                     const std::vector<short>* G = nullptr, const std::vector<MDA>* mdas = nullptr,
                     unsigned self = 0, unsigned parent = 0) {
    const auto t0 = std::chrono::steady_clock::now();
    ++st().ttCalls;
    int D = std::max(B, ctx().rootLB);
    { const int w = winBound(start); if (w > D) D = w; }
    if (TTOn()) {
        if (NGOn() && !ctx().ng.empty()) {
            const int nb = ngBound(start);
            if (nb >= D) { D = nb + 1; ++st().ngHits; }
        }
        const int U = (g_use_ub && st().realInc && g_incumbent < SHRT_MAX) ? (int)g_incumbent : INT_MAX;
        if (U != INT_MAX && D < U && refuted(start, U - 1)) D = U;   // refuted at U-1 => at every D <= U-1
        const bool mdaRef = MdaRefOn() && TTFull() && G && mdas && !mdas->empty();
        const bool warm = WarmOn();
        bool fix = false;                                    // esBuf / lsBuf hold this node's fixpoint at D
        bool havePar = false;
        for (long s = 0; D < U && s < TTSteps(); ++s) {
            const short* wb = warm ? warmFind(start, D, parent, havePar) : nullptr;   // the first deadline is the parent's f
            if (wb ? refutedWarm(start, D, wb) : refuted(start, D)) { ++D; continue; }
            if (mdaRef && mdaRefuted(start, *G, *mdas)) { ++st().mdaRef; ++D; continue; }
            fix = true;
            break;
        }
        if (fix && warm) warmStore(start, D, self);
    }
    if (D > B) ++st().ttRaised;
    st().ttSec += std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    return D;
}

}  // namespace cpcbs
#endif
