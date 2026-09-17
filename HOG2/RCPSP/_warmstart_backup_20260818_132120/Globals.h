#ifndef GLOBALS_H
#define GLOBALS_H

#include <vector>
#include <string>
#include <cstdint>
#include <limits>
#include "petriclasses.h"

using namespace P_RCPSP;
short LB=0;
// ── Structs and Enums ─────────────────────────────────────────────
struct ResourceInfo {
    std::string resource_nume;
    short capacity;
    std::vector<short> activity_indices;
    std::vector<short> demands;
    std::unordered_map<short, short> demand_lookup; // activity_idx -> demand

};

struct Conflict {
    short t                  = 0;
    short resourceType       = 0;
    short num_activities     = 0;
    int   activity_start_index = 0;
};

enum class ConflictType : uint8_t {
    CARDINAL      = 0,
    SEMI_CARDINAL = 1,
    NON_CARDINAL  = 2
};

enum class HeuristicType : uint8_t {
    NONE = 0,
    CG   = 1,
    DG   = 2,
    HCBS = 3,
};
struct MDA {
    std::vector<short> activities;
    short cost; // max delta over members, with new_start computed from non-set members
};

template<short N>
struct ConflictKey;

template<>
struct ConflictKey<32> {
    uint32_t mask;
    short res_idx;

    bool operator==(const ConflictKey& o) const {
        return mask == o.mask && res_idx == o.res_idx;
    }
};

template<>
struct ConflictKey<62> {
    uint64_t mask;
    short res_idx;

    bool operator==(const ConflictKey& o) const {
        return mask == o.mask && res_idx == o.res_idx;
    }
};

template<>
struct ConflictKey<92> {
    uint64_t low;
    uint32_t high;
    short res_idx;

    bool operator==(const ConflictKey& o) const {
        return low == o.low && high == o.high && res_idx == o.res_idx;
    }
};

template<>
struct ConflictKey<122> {
    uint64_t low;
    uint64_t high;
    short res_idx;

    bool operator==(const ConflictKey& o) const {
        return low == o.low && high == o.high && res_idx == o.res_idx;
    }
};

template<short N>
struct ConflictKeyHash;

template<>
struct ConflictKeyHash<32> {
    size_t operator()(const ConflictKey<32>& k) const {
        return std::hash<uint32_t>()(k.mask) ^
               (std::hash<short>()(k.res_idx) << 1);
    }
};

template<>
struct ConflictKeyHash<62> {
    size_t operator()(const ConflictKey<62>& k) const {
        return std::hash<uint64_t>()(k.mask) ^
               (std::hash<short>()(k.res_idx) << 1);
    }
};

template<>
struct ConflictKeyHash<92> {
    size_t operator()(const ConflictKey<92>& k) const {
        return std::hash<uint64_t>()(k.low) ^
               (std::hash<uint32_t>()(k.high) << 1) ^
               (std::hash<short>()(k.res_idx) << 2);
    }
};

template<>
struct ConflictKeyHash<122> {
    size_t operator()(const ConflictKey<122>& k) const {
        return std::hash<uint64_t>()(k.low) ^
               (std::hash<uint64_t>()(k.high) << 1) ^
               (std::hash<short>()(k.res_idx) << 2);
    }
};



// cache type alias per N
template<short N>
using MDACache = std::unordered_map<ConflictKey<N>, std::vector<std::vector<short>>, ConflictKeyHash<N>>;

// one global cache per N
template<short N>
MDACache<N>& get_mda_cache() {
    static MDACache<N> cache;
    return cache;
}

// Cache: conflict bitmask -> minimal sets (no cost)
// std::unordered_map<ConflictKey, std::vector<std::vector<short>>, ConflictKeyHash> g_mda_cache;

struct CBSConfig {
    bool use_conflict_prioritization = true;
    bool use_first_conflict          = false;
    bool use_ancestor_branching          = false;
    bool use_dominance          = false;   // existing: sibling-only pruning among a parent's children
    bool use_dr5                = false;   // DR5 cutset dominance vs. a global table (DominanceCBS.h)
    bool use_pair_decomposition         = false;
    bool use_greed_conflic_resultion_asstimation=false;
    bool use_MDA_sets=true;
    bool use_MDA_cache=true;
    bool use_strong_constraints=true;
    bool use_MDA_BAB=true;
    bool use_non_minimal_delay=false; // delay each activity to earliest truly-feasible time instead of min-push
    HeuristicType heuristic          = HeuristicType::HCBS;
};

// ── Problem instance ──────────────────────────────────────────────
inline PetriExample    petri;
inline RCPSP_example   RCPSPex;
inline short           g_num_activities = 0;
inline short           g_sink_id        = 0;
inline std::string     finalstatename;
inline std::string     initialstatename;

// ── Precomputed structures ────────────────────────────────────────
inline std::vector<std::vector<short>> downstream;
inline std::vector<std::vector<short>> upstream;
inline std::vector<ResourceInfo>       resource_info;

// ── CBS Configuration ─────────────────────────────────────────────
inline CBSConfig       setting;

