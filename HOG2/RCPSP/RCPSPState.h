//
// Created by idolu on 06/01/2025.
//
#pragma once
#include <set>
#include "petriclasses.h"
#include "readPetri.cpp"
#include <span> // Only if you are using C++20 for the helper function
#include "Globals.h"
#ifndef RCPSPSTATE_H
#define RCPSPSTATE_H
using namespace P_RCPSP;

template<short N>
class oldRCPSPState {
public:
    oldRCPSPState();
    oldRCPSPState(const oldRCPSPState& predecessor, const std::vector<short>& subset, uint64_t& count);
    // std::vector<short> marking;
    std::vector<std::pair<short, short>> activeTransitionIndices;  // can possibly be calculated on the fly

    std::array<short, N> startedActivitiys;   // Was vector
    std::array<short, N> finishedActivitiys;  // Was vector

    // bool status;
    short g=0;
    // mutable short h;

    bool operator==(const oldRCPSPState& other) const;
};
using RCPSPState_old_30 = oldRCPSPState<32>;
using RCPSPState_old_60 = oldRCPSPState<62>;
using RCPSPState_old_90 = oldRCPSPState<92>;

class RCPSPState {
  public:
  RCPSPState();
     RCPSPState(const RCPSPState& predecessor,const P_RCPSP::Transition& newTransition,bool status,short location,uint64_t &count);

    std::vector<short> marking;
    std::vector<std::pair<short, short>> activeTransitionIndices;  // can possibly be calculated on the fly

    std::array<short, 128> startedActivitiys;   // Was vector
    std::array<short, 128> finishedActivitiys;  // Was vector

    bool status;
  short g=0;
  mutable short h;

  bool operator==(const RCPSPState& other) const;
};

class RCPSPState_TT {
public:
    std::array<std::vector<std::pair<short, short>>, 4> resource_nodes;
    std::vector<std::pair<short, short>> activity_nodes;
    std::array<short, 128> finishedActivitiys;  // Changed from vector
    short g = 0;
mutable short h = 0;
    short predessesor_h = 0;
    RCPSPState_TT();
    RCPSPState_TT(const RCPSPState_TT& prev, short ID, short firingTime);
    bool operator==(const RCPSPState_TT& other) const;
};

#include <vector>
#include <array>
#include <bitset>
#include <memory>
#include <utility> // for std::pair


int computeEarlyFinishTime(int activityId);

class RCPSPState_Bi {
public:
    // Core state from TT
    std::array<std::vector<std::pair<short, short>>, 4> resource_nodes;
    std::vector<std::pair<short, short>> activity_nodes;
    std::array<short, 128> finishedActivitiys;
    short g = 0;

    // BAE-specific fields
    bool direction = true;     // true = forward, false = backward
    short f = 0;             // f-value for priority queue
    short g_f = 0;           // g-value in forward direction
    short g_b = 0;           // g-value in backward direction
    short h_f = 0;           // forward heuristic
    short h_b = 0;           // backward heuristic

    // Constructors
    RCPSPState_Bi();
    RCPSPState_Bi(const RCPSPState_Bi& prev, short ID, short firingTime);

    // Convert from TT state
    RCPSPState_Bi(const RCPSPState_TT& tt_state) {
        resource_nodes = tt_state.resource_nodes;
        activity_nodes = tt_state.activity_nodes;
        finishedActivitiys = tt_state.finishedActivitiys;
        g = tt_state.g;
        direction = true;
        f = g_f = g_b = h_f = h_b = 0;
    }

    // Equality operator
    bool operator==(const RCPSPState_Bi& other) const {
        // Debug output
        static int compare_count = 0;
        if (++compare_count <= 5) {
            std::cout << "Comparing states:" << std::endl;
            std::cout << "  This direction=" << direction << ", other direction=" << other.direction << std::endl;

            int this_finished_count = 0, other_finished_count = 0;
            for (int i = 0; i < 128; i++) {
                if (finishedActivitiys[i] != -1) this_finished_count++;
                if (other.finishedActivitiys[i] != -1) other_finished_count++;
            }
            std::cout << "  This finished=" << this_finished_count << ", other finished=" << other_finished_count << std::endl;
        }

        // Check finished activities
        for (int i = 0; i < 128; i++) {
            bool this_finished = (finishedActivitiys[i] != -1);
            bool other_finished = (other.finishedActivitiys[i] != -1);
            if (this_finished != other_finished) return false;
        }

        // Check activity place markings
        if (activity_nodes.size() != other.activity_nodes.size()) return false;
        for (int i = 0; i < activity_nodes.size(); i++) {
            if (activity_nodes[i].first != other.activity_nodes[i].first) {
                return false;
            }
        }

        if (compare_count <= 5) {
            std::cout << "  STATES ARE EQUAL!" << std::endl;
        }

        return true;
    }

