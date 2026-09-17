/**
 * @file BAE.h
 * @package hog2
 * @brief A templated version of BAE*
 * @author Vidal Alcázar
 * @author Lior Siag
 * @date 7/25/19, modified 7/24/24
 *
 * This is a templated version of BAE*, implemented from algorithm presented in the paper "Bidirectional Heuristic
 * Search based on Error Estimate" by Samir K Sadhukhan (2013).
 * Slight modifications and bug-fixes, cleanup, and documentation was done by Lior before integrating into HOG2.
 * The code was not attempted on directed domains, and as such can have still bugs when dealing with them.
 */

#ifndef BAE_RCPSP_H
#define BAE_RCPSP_H

//#include "AStarOpenClosed.h"#
#include "../algorithms/AStarOpenClosed.h"

#include "../utils//FPUtil.h"
//#include "FPUtil.h"
//#include "Timer.h"
#include "../utils//Timer.h"
#include "../utils//Graphics.h"
#include <unordered_map>
#include <cmath>
#include <iostream>
//#include "Heuristic.h"
#include "../search//Heuristic.h"
#include <vector>
#include <algorithm>
#include <chrono>
#include <functional>
#include <queue>
#ifdef __linux__
#include <cstdio>
#include <unistd.h>
// Resident set size right now, from /proc/self/statm. Used only for the peakRSSMB column.
static inline long bae_rcpsp_rss_kb() {
    long size = 0, res = 0;
    std::FILE* f = std::fopen("/proc/self/statm", "r");
    if (!f) return 0;
    if (std::fscanf(f, "%ld %ld", &size, &res) != 2) res = 0;
    std::fclose(f);
    return res * (sysconf(_SC_PAGESIZE) / 1024);
}
#else
static inline long bae_rcpsp_rss_kb() { return 0; }
#endif

//              *****ido lublin 3.4.25*****
static bool GetExpandForward_rcpsp;
// 1 (default) => b = g + h, admissible bound. 0 => b = 2g + h - h_opp (needs a CONSISTENT h_opp).
// DEFAULT 0 = the real BAE* priority b = 2g + h - h_opp. The sub-optimal answers we chased
// (j30 1:1 -> 45 instead of 43, 13:1 -> 60/62 instead of 58) were caused by DR5, not by this:
// dominance drains a frontier and the drained queue was being certified as a proof. With
// dominance off this priority returns the true optimum everywhere tested. RCPSP_BI_PRIO=1
// selects b = f, whose bound (f_F+f_B)/2 is admissible but which discards BAE*'s error term.
static const bool BAE_RCPSP_USE_F_PRIORITY = [](){
    const char* e = std::getenv("RCPSP_BI_PRIO");
    return e ? (std::atoi(e) != 0) : false;
}();
static std::chrono::steady_clock::time_point timeout2_rcpsp;
// RCPSP_BI_ACTIVETB=1 (diagnostic, default 0): last plateau tie-break prefers MORE active
// transitions, which is what AStarCompareWithF<RCPSPState_TT2> actually does in code (its
// comment says "fewer"). Default keeps the current HBAE order (fewer active first).
static const bool BAE_RCPSP_TT2_ACTIVE_TB = [](){
    const char* e = std::getenv("RCPSP_BI_ACTIVETB");
    return e ? (std::atoi(e) != 0) : false;
}();
// RCPSP_BI_DZFIRST (default 1): on equal priority, expand a zero-delay child (reached by starting an activity
// at delay 0, so same g and, with a consistent h, same f as its parent) before the rest of that layer -- i.e.
// finish a zero-delay run first. This is the order the old h = 0 scoring produced by accident, kept as a
// pure tie-break so f stays true and consistent.
static const bool BAE_RCPSP_DZ_FIRST = [](){
    const char* e = std::getenv("RCPSP_BI_DZFIRST");
    return e ? (std::atoi(e) != 0) : true;
}();

//              ***************************



// ─────────────────────────────────────────────────────────────────────────────
// LOCAL COPY of HOG2/generic/BAE.h (HOG2 core is NOT modified), with ONE change:
// the cross-queue MEET lookup is separated from the within-queue node IDENTITY.
//
// Stock BAE* uses env->GetStateHash for both, which is right for a reversible graph.
// Our two directions are two DIFFERENT nets: identity must be the FULL marking, while
// the meet must be on the frame-independent NORMALISED CUT key (forward states sit on
// START events, reversed states on FINISH events, so their full markings never coincide).
// Using one hash for both is unsound either way round: cut-key identity would merge
// forward states that differ only in what was just committed at this instant, turning
// "start a at delay 0" into a zero-cost self-loop.
//
// So each queue keeps a side index  meetHash -> node id  and cross-queue lookups go
// through it. The environment must supply GetMeetHash(state) alongside GetStateHash.
// Everything else — the b(n)=2g+h-h_opp priority, the alternating/cardinality policy,
// the (errF+errB)/2 bound and its termination — is HOG2's original code, untouched.
// ─────────────────────────────────────────────────────────────────────────────
template<class state>
//ido lublin 28.4.25
struct BAECompare_RCPSP {
    // Canonical BAE* ordering, state-agnostic. The repo's in-tree BAECompare was specialised
    // for the old RCPSPState_BI_TT2 (g_f/g_b/h_f/h_b/finishedActivitiys), which our wrapper
    // state does not have — and it does not need to: AStarOpenClosed already stores g, and
    // BAE* stores the priority b(n) = 2g + h - h_opp in the .h field. So order by b, then
    // prefer the deeper node on ties. (The queue is a max-heap over this comparator, so
    // "greater" means lower priority.)
    bool operator()(const AStarOpenClosedData<state> &i1, const AStarOpenClosedData<state> &i2) const {
        if (!fequal(i1.h, i2.h))
            return fgreater(i1.h, i2.h);   // smaller b first
        if (BAE_RCPSP_DZ_FIRST && i1.data.s.isDeltaZero != i2.data.s.isDeltaZero)
            return !i1.data.s.isDeltaZero;  // zero-delay child first (see RCPSP_BI_DZFIRST)
        if (!fequal(i1.g, i2.g))
            return fless(i1.g, i2.g);      // higher g first
        // Same plateau ordering as AStarCompareWithF<RCPSPState_TT2>. NOTE this reaches into
        // the wrapper state (.data.s), so this copy of BAE* is RCPSP-specific by design.
        size_t f1 = i1.data.s.finishedActivitiys.count(), f2 = i2.data.s.finishedActivitiys.count();
        if (f1 != f2) return f1 < f2;      // more finished first
        if (BAE_RCPSP_TT2_ACTIVE_TB)       // more active first (TT2's comparator as coded)
            return i1.data.s.activeTransitionIndices.size() < i2.data.s.activeTransitionIndices.size();
        return i1.data.s.activeTransitionIndices.size() > i2.data.s.activeTransitionIndices.size();
    }
};
/*
struct BAECompare_RCPSP {
    bool operator()(const AStarOpenClosedData<state> &i1, const AStarOpenClosedData<state> &i2) const {
        // Note that h here is used to contain the b-value of a node, which is the priority of it.
        double p1 = i1.h;
        double p2 = i2.h;
        if (fequal(p1, p2)) {
            return (fless(i1.g, i2.g)); // high g-cost over low
        }
        return (fgreater(p1, p2)); // low priority over high
    }
};
*/
/**
 * A class which implements the BAE algorithm. This implementation uses two papers' details:
 * A. "Bidirectional Heuristic Search based on Error Estimate" by Samir K Sadhukhan (2013).
 * B. "A Unifying View on Individual Bounds and Heuristic Inaccuracies in Bidirectional Search" by Vidal Alcázar, Pat
 * Riddle, Mike Barley (2020).
 */
template<class state, class action, class environment, class priorityQueue = AStarOpenClosed<state, BAECompare_RCPSP<state>>>
class BAE_RCPSP {
public:
    /**
     *
     * @param alternating_ Is the side choosing policy alternating or Pohl's cardinality criterion
     * @param epsilon_ Cost of the least-cost edge
     * @param gcd_ Greatest common denominator between all edges. Note that for edges e.g., 1 and 1.5, the gcd is 0.5.
     */