// ── Debug ─────────────────────────────────────────────────────────
inline long            debug_cardinal_num = 0;
inline bool            allcorrect         = true;

// ── Upper-bound pruning (env RCPSP_UB=1) ─────────────────────────
// g_incumbent: makespan of the best FEASIBLE schedule known (SGS seed at the
// root, tightened whenever a conflict-free child appears). Children whose
// makespan >= g_incumbent are dropped (B&P Descendants Line 4): propagate only
// pushes forward, so their whole subtree is >= incumbent, which we already hold.
// If OPEN exhausts without a goal, the incumbent IS the optimum.
inline bool  g_use_ub     = false;
inline short g_incumbent  = std::numeric_limits<short>::max();
inline long  g_ub_pruned  = 0;

// ── Per-rule prune counters (debug) ──────────────────────────────
// Reset per instance in solveRCPSP_CBS. UB/dominance already counted elsewhere
// (g_ub_pruned; the dominance table's own `pruned`); leftshift had none.
inline long  g_leftshift_pruned = 0;   // children dropped by left_shift_prunable()

// ── Experiment flags (each off by default; env-driven) ───────────
inline bool  g_use_leftshift = false;  // RCPSP_LEFTSHIFT=1: drop left-shiftable children
inline bool  g_use_bidir     = false;  // RCPSP_BIDIR=1: new states also kill dominated stored states
inline bool  g_use_hybrid    = false;  // RCPSP_HYBRID=1: prio falls back to earliest conflict when best score < 1
inline float g_hybrid_threshold = 0.5f; // RCPSP_HYBRID_T: cardinality score below which we prefer first-conflict
inline bool  g_use_lean      = false;  // RCPSP_LEAN=1: strip conflict vectors from OPEN/CLOSED copies (memory for time)
inline bool  g_use_inline    = false;  // RCPSP_INLINE=1: expand intermediate children in place (B&P Descendants recursion)
inline bool  g_dom_skyline   = false;  // RCPSP_SKYLINE=1: keep dominance buckets as a Pareto frontier (retire stored entries dominated by a newer one). Search-IDENTICAL for pruning (transitivity), only shrinks buckets => faster checks + less memory. Unlike BIDIR it does NOT feed the kill-set, so it never changes which nodes are expanded.
inline bool  g_use_lazy      = false;  // RCPSP_LAZY=1: use LazyAStarCBS (deferred-heuristic A*) instead of TemplateAStar — compute the expensive compute_h_and_RVS only when a node is POPPED, not at insertion, so never-expanded OPEN nodes skip it. Optimal (h=0 is an admissible lower bound + lazy re-insertion).
inline bool  g_tt2_dr5       = false;  // RCPSP_TT2_DR5=1: cutset (DR5) dominance for the TT2/TTPNR search (DominanceTT2.h), checked+inserted in RCPSP_TT2::GetSuccessors. Includes always-on skyline thinning.
inline bool  g_tt2_batch     = false;  // RCPSP_TT2_BATCH=1: batch/macro expansion — each successor starts a resource-feasible SUBSET of the now-available activities in one edge (plus the time-advancing single firings), trading depth for branching. Superset of single-firing (singletons included) => still optimal.
inline long  g_tt2_batch_cap = 200000; // RCPSP_TT2_BATCH_CAP: safety cap on feasible subsets enumerated per node (singletons emitted first, so a cap never breaks optimality). Guards the 2^|A0| worst case on resource-abundant instances.

// ── TT2 symmetry breaking (RCPSP_TT2_SYM=1), NON-BATCH mode only ─────────────
// Within a "zero-delay run" (consecutive tau=0 firings, no clock advance) the order
// of the fired activities is irrelevant: a tau=0 firing only CONSUMES resources and
// emits its outputs at delay=duration>0, so it can never enable anything new =>
// anything available after is available before, and a->b / b->a yield a bit-identical
// state (active list sorted, resource tokens canonically sorted+merged). Those
// duplicates are caught today only AFTER being constructed+hashed. This imposes a
// canonical INCREASING-id order so the redundant orderings are never generated at all.
// Saves generation cost (ctor + hash + open/closed lookup), not tree size.
// 0=off, 1=canonical INCREASING id (skip b<last), 2=canonical DECREASING id (skip b>last).
// Both are equally sound (any fixed total order works); they differ ONLY in traversal
// order, which is the clean way to isolate plateau/tie-break effects from real pruning.
inline int   g_tt2_sym        = 0;
inline long  g_tt2_sym_pruned = 0;     // successors skipped by the canonical-order rule
inline bool  g_tt2_gendesc    = false;   // RCPSP_TT2_GENDESC=1: generate TT2 successors in DESCENDING id; with the heap's last-in-first-out tie tendency this makes the canonical (smaller-id) sibling expand first — canonical-first ordering without a comparator tie-break.
inline bool  g_tt2_sym_tiebreak = false; // RCPSP_TT2_SYMTB=1: deterministic final tie-break preferring SMALLER lastTransitionId (canonical-aligned) so the canonical parent expands before the non-canonical one => symmetry breaking can then only remove REDISCOVERIES, never a first-discovery edge => should become strictly-not-worse.

#endif // GLOBALS_H