#ifndef LazyAStarCBS_H
#define LazyAStarCBS_H
// ─────────────────────────────────────────────────────────────────────────────
// Lazy (deferred-heuristic) A* for the CBS search.
//
// WHY: in RCPSP_CBS, HCost(state) == compute_h_and_RVS(), which is the SINGLE most
// expensive per-node operation: it scans every resource for conflicts and builds
// the branching pool / MDA sets. In stock A* (TemplateAStar) that runs at
// INSERTION — once for every node added to OPEN (TemplateAStar.h:650) — even
// though the branching pool is only ever NEEDED when a node is expanded. On the
// hard RS=0.2 instances OPEN is far larger than CLOSED (e.g. j30 (13,1): ~2.5M
// generated, 146K expanded), so the vast majority of those scans are on nodes
// that are never expanded — pure waste. And with conflict-prioritization the
// heuristic value itself is often 0, so deferring it costs almost no guidance.
//
// WHAT: this variant inserts children onto OPEN with a trivial lower bound (h=0)
// and computes the real HCost (== compute_h_and_RVS, which also caches the
// branching pool on the state) only when a node is POPPED. If the true f = g+h
// turns out to exceed the next-best node on OPEN, the node is re-inserted with its
// corrected f instead of being expanded (standard lazy-A* re-insertion). h=0 is an
// admissible lower bound and the re-insertion keeps expansion in non-decreasing
// true-f order, so the returned makespan is still optimal — identical to eager A*.
// Expensive scans then run at most once per node that is ever popped (cached via
// the state's h_cached), i.e. ~O(expanded) instead of ~O(inserted).
//
// This is a COPY-BASED sibling of HOG2's DelayedHeuristicAStar (which batches the
// heuristic rather than skipping it); it lives here in RCPSP/ so the HOG2 library
// is untouched. It reuses AStarOpenClosed and the shared timeout / LB globals from
// TemplateAStar.h. Driver selects it via RCPSP_LAZY=1 (g_use_lazy).
//
// Interface intentionally mirrors the subset of TemplateAStar that Driver uses:
// GetPath(env, from, to, path), GetNodesExpanded(), GetNodesTouched().
// ─────────────────────────────────────────────────────────────────────────────

#include "../generic/TemplateAStar.h"   // AStarOpenClosed, AStarCompareWithF, Heuristic,
                             // Constraint, and the shared globals:
                             //   astar_timeout_seconds, timeout, LB

// Diagnostics (parallel to g_ub_pruned etc. in Globals.h): how many HCost scans
// were actually done (== nodes evaluated at pop) and how many re-insertions the
// lazy bound caused. Reset per instance by the driver.
inline long g_lazy_hcost_evals = 0;   // full compute_h_and_RVS calls (at pop)
inline long g_lazy_reinserts   = 0;   // pops that were re-queued because f grew

template <class state, class action, class environment,
          class openList = AStarOpenClosed<state, AStarCompareWithF<state>, AStarOpenClosedDataWithF<state>> >
class LazyAStarCBS {
public:
    LazyAStarCBS() {
        env = 0; theHeuristic = 0; theConstraint = 0;
        stopAfterGoal = true;
        nodesExpanded = nodesTouched = 0;
        phi = [](double h, double g){ return g + h; };
    }

    void SetHeuristic(Heuristic<state> *h) { theHeuristic = h; }
    void SetConstraint(Constraint<state> *c) { theConstraint = c; }
    void SetStopAfterGoal(bool v) { stopAfterGoal = v; }

    uint64_t GetNodesExpanded() const { return nodesExpanded; }
    uint64_t GetNodesTouched()  const { return nodesTouched;  }

    void GetPath(environment *e, const state &from, const state &to, std::vector<state> &path);

    openList openClosedList;

private:
    bool InitializeSearch(environment *e, const state &from, const state &to, std::vector<state> &path);
    bool DoSingleSearchStep(std::vector<state> &path);
    void ExtractPathToStartFromID(uint64_t node, std::vector<state> &path);

    environment *env;
    Heuristic<state> *theHeuristic;
    Constraint<state> *theConstraint;
    bool stopAfterGoal;
    uint64_t nodesExpanded, nodesTouched;
    std::function<double(double, double)> phi;

    state start, goal;
    std::vector<state> neighbors;
};

template <class state, class action, class environment, class openList>
void LazyAStarCBS<state, action, environment, openList>::GetPath(
        environment *e, const state &from, const state &to, std::vector<state> &path)
{
    if (!InitializeSearch(e, from, to, path)) return;
    while (!DoSingleSearchStep(path)) {}
}

template <class state, class action, class environment, class openList>
bool LazyAStarCBS<state, action, environment, openList>::InitializeSearch(
        environment *e, const state &from, const state &to, std::vector<state> &path)
{
    if (theHeuristic == 0) theHeuristic = e;
    env = e;
    path.clear();
    openClosedList.Reset(env->GetMaxHash());
    nodesExpanded = nodesTouched = 0;
    start = from; goal = to;

    // Shared timeout budget (same mechanism as TemplateAStar's authorized change).
    timeout = std::chrono::steady_clock::now() + std::chrono::seconds(astar_timeout_seconds);

    if (env->GoalTest(from, to) && stopAfterGoal) return false;   // start already a goal

    // Root: insert with the trivial lower bound; it is expanded first regardless.
    // Explicit 6-arg form (f,g,h,parent) to disambiguate the overload; the
    // kTAStarNoNode parent makes AStarOpenClosed set the root's parentID to itself.
    openClosedList.AddOpenNode(start, env->GetStateHash(start), 0.0, 0.0, 0.0, kTAStarNoNode);
    return true;
}

template <class state, class action, class environment, class openList>
bool LazyAStarCBS<state, action, environment, openList>::DoSingleSearchStep(std::vector<state> &path)
{
    if (openClosedList.OpenSize() == 0) { path.clear(); return true; }   // no path

    // Timeout: bail exactly like TemplateAStar (the caller detects the empty path
    // + non-empty root pool as "timed out").
    if (std::chrono::steady_clock::now() > timeout) { path.clear(); return true; }

    uint64_t nodeid = openClosedList.Close();

    // ── LAZY EVALUATION ──────────────────────────────────────────────────────
    // The node was queued with h=0. If its true heuristic has not been computed
    // yet (state not h_cached), compute it now (this also caches the branching
    // pool on the state, so the subsequent GetSuccessors needs no rescan). If the
    // corrected f exceeds the next-best node on OPEN, this node is no longer the
    // minimum — re-queue it and pick again. h=0 is admissible and this keeps
    // expansion in true-f order, so optimality is preserved.
    if (!openClosedList.Lookup(nodeid).data.h_cached) {
        const double g = openClosedList.Lookup(nodeid).g;
        const double h = theHeuristic->HCost(openClosedList.Lookup(nodeid).data, goal);
        ++g_lazy_hcost_evals;
        openClosedList.Lookup(nodeid).h = h;
        openClosedList.Lookup(nodeid).f = phi(h, g);
        if (openClosedList.OpenSize() > 0 &&
            fgreater(openClosedList.Lookup(nodeid).f,
                     openClosedList.Lookup(openClosedList.Peek()).f)) {
            openClosedList.Reopen(nodeid);   // corrected f is no longer minimal
            ++g_lazy_reinserts;
            return false;
        }
    }

    // ── This node is the true minimum: expand it. ────────────────────────────
    nodesExpanded++;
    LB = std::max(LB, (short)openClosedList.Lookup(nodeid).f);   // proven lower bound

    if (stopAfterGoal && env->GoalTest(openClosedList.Lookup(nodeid).data, goal)) {
        ExtractPathToStartFromID(nodeid, path);
        std::reverse(path.begin(), path.end());
        return true;
    }

    neighbors.resize(0);
    env->GetSuccessors(openClosedList.Lookup(nodeid).data, neighbors);

    for (unsigned int x = 0; x < neighbors.size(); x++) {
        nodesTouched++;
        const double childG = openClosedList.Lookup(nodeid).g + env->GCost(openClosedList.Lookup(nodeid).data, neighbors[x]);

        if (theConstraint &&
            theConstraint->ShouldNotGenerate(start, openClosedList.Lookup(nodeid).data, neighbors[x], childG, goal))
            continue;

        uint64_t theID;
        dataLocation loc = openClosedList.Lookup(env->GetStateHash(neighbors[x]), theID);
        switch (loc) {
            case kClosedList:
                // A cheaper path to a closed node: reopen it (rare here — CBS edge
                // costs are non-negative and states are delay-monotone, but keep the
                // standard A* handling for correctness under re-expansion).
                if (fless(childG, openClosedList.Lookup(theID).g)) {
                    auto &i = openClosedList.Lookup(theID);
                    i.parentID = nodeid; i.g = childG; i.f = phi(i.h, i.g);
                    i.data = neighbors[x];
                    openClosedList.Reopen(theID);
                }
                break;
            case kOpenList:
                if (fless(childG, openClosedList.Lookup(theID).g)) {
                    auto &i = openClosedList.Lookup(theID);
                    i.parentID = nodeid; i.g = childG; i.f = phi(i.h, i.g);
                    i.data = neighbors[x];
                    openClosedList.KeyChanged(theID);
                }
                break;
            case kNotFound:
                // LAZY INSERT: queue with h=0; the real scan is deferred to pop.
                openClosedList.AddOpenNode(neighbors[x], env->GetStateHash(neighbors[x]),
                                           phi(0, childG), childG, 0, nodeid);
                break;
        }
    }
    return false;
}

template <class state, class action, class environment, class openList>
void LazyAStarCBS<state, action, environment, openList>::ExtractPathToStartFromID(
        uint64_t node, std::vector<state> &path)
{
    do {
        path.push_back(openClosedList.Lookup(node).data);
        node = openClosedList.Lookup(node).parentID;
    } while (openClosedList.Lookup(node).parentID != node);
    path.push_back(openClosedList.Lookup(node).data);
}

#endif // LazyAStarCBS_H
