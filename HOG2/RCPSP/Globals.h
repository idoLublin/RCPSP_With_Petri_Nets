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
    bool use_mda_recursive_delay=false; // Improvement 2: recursively resolve an internally-conflicted MDA (branch over MDA' sub-selections). NMD path only.
    bool use_nmd_precedence=false; // Improvement 1: emit the minimal-delay child alongside the non-minimal one (completeness-anchor sibling) so the opposite orderings the non-minimal jump drops stay reachable. NMD path only.
    bool use_nmd_prec_record=false; // Improvement 1 (extra, EXPERIMENTAL): also record+enforce p->m precedences on the non-minimal child. UNSOUND until operator== distinguishes added_precedences (a constrained state can otherwise shadow an equal unconstrained one). Default off.
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
inline bool  g_use_dr4       = false;  // RCPSP_DR4=1: DR4 (Liu non-delaying rule) reduced to the CBS partial schedule == the active-schedule / left-shift dominance (drives the same verified-sound left_shift_prunable() prune). Recorded as useDR4; prunes counted in leftshiftPruned.
inline bool  g_use_bidir     = false;  // RCPSP_BIDIR=1: new states also kill dominated stored states
inline bool  g_use_hybrid    = false;  // RCPSP_HYBRID=1: prio falls back to earliest conflict when best score < 1
inline float g_hybrid_threshold = 0.5f; // RCPSP_HYBRID_T: cardinality score below which we prefer first-conflict
inline bool  g_use_lean      = false;  // RCPSP_LEAN=1: strip conflict vectors from OPEN/CLOSED copies (memory for time)
inline bool  g_use_inline    = false;  // RCPSP_INLINE=1: expand intermediate children in place (B&P Descendants recursion)
inline bool  g_dom_skyline   = false;  // RCPSP_SKYLINE=1: keep dominance buckets as a Pareto frontier (retire stored entries dominated by a newer one). Search-IDENTICAL for pruning (transitivity), only shrinks buckets => faster checks + less memory. Unlike BIDIR it does NOT feed the kill-set, so it never changes which nodes are expanded.
inline bool  g_use_lazy      = false;  // RCPSP_LAZY=1: use LazyAStarCBS (deferred-heuristic A*) instead of TemplateAStar — compute the expensive compute_h_and_RVS only when a node is POPPED, not at insertion, so never-expanded OPEN nodes skip it. Optimal (h=0 is an admissible lower bound + lazy re-insertion).
inline bool  g_tt2_dr5       = false;  // RCPSP_TT2_DR5=1: cutset (DR5) dominance for the TT2/TTPNR search (DominanceTT2.h), checked+inserted in RCPSP_TT2::GetSuccessors. Includes always-on skyline thinning.
inline bool  g_tt2_dr4       = false;  // RCPSP_TT2_DR4=1: DR4 delayed-start dominance in TT2 successor generation (Liu et al. rule 4, ported from new_herustic). Prune firing l at delta d_l if some other available transition i has d_i < d_l and d_i + p_i <= d_l (sound left-shift; min-delta child never pruned).
inline long  g_tt2_dr4_pruned = 0;     // successors skipped by DR4 (per instance; CSV: dr4Pruned)
inline bool  g_tt2_immsel    = false;  // RCPSP_TT2_IMMSEL=1: immediate selection (D&H / Hartmann Remark 1) — when every available transition is fireable NOW and one of them cannot be co-processed with any not-yet-started activity, it is forced: emit only that single firing (a left-shift; no optimum lost). Branching reduction, not a state prune.
inline long  g_tt2_immsel_fired = 0;   // nodes where immediate selection collapsed the branching (per instance)
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

// ── Θ-tree resource-completion dual bound (RCPSP_TT2_THETA=1) ────────────────
// Adds Vilím's Θ-tree Earliest-Completion-Time bound (thetaResourceBound_TT2) as
// a THIRD admissible lower bound, max()-combined with the existing CPM critical-
// path + energy bounds in RCPSP_TT2::HCost. Resource-contention-aware: respects
// each activity's earliest start AND capacity jointly, so it is >= the plain
// Σenergy/C energy bound. Bound-only (no filtering/pruning); always admissible,
// so it can never make the search return a suboptimal makespan — only tighten h.
inline bool  g_tt2_theta        = false; // RCPSP_TT2_THETA=1: enable the Θ-tree bound in the TT2/TTPNR search
inline long  g_tt2_theta_better = 0;     // times the Θ-tree bound strictly beat the existing bound at a state (per instance; CSV: thetaBoundBetter)
// Same Θ-tree bound adapted to CBS (RCPSP_CBS_THETA=1): floored into HCost like the
// warm-start LB (est_i = start_times[i], an admissible earliest start). Ablatable,
// independently admissible, default off. Recorded in the CBS CSV (useThetaBound/thetaBoundBetter).
inline bool  g_cbs_theta        = false; // enable the Θ-tree bound in the CBS search
inline long  g_cbs_theta_better = 0;     // times the Θ-tree floor strictly beat the CBS HCBS bound at a state (per instance)