    BAE_RCPSP(bool alternating_ = true, double epsilon_ = 1.0, double gcd_ = 1.0) {
        forwardHeuristic = 0;
        backwardHeuristic = 0;
        env = 0;
        ResetNodeCount();
        epsilon = epsilon_;
        gcd = gcd_;
        alternating = alternating_;
    }

    virtual ~BAE_RCPSP() {}
//******ido*****
    double GetSolutionCost() const { return currentCost; }



    void GetPath(environment *env, const state &from, const state &to,
                 Heuristic<state> *forward, Heuristic<state> *backward, std::vector<state> &thePath);

    bool InitializeSearch(environment *env, const state &from, const state &to,
                          Heuristic<state> *forward, Heuristic<state> *backward, std::vector<state> &thePath);

    bool DoSingleSearchStep(std::vector<state> &thePath);

    virtual const char *GetName() { return "BAE"; }

    void ResetNodeCount() { nodesExpanded = nodesTouched = uniqueNodesExpanded = 0; reopenedNodes = 0; fDropF = fDropB = 0; fDropMax = 0; }
    uint64_t GetReopenedNodes() const { return reopenedNodes; }
    // children generated with a LOWER true f than their parent (an inconsistent h on that edge), per side, and the largest drop
    uint64_t GetFDrop(bool fwd) const { return fwd ? fDropF : fDropB; }
    double   GetFDropMax() const { return fDropMax; }
    uint64_t GetSolutionUpdates() const { return solutionUpdates; }
    const state &GetMiddleNode() const { return middleNode; }
    // true only if we terminated on the BOUND. A queue draining is a proof ONLY when no
    // dominance pruning was active - the caller knows that, so it must decide.
    bool StoppedByBound() const { return stoppedByBound; }
    // RootBounds.h: start with U = a verified heuristic makespan and stop as soon as U <= the root lower bound.
    // Sound: U is a real schedule (>= C*) and lb <= C*. Set before GetPath.
    void SetRootBounds(double u, double lb) { seedU = u; lbFloor = lb; }
    bool StoppedByFloor() const { return stoppedByFloor; }
    // which queue was empty at termination (0 none / 1 forward / 2 backward)
    int  GetDrainedSide() const { return drainedSide; }
    // ── per-side instrumentation, read by the driver after GetPath. None of it affects the search. ──
    uint64_t GetExpanded(bool fwd) const { return fwd ? expandedFwd : expandedBwd; }
    double   GetSideSec(bool fwd) const { return fwd ? sideSecFwd : sideSecBwd; }
    size_t   GetOpenCount(bool fwd) { return fwd ? forwardQueue.OpenSize() : backwardQueue.OpenSize(); }
    size_t   GetItemCount(bool fwd) { return fwd ? forwardQueue.size() : backwardQueue.size(); }
    size_t   GetMeetIndexSize(bool fwd) const { return fwd ? forwardMeet.size() : backwardMeet.size(); }
    // lowest true f among open nodes now (-1 if that side has none): each side's own lower bound
    double   GetMinFOpenNow(bool fwd) { priorityQueue& q = fwd ? forwardQueue : backwardQueue;
                                        if (q.OpenSize() == 0) return -1; double m = minTrueF(fwd);
                                        return (m >= DBL_MAX) ? -1 : m; }
    double   GetMinFAt(int i, bool fwd) const { return fwd ? minFAtF[i] : minFAtB[i]; }   // i: 0=60s 1=120s 2=240s
    int64_t  GetExpAtMaxBound() const { return expAtMaxBound; }
    double   GetUAtMaxBound() const { return uAtMaxBound; }
    long     GetPeakRSSKB() const { return peakRSSKB; }
    // ── where and when the frontiers met (the FINAL incumbent unless named First) ──
    int64_t  GetMeetExp() const { return meetExp; }            // expansion that produced the final incumbent
    int64_t  GetFirstMeetExp() const { return firstMeetExp; }  // ... and the first incumbent
    double   GetFirstU() const { return firstU; }
    uint64_t GetMeetHits() const { return meetHits; }          // generated nodes whose cut matched the other side
    int      GetMeetFromFwd() const { return meetFromFwd; }    // 1 forward generated it, 0 backward, -1 no meet
    double   GetMeetGF() const { return meetGF; }              // forward half of the incumbent (time from the start)
    double   GetMeetGB() const { return meetGB; }              // backward half (time to the end)
    int      GetMaxProg(bool fwd) const { return fwd ? maxProgF : maxProgB; }  // most activities started, expanded nodes
    double   GetMaxG(bool fwd) const { return fwd ? maxGF : maxGB; }
    uint64_t GetFBoundCalls() const { return fBoundCalls; }
    double   GetFBoundSec() const { return fBoundSec; }
    double   GetExpandSplit(int i) const { return bxSec[i]; }   // RCPSP_TT2_PROF: 0 pop, 1 successors, 2 child loop, 3 heuristic calls, 4 add to OPEN
    // ── DOMINANCE MEET (RCPSP_BI_DOMMEET, default 0) ──
    // Besides exact cut matches, every generated / improved node is offered to env->DomMeet, which joins it
    // with an opposite-side cut it DR5-dominates (see RCPSP_BiEnv::DomMeet). With RCPSP_BI_NOUB on (default)
    // the incumbent never changes the expansion order, so this can only stop the search earlier.
    static bool DomMeetOn() { static const bool on = [](){ const char* e = std::getenv("RCPSP_BI_DOMMEET");
                                                          return e && std::atoi(e) != 0; }();
                              return on; }
    uint64_t GetDomMeetUpdates() const { return domMeetUpdates; }
    bool     StoppedByMax() const { return stoppedByMax; }      // RCPSP_BI_STOP=max ended the search
    // An incumbent found outside the queues (dominance meet, or a DR5-pruned state): a real schedule of this cost.
    void OfferSolution(double cost, const state& mid, bool fromFwd, double gOwn) {
        if (!fless(cost, currentCost)) return;
        ++solutionUpdates; ++domMeetUpdates;
        currentCost = cost;
        middleNode = mid;
        recordMeet(fromFwd, gOwn, cost - gOwn);
        if (std::getenv("RCPSP_BI_TRACE"))
            std::cout << "    [meet-dom] U=" << currentCost << " gOwn=" << gOwn << " fwd=" << fromFwd << " exp=" << nodesExpanded << std::endl;
    }
    static bool MinFHeapOn() { static const bool on = [](){ const char* e = std::getenv("RCPSP_BI_MINF_HEAP");
                                                            return e ? (std::atoi(e) != 0) : true; }();
                               return on; }
    // ── MEETING AUDIT (RCPSP_BI_AUDIT=1): bookkeeping only, the search is unchanged ──
    // Per node, the expansion count at which its current g was set (generation, g improvement, reopen), and the
    // cheapest JOIN of two queue nodes: an exact meet (one cut indexed on both sides, shift 0), or a dominance /
    // shift meet with its shift. The driver rebuilds that schedule and checks, for every integer time t, whether
    // both sides hold its cut, and whether the incumbent reached C* no later than the first moment both did.
    static bool AuditOn() { static const bool on = [](){ const char* e = std::getenv("RCPSP_BI_AUDIT");
                                                         return e && std::atoi(e) != 0; }();
                            return on; }
    bool GetExactMeet(uint64_t &fID, uint64_t &bID, int &shift, double &cost) const {
        if (exactCost >= DBL_MAX) return false;
        fID = exactF; bID = exactB; shift = exactShift; cost = exactCost; return true;
    }
    // the states from that side's root to node `id`, each with its node g
    void PathFromID(bool fwd, uint64_t id, std::vector<std::pair<state, double>> &out) {
        priorityQueue &q = fwd ? forwardQueue : backwardQueue;
        out.clear();
        for (size_t guard = 0; guard <= q.size(); ++guard) {
            out.push_back({q.Lookup(id).data, q.Lookup(id).g});
            const uint64_t p = q.Lookup(id).parentID;
            if (p == id) break;
            id = p;
        }
        std::reverse(out.begin(), out.end());
    }
    // that side's best-g node for a meet hash: its g, and the expansion count when that g was set
    bool MeetNode(bool fwd, uint64_t meetHash, double &g, int64_t &gExp) {
        auto &m = fwd ? forwardMeet : backwardMeet;
        auto it = m.find(meetHash);
        if (it == m.end()) return false;
        priorityQueue &q = fwd ? forwardQueue : backwardQueue;
        g = q.Lookup(it->second).g;
        const auto &v = fwd ? gExpF : gExpB;
        gExp = (it->second < v.size()) ? v[it->second] : -1;
        return true;
    }