    //
    // // 2. Same activity place markings (token counts only)
    // if (activity_nodes.size() != other.activity_nodes.size()) return false;
    // for (int i = 0; i < activity_nodes.size(); i++) {
    //     if (activity_nodes[i].first != other.activity_nodes[i].first) {
    //         return false;
    //     }
    // }

    // 3. NEW: Compare g-values (time/cost)?
    // This could distinguish states at different times
    // BUT: for bidirectional, we might NOT want this
    // because forward at time=50 should match backward at time=50-from-goal

    // 4. NEW: Compare direction?
    // if (direction != other.direction) return false;

//     return true;
// }

    // Create goal state
};


// Tag for the explicit-wait child constructor (RCPSP_TT2_WAIT=1): advance the clock without firing anything.
struct TT2WaitTag {};

class RCPSPState_TT2 {
public:
    // 1. Resources: Pairs of <Amount/ID, TimeRemaining>
    // TimeRemaining = 0 means available NOW.
    std::array<std::vector<std::pair<short, short>>, 4> resource_nodes;

    // 2. Activity Tokens: Pairs of <PlaceID, TimeRemaining>
    std::vector<std::pair<short, short>> activity_nodes;

    // 3. Finished Tasks: 16 bytes vs 128 bytes
    std::bitset<128> finishedActivitiys;

    // 4. Cached Indices (Derived Data)
    // You want to keep this for now.
    // WARNING: This is redundant data (derived from the nodes above).
    std::vector<std::pair<short, short>> activeTransitionIndices;

    // Add these for caching transitions
    mutable std::vector<std::pair<short, short>> AvailableTransitionIndices_TT2;
    mutable bool transitionsCached = false;
    bool direction=1;
    // false at the ROOT: the root has no incoming firing. (Was uninitialized, which
    // also let HCost's zero-cost h-caching reuse predessesor_h=0 for the root.)
    bool isDeltaZero = false;
    bool isCriticalInActive;
    short g = 0;
    short g_pre = 0;
    mutable short h = 0;
    mutable bool  hKnown = false;   // annotation (NOT in ==/hash): h already supplied (fused successor cache) => HCost returns it
    short predessesor_h = 0;
    short lastTransitionId=0; // transition fired to create THIS state (most-recent, not final)
    // Explicit-wait model with DR4 (RCPSP_TT2_WAIT=1): how many more units this wait chain may still wait and then
    // start activities. -1 = not inside a wait chain (root, a firing, or DR4 off). Part of the identity (== and the
    // hashes) because it restricts the successors.
    short waitSlack = -1;
    mutable short nextCritical;
    // (abs_start removed: was a 256-byte shadow schedule; single-res releases are now derived
    //  from the marking, order-swap is stubbed, and the final schedule comes from the path.)
    RCPSPState_TT2();
    RCPSPState_TT2(const RCPSPState_TT2& prev, short ID, short firingTime,bool Direction =1);
    RCPSPState_TT2(const RCPSPState_TT2& prev, short waitTime, TT2WaitTag);   // explicit-wait child (RCPSPState.cpp)
    bool order_swap_prunable() const;   // Hartmann 1998 Rule 7, TT2 form (uses abs_start)