// ── Conflict-subset look-ahead LB (RCPSP_CBS_SUBSET=1), CBS only ─────────────
// At a node, take the branched conflict's participants + their SHALLOW downstream
// (bounded horizon, so it stays a small look-ahead, not a near-whole re-solve),
// solve that sub-RCPSP with releases = start_times via the standalone (CPM-guided,
// NON-recursive) mini solver capped at g_cbs_subset_expand expansions, and floor h
// with (M_subset − g). Admissible: the subset is a relaxation, so its optimum is a
// LB on this subtree's optimum; the expand cap only loosens it (min-f-on-OPEN).
// Targets HCBS's blind spot — the downstream cascade a resolved conflict forces.
inline bool  g_cbs_subset        = false; // enable the conflict-subset look-ahead bound
inline long  g_cbs_subset_expand = 500;   // RCPSP_CBS_SUBSET_EXPAND: sub-search expansion cap
inline int   g_cbs_subset_hops   = 3;     // RCPSP_CBS_SUBSET_HOPS: downstream horizon (precedence hops)
inline int   g_cbs_subset_size   = 16;    // RCPSP_CBS_SUBSET_SIZE: hard cap on subset activity count
// Gate: run the (costly) sub-solve only where it can pay off — a node is eligible if it
// is SHALLOW (strong early bound near the root, kills a big subtree) OR NEAR-INCUMBENT
// (f within the band below UB, where an admissible tightening can actually cross UB and
// prune). OR of two complementary regimes; either alone is cost-bounded. depth proxy =
// added_precedences.size(). The near-incumbent arm needs an incumbent (RCPSP_UB).
inline int   g_cbs_subset_maxdepth = 4;   // RCPSP_CBS_SUBSET_MAXDEPTH: run if depth <= this
inline int   g_cbs_subset_gap      = 5;   // RCPSP_CBS_SUBSET_GAP: run if 0 < UB-(g+h) <= this
// Per-instance telemetry to CALIBRATE the expand cap (avg expands + cap-hit rate):
inline long  g_cbs_subset_better       = 0; // times the subset LB strictly beat the existing h
inline long  g_cbs_subset_solves       = 0; // sub-solves performed
inline long  g_cbs_subset_expands_total= 0; // Σ sub-search expansions
inline long  g_cbs_subset_capped       = 0; // sub-solves that hit the expand cap (didn't finish)
inline long  g_cbs_subset_maxexpands   = 0; // max expansions any single sub-solve used
// Result cache: the wide-shallow CBS tree revisits the SAME (subset activity-set +
// release vector) many times, and that pair fully determines M_subset (durations/
// demands/caps/precedence are instance-constant). Memoize it so 70k solves collapse
// to the far fewer DISTINCT subproblems. Per-instance (cleared in solveRCPSP_CBS).
inline std::unordered_map<uint64_t, long> g_cbs_subset_cache;
inline long  g_cbs_subset_cache_hits   = 0; // sub-solves served from the cache

// ── Pragmatic min-cut resource LB (RCPSP_CBS_MINCUT=1), CBS only ─────────────
// Admissible ABSOLUTE makespan LB from a preemptive, window-based energetic
// relaxation solved as a max-flow (min-cut) per resource; binary-searched to the
// smallest feasible makespan (MinCutCBS.h). Floored into HCost like Θ/subset. It
// dominates the workload bound and is a different relaxation than the Θ-tree bound,
// so max(Θ, min-cut, …) can be tighter than either. Costly (K max-flows per binary-
// search step), so it is GATED to shallow-or-near-incumbent nodes like the subset
// bound. Independently admissible => bound-only, never changes which goals are legal.
inline bool g_cbs_mincut         = false; // RCPSP_CBS_MINCUT=1: enable the min-cut resource LB in CBS
inline long g_cbs_mincut_better  = 0;     // times the min-cut floor strictly beat the existing h (per instance; CSV: minCutBetter)
inline long g_cbs_mincut_calls   = 0;     // times the (gated) bound was actually computed (per instance; CSV: minCutCalls)
inline int  g_cbs_mincut_maxdepth = 6;    // RCPSP_CBS_MINCUT_MAXDEPTH: run if depth (added_precedences) <= this
inline int  g_cbs_mincut_gap      = 8;    // RCPSP_CBS_MINCUT_GAP: run if 0 < UB-(g+h) <= this (needs RCPSP_UB)