    inline const int GetNumForwardItems() { return forwardQueue.size(); }

    inline const AStarOpenClosedData<state> &GetForwardItem(unsigned int which) { return forwardQueue.Lookat(which); }

    inline const int GetNumBackwardItems() { return backwardQueue.size(); }

    inline const AStarOpenClosedData<state> &GetBackwardItem(unsigned int which) {
        return backwardQueue.Lookat(which);
    }

    uint64_t GetUniqueNodesExpanded() const { return uniqueNodesExpanded; }

    uint64_t GetNodesExpanded() const { return nodesExpanded; }

    uint64_t GetNodesTouched() const { return nodesTouched; }

    uint64_t GetNecessaryExpansions() {
        uint64_t necessary = 0;
        for (const auto &count: counts) {
            if (count.first < currentCost)
                necessary += count.second;
        }
        return necessary;
    }



private:

    void Nip(const state &, priorityQueue &reverse);

    void ExtractPathToGoal(state &node, std::vector<state> &thePath) {
        auto it = backwardMeet.find(env->GetMeetHash(node));
        if (it == backwardMeet.end()) { thePath.clear(); return; }
        ExtractPathToGoalFromID(it->second, thePath);
    }

    void ExtractPathToGoalFromID(uint64_t node, std::vector<state> &thePath) {
        do {
            thePath.push_back(backwardQueue.Lookup(node).data);
            node = backwardQueue.Lookup(node).parentID;
        } while (backwardQueue.Lookup(node).parentID != node);
        thePath.push_back(backwardQueue.Lookup(node).data);
    }

    void ExtractPathToStart(state &node, std::vector<state> &thePath) {
        auto it = forwardMeet.find(env->GetMeetHash(node));
        if (it == forwardMeet.end()) { thePath.clear(); return; }
        ExtractPathToStartFromID(it->second, thePath);
    }

    void ExtractPathToStartFromID(uint64_t node, std::vector<state> &thePath) {
        do {
            thePath.push_back(forwardQueue.Lookup(node).data);
            node = forwardQueue.Lookup(node).parentID;
        } while (forwardQueue.Lookup(node).parentID != node);
        thePath.push_back(forwardQueue.Lookup(node).data);
    }

    void Expand(priorityQueue &current, priorityQueue &opposite,
                std::unordered_map<uint64_t, uint64_t> &currentMeet,
                std::unordered_map<uint64_t, uint64_t> &oppositeMeet,
                std::vector<double> &currentF,
                Heuristic<state> *heuristic,
                Heuristic<state> *reverse_heuristic, const state &target, const state &source);

    double getLowerBound();
    // ── SOUND TERMINATION BOUND ────────────────────────────────────────────────
    // `.h` in the queue holds the PRIORITY b (2g + h - h_opp), so true f = g + h_own is
    // not recoverable from the node data -- we track it alongside. b is kept for the
    // EXPANSION ORDER (that is what makes this BAE*), but termination is authorised only
    // by the f-bound: (min f_F + min f_B)/2. HOG2's b-bound is correct for BAE* as defined,
    // where each side's h_opp IS the other side's own heuristic, so on an optimal path the two
    // error terms cancel and the bound cannot pass C*. Here h_opp is hOppOf -- a precedence-
    // only EST lower bound on elapsed time, on the node's own net -- not the other side's
    // resource-aware TT2 heuristic, so the terms do not cancel and the average can overshoot
    // C*. It does so with dominance OFF: j30 25:8 stops at 71 vs 69 (discard on + b-bound),
    // while discard on + f-bound returns 69.
    // Since h_opp <= g for an admissible h_opp, b >= f pointwise, so the b-bound is >= the
    // f-bound and serves as a cheap gate that can never miss a real termination.
    std::vector<double> trueFF, trueFB;
    // ── EXACT min true f over OPEN, without scanning OPEN ───────────────────────
    // The f-bound needs each side's min f. Scanning OPEN is O(|OPEN|) per call, and under b order
    // the b-gate can pass long before the f-bound does, so the scan ran on EVERY expansion there.
    // Instead: a lazy min-heap of (f, id), pushed whenever a node's true f is set or lowered. An
    // entry is stale once that node's f changed or the node left OPEN; stale tops are popped on
    // read. A node re-enters OPEN only through a reopen, which lowers g and so pushes a fresh entry.
    // Same values as the scan, so the search is unchanged. RCPSP_BI_MINF_HEAP=0 restores the scan;
    // RCPSP_BI_MINF_CHECK=1 computes both on every call and exits(4) on any difference.
    typedef std::pair<double, uint64_t> FEntry;
    typedef std::priority_queue<FEntry, std::vector<FEntry>, std::greater<FEntry>> FHeap;
    FHeap fHeapF, fHeapB;
    void setTrueF(std::vector<double> &v, uint64_t id, double f) {
        if (id >= v.size()) v.resize(id + 1, DBL_MAX);
        v[id] = f;
        if (MinFHeapOn()) (&v == &trueFF ? fHeapF : fHeapB).push(FEntry(f, id));
    }
    void shiftTrueF(std::vector<double> &v, uint64_t id, double delta) {
        if (id < v.size() && v[id] != DBL_MAX) {
            v[id] -= delta;
            if (MinFHeapOn()) (&v == &trueFF ? fHeapF : fHeapB).push(FEntry(v[id], id));
        }
    }
    double minFOpen(priorityQueue &q, const std::vector<double> &v) {
        double m = DBL_MAX;
        for (size_t i = 0; i < q.OpenSize(); ++i) {
            uint64_t id = q.GetOpenItem(i);
            if (id < v.size() && v[id] < m) m = v[id];
        }
        return m;
    }
    double minTrueF(bool fwd) {
        priorityQueue &q = fwd ? forwardQueue : backwardQueue;
        std::vector<double> &v = fwd ? trueFF : trueFB;
        if (!MinFHeapOn()) return minFOpen(q, v);
        FHeap &hp = fwd ? fHeapF : fHeapB;
        double m = DBL_MAX;
        while (!hp.empty()) {
            const FEntry &t = hp.top();
            if (t.second < v.size() && v[t.second] == t.first && q.Lookup(t.second).where == kOpenList) { m = t.first; break; }
            hp.pop();
        }
        static const bool check = [](){ const char* e = std::getenv("RCPSP_BI_MINF_CHECK");
                                        return e && std::atoi(e) != 0; }();
        if (check) {
            const double sc = minFOpen(q, v);
            if (sc != m) {
                std::cerr << "[MINF-CHECK] " << (fwd ? "forward" : "backward") << " heap=" << m << " scan=" << sc
                          << " open=" << q.OpenSize() << " expanded=" << nodesExpanded << std::endl;
                std::exit(4);
            }
        }
        return m;
    }
    double getFLowerBound() {
        if (forwardQueue.OpenSize() == 0 || backwardQueue.OpenSize() == 0) return DBL_MAX;
        ++fBoundCalls;
        const auto t0 = std::chrono::steady_clock::now();
        double a = minTrueF(true), b = minTrueF(false);
        fBoundSec += std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        if (a == DBL_MAX || b == DBL_MAX) return DBL_MAX;
        return ceil(((a + b) / 2) / gcd) * gcd;
    }
    void recordMeet(bool fromFwd, double gOwn, double gOpp) {   // called whenever the incumbent improves
        meetFromFwd = fromFwd ? 1 : 0;
        meetGF = fromFwd ? gOwn : gOpp;
        meetGB = fromFwd ? gOpp : gOwn;
        meetExp = (int64_t)nodesExpanded;
        if (firstMeetExp < 0) { firstMeetExp = meetExp; firstU = gOwn + gOpp; }
    }

    priorityQueue forwardQueue, backwardQueue;
    // meetHash -> node id, one per side. This is the ONLY structural addition.
    std::unordered_map<uint64_t, uint64_t> forwardMeet, backwardMeet;
    state goal, start;
    uint64_t nodesTouched, nodesExpanded, uniqueNodesExpanded;
    uint64_t reopenedNodes = 0;   // closed nodes later reached with a better g
    uint64_t fDropF = 0, fDropB = 0;   // see GetFDrop
    double   fDropMax = 0;
    uint64_t solutionUpdates = 0; // how many times a meet improved the incumbent
    bool stoppedByBound = false;  // true = genuine (f_F+f_B)/2 proof, false = a queue drained
    double seedU = DBL_MAX;       // RCPSP_ROOT_SEED: incumbent from a checked heuristic schedule (no join node)
    double lbFloor = 0.0;         // RCPSP_ROOT_DLB: root destructive lower bound (<= C*)
    bool stoppedByFloor = false;  // stopped because U <= lbFloor
    int  drainedSide = 0;         // 0 = none, 1 = forward queue empty, 2 = backward
    bool lbReported = false;      // RCPSP_BI_LBOPT probe fires once per instance
    bool fInvReported = false;    // frontier-invariant probe fires once per instance
    uint64_t expandedFwd = 0, expandedBwd = 0;           // expansions per side
    double   sideSecFwd = 0.0, sideSecBwd = 0.0;         // wall time inside Expand, per side
    bool     lastSideForward = true;                     // which side the current step expands
    double   minFAtF[3] = {-1, -1, -1}, minFAtB[3] = {-1, -1, -1};   // min open f per side at 60/120/240 s
    int      minFAtNext = 0;
    // the first moment U <= max(min f_F, min f_B) (128-expansion resolution), and U then. Never used to stop.
    int64_t  expAtMaxBound = -1;
    double   uAtMaxBound = -1;
    long     peakRSSKB = 0;
    int64_t  meetExp = -1, firstMeetExp = -1;
    double   firstU = -1, meetGF = -1, meetGB = -1;
    uint64_t meetHits = 0;
    int      meetFromFwd = -1;
    int      maxProgF = -1, maxProgB = -1;
    double   maxGF = -1, maxGB = -1;
    uint64_t fBoundCalls = 0;
    double   fBoundSec = 0.0;
    double   bxSec[5] = {0, 0, 0, 0, 0};
    uint64_t domMeetUpdates = 0;
    bool     stoppedByMax = false;
    std::vector<int64_t> gExpF, gExpB;   // RCPSP_BI_AUDIT: expansion count when each node's current g was set
    uint64_t exactF = 0, exactB = 0;     // RCPSP_BI_AUDIT: forward / reversed node of the cheapest join ...
    int      exactShift = 0;             // ... the shift of its suffix ...
    double   exactCost = DBL_MAX;        // ... and its cost
    void noteG(bool fwd, uint64_t id) {
        if (!AuditOn()) return;
        auto &v = fwd ? gExpF : gExpB;
        if (id >= v.size()) v.resize(id + 1, -1);
        v[id] = (int64_t)nodesExpanded;
    }
    // a join of this side's node `own` with the other side's node `opp` (shifted `shift`) of cost c: keep the cheapest
    void noteJoin(bool fwd, uint64_t own, uint64_t opp, int shift, double c) {
        if (!AuditOn() || opp == UINT64_MAX || !(c < exactCost)) return;
        exactF = fwd ? own : opp; exactB = fwd ? opp : own; exactShift = shift; exactCost = c;
    }
    std::chrono::steady_clock::time_point searchStart;
    state middleNode; // Meeting point of the current solution
    double currentCost; // Cost of the best solution found so far

    std::vector<state> neighbors;
    environment *env;
    Heuristic<state> *forwardHeuristic;
    Heuristic<state> *backwardHeuristic;

    double epsilon; // Cost of the least-cost edge
    double gcd; // Greatest common denominator between all edges

    bool alternating; // Is the side-choosing policy alternating or not
    bool expandForward; // Is the current expansion direction forward. This is used for the alternating policy

    std::unordered_map<double, int> counts;
};

/**
 * Calculates the current lower-bound bases on min b in both sides, uses the gcd trick from Alcázar et al.
 * @return Current lower-bound on the search
 */
template<class state, class action, class environment, class priorityQueue>
double BAE_RCPSP<state, action, environment, priorityQueue>::getLowerBound() {
    if (forwardQueue.OpenSize() == 0 || backwardQueue.OpenSize() == 0)
        return DBL_MAX;

    double totalErrorForward = forwardQueue.Lookup(forwardQueue.Peek()).h;
    double totalErrorBackward = backwardQueue.Lookup(backwardQueue.Peek()).h;
    double unroundedLowerBound = (totalErrorForward + totalErrorBackward) / 2;

    // round up to the next multiple of gcd
    return ceil(unroundedLowerBound / gcd) * gcd;
}

/**
 * Given a start and a goal, find a path between them
 * @param env A pointer to the domain environment
 * @param from Initial state of the search
 * @param to The goal state
 * @param forward A forward heuristic, i.e., a heuristic from some node n to the goal
 * @param backward A backward heuristic, i.e., a heuristic from the start to some node n
 * @param thePath The solution path which we will fill when the search is done
 */
template<class state, class action, class environment, class priorityQueue>
void BAE_RCPSP<state, action, environment, priorityQueue>::GetPath(environment *env, const state &from, const state &to,
                                                             Heuristic<state> *forward, Heuristic<state> *backward,
                                                             std::vector<state> &thePath) {
    if (InitializeSearch(env, from, to, forward, backward, thePath) == false)
        return;
    while (!DoSingleSearchStep(thePath)) {}
}

/**
 * Initializes the internal structure and data for a new search
 * @param env A pointer to the domain environment
 * @param from Initial state of the search
 * @param to The goal state
 * @param forward A forward heuristic, i.e., a heuristic from some node n to the goal
 * @param backward A backward heuristic, i.e., a heuristic from the start to some node n
 * @param thePath The solution path which we will fill when the search is done
 * @return whether the start and goal are not the same (false means they are the same)
 */
template<class state, class action, class environment, class priorityQueue>
bool BAE_RCPSP<state, action, environment, priorityQueue>::InitializeSearch(environment *env, const state &from,
                                                                      const state &to,
                                                                      Heuristic<state> *forward,
                                                                      Heuristic<state> *backward,
                                                                      std::vector<state> &thePath) {
    this->env = env;
    forwardHeuristic = forward;
    backwardHeuristic = backward;
    currentCost = DBL_MAX;
    if (seedU < DBL_MAX) currentCost = seedU;   // RCPSP_ROOT_SEED
    stoppedByFloor = false;
    lbReported = false;
    fInvReported = false;
    forwardQueue.Reset();
    backwardQueue.Reset();
    ResetNodeCount();
    thePath.resize(0);
    start = from;
    goal = to;
    if (start == goal)
        return false;

    forwardMeet.clear(); backwardMeet.clear();
    trueFF.clear(); trueFB.clear();
    expandedFwd = expandedBwd = 0; sideSecFwd = sideSecBwd = 0.0; lastSideForward = true;
    for (int i = 0; i < 3; i++) { minFAtF[i] = -1; minFAtB[i] = -1; }
    minFAtNext = 0; expAtMaxBound = -1; uAtMaxBound = -1;
    meetExp = firstMeetExp = -1; firstU = meetGF = meetGB = -1; meetHits = 0; meetFromFwd = -1;
    maxProgF = maxProgB = -1; maxGF = maxGB = -1; fBoundCalls = 0; fBoundSec = 0.0;
    fHeapF = FHeap(); fHeapB = FHeap();
    for (double &x : bxSec) x = 0.0;
    domMeetUpdates = 0;
    stoppedByMax = false;
    gExpF.clear(); gExpB.clear(); exactCost = DBL_MAX;
    peakRSSKB = bae_rcpsp_rss_kb(); searchStart = std::chrono::steady_clock::now();
    forwardMeet[env->GetMeetHash(start)] =
        forwardQueue.AddOpenNode(start, env->GetStateHash(start), 0, forwardHeuristic->HCost(start, goal));
    setTrueF(trueFF, forwardMeet[env->GetMeetHash(start)], forwardHeuristic->HCost(start, goal));
    backwardMeet[env->GetMeetHash(goal)] =
        backwardQueue.AddOpenNode(goal, env->GetStateHash(goal), 0, backwardHeuristic->HCost(goal, start));
    setTrueF(trueFB, backwardMeet[env->GetMeetHash(goal)], backwardHeuristic->HCost(goal, start));
    noteG(true, forwardMeet[env->GetMeetHash(start)]); noteG(false, backwardMeet[env->GetMeetHash(goal)]);
    if (DomMeetOn()) { env->DomMeetReset(); env->DomMeet(start, 0.0, DBL_MAX, forwardMeet[env->GetMeetHash(start)]);
                       env->DomMeet(goal, 0.0, DBL_MAX, backwardMeet[env->GetMeetHash(goal)]); }

    expandForward = true;
    //****
    // honour the harness per-instance budget rather than a hardcoded 5 minutes
    timeout2_rcpsp = std::chrono::steady_clock::now() + std::chrono::seconds(astar_timeout_seconds);
    GetExpandForward_rcpsp=expandForward;
    return true;
}

/**
 * Do a single cycle of search, which means check if the search is over, and if not expand a single node
 * @param thePath The solution path which we will fill when the search is done
 * @return Wether the search was done or not
 */
template<class state, class action, class environment, class priorityQueue>
bool BAE_RCPSP<state, action, environment, priorityQueue>::DoSingleSearchStep(std::vector<state> &thePath) {
    // A side exhausting its open list with no incumbent means no solution was found on this
    // budget. The in-tree copy called exit(0) here, which would abort an entire benchmark run;
    // report completion and let the caller read currentCost == DBL_MAX instead.
    if ((forwardQueue.OpenSize() == 0 || backwardQueue.OpenSize() == 0) && currentCost == DBL_MAX)
        return true;
    if (std::chrono::steady_clock::now() > timeout2_rcpsp) {
        return true; // Exit early on timeout
    }
    // RCPSP_ROOT_DLB: the incumbent already meets the root lower bound => it is optimal.
    if (lbFloor > 0 && currentCost < DBL_MAX && currentCost <= lbFloor) {
        stoppedByBound = true; stoppedByFloor = true; drainedSide = 0;
        if (meetFromFwd >= 0) {
            std::vector<state> pFor, pBack;
            ExtractPathToGoal(middleNode, pBack);
            ExtractPathToStart(middleNode, pFor);
            std::reverse(pFor.begin(), pFor.end());
            thePath = pFor;
            if (!pBack.empty()) thePath.insert(thePath.end(), pBack.begin() + 1, pBack.end());
        }
        return true;
    }
    // ── instrumentation: memory, per-side bound checkpoints, and the max-bound shadow test ──
    if ((nodesExpanded & 4095) == 0) { const long r = bae_rcpsp_rss_kb(); if (r > peakRSSKB) peakRSSKB = r; }
    if (minFAtNext < 3 && (nodesExpanded & 255) == 0) {
        static const double cps[3] = {60.0, 120.0, 240.0};
        const double el = std::chrono::duration<double>(std::chrono::steady_clock::now() - searchStart).count();
        while (minFAtNext < 3 && el >= cps[minFAtNext]) {
            minFAtF[minFAtNext] = GetMinFOpenNow(true);
            minFAtB[minFAtNext] = GetMinFOpenNow(false);
            ++minFAtNext;
        }
    }
    // Shadow test: the first moment U <= max(min true f_F, min true f_B). Pre-filter: a queue top's
    // priority (f, or b >= f) is >= that side's min true f, so while U is above the priority max the
    // true-f max cannot reach U either; only then read the exact minima (minTrueF), every 128 expansions.
    if (expAtMaxBound < 0 && currentCost < DBL_MAX && (nodesExpanded & 127) == 0
        && forwardQueue.OpenSize() > 0 && backwardQueue.OpenSize() > 0
        && currentCost <= ceil(std::max(forwardQueue.Lookup(forwardQueue.Peek()).h,
                                        backwardQueue.Lookup(backwardQueue.Peek()).h) / gcd) * gcd) {
        const double m = std::max(minTrueF(true), minTrueF(false));
        if (m < DBL_MAX && currentCost <= ceil(m / gcd) * gcd) { expAtMaxBound = (int64_t)nodesExpanded; uAtMaxBound = currentCost; }
    }
    // This means that the best solution so far is better (or equal) than any solution we will be able to achieve from
    // this point forward, i.e., we are done
    // ── BOUND ADMISSIBILITY PROBE ───────────────────────────────────────────────
    // RCPSP_BI_LBOPT=<known optimum>. A lower bound may never exceed C*. If it does, the
    // bound is inadmissible; dump both frontier minima once so we can see WHICH side lost
    // its optimal-path nodes (that side's min priority will be the one above C*).
    {
        static const int lbOpt = [](){ const char* e = std::getenv("RCPSP_BI_LBOPT");
                                       return e ? std::atoi(e) : 0; }();
        // FRONTIER INVARIANT: while a cheaper solution still exists (currentCost > C*), each
        // side must hold an open node with f <= C* -- otherwise min f is not a lower bound and
        // the f-bound is unsound. Check periodically; report the first violation and which side.
        if (lbOpt > 0 && !fInvReported && (nodesExpanded % 64) == 0
            && forwardQueue.OpenSize() > 0 && backwardQueue.OpenSize() > 0
            && currentCost > lbOpt) {
            double mf = minFOpen(forwardQueue, trueFF), mb = minFOpen(backwardQueue, trueFB);
            if (mf > lbOpt || mb > lbOpt) {
                fInvReported = true;
                std::cout << "    [F-INV VIOLATED] C*=" << lbOpt << " U=" << currentCost
                          << "  minF_fwd=" << mf << (mf > lbOpt ? " <== " : "     ")
                          << "  minF_bwd=" << mb << (mb > lbOpt ? " <== " : "     ")
                          << "  openF=" << forwardQueue.OpenSize()
                          << " openB=" << backwardQueue.OpenSize()
                          << "  expanded=" << nodesExpanded << std::endl;
            }
        }
        if (lbOpt > 0 && !lbReported && forwardQueue.OpenSize() > 0 && backwardQueue.OpenSize() > 0) {
            const double lbNow = getLowerBound();
            if (lbNow > lbOpt) {
                lbReported = true;
                const auto& pf = forwardQueue.Lookup(forwardQueue.Peek());
                const auto& pb = backwardQueue.Lookup(backwardQueue.Peek());
                std::cout << "    [LB>OPT] lb=" << lbNow << " > C*=" << lbOpt
                          << "  U=" << currentCost
                          << "   fwd(minPrio=" << pf.h << " g=" << pf.g
                          << " open=" << forwardQueue.OpenSize() << ")"
                          << "   bwd(minPrio=" << pb.h << " g=" << pb.g
                          << " open=" << backwardQueue.OpenSize() << ")"
                          << "   expanded=" << nodesExpanded << std::endl;
            }
        }
    }
    // ── RCPSP_BI_STOP=max ─────────────────────────────────────────────────────
    // Stop as soon as U <= max(min f_F, min f_B). Valid under the same assumption the average rule needs: while
    // U > C*, EACH side keeps an open node on some optimal path, so each side's min f is <= C* on its own. It was
    // logged as a shadow test on the j30 server runs first (0 wrong in 1920 rows). With strict alternation
    // (RCPSP_BI_ALT=1) the search stops within about twice the expansions of the better single direction.
    {
        static const bool stopMax = [](){ const char* e = std::getenv("RCPSP_BI_STOP"); return e && std::string(e) == "max"; }();
        if (stopMax && currentCost < DBL_MAX && forwardQueue.OpenSize() > 0 && backwardQueue.OpenSize() > 0) {
            const double mf = minTrueF(true), mb = minTrueF(false);
            if (mf < DBL_MAX && mb < DBL_MAX && currentCost <= ceil(std::max(mf, mb) / gcd) * gcd) {
                stoppedByBound = true; stoppedByMax = true; drainedSide = 0;
                if (std::getenv("RCPSP_BI_TRACE"))
                    std::cout << "    [stop-max] currentCost=" << currentCost << " minF_fwd=" << mf << " minF_bwd=" << mb
                              << "  expanded=" << nodesExpanded << std::endl;
                if (meetFromFwd >= 0) {   // a seeded incumbent (RCPSP_ROOT_SEED) has no join node to walk from
                    std::vector<state> pFor, pBack;
                    ExtractPathToGoal(middleNode, pBack);
                    ExtractPathToStart(middleNode, pFor);
                    std::reverse(pFor.begin(), pFor.end());
                    thePath = pFor;
                    if (!pBack.empty()) thePath.insert(thePath.end(), pBack.begin() + 1, pBack.end());
                }
                return true;
            }
        }
    }
    if (currentCost <= getLowerBound()) {
        // The b-bound only GATES: it is >= the f-bound (b >= f pointwise), so it can never
        // hide a real termination, but under dominance it can sit ABOVE C* and certify a
        // non-optimal incumbent. The f-bound is what authorises stopping. A drained queue
        // makes both DBL_MAX, so the drain path is unchanged.
        // RCPSP_BI_FBOUND=0 restores HOG2's original stopping rule (b-bound only), for A/B tests.
        static const bool useFBound = [](){ const char* e = std::getenv("RCPSP_BI_FBOUND");
                                            return e ? (std::atoi(e) != 0) : true; }();
        if (useFBound && currentCost > getFLowerBound())
            goto keepSearching;
        stoppedByBound = (forwardQueue.OpenSize() > 0 && backwardQueue.OpenSize() > 0);
        drainedSide = stoppedByBound ? 0 : (forwardQueue.OpenSize() == 0 ? 1 : 2);
        if (std::getenv("RCPSP_BI_TRACE")) {
            bool emptyF = (forwardQueue.OpenSize() == 0), emptyB = (backwardQueue.OpenSize() == 0);
            std::cout << "    [stop] currentCost=" << currentCost
                      << " lb=" << getLowerBound()
                      << (emptyF || emptyB ? "  QUEUE-EMPTY(" : "  BOUND-PROOF(")
                      << "F=" << forwardQueue.OpenSize() << ",B=" << backwardQueue.OpenSize() << ")"
                      << "  expanded=" << nodesExpanded << std::endl;
        }
        if (meetFromFwd >= 0) {   // a seeded incumbent (RCPSP_ROOT_SEED) has no join node to walk from
            std::vector<state> pFor, pBack;
            ExtractPathToGoal(middleNode, pBack);
            ExtractPathToStart(middleNode, pFor);
            std::reverse(pFor.begin(), pFor.end());
            thePath = pFor;
            if (!pBack.empty()) thePath.insert(thePath.end(), pBack.begin() + 1, pBack.end());
        }

        return true;
    }
    keepSearching:;
    const auto _sideT0 = std::chrono::steady_clock::now();
    const uint64_t _sideE0 = nodesExpanded;

    // If we are not done, expand a single node based on the side-choosing policy set by the user
    static const int oneSideRcpsp = [](){ const char* e=std::getenv("RCPSP_BI_ONESIDE");
                                          return e ? std::atoi(e) : 0; }();
    if (oneSideRcpsp == 1) {
        { lastSideForward = true; Expand(forwardQueue, backwardQueue, forwardMeet, backwardMeet, trueFF, forwardHeuristic, backwardHeuristic, goal, start); }
    } else if (oneSideRcpsp == 2) {
        { lastSideForward = false; Expand(backwardQueue, forwardQueue, backwardMeet, forwardMeet, trueFB, backwardHeuristic, forwardHeuristic, start, goal); }
    } else if (alternating) { // original BAE* definition
        if (expandForward) {
            { lastSideForward = true; Expand(forwardQueue, backwardQueue, forwardMeet, backwardMeet, trueFF, forwardHeuristic, backwardHeuristic, goal, start); }
            expandForward = false;
            //****
            // GetExpandForward_rcpsp=expandForward;
            //****
        } else {
            { lastSideForward = false; Expand(backwardQueue, forwardQueue, backwardMeet, forwardMeet, trueFB, backwardHeuristic, forwardHeuristic, start, goal); }
            expandForward = true;
            //****
            // GetExpandForward_rcpsp=expandForward;
            //****
        }
    } else { // BS* policy, roughly Pohl's criterion
        if (forwardQueue.OpenSize() > backwardQueue.OpenSize())
            { lastSideForward = false; Expand(backwardQueue, forwardQueue, backwardMeet, forwardMeet, trueFB, backwardHeuristic, forwardHeuristic, start, goal); }
        else
            { lastSideForward = true; Expand(forwardQueue, backwardQueue, forwardMeet, backwardMeet, trueFF, forwardHeuristic, backwardHeuristic, goal, start); }
    }

    if (nodesExpanded > _sideE0) {
        const double _dt = std::chrono::duration<double>(std::chrono::steady_clock::now() - _sideT0).count();
        if (lastSideForward) { expandedFwd += nodesExpanded - _sideE0; sideSecFwd += _dt; }
        else                 { expandedBwd += nodesExpanded - _sideE0; sideSecBwd += _dt; }
    }
    return false;
}

/**
 * Expands a single node
 * @param current The current open list from which we will take our node
 * @param opposite The opposite openClosed list (e.g., if current is forward, this is backward)
 * @param heuristic The heuristic for the current direction
 * @param reverse_heuristic The heuristic for the opposite direction
 * @param target The node we are aiming for (goal in forward, start in backward)
 * @param source The node we started from, opposite of target
 */
template<class state, class action, class environment, class priorityQueue>
void BAE_RCPSP<state, action, environment, priorityQueue>::Expand(priorityQueue &current, priorityQueue &opposite,
                                                            std::unordered_map<uint64_t, uint64_t> &currentMeet,
                                                            std::unordered_map<uint64_t, uint64_t> &oppositeMeet,
                                                            std::vector<double> &currentF,
                                                                  Heuristic<state> *heuristic,
                                                            Heuristic<state> *reverse_heuristic,
                                                            const state &target, const state &source) {
    // resolve a state in the OPPOSITE queue by its meet hash (not its identity hash)
    auto oppLookup = [&](const state &st, uint64_t &objKey) -> dataLocation {
        auto it = oppositeMeet.find(env->GetMeetHash(st));
        if (it == oppositeMeet.end()) return kNotFound;
        objKey = it->second;
        return opposite.Lookup(objKey).where;
    };
    auto oppLookupH = [&](uint64_t mh, uint64_t &objKey) -> dataLocation {   // same, hash already computed
        auto it = oppositeMeet.find(mh);
        if (it == oppositeMeet.end()) return kNotFound;
        objKey = it->second;
        return opposite.Lookup(objKey).where;
    };
    static const bool prof = [](){ const char* e = std::getenv("RCPSP_TT2_PROF"); return e && std::atoi(e) != 0; }();
    using BxClock = std::chrono::steady_clock;
    auto bxNow = [&]() { return prof ? BxClock::now() : BxClock::time_point{}; };
    auto bxAdd = [&](int i, BxClock::time_point t0) { if (prof) bxSec[i] += std::chrono::duration<double>(BxClock::now() - t0).count(); };
    const auto bxPop0 = bxNow();
    uint64_t nextID;
    bool success = false;
    // This does lazy nipping, which means we do not we do not expand a node that was already closed in the opposite
    // direction. We search until we find one that is not closed
    while (current.OpenSize() > 0) {
        nextID = current.Close();
        uint64_t reverseLoc;
        static const bool nipOff = [](){ const char* e=std::getenv("RCPSP_BI_NIP");
                                         return e && std::atoi(e)==0; }();
        if (nipOff) { success = true; break; }
        auto loc = oppLookup(current.Lookup(nextID).data, reverseLoc);
        if (loc != kClosedList) {
            success = true;
            break;
        }
        env->Probe(current.Lookup(nextID).data, 4, current.Lookup(nextID).g, -1, currentCost);
    }

    bxAdd(0, bxPop0);
    // This can only fail if we found no nodes like that, meaning the current openList is empty as well
    if (!success)
        return;

    bool foundBetterSolution = false;
    nodesExpanded++;

    // This is to update for necessary expansions
    counts[getLowerBound()] += 1;

    if (current.Lookup(nextID).reopened == false)
        uniqueNodesExpanded++;

env->SetStateG(current.Lookup(nextID).data, current.Lookup(nextID).g);
    {   // how deep this side has got: activities started on its own net, and g
        const int pr = env->Progress(current.Lookup(nextID).data);
        const double gg = current.Lookup(nextID).g;
        if (&current == &forwardQueue) { if (pr > maxProgF) maxProgF = pr; if (gg > maxGF) maxGF = gg; }
        else                           { if (pr > maxProgB) maxProgB = pr; if (gg > maxGB) maxGB = gg; }
    }
    { const auto t0 = bxNow(); env->GetSuccessors(current.Lookup(nextID).data, neighbors); bxAdd(1, t0); }
    const auto bxChild0 = bxNow();
    uint64_t passParent = nextID;   // RCPSP_TT2_WAITJUMP: the previous re-created wait node of this expansion
    for (auto &succ: neighbors) {

        nodesTouched++;
        uint64_t childID;
        uint64_t hash = env->GetStateHash(succ);
        auto loc = current.Lookup(hash, childID);
        auto &childData = current.Lookup(childID);
        auto &parentData = current.Lookup(nextID);

        double edgeCost = env->GCost(parentData.data, succ);

        // RCPSP_TT2_WAITJUMP: a wait node skipped by a longer wait edge, re-created only so this side holds its cut.
        // Added CLOSED (its one successor, the next wait, is in this same list), indexed for the meet, and joined with
        // the other side exactly like a generated node; no heuristic and no OPEN entry. A known node reached more
        // cheaply this way is relinked (an OPEN one keeps its place with the lower g); nothing is reopened, since such
        // a node's only successor is the next wait, handled right here.
        if (env->IsPassThrough(succ)) {
            const bool fwdSide = (&current == &forwardQueue);
            const double g = parentData.g + edgeCost;
            uint64_t pid;
            if (loc == kNotFound) {
                pid = current.AddClosedNode(succ, hash, g, 0.0, passParent);
                noteG(fwdSide, pid);
            } else if (fless(g, childData.g)) {
                pid = childID;
                const double gDiff = childData.g - g;
                childData.parentID = passParent;
                childData.g = g;
                if (childData.where == kOpenList) {
                    childData.h = childData.h - (BAE_RCPSP_USE_F_PRIORITY ? gDiff : (2 * gDiff));
                    shiftTrueF(currentF, childID, gDiff);
                    current.KeyChanged(childID);
                }
                env->SetStateG(childData.data, g);
                noteG(fwdSide, childID);
            } else { passParent = childID; continue; }
            passParent = pid;
            const uint64_t mhp = env->GetMeetHash(succ);
            {   auto mit = currentMeet.find(mhp);
                if (mit == currentMeet.end() || fless(g, current.Lookup(mit->second).g)) currentMeet[mhp] = pid; }
            uint64_t passOpp;
            const auto ol = oppLookupH(mhp, passOpp);
            if (ol == kOpenList || ol == kClosedList) {
                ++meetHits;
                const double c = g + opposite.Lookup(passOpp).g;
                noteJoin(fwdSide, pid, passOpp, 0, c);
                if (fless(c, currentCost)) {
                    foundBetterSolution = true; solutionUpdates++;
                    currentCost = c;
                    middleNode = succ;
                    recordMeet(fwdSide, g, opposite.Lookup(passOpp).g);
                }
            }
            if (DomMeetOn()) {
                const double c = env->DomMeet(succ, g, currentCost, pid);
                if (fless(c, currentCost)) { OfferSolution(c, succ, fwdSide, g); noteJoin(fwdSide, pid, env->dmLastRef, env->dmLastShift, c); }
            }
            continue;
        }

        // ignore states with greater cost than best solution
        // DEFAULT ON = do NOT discard children with g/f >= U. HOG2's BAE.h does discard them
        // (lines 506/556) and that is SOUND here: with the state-g sync in place, reopened=0 on
        // all 2,053 post-fix j30 gate rows, so no node is ever reached more cheaply after it was
        // expanded. (An earlier comment claimed BAE* closes nodes with suboptimal g -- wrong; that
        // symptom was the state-g desync.) Keeping f >= U nodes is a COVERAGE choice: they are
        // what lets min-f rise to U, so the f-bound can PROVE optimality. Discard them and a
        // queue drains instead, and a drain is not accepted as proof while dominance is on
        // (7 j30 instances, DR5=3+DR4: discard -> 0/7 proven, keep -> 5/7). Costs memory.
        // RCPSP_BI_NOUB=0 restores HOG2's discarding.
        static const bool noUB = [](){ const char* e = std::getenv("RCPSP_BI_NOUB");
                                       return e ? (std::atoi(e) != 0) : true; }();
        if (!noUB && fgreatereq(parentData.g + edgeCost, currentCost))
            continue;

        switch (loc) {
            case kClosedList:
                // The in-tree copy asserted this away with exit(0), on the grounds that a
                // consistent heuristic makes it impossible. That holds when nodes are expanded
                // in f order — but BAE* expands in b(n) = 2g + h - h_opp order, which is NOT f,
                // so a node can legitimately close with a sub-optimal g and be reached better
                // later. (Zero-cost edges, from e==0 fires, make it commonplace here.) The
                // correct handling was already written directly below the exit, just dead:
                // pathmax the stored priority, relink, and REOPEN. Enabled.
                if (fless(parentData.g + edgeCost, childData.g)) {
                    reopenedNodes++;
                    // In HOG2's BAE.h this whole branch is DEAD CODE after exit(0) ("Since BAE*
                    // requires a consistent heuristic, this situation should be impossible"). It
                    // has not fired once since the fixes (reopened=0 on 2,053 j30 gate rows); the
                    // `reopened` CSV column will flag it if that assumption ever breaks.
                    // `.h` holds the PRIORITY b (2g + h - h_opp, or g + h under
                    // BAE_RCPSP_USE_F_PRIORITY), NOT a heuristic. The in-tree line here was
                    //     childData.h = max(childData.h, parentData.h - edgeCost);
                    // i.e. pathmax, which is meaningless on a priority and can only RAISE it
                    // -- while g DROPS just below. The node then sits in OPEN advertising a
                    // priority higher than its true g+h, which inflates min-f over the open
                    // list and therefore inflates getLowerBound() ABOVE C*, certifying a
                    // non-optimal incumbent. h and h_opp are static per state, so the correct
                    // update is exactly the kOpenList one: shift the priority by the g change.
                    double gDiff = childData.g - (parentData.g + edgeCost);
                    childData.parentID = nextID;
                    childData.g = parentData.g + edgeCost;
                    childData.h = childData.h - (BAE_RCPSP_USE_F_PRIORITY ? gDiff : (2 * gDiff));
                    shiftTrueF(currentF, childID, gDiff);
                    env->SetStateG(childData.data, childData.g);
                    noteG(&current == &forwardQueue, childID);
                    current.Reopen(childID);
                }
                break;
            case kOpenList: // Update cost if needed
            {
                if (fless(parentData.g + edgeCost, childData.g)) {



                    childData.parentID = nextID;
                    double gDiff = childData.g - (parentData.g + edgeCost);
                    childData.g = parentData.g + edgeCost;



                    // Modify total error accordingly. Since b uses 2g in the formula, and we reduced g (as h and h_r
                    // are static), all that needs to change is 2 times the difference
                    childData.h = childData.h - (BAE_RCPSP_USE_F_PRIORITY ? gDiff : (2 * gDiff));
                    shiftTrueF(currentF, childID, gDiff);   // h_own is static; f drops with g
                    env->SetStateG(childData.data, childData.g);
                    noteG(&current == &forwardQueue, childID);
                    current.KeyChanged(childID);
                    const uint64_t mh2 = env->GetMeetHash(succ);   // once per child: indexed here, looked up below
                    {   // g improved => this node may now be the best one for its cut
                        auto mit2 = currentMeet.find(mh2);
                        if (mit2 == currentMeet.end() || fless(childData.g, current.Lookup(mit2->second).g))
                            currentMeet[mh2] = childID;
                    }

                    // Check if we found a potential solution
                    uint64_t reverseLoc;
                    auto loc = oppLookupH(mh2, reverseLoc);
                    if (loc == kOpenList||loc==kClosedList) {

                        ++meetHits;
                        noteJoin(&current == &forwardQueue, childID, reverseLoc, 0, parentData.g + edgeCost + opposite.Lookup(reverseLoc).g);
                        if (fless(parentData.g + edgeCost + opposite.Lookup(reverseLoc).g, currentCost)) {
                            foundBetterSolution = true; solutionUpdates++;
                            currentCost = parentData.g + edgeCost + opposite.Lookup(reverseLoc).g;
                            if (std::getenv("RCPSP_BI_TRACE"))
                                std::cout << "    [meet-a] U=" << currentCost
                                          << " gOwn=" << (parentData.g + edgeCost)
                                          << " gOpp=" << opposite.Lookup(reverseLoc).g
                                          << " exp=" << nodesExpanded << std::endl;
                            middleNode = succ;
                            recordMeet(&current == &forwardQueue, parentData.g + edgeCost, opposite.Lookup(reverseLoc).g);
                        }
                    }
                    if (DomMeetOn()) {
                        const double gOwn = parentData.g + edgeCost;
                        const double c = env->DomMeet(succ, gOwn, currentCost, childID);
                        if (fless(c, currentCost)) { OfferSolution(c, succ, &current == &forwardQueue, gOwn);
                                                     noteJoin(&current == &forwardQueue, childID, env->dmLastRef, env->dmLastShift, c); }
                    }
                }
            }
                break;
            case kNotFound: {
                double g = parentData.g + edgeCost;
                const auto bxH0 = bxNow();
                double h = std::max(heuristic->HCost(succ, target), epsilon);
                bxAdd(3, bxH0);

                // Ignore nodes that don't have lower f-cost than the incumbent solution
                if (!fless(g + h, currentCost)) {
                    env->Probe(succ, 3, g, h, currentCost);
                    if (!noUB) break;
                }

                const auto bxH1 = bxNow();
                double totalError = BAE_RCPSP_USE_F_PRIORITY
                                  ? (g + h)
                                  : ((2 * g) + h - reverse_heuristic->HCost(succ, source));
                bxAdd(3, bxH1);

                const auto bxA0 = bxNow();
                uint64_t newID = current.AddOpenNode(succ, hash, g, totalError, nextID);
                noteG(&current == &forwardQueue, newID);
                env->Probe(succ, 2, g, h, currentCost);
                setTrueF(currentF, newID, g + h);   // true f, independent of the b priority
                if (nextID < currentF.size() && currentF[nextID] != DBL_MAX && fless(g + h, currentF[nextID])) {
                    ++(&current == &forwardQueue ? fDropF : fDropB);   // f fell along this edge: h is inconsistent here
                    fDropMax = std::max(fDropMax, currentF[nextID] - (g + h));
                }
                bxAdd(4, bxA0);
                const uint64_t mh = env->GetMeetHash(succ);   // once per child: indexed here, looked up below
                {   // index for the opposite side, keeping the BEST-g node for this cut
                    auto mit = currentMeet.find(mh);
                    if (mit == currentMeet.end() || fless(g, current.Lookup(mit->second).g))
                        currentMeet[mh] = newID;
                }

                // Check if we found a potential solution
                uint64_t reverseLoc;
                auto loc = oppLookupH(mh, reverseLoc);
                if (loc == kOpenList||loc==kClosedList) {


                    ++meetHits;
                    noteJoin(&current == &forwardQueue, newID, reverseLoc, 0, current.Lookup(nextID).g + edgeCost + opposite.Lookup(reverseLoc).g);
                    if (fless(current.Lookup(nextID).g + edgeCost + opposite.Lookup(reverseLoc).g, currentCost)) {
                        foundBetterSolution = true; solutionUpdates++;
                        currentCost = current.Lookup(nextID).g + edgeCost + opposite.Lookup(reverseLoc).g;
                        if (std::getenv("RCPSP_BI_TRACE"))
                            std::cout << "    [meet-b] U=" << currentCost
                                      << " gOwn=" << (current.Lookup(nextID).g + edgeCost)
                                      << " gOpp=" << opposite.Lookup(reverseLoc).g
                                      << " exp=" << nodesExpanded << std::endl;
                        middleNode = succ;
                        recordMeet(&current == &forwardQueue, current.Lookup(nextID).g + edgeCost, opposite.Lookup(reverseLoc).g);
                    }
                }
                if (DomMeetOn()) {
                    const double c = env->DomMeet(succ, g, currentCost, newID);
                    if (fless(c, currentCost)) { OfferSolution(c, succ, &current == &forwardQueue, g);
                                                 noteJoin(&current == &forwardQueue, newID, env->dmLastRef, env->dmLastShift, c); }
                }
            }
        }
    }
    bxAdd(2, bxChild0);
}

/**
 * A function that does an active nipping
 * @param s The state to nip the successors of
 * @param reverse The openClosed list in the opposite direction to which the state was expanded from
 * @deprecated
 */
template<class state, class action, class environment, class priorityQueue>
void BAE_RCPSP<state, action, environment, priorityQueue>::Nip(const state &s, priorityQueue &reverse)
{
    assert(!"Not using this code currently - the correct implementation of 'remove' is unclear from BS*");
    // At this point parent has been removed from open
    // Need to find any successors that have a parent id of parent & recursively remove them from open

    std::vector<state> n;

    uint64_t parentID;
    auto loc = reverse.Lookup(env->GetStateHash(s), parentID);
    assert(loc == kClosedList);
    env->GetSuccessors(s, n);
    for (auto &succ: n) {
        uint64_t childID;
        uint64_t hash = env->GetStateHash(succ);
        auto loc = reverse.Lookup(hash, childID);
        auto &childData = reverse.Lookup(childID);
        if (loc == kClosedList && childData.parentID == parentID) {
            Nip(childData.data, reverse);
        } else if ((loc == kOpenList||loc==kClosedList) && (childData.parentID == parentID)) {
            if (childData.data == middleNode) {
                std::cout << "Error - removing middle node\n";
                if (&reverse == &forwardQueue)
                    std::cout << "In backward search - removing from for\n";
                else
                    std::cout << "In forward search - removing from back\n";
                std::cout << "Parent: " << s << "\n";
                std::cout << "Middle: " << middleNode << "\n";
                std::vector<state> pFor, pBack, final;
                ExtractPathToGoal(middleNode, pBack);
                ExtractPathToStart(middleNode, pFor);
                reverse(pFor.begin(), pFor.end());
                std::cout << "Path forward: \n";

                for (auto &s: pFor)
                    std::cout << s << "\n";
                std::cout << "Path backward: \n";
                for (auto &s: pBack)
                    std::cout << s << "\n";

                exit(0);
            }
            reverse.Remove(env->GetStateHash(childData.data));
        }
    }
}


#endif //BAE_H