    // Equality is CRITICAL for Relative Time
    bool operator==(const RCPSPState_TT2& other) const {
        // 1. Bitset comparison (fast)
        if (finishedActivitiys != other.finishedActivitiys) return false;
        if (waitSlack != other.waitSlack) return false;   // explicit-wait model: different remaining waits

        // 2. Vectors must be SORTED for this to work!
        // If State A has [{1, 5}, {2, 3}] and State B has [{2, 3}, {1, 5}],
        // they are the same state, but == will return false if not sorted.
        if (activity_nodes != other.activity_nodes) return false;
        if (resource_nodes != other.resource_nodes) return false;
        if (activeTransitionIndices != other.activeTransitionIndices) {
            std::cerr << "HASH COLLISION: states equal but different activeTransitionIndices!" << std::endl;
            return false;  // treat as different states for now
        }
        return true;
        // CRITICAL: Do NOT compare activeTransitionIndices here!
        // Since it is derived data, if it is calculated/sorted slightly differently
        // it might prevent merging of identical states.
        // Only compare the "Physical" state.

        return true;
    }
};

// RCPSP_TT2_TBSPEC=<keys>: explicit order among nodes of equal priority (f in TT2, b in HBAE), replacing the
// built-in chains (TT2: higher g, more finished, more active; HBAE: zero-delay child, higher g, more finished,
// active). One letter per key, most significant first; UPPER case = larger first, lower case = smaller first:
//   G elapsed time g            F finished activities        A activities in progress
//   P started = finished + in progress                        R total remaining time of the activities in progress
//   M longest remaining time among them                       Z zero-delay child (reached by a start at delay 0)
//   L id of the transition fired last
// "GFA" is the TT2 default, "ZGFA" the HBAE default with RCPSP_BI_ACTIVETB=1. Any order among equal-priority
// nodes keeps the search optimal. Read from the environment here so every search mode sees the same value.
inline const std::string& tt2TbSpec() {
    static const std::string v = [] { const char* e = std::getenv("RCPSP_TT2_TBSPEC"); return std::string(e ? e : ""); }();
    return v;
}
// -1: a is expanded first, +1: b first, 0: tie under the spec
inline int tt2TbCompare(const RCPSPState_TT2& a, double ga, const RCPSPState_TT2& b, double gb) {
    for (const char c : tt2TbSpec()) {
        long x = 0, y = 0;
        switch (c | 0x20) {
            case 'g': x = (long)ga; y = (long)gb; break;
            case 'f': x = (long)a.finishedActivitiys.count(); y = (long)b.finishedActivitiys.count(); break;
            case 'a': x = (long)a.activeTransitionIndices.size(); y = (long)b.activeTransitionIndices.size(); break;
            case 'p': x = (long)(a.finishedActivitiys.count() + a.activeTransitionIndices.size());
                      y = (long)(b.finishedActivitiys.count() + b.activeTransitionIndices.size()); break;
            case 'r': for (const auto& p : a.activeTransitionIndices) x += p.second;
                      for (const auto& p : b.activeTransitionIndices) y += p.second; break;
            case 'm': for (const auto& p : a.activeTransitionIndices) if (p.second > x) x = p.second;
                      for (const auto& p : b.activeTransitionIndices) if (p.second > y) y = p.second; break;
            case 'z': x = a.isDeltaZero ? 1 : 0; y = b.isDeltaZero ? 1 : 0; break;
            case 'l': x = a.lastTransitionId; y = b.lastTransitionId; break;
            default:  continue;
        }
        if (x != y) return ((c >= 'A' && c <= 'Z') ? (x > y) : (x < y)) ? -1 : 1;
    }
    return 0;
}


class RCPSPState_BI_TT2 {
public:
    // 1. Resources: Pairs of <Amount/ID, TimeRemaining>
    // TimeRemaining = 0 means available NOW.
    std::array<std::vector<std::pair<short, short>>, 4> resource_nodes;

    // 2. Activity Tokens: Pairs of <PlaceID, TimeRemaining>
    std::vector<std::pair<short, short>> activity_nodes;

    // 3. Finished Tasks: 16 bytes vs 128 bytes
    std::bitset<128> finishedActivitiys;

    // 4. Cached Indices (Derived Data)
    // You want to keep this for now.
    // WARNING: This is redundant data (derived from the nodes above).
    std::vector<std::pair<short, short>> activeTransitionIndices;
    short fireTime=0;
    // Add these for caching transitions
    mutable std::vector<std::pair<short, short>> AvailableTransitionIndices_TT2;
    mutable bool transitionsCached = false;
    bool direction=1;
    bool isDeltaZero;
    short g_f = 0;
    short g_b = 0;
    short g_pre = 0;
    mutable short h_f = 0;
    mutable short h_b = 0;
    // mutable short f_f=0;
    // mutable short f_b=0;
    short predessesor_h_f = 0;
    short predessesor_h_b = 0;
    short lastTransitionId=0; // <--- SAVE IT HERE
    RCPSPState_BI_TT2();
    RCPSPState_BI_TT2(const RCPSPState_BI_TT2& prev, short transitionId, short firingTime,bool Direction =1);

    // Equality is CRITICAL for Relative Time
    bool operator==(const RCPSPState_BI_TT2& other) const {

        if (finishedActivitiys != other.finishedActivitiys) return false;
        if (activity_nodes != other.activity_nodes) return false;
        if (resource_nodes != other.resource_nodes) return false;

        // Temporarily add this:
        if (activeTransitionIndices != other.activeTransitionIndices) {
            std::cerr << "HASH COLLISION: states equal but different activeTransitionIndices!" << std::endl;
            return false;  // treat as different states for now
        }
        return true;
    }

};
inline size_t CONFLICT_SIZE = 32; // set this before creating any object


// RCPSP_CBS_DELAY (Globals.h): one remembered delay. Every activity of M starts no earlier than the earliest finish
// among X. A precedence arc u -> v is X = {u}, M = {v}. Immutable; shared by a node and its descendants (newest first).
struct CbsRel {
    std::vector<short> v;                 // X (the first nX entries, sorted), then M (sorted)
    unsigned short nX = 0;
    uint64_t h = 0;                       // hash of (X, M)
    std::shared_ptr<const CbsRel> up;     // the older conditions
};

template<short N>
class RCPSPState_CBS {
public:
    std::array<short, N> start_times;
     mutable short t;                    // time of the conflict CBS branches on (prioritized, or first)
     mutable short resourceType;
     mutable bool  found_conflict      = false;
     mutable short conflict_number=0;

     // DR5 (DominanceCBS.h): the EARLIEST conflict, tracked separately from the
     // branched-on conflict above. The schedule is conflict-free before t_first,
     // so t_first — not t — is the time the cutset reduction is taken at. Kept
     // independent of prioritization so DR5 works in either conflict mode.
     mutable short t_first = -1;                      // earliest conflict time (t*), -1 if none
     mutable std::vector<short> first_conflict_pool;  // participants of the earliest conflict

     // HCost cache: compute_h_and_RVS is expensive and state-deterministic, so
     // the env caches its result here after the first call. Fresh child states
     // start invalid (member initializer), so a copy never inherits a stale h.
     mutable double h_cache  = 0;
     mutable bool   h_cached = false;

     mutable std::vector<short> rvs_activities_pool; //activies in the conflict -- off if useing MDA

     mutable std::vector<MDA> conflict_solutions;//need to enable-- if we use MDA cache it stay empty
     mutable ConflictKey<N> conflict_key;  // empty if not using both MDA sets and cache

     std::vector<std::pair<short,short>> added_precedences={}; // accumulated from root
     mutable  bool is_size2_conflict = false;

     // RCPSP_CBS_DELAY: the remembered delays of this node (null when the flag is 0). rel_sig / rel_n identify the
     // SET of conditions (the same set reached in another order gives the same values) and are part of the identity.
     std::shared_ptr<const CbsRel> rel;
     uint64_t rel_sig = 0;
     unsigned short rel_n = 0;
     bool propagate_rel();                                                             // least schedule satisfying everything; false = none
     bool settle_rel(short parent_makespan);                                           // re-settle a child that adds no condition
     bool attach_rel(std::vector<short> X, std::vector<short> M, short parent_makespan);   // add (X, M) and settle

     // Warm-start branch-ordering key (RCPSP_WARMSTART=1). Set by the child ctors
     // to the inflated-schedule start time of the activity this child delays;
     // GetSuccessors stable-sorts siblings by it. 0 when warm start is off.
     short warmstart_key = 0;

     // RCPSP_CBS_TB tie-break annotations. Inherited, never part of the identity (== / hash).
     short tb_depth  = 0;     // resolutions applied since the root
     short tb_tfirst = -1;    // lower bound on this node's earliest conflict time, from its ancestors
     // RCPSP_CBS_TBSPEC keys C / O / N. C and O are the PARENT's values until this node is evaluated.
     mutable short tb_nconf = 0;   // conflict events (time point x resource over capacity)
     mutable int   tb_over  = 0;   // total overload over those events
     // RCPSP_CBS_TBSPEC keys X / K / M: the branching conflict's excess demand, size and number of MDAs.
     mutable short tb_x = 0, tb_m = 0;
     mutable unsigned char tb_k = 0;
     unsigned      tb_seq   = 0;   // creation order within the instance
     short f_floor   = 0;     // RCPSP_CBS_PATHMAX: the parent's final f (integral makespan lower bound)
     // RCPSP_CBS_LEARN: the activities this child delayed, kept only when the delay left the makespan unchanged
     // (a free MDA). HCost credits them once the child's f is known. Never part of the identity (== / hash).
     unsigned char lw_n = 0;
     std::array<unsigned char, 3> lw_act{};
     void lw_note(const RCPSPState_CBS& prev, const short* acts, size_t n) {
         if (!g_cbs_learn || start_times[g_sink_id] != prev.start_times[g_sink_id]) return;
         for (size_t i = 0; i < n && lw_n < 3; ++i)
             if (acts[i] >= 0 && acts[i] < 128) lw_act[lw_n++] = (unsigned char)acts[i];
     }
     // "feasible until": exact once evaluated (conflict-free => +inf), else the inherited lower bound
     short tb_key() const {
         if (h_cached) return found_conflict ? std::max(tb_tfirst, t_first) : (short)32000;
         return tb_tfirst;
     }
     // RCPSP_CBS_TB >= 4: total delay of the schedule (sum of start times; the CPM schedule has the minimum)
     int tb_sum() const { int s = 0; for (short i = 0; i <= g_sink_id; ++i) s += start_times[i]; return s; }

    short compute_h_and_RVS() const;

    // Light scan: find ONLY the earliest resource conflict (t*, participants,
    // resource) without any scoring/MDA work. Sets t_first/first_conflict_pool
    // and returns whether a conflict exists; res_out gets the resource index
    // (unchanged if no conflict). Deliberately does NOT touch t / resourceType /
    // found_conflict / rvs_activities_pool — those belong to the branched-on
    // conflict, written by the full compute_h_and_RVS. Semantics are identical
    // to the earliest conflict the full scans find.
    bool compute_first_conflict(short& res_out) const;

    // Greedy dive to a FEASIBLE schedule, reusing the CBS machinery: repeatedly
    // take the earliest conflict (compute_first_conflict — no RVS/MDA/scoring),
    // delay one participant out of that time slot (min-push), propagate, until
    // conflict-free. Returns that feasible schedule's makespan — a valid upper
    // bound. Returns SHRT_MAX if it can't converge (then no UB, safe). The
    // returned schedule is verified conflict-free before its makespan is trusted.
    short greedy_dive_makespan() const;

    void propagate_with_strong_form_0();
    short compute_start_recursive(short act, std::array<bool, N>& visited);

    void propagate(short activityId);
    // void propagate_latest(short delayedActivity);
    void computeLatestStarts(std::array<short, N>& latest) const;
    RCPSPState_CBS();
    RCPSPState_CBS(const RCPSPState_CBS& prev, short delayedActivity, short duration);
    RCPSPState_CBS(const RCPSPState_CBS& prev, const std::vector<short>& mda_activities, short conflict_t);
    RCPSPState_CBS(const RCPSPState_CBS& prev, short from, short to, short conflict_t);

    bool isLeftShiftable() const;

    // Sound left-shift pruning test (B&P/Schrage), restricted to what provably
    // persists in every descendant: an activity of the SCHEDULED SET (finish <=
    // t_first) idle w.r.t. its precedence-earliest start whose one-unit left
    // shift is resource-feasible. Scheduled-set starts are frozen (future
    // conflicts sit at t >= t_first) and usage at s-1 only decreases, so the
    // gap never closes: no descendant is an active schedule. NOTE: unlike
    // isLeftShiftable(), this checks RESOURCE feasibility of the shift —
    // without it every CBS delay would be flagged.
    bool left_shift_prunable() const;

