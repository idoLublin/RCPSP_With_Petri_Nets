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
    short predessesor_h = 0;
    short lastTransitionId=0; // transition fired to create THIS state (most-recent, not final)
    mutable short nextCritical;
    // SHADOW absolute start time per activity (1-based id -> start clock; -1 = not
    // started). Set when an activity fires (= g at that point). Used ONLY by the
    // order-swap dominance check (RCPSP_ORDERSWAP). DELIBERATELY excluded from
    // operator== AND GetStateHash — two states with the same marking but different
    // start histories must still merge; abs_start is an annotation, never identity.
    std::array<short, 128> abs_start;
    RCPSPState_TT2();
    RCPSPState_TT2(const RCPSPState_TT2& prev, short ID, short firingTime,bool Direction =1);
    bool order_swap_prunable() const;   // Hartmann 1998 Rule 7, TT2 form (uses abs_start)

    // Equality is CRITICAL for Relative Time
    bool operator==(const RCPSPState_TT2& other) const {
        // 1. Bitset comparison (fast)
        if (finishedActivitiys != other.finishedActivitiys) return false;

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

     // Warm-start branch-ordering key (RCPSP_WARMSTART=1). Set by the child ctors
     // to the inflated-schedule start time of the activity this child delays;
     // GetSuccessors stable-sorts siblings by it. 0 when warm start is off.
     short warmstart_key = 0;

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


double getForwardHcost(std::vector<short>unstartedTransitions,
                      std::vector<std::pair<short, short>>activeTransitionIndices,
                      short& nextCritical  // ← add this

                      );
double getforwardResource(std::vector<short>unstartedTransitions,std::vector<std::pair<short, short>>activeTransitionIndices);
double thetaResourceBound_TT2(const std::vector<short>& tempUnfinished,
                              const std::vector<std::pair<short, short>>& activeTransitionIndices);
double getForwardHcost0Based(std::vector<short> unstartedTransitions,
                      std::vector<std::pair<short, short>> activeTransitionIndices);

#endif // RCPSPSTATE_H