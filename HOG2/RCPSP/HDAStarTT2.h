#pragma once
#ifndef HDA_STAR_TT2_H
#define HDA_STAR_TT2_H
// ─────────────────────────────────────────────────────────────────────────────
// HDAStarTT2.h — hash-distributed A* (HDA*, Kishimoto, Fukunaga & Botea 2009) for the
// forward TT2 search, assembled from HOG2 parts. HOG2 has no HDA* (only ParallelIDAStar and,
// upstream, ParallelAStarIDAStar, which are IDA* below a frontier: no duplicate detection and
// no DR5), so the two HOG2 pieces are used as-is and nothing in HOG2 is modified:
//   * each worker runs its own HOG2 TemplateAStar over the states it OWNS (HOG2's duplicate
//     detection, decrease-key and re-opening unchanged; re-opening is switched on because HDA*
//     expands out of global f order);
//   * workers exchange states through HOG2's SharedQueue (utils/SharedQueue.h), one inbox per
//     worker, one batch per expansion and destination.
//
// OWNERSHIP = hash of TT2DominanceTable::MakeKey (scheduled set = finished ∪ active). Two TT2
// states can only dominate each other (DR5) or be duplicates when they share that key, so every
// DR5 check and every duplicate check is exact and stays inside one worker. DR5 runs at the owner
// (RCPSP_TT2::GetSuccessors skips it inside workers, g_tt2_hda_worker), against that worker's own
// thread_local table.
//
// TERMINATION. UB = best goal cost found (atomic). A worker is BUSY while its open list holds a
// node with f < UB. `work` counts busy workers + batches sent and not yet processed. A sender
// counts a batch before queueing it, a receiver counts itself busy before releasing the batch, and
// UB only falls, so an idle worker cannot become busy without a batch. Hence work == 0 means no
// node with f < UB exists anywhere: UB is optimal (h is admissible; DR5 keeps a dominating state
// for every pruned one).
// On the deadline every worker stops expanding, waits for the others, drains its inbox into its
// open list, and reports its f_min. The proven LB is min over workers (capped by UB).
// ─────────────────────────────────────────────────────────────────────────────

#include <atomic>
#include <thread>
#include <vector>
#include <memory>
#include <chrono>
#include <climits>
#include <cfloat>
#include "../utils/SharedQueue.h"
#include "DominanceTT2.h"

namespace hda {

using Batch = std::vector<RCPSPState_TT2>;
using BatchPtr = std::shared_ptr<Batch>;   // SharedQueue::Remove copies the item: pass a pointer, not the states

struct Shared {
    int N = 1;
    std::vector<std::unique_ptr<SharedQueue<BatchPtr>>> inbox;
    std::atomic<int>  ub{INT_MAX};       // best makespan found (or seeded incumbent)
    std::atomic<long> work{0};           // busy workers + batches in flight
    std::atomic<bool> stop{false};
    std::atomic<bool> timedOut{false};
    std::atomic<int>  arrived{0};        // deadline barrier
    std::chrono::steady_clock::time_point deadline;
};

inline int Owner(const RCPSPState_TT2& s, int N) {
    return (int)(TT2DominanceTable::KeyHash{}(TT2DominanceTable::MakeKey(s)) % (uint64_t)N);
}

inline void AtomicMin(std::atomic<int>& a, int v) {
    int cur = a.load();
    while (v < cur && !a.compare_exchange_weak(cur, v)) {}
}

// Per-worker statistics, read by the driver after join.
struct WorkerStats {
    uint64_t expanded = 0, touched = 0;
    long sentStates = 0, sentBatches = 0, recvStates = 0;
    long dr5Pruned = 0, dr5Inserts = 0, dr5Thinned = 0;
    double fMinAtEnd = DBL_MAX;          // after the final drain
    double heurSec = 0.0;
};

// The worker's environment: RCPSP_TT2 plus routing. Children this worker owns go through DR5 on
// its own table and are returned to its TemplateAStar; the rest are batched to their owners.
class RCPSP_TT2_HDA : public RCPSP_TT2 {
public:
    RCPSP_TT2_HDA(int me, Shared* sh, WorkerStats* st) : me(me), sh(sh), st(st), out(sh->N) {}
    void GetSuccessors(const RCPSPState_TT2& node, std::vector<RCPSPState_TT2>& neighbors) const override {
        all.clear();
        RCPSP_TT2::GetSuccessors(node, all);
        neighbors.clear();
        for (auto& c : all) {
            const int o = Owner(c, sh->N);
            if (o == me) {
                if (!g_tt2_dr5 || !get_tt2_dominance_table().check_and_insert(c)) neighbors.push_back(std::move(c));
            } else out[o].push_back(std::move(c));
        }
        for (int o = 0; o < sh->N; ++o) if ((int)out[o].size() >= BatchSize()) Send(o);
    }
    // Send every buffered state. The worker calls this every few expansions, before it goes idle
    // and before it stops: a buffered state is work this (busy) worker still holds.
    void Flush() const { for (int o = 0; o < sh->N; ++o) if (!out[o].empty()) Send(o); }
    // RCPSP_HDA_BATCH (default 64): states buffered per destination before a send.
    static int BatchSize() {
        static const int b = [](){ const char* e = std::getenv("RCPSP_HDA_BATCH"); int v = e ? std::atoi(e) : 64; return v > 0 ? v : 1; }();
        return b;
    }
private:
    void Send(int o) const {
        st->sentStates += (long)out[o].size(); ++st->sentBatches;
        sh->work.fetch_add(1);                        // counted BEFORE it becomes visible
        sh->inbox[o]->Add(std::make_shared<Batch>(std::move(out[o])));
        out[o] = Batch();
    }
    int me; Shared* sh; WorkerStats* st;
    mutable Batch all;
    mutable std::vector<Batch> out;
};

using WorkerAStar = TemplateAStar<RCPSPState_TT2, int, RCPSP_TT2_HDA>;

// Insert a state received from another worker into this worker's search: DR5 on the owner's
// table, then HOG2's own open/closed list. New -> AddOpenNode (h computed here); already
// present with a higher g -> lower it (decrease-key on OPEN, re-open on CLOSED), the same
// updates TemplateAStar::DoSingleSearchStep makes for a generated child.
inline void Inject(WorkerAStar& astar, RCPSP_TT2_HDA& env, const RCPSPState_TT2& goal, const RCPSPState_TT2& s) {
    if (g_tt2_dr5 && get_tt2_dominance_table().check_and_insert(s)) return;
    auto& oc = astar.openClosedList;
    const uint64_t hash = env.GetStateHash(s);
    uint64_t id;
    const dataLocation loc = oc.Lookup(hash, id);
    if (loc == kNotFound) {
        const double h = env.HCost(s, goal);
        oc.AddOpenNode(s, hash, (double)s.g + h, (double)s.g, h);
        return;
    }
    auto& n = oc.Lookup(id);
    if (fless((double)s.g, n.g)) {
        n.g = s.g; n.f = n.g + n.h; n.data = s;
        n.data.h = (short)n.h;                    // s.h was never computed here; children inherit data.h (zero-delay shortcut)
        if (loc == kOpenList) oc.KeyChanged(id); else oc.Reopen(id);
    }
}

inline bool HasWork(WorkerAStar& astar, const Shared& sh) {
    auto& oc = astar.openClosedList;
    return oc.OpenSize() > 0 && oc.Lookup(oc.Peek()).f < (double)sh.ub.load();
}

inline double FMin(WorkerAStar& astar) {
    auto& oc = astar.openClosedList;
    return oc.OpenSize() > 0 ? oc.Lookup(oc.Peek()).f : DBL_MAX;
}

// One worker. `astar` was InitializeSearch'ed on the main thread (HOG2's TemplateAStar writes a
// namespace-level timeout there); only the root's owner kept the root in its open list.
inline void Worker(int me, Shared& sh, WorkerAStar& astar, RCPSP_TT2_HDA& env,
                   const RCPSPState_TT2& goal, bool startsBusy, WorkerStats& st) {
    g_tt2_hda_worker = true;
    get_tt2_dominance_table().clear();
    std::vector<RCPSPState_TT2> path;
    bool busy = startsBusy;                       // the driver counted it in `work`
    BatchPtr batch;
    uint64_t iter = 0;
    double lostF = DBL_MAX;                       // f of a node HOG2's own timeout cut short (LB)
    while (!sh.stop.load(std::memory_order_relaxed)) {
        while (sh.inbox[me]->Remove(batch)) {
            st.recvStates += (long)batch->size();
            for (const auto& s : *batch) Inject(astar, env, goal, s);
            batch.reset();
            if (!busy && HasWork(astar, sh)) { busy = true; sh.work.fetch_add(1); }
            if (sh.work.fetch_sub(1) == 1) sh.stop = true;     // that batch was the last thing pending
        }
        if (!HasWork(astar, sh)) {
            env.Flush();
            if (busy) { busy = false; if (sh.work.fetch_sub(1) == 1) sh.stop = true; }
            else if (sh.work.load() == 0) sh.stop = true;
            std::this_thread::yield();
        } else {
            path.clear();
            const double fBefore = FMin(astar);
            if (astar.DoSingleSearchStep(path)) {
                if (!path.empty() && env.GoalTest(path.back(), goal)) AtomicMin(sh.ub, (int)path.back().g);
                else {   // HOG2's TemplateAStar timeout fired mid-expansion: its children are lost
                    lostF = std::min(lostF, fBefore); sh.timedOut = true; sh.stop = true;
                }
            }
            if ((iter & 15) == 15) env.Flush();
        }
        if ((++iter & 255) == 0 && std::chrono::steady_clock::now() > sh.deadline) { sh.timedOut = true; sh.stop = true; }
    }
    // Final drain (needed for a valid LB after a deadline): nobody expands past the barrier, so no
    // new batches appear; take in what is queued and report the open list's f_min.
    env.Flush();                                  // buffered states must reach their owners before the LB drain
    sh.arrived.fetch_add(1);
    while (sh.arrived.load() < sh.N) std::this_thread::yield();
    while (sh.inbox[me]->Remove(batch)) { for (const auto& s : *batch) Inject(astar, env, goal, s); batch.reset(); }
    st.fMinAtEnd = std::min(FMin(astar), lostF);
    st.expanded = astar.GetNodesExpanded();
    st.touched = astar.GetNodesTouched();
    auto& dt = get_tt2_dominance_table();
    st.dr5Pruned = (long)dt.pruned; st.dr5Inserts = (long)dt.inserts; st.dr5Thinned = (long)dt.thinned;
    st.heurSec = g_heur_time_sec;
    dt.clear();                                   // free the table inside the thread that owns it
}

struct Result {
    bool proven = false, timedOut = false;
    int makespan = -1;                            // best found (UB), -1 if none
    double lb = 0.0;                              // proven lower bound
    std::vector<WorkerStats> stats;
};

// Solve from `root` with N workers. `seedUB` (if > 0) is a known feasible makespan.
inline Result Solve(const RCPSPState_TT2& root, const RCPSPState_TT2& goal, int N, double timeoutSec, int seedUB = 0) {
    Shared sh;
    sh.N = N;
    for (int i = 0; i < N; ++i) sh.inbox.emplace_back(new SharedQueue<BatchPtr>());
    if (seedUB > 0) sh.ub = seedUB;
    sh.deadline = std::chrono::steady_clock::now() + std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<double>(timeoutSec));

    Result res;
    res.stats.resize(N);
    std::vector<std::unique_ptr<RCPSP_TT2_HDA>> envs;
    std::vector<std::unique_ptr<WorkerAStar>> astars;
    const int rootOwner = Owner(root, N);
    std::vector<RCPSPState_TT2> path;
    for (int i = 0; i < N; ++i) {
        envs.emplace_back(new RCPSP_TT2_HDA(i, &sh, &res.stats[i]));
        astars.emplace_back(new WorkerAStar());
        astars[i]->SetReopenNodes(true);
        if (!astars[i]->InitializeSearch(envs[i].get(), root, goal, path)) {   // root is a goal
            res.proven = true; res.makespan = (int)root.g; res.lb = root.g; return res;
        }
        if (i != rootOwner) astars[i]->openClosedList.Reset(envs[i]->GetMaxHash());
    }
    sh.work = 1;                                  // the root's owner starts busy
    std::vector<std::thread> threads;
    for (int i = 0; i < N; ++i)
        threads.emplace_back(Worker, i, std::ref(sh), std::ref(*astars[i]), std::ref(*envs[i]),
                             std::cref(goal), i == rootOwner, std::ref(res.stats[i]));
    for (auto& t : threads) t.join();

    const int ub = sh.ub.load();
    res.timedOut = sh.timedOut.load();
    res.makespan = (ub == INT_MAX) ? -1 : ub;
    if (!res.timedOut && sh.work.load() == 0) {   // exhausted below UB
        res.proven = (ub != INT_MAX);
        res.lb = res.proven ? ub : 0.0;
    } else {
        double lb = DBL_MAX;
        for (const auto& s : res.stats) lb = std::min(lb, s.fMinAtEnd);
        res.lb = std::min(lb, (double)ub);
        res.proven = (ub != INT_MAX && res.lb >= ub);
    }
    return res;
}

} // namespace hda

#endif // HDA_STAR_TT2_H