    // Order-swap candidate (Hartmann 1998 Bounding Rule 7), MEASUREMENT ONLY.
    // True iff the frozen set (finish <= t_first) contains a back-to-back pair
    // i>j with f_i == s_j, sharing a resource, and no precedence either way — the
    // non-canonical order that rule 7 would prune. Detector counts how often this
    // survives DR5; it does NOT prune (soundness of the prune in a delay search
    // needs the canonical order to be reachable via a sibling — validated later).
    bool order_swap_candidate() const;
    bool dominates(const RCPSPState_CBS& other) const;
    std::span<const short> get_conflict_activities(const Conflict& c) const {
        return std::span<const short>(&rvs_activities_pool[c.activity_start_index], c.num_activities);
    }
    // Equality is CRITICAL for Relative Time
    bool operator==(const RCPSPState_CBS& other) const {
        // if (added_precedences != other.added_precedences) return false;
        if (start_times != other.start_times) return false;
        if (rel_sig != other.rel_sig || rel_n != other.rel_n) return false;   // RCPSP_CBS_DELAY: same schedule, other conditions = another node

        return true;
    }
    short makespan() const {
        return start_times[g_sink_id];
    }
    short compute_mda_cost(
        const std::vector<short>& mda_set,
        const std::vector<short>& current_jobs,
        const std::array<short, N>& latest_starts,
        int res_idx) const;

    // Returns earliest t' >= prev_starts[activity] where
    // background demand (bg_activities active at t') + demand(activity) <= capacity.
    // Used by use_non_minimal_delay to find true feasible start instead of min-push.
    short compute_nonminimal_delay(
        short activity,
        const std::array<short, N>& prev_starts,
        const std::vector<short>& bg_activities,
        int res_idx) const;

    // Improvement 2 (RCPSP_MDA_RECURSE): *this is an MDA child whose delayed set
    // `mda_members` has already cleared the original conflict's complement, but may
    // itself be internally over-capacity on res_idx. Detect the earliest internal
    // over-capacity window, branch over the sub-MDA selections that resolve it, push
    // each further past the rest (non-minimal), and recurse until internally
    // feasible. One leaf child per resolution is appended to `out`; if `mda_members`
    // is already internally feasible, *this is emitted unchanged. `depth` is the
    // recursion level (0 at the outer call). Requires use_non_minimal_delay.
    void gen_internal_mda_split(const std::vector<short>& mda_members,
                                int res_idx, int depth,
                                std::vector<RCPSPState_CBS<N>>& out) const;

    std::vector<MDA> compute_mdas(
    const std::vector<short>& current_jobs,
    short excess_demand,
    const std::array<short, N>& latest_starts,
    int res_idx,
    ConflictKey<N>& key) const;
    void enumerate_sets(
    const std::vector<short>& current_jobs,
    short excess_demand,
    int res_idx,
    std::vector<std::vector<short>>& sets) const;
    void enumerate_sets_bnb(
    const std::vector<short>& sorted_jobs,
    const std::vector<short>& demands,
    short excess_demand,
    int start_idx,
    short current_demand,
    std::vector<short>& current_set,
    std::vector<std::vector<short>>& sets) const;
};
using RCPSPState_CBS_30 = RCPSPState_CBS<32>;
using RCPSPState_CBS_60 = RCPSPState_CBS<62>;
using RCPSPState_CBS_90 = RCPSPState_CBS<92>;
using RCPSPState_CBS_120 = RCPSPState_CBS<122>;
class RCPSPState_BAP {
public:
    std::array<short, 122> start_times; // 244 bytes
    mutable short t;
    mutable short resourceType;
    mutable short num_activities;    // short sink_id;
    mutable std::vector<short> rvs_activities_pool; // THE GLOBAL POOL
    void computeFirstRVS() const;

    void propagate(short activityId);

    RCPSPState_BAP();
    RCPSPState_BAP(const RCPSPState_BAP& prev, short delayedActivity, short duration);
    // In RCPSPState.h - replace span function with:
    // const short* get_conflict_start(const Conflict& c) const {
    //     return rvs_activities_pool.data() + c.activity_start_index;
    // }
    std::span<const short> get_conflict_activities(const Conflict& c) const {
        return std::span<const short>(&rvs_activities_pool[c.activity_start_index], c.num_activities);
    }

    // Equality is CRITICAL for Relative Time
    bool operator==(const RCPSPState_BAP& other) const {

        if (start_times != other.start_times) return false;
        return true;
    }
    short makespan() const {
        return start_times[g_sink_id] + start_times[g_sink_id];
    }

};
#include "../../HOG2/generic/TemplateAStar.h"

// template<>
// struct AStarCompareWithF<RCPSPState_CBS> {
//     bool operator()(const AStarOpenClosedDataWithF<RCPSPState_CBS>& i1,
//                     const AStarOpenClosedDataWithF<RCPSPState_CBS>& i2) const {
//         if (i1.f != i2.f) return i1.f > i2.f;
//         if (i1.g != i2.g) return i1.g < i2.g;
//         if (i1.data.best_score != i2.data.best_score)
//             return i1.data.best_score < i2.data.best_score;
//         return i1.data.depth < i2.data.depth;
//     }
// };
// IN YOUR HEADER FILE (.h)
std::vector<std::pair<short, short>> getAvailableTransitionIndices_TT2(
    const std::vector<short> &unstartedTransitions,
    const std::bitset<128> &finishedActivitiys,  // ← Changed to bitset
    const std::array<std::vector<std::pair<short, short>>, 4> &resource_nodes,
    const std::vector<std::pair<short, short>> &activity_nodes,
    const std::vector<std::pair<short, short>> &activeTransitionIndices
);

std::vector<std::pair<short, short>> getAvailableTransitionIndices_TT2_backward(
    const std::vector<short> &unstartedTransitions,
    const std::bitset<128> &finishedActivitiys,  // ← Changed to bitset
    const std::array<std::vector<std::pair<short, short>>, 4> &resource_nodes,
    const std::vector<std::pair<short, short>> &activity_nodes,
    const std::vector<std::pair<short, short>> &activeTransitionIndices
);


std::vector<std::pair<short, short>> getAvailableTransitionIndices_TT(
    const std::vector<short> &unstartedTransitions,
    const std::vector<short> &finishedActivitiys,
    const std::array<std::vector<std::pair<short, short>>, 4> &resource_nodes,
    const std::vector<std::pair<short, short>> &activity_nodes
);
double getForwardHcost_TT(std::vector<short>unstartedTransitions);
double getForwardHcost(std::vector<short>unstartedTransitions,
                      std::vector<std::pair<short, short>>activeTransitionIndices
                      );
double getBackwardHcost(std::vector<short>unstartedTransitions,
                      std::vector<std::pair<short, short>>activeTransitionIndices
                      );
short getForwardHcost_TT2(
    const std::vector<short>& unstartedTransitions,
    const std::vector<std::pair<short, short>>& activity_tokens,
    const std::vector<std::pair<short, short>>& active_activities,
    const std::bitset<128>& finishedActivitiys) ;
double getForwardHcost_TT(const std::array<short, 128>& unstartedTransitions);


double getForwardHcost(const std::vector<short>& unstartedTransitions,
                      const std::vector<std::pair<short, short>>& activeTransitionIndices,
                      short& nextCritical);
bool cpFastOn();   // RCPSP_CP_FAST (default 1): array-DP critical path instead of the std::map version
double getforwardResource(const std::vector<short>& unstartedTransitions,
                          const std::vector<std::pair<short, short>>& activeTransitionIndices);
bool resFastOn();
// RCPSP_GEN_FAST (default 1): successor generation / availability fast paths; RCPSP_GEN_CHECK=1 compares with the reference.
bool genFastOn();
bool genCheckOn();
struct ActDemandSlot {                       // per-instance activity demand tables (see actDemandSlot)
    uint64_t gen = ~0ULL; const void* ptr = nullptr; int n = -1;
    std::vector<std::vector<std::pair<short, short>>> ent;   // per activity: (resource place id, demand), resource_demands order
    std::vector<std::array<int, 4>> byPlace;                 // per activity: positive demand summed per resource place id
    bool placeOK = true;                                    // every positive-demand resource place id is < 4
};
const ActDemandSlot& actDemandSlot();
   // RCPSP_RES_FAST (default 1): cached demand matrix instead of per-call map lookups
double thetaResourceBound_TT2(const std::vector<short>& tempUnfinished,
                              const std::vector<std::pair<short, short>>& activeTransitionIndices);
double getForwardHcost0Based(std::vector<short> unstartedTransitions,
                      std::vector<std::pair<short, short>> activeTransitionIndices);

#endif // RCPSPSTATE_H