// ── RS-adaptive gating (RCPSP_CBS_RSADAPT=1), CBS ────────────────────────────
// Instance Resource Strength (RS) predicts difficulty: on j90, RS>=0.7 is already
// ~100% solved by the cheap heuristic, RS=0.5 is the swing zone, RS=0.2 the hard
// frontier (measured 2026-08-23). So the expensive resource bounds (min-cut,
// single-resource, Θ) only earn their cost at low RS. When RSADAPT is on, those
// bounds fire ONLY if the instance RS <= g_cbs_rs_threshold; at high RS the search
// runs on the cheap HCBS (=max(CP,RC)-style) heuristic alone. Pure gating — never
// changes admissibility, only where cost is spent.
inline bool   g_cbs_rsadapt      = true;  // RCPSP_CBS_RSADAPT (default ON): gate expensive bounds by RS, so a
                                          // resource bound (single-res/min-cut/Θ) is a LOW-RS heuristic by
                                          // default. Set RCPSP_CBS_RSADAPT=0 to run a bound at ALL RS.
inline double g_cbs_rs_threshold = 0.6;   // RCPSP_CBS_RS_THRESH: expensive bounds fire only if RS <= this
inline double g_instance_rs      = -1.0;  // current instance Resource Strength (set at load; <0 = unknown => gate open)
// True when the (costly) resource bounds are allowed to run at this instance.
inline bool cbs_rs_allows_expensive() {
  return !g_cbs_rsadapt || g_instance_rs < 0.0 || g_instance_rs <= g_cbs_rs_threshold + 1e-9;
}

// ── Single-resource relaxation max LB (RCPSP_CBS_SINGLERES=1), CBS ────────────
// For each resource k, relax all OTHER resources to unlimited and (approximately,
// via the capped subset mini-solver) solve the resulting single-resource RCPSP on
// the residual with releases = start_times; take the max over k. Admissible (each
// is a relaxation => optimum <= real optimum; the expand cap only loosens it), and
// STRONGER than Θ/energetic (which are themselves lower bounds OF the single-
// resource problem). Costly (R capped searches per node) => gated by depth and by
// RS (above). Floored into HCost like Θ/min-cut.
inline bool g_cbs_singleres          = false; // RCPSP_CBS_SINGLERES=1: enable the single-resource max LB
inline long g_cbs_singleres_better   = 0;     // times it strictly beat h (per instance; CSV: singleResBetter)
inline long g_cbs_singleres_calls    = 0;     // times the (gated) bound was computed (per instance; CSV: singleResCalls)
inline int  g_cbs_singleres_maxdepth = 3;     // RCPSP_CBS_SINGLERES_MAXDEPTH: run if depth <= this (shallow arm)
inline int  g_cbs_singleres_gap      = 8;     // RCPSP_CBS_SINGLERES_GAP: also run if 0 < UB-(g+h) <= this (near-incumbent arm; needs RCPSP_UB)
inline long g_cbs_singleres_expand   = 800;   // RCPSP_CBS_SINGLERES_EXPAND: per single-resource sub-solve expand cap
inline int  g_cbs_singleres_maxsize  = 45;    // RCPSP_CBS_SINGLERES_MAXSIZE: skip if residual larger than this (cost guard)

// ── TT2 port of RS-adaptive gating + single-resource LB ──────────────────────
// Same idea as the CBS versions above, on the relative-time TT2/TTPNR search.
// RS gate wraps TT2's expensive bounds (Θ, single-res); releases for the single-
// resource residual come from the abs_start shadow (started => abs_start, unstarted
// => g, both admissible lower bounds on the true start). TT2 has no UB pruning, so
// the single-res gate is RS + residual-size only (no near-incumbent arm).
inline bool   g_tt2_rsadapt      = true;  // RCPSP_TT2_RSADAPT (default ON): single-res/Θ are LOW-RS heuristics
                                          // by default. Set RCPSP_TT2_RSADAPT=0 to run a bound at ALL RS.
inline double g_tt2_rs_threshold = 0.6;   // RCPSP_TT2_RS_THRESH
inline bool tt2_rs_allows_expensive() {
  return !g_tt2_rsadapt || g_instance_rs < 0.0 || g_instance_rs <= g_tt2_rs_threshold + 1e-9;
}
inline bool g_tt2_singleres         = false; // RCPSP_TT2_SINGLERES=1: single-resource max LB on TT2
inline long g_tt2_singleres_better  = 0;     // times it beat base h (per instance; CSV: tt2SingleResBetter)
inline long g_tt2_singleres_calls   = 0;     // times computed (per instance; CSV: tt2SingleResCalls)
inline long g_tt2_singleres_expand  = 800;   // RCPSP_TT2_SINGLERES_EXPAND: per sub-solve expand cap
inline int  g_tt2_singleres_maxsize = 45;    // RCPSP_TT2_SINGLERES_MAXSIZE: skip if residual larger than this

// ── Inflated-resource warm start (RCPSP_WARMSTART=1), CBS only ───────────────
// ONE-TIME, before the real search: solve the SAME instance with every resource
// capacity inflated by k (RCPSP_WARMSTART_K, ceil). The inflated problem is a
// relaxation — strictly easier — so its optimal schedule is found fast (a hard
// node/wall budget, RCPSP_WARMSTART_BUDGET_S, with graceful fallback). That
// schedule is *infeasible* for the real caps, so it is NOT used as a bound; it
// only supplies a per-activity ordering to bias CBS branch expansion (dive along
// a resource-aware schedule first => a good feasible incumbent early => more UB
// pruning). Pure reorder of GetSuccessors' children => search stays OPTIMAL/SOUND
// regardless of the ordering or direction. Off by default (zero behaviour change).
inline bool  g_use_warmstart      = false; // RCPSP_WARMSTART=1: enable the warm-start branch ordering
inline double g_warmstart_k       = 2.0;   // RCPSP_WARMSTART_K: capacity inflation factor (ceil(k*cap))
inline long long g_warmstart_budget_s = 0; // RCPSP_WARMSTART_BUDGET_S: wall cap for the inflated solve; 0 => use the FULL search timeout (a separate budget from the real search, not stolen from it)
inline double g_warmstart_sec     = 0.0;   // wall time spent on the inflated (sub-problem) solve this instance — recorded in the CSV (warmstartSec)
inline int   g_warmstart_dir      = 0;     // RCPSP_WARMSTART_DIR: 0/1 flips which end of the relaxed order is explored first (pure tie-break; pick empirically)
inline bool  g_warmstart_reorder  = false; // RCPSP_WARMSTART_REORDER=1: ALSO reorder CBS children by the relaxed schedule. OFF by default — the LB-floor (below) is the real lever; the reorder was net-negative (RS=0.4 worse, others neutral) since UB already seeds the incumbent.
inline double g_warmstart_rs      = 0.0;   // RCPSP_WARMSTART_RS: if >0, inflate each resource to this target Resource Strength (instance-adaptive) INSTEAD of the flat k multiplier; capped to never drop below the real capacity
inline bool  g_warmstart_ok       = false; // set true iff the inflated solve produced a usable schedule this instance
inline short g_warmstart_infl_mk  = -1;    // makespan of the inflated relaxation this instance (-1 if warm start didn't engage) — diagnostic for judging k / RS quality
inline std::vector<short> g_warmstart_start; // per-activity (0-based) start time from the inflated schedule; the branch-ordering key. Empty/!ok => no reorder.

// ── Effective-heuristic NAMES per RS regime (for self-documenting CSVs) ───────
// With RS-adaptive gating the heuristic differs by RS: the gated resource bounds
// (Θ/min-cut/single-res on CBS; Θ/single-res on TT2) apply only at low RS. These
// build a readable NAME of what runs in each regime, written to the CSV columns
// heuristicLowRS / heuristicHighRS. No commas in names (CSV-safe). When RSADAPT is
// off, low and high names are identical (the gated bounds run everywhere). Declared
// here (after all flags) so every g_* symbol referenced is already visible.
inline std::string cbs_heuristic_name(bool includeGated) {
  std::string s = "HCBS";                       // base cardinal-conflict heuristic
  if (g_use_warmstart) s += "+WS";              // warmstart LB floor (never RS-gated)
  if (includeGated) {
    if (g_cbs_theta)     s += "+Theta";
    if (g_cbs_mincut)    s += "+MinCut";
    if (g_cbs_singleres) s += "+SingleRes";
    if (g_cbs_subset)    s += "+Subset";
  }
  return s;
}
inline std::string tt2_heuristic_name(bool includeGated) {
  std::string s = "max(CP/RC)";                 // TT2 base bound
  if (includeGated) {
    if (g_tt2_theta)     s += "+Theta";
    if (g_tt2_singleres) s += "+SingleRes";
  }
  return s;
}

// ── Set-together MDA delay (RCPSP_SETDELAY=1), MDA path only ─────────────────
// When delaying a minimal alternative D, instead of the textbook minimal delay
// (all of D to the earliest completion of the non-delayed conflict members S\D),
// advance the WHOLE set D as a unit through the S\D completion times and stop at
// the first where demand(D) + demand(S\D still running) <= capacity — i.e. the
// earliest time D actually fits alongside the fixed non-delayed members. Checks D
// as a unit against ONLY S\D (unlike use_non_minimal_delay, which advances each
// member independently and drags in unrelated activities). If D can never co-fit
// (demand(D) > capacity), falls back to the standard minimal delay so re-branching
// still resolves the intra-D conflict. SOUNDNESS UNPROVEN — validate via nmd_test /
// j30 correctness before trusting. Off by default (zero behaviour change).
inline bool  g_use_setdelay       = false; // RCPSP_SETDELAY=1: set-together MDA delay

// ── Order-swap dominance MEASUREMENT (RCPSP_ORDERSWAP=1) ─────────────────────
// Hartmann 1998 Bounding Rule 7 (order-swap / order-monotonous schedules): a
// back-to-back frozen pair i>j (f_i==s_j) with no precedence between them and a
// shared resource is order-swap reducible -> the canonical (lower-index-first)
// order dominates. HYPOTHESIS: DR5's cutset key (scheduled-set + unscheduled
// starts, ignoring frozen internal order) already prunes these. So we first only
// COUNT children that survive DR5 yet are order-swap candidates — the NET-NEW
// prunes rule 7 would add. If ~0, order-swap is subsumed; no prune worth adding.
inline bool  g_orderswap      = false;  // RCPSP_ORDERSWAP=1: measure order-swap candidates (no pruning yet)
inline long  g_orderswap_cand = 0;      // children kept by DR5 that ARE order-swap reducible (per instance)

// ── Non-minimal delay env toggle (RCPSP_NMD=1) ───────────────────────────────
// Exposes setting.use_non_minimal_delay to the normal benchmark/sweep runner (it
// was previously only reachable via the dedicated `nmd_test` mode). Improvements
// on top of the non-minimal delay (e.g. RCPSP_MDA_RECURSE) require this on.
inline bool  g_use_nmd            = false; // RCPSP_NMD=1: use the non-minimal delay in sweep/single runs

// ── Improvement 2 instrumentation (RCPSP_MDA_RECURSE=1) ──────────────────────
// The spec asks to measure how often an internally-conflicted MDA actually fires
// and how deep the recursive split goes, to judge the branching-factor cost.
// Reset per instance in solveRCPSP_CBS_impl.
inline long  g_imp2_fires     = 0;  // # times a chosen MDA was itself internally infeasible and got split
inline int   g_imp2_max_depth = 0;  // deepest internal-split recursion level reached this instance

// ── Non-minimal delay overshoot trace (diagnostic) ───────────────────────────
// When compute_nonminimal_delay pushes a member past one or more complement
// finish-times it did NOT fit at, it skips the intermediate resolution node that
// minimal delay would create — and with it the sibling branch "delay that
// complement member instead of this one". Count those skips: a positive count on
// an instance that returns a SUBOPTIMAL makespan is direct evidence of the lost
// branch. Reset per instance in solveRCPSP_CBS_impl.
inline long  g_nmd_overshoot_events = 0; // # non-minimal pushes that skipped >=1 complement finish
inline long  g_nmd_lost_siblings    = 0; // Σ complement finish-times skipped (lost "delay complement" siblings)

// ── Optimal-schedule reachability tracer (RCPSP_STAR="s1,s2,...") ─────────────
// A known-optimal schedule S* (per-activity 0-indexed starts). A CBS node N can
// still reach S* only if N.start_times[i] <= S*[i] for all i (delays only push
// forward). We flag the FIRST node that is consistent with S* but whose children
// ALL overshoot it — that is exactly the conflict where the optimum is lost.
inline std::vector<short> g_star;            // S* starts; empty => tracer off
inline int  g_star_hits = 0;                 // culprit nodes reported (capped)

#endif // GLOBALS_H