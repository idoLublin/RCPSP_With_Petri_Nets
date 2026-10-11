#ifndef GLOBALS_H
#define GLOBALS_H

#include <vector>
#include <array>
#include <string>
#include <cstdint>
#include <limits>
#include <unordered_set>
#include "petriclasses.h"

using namespace P_RCPSP;
thread_local short LB=0;   // per thread: TemplateAStar raises it on every expansion (HDA* workers each run one)
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
inline thread_local long  g_ub_pruned  = 0;

// ── Per-rule prune counters (debug) ──────────────────────────────
// Reset per instance in solveRCPSP_CBS. UB/dominance already counted elsewhere
// (g_ub_pruned; the dominance table's own `pruned`); leftshift had none.
inline long  g_leftshift_pruned = 0;   // children dropped by left_shift_prunable()

// ── Experiment flags (each off by default; env-driven) ───────────
inline bool  g_use_leftshift = false;  // RCPSP_LEFTSHIFT=1: drop left-shiftable children
// RCPSP_CBS_TB: plateau tie-break for the CBS open list, applied only between nodes with equal f AND equal g.
//   0 = off (f, then larger g; the old conflict_number key is always 0 under MDA branching, so it is inert)
//   1 = then larger "feasible-until" time: the node's earliest conflict time t_first once it has been
//       evaluated (conflict-free => +inf), and before that the value inherited from its parent, which is a
//       sound lower bound (no descendant can conflict before the parent's t_first)
//   2 = then deeper (more resolutions applied)
//   3 = feasible-until, then deeper
//   4 = deeper, then least total delay (sum of start times)     5 = mode 3, then least total delay
//   6 = deeper, then most total delay                            7 = mode 3, then most total delay
// Any order among equal-f nodes keeps A* optimal, so every mode is sound. It only decides WHICH f = C*
// node is expanded first - the whole game on instances whose lower bound already equals the optimum.
inline int   g_cbs_tb        = 0;
// RCPSP_CBS_TBSPEC=<keys>: an explicit order among equal-f nodes, replacing the g / RCPSP_CBS_TB chain above.
// One letter per key, most significant first; UPPER case = larger first, lower case = smaller first:
//   G makespan          D depth (resolutions)      T feasible-until (earliest conflict time)
//   S total delay (sum of start times)             C conflict events       O total overload over those events
//   N creation order (N = newest first: depth-first / LIFO among ties, n = oldest first)
//   of the BRANCHING conflict (the parent's values until the node is evaluated):
//   X excess demand         K activities in it         M minimal delay alternatives (= children)
// "GTD" is RCPSP_CBS_TB=3. Any order among equal-f nodes keeps A* optimal, so every spec is sound.
inline std::string g_cbs_tb_spec;
inline unsigned long g_cbs_tb_seq = 0;      // per-instance creation counter behind the N key
// RCPSP_CBS_RESTART="<sec>/<TBSPEC>/<T>,<sec>/<TBSPEC>/<T>,...": restart sequence for the lazy CBS search.
// Stage i runs a complete A* from the root with plateau order TBSPEC and hybrid threshold T for <sec> seconds;
// if it does not finish, the search state is thrown away and the next stage starts. The LAST stage's <sec> is
// ignored: it gets whatever is left of the per-instance budget. Every stage is a full, admissible A*, so a
// path returned by any stage is optimal, and an abandoned stage still leaves a valid lower bound (LB keeps
// the maximum). Motivation: on the f = C* plateau a dive either succeeds within seconds or not at all, and
// different orders succeed on different instances. Empty = off.
struct CbsRestartStage { long sec; std::string spec; float T; bool mirror = false; };
inline std::string g_cbs_restart;                       // the raw spec, as given
inline std::vector<CbsRestartStage> g_cbs_restart_stages;
inline int g_cbs_restart_stage = 0;                     // per instance: the stage in which the search ended (1-based)
// RCPSP_CBS_RESTART_KEEPLB=1: a restart keeps the lower bound the abandoned stages proved. An interrupted A* has
// expanded every node with f < L (L = the last f it expanded), so C* >= L. The next stage floors the f of every
// node at L (h := max(h, L - makespan), the same slot as RCPSP_CBS_PATHMAX's parent floor). L never exceeds C*,
// so the floored f is still admissible and the stage is still an optimal A*. It does not re-prove the deadlines
// below L, and when L = C* the stage starts directly among the f = C* nodes instead of climbing to them again.
inline bool g_cbs_restart_keeplb = false;
inline int  g_cbs_lb_floor = 0;                         // per instance: the floor in force (absolute makespan; 0 = none)
// RCPSP_CBS_LEARN: conflict selection learns from the search - the analogue of a CP solver's conflict-driven
// branching. The cardinal score of a conflict counts the MDAs whose delay raises the MAKESPAN, which the latest
// starts tell us before branching. A "free" MDA (makespan unchanged) can still raise f through h_CBS or the
// deadline propagation, which is known only once the child has been evaluated. For each activity a we count the
// children created by a free delay of a (g_lw_tries) and those whose f ended above the parent's (g_lw_raise).
// The selection score becomes (cardinal MDAs + sum over the free MDAs of the learned rate) / MDAs, where the rate
// of an MDA is the largest raise / (tries + RCPSP_CBS_LEARN_PRIOR) among its activities; it goes through the same
// threshold rule. Only WHICH conflict is branched on changes (any conflict gives a complete child set); h_CBS
// still uses the static score. With no observations the score is the static one, so the search is unchanged.
//   1 = the counters are per instance and SURVIVE restarts: a later stage rebuilds the top of the tree with what
//       the earlier stages observed        2 = the counters are cleared at every restart (control for 1)
inline int  g_cbs_learn = 0;
inline int  g_cbs_learn_prior = 8;
inline std::array<unsigned, 128> g_lw_tries{}, g_lw_raise{};
inline long g_cbs_learn_tries = 0, g_cbs_learn_raises = 0;   // per instance: children credited / of those, f rose
// RCPSP_CBS_FREEFIX=1 (default 0): besides the MDA branching, for every MDA child c (a brother) one extra child c+
// = c with as few conflicts as possible WITHOUT a higher f than c's: conflicts are resolved one at a time in time
// order, each by the MDA that leaves the fewest overloaded resource-time units among those keeping makespan <= F_c
// and (RCPSP_CBS_FREEFIX_PROP=1, with RCPSP_CBS_TT) not refuted by the CP contradiction test of h_CBS-RCPSP at
// D = F_c, where F_c = max(g(c), f(node)) <= f(c) (pathmax); conflicts with no such MDA are skipped. Built with the ordinary MDA constructor, so it is an ordinary CBS node (CV(child) is a subset of CV(node),
// Lemma 3.1 holds); the complete MDA branching is still generated, so coverage (Lemma 4.1) and optimality are
// unchanged (RCPSP.h, cbs_free_resolve). =2: the same, plus a diagnostic that evaluates the extra child at once and
// counts how often its f stayed at or below its brother's.
// RCPSP_CBS_FREEFIX_MAX (default 64): at most this many resolutions per extra child.
// RCPSP_CBS_FREEFIX_TRIES (default 8): candidate MDAs evaluated per conflict.
inline int  g_cbs_freefix = 0;
inline int  g_cbs_freefix_max = 64;
inline int  g_cbs_freefix_prop = 1;
inline int  g_cbs_freefix_tries = 8;
inline long g_cbs_freefix_kids = 0, g_cbs_freefix_resolved = 0;
inline long g_ffd_n = 0, g_ffd_same = 0, g_ffd_goal = 0, g_ffd_up = 0, g_ffd_upsum = 0;   // RCPSP_CBS_FREEFIX=2 diagnostic: extra child f vs parent f   // per instance: extra children emitted / resolutions in them
inline long g_cbs_learn_pickdiff = 0;                        // per instance: nodes where the learned score changed the pick
// RCPSP_CBS_FASTFIRST=1|2|3: with a hybrid threshold above 1 no conflict can be picked by its score, so the
// branching conflict is always the EARLIEST one, and most of the full scan is work nobody uses. In every level the
// pick and its children are IDENTICAL to the full scan's, and nothing happens while the threshold is <= 1 (e.g. in
// the prioritised stages of a restart).
//   1  the MDA set is built for the earliest conflict only; the other conflicts are just counted (keys C / O of
//      the tie-break). h_CBS comes from the picked conflict alone (the full scan takes the maximum over all fully
//      cardinal conflicts) - smaller or equal, still admissible.
//   2  + no h_CBS at all (h = 0 from the scan; f then comes from pathmax and the deadline propagation), which also
//      drops the latest-start pass and the MDA costs. Conflicts are still counted.
//   3  + no counting: one light sweep that stops at the first conflict (compute_first_conflict). Keys C and O stay
//      0, so pair it with keys the light sweep gives for free: T, D, N, X, K, M. RCPSP_CBS_MDAREF is inactive here.
inline int  g_cbs_fastfirst = 0;
inline long g_cbs_fastfirst_skipped = 0;                     // per instance: conflicts counted but not examined
// ── Bell & Park (1990) as published: what the plain baseline (cfg 1) lacked or did differently ──────────────────
// RCPSP_BAP_MRVS=1: branch on a MINIMAL resource violating set ("only minimal resource violating sets, which have
// no proper resource violating subsets, are considered in sequencing decisions", p. 67). The plain baseline
// branches on every activity in progress at RVST on the violated resource. The paper does not say WHICH minimal
// set; we take the one with the fewest activities: largest demands first until the capacity is exceeded (any proper
// subset of it holds at most its k-1 largest demands, which fit, so it is minimal). One child per member b: b starts
// at the earliest finish among the other members - the paper's weak form. Complete: the members overlap at RVST and
// only move later, so in any feasible schedule two of them are disjoint (intervals on a line with no common point).
inline bool g_bap_mrvs = false;
// RCPSP_CBS_MIRROR=1: solve the MIRRORED project - every precedence reversed, activities renumbered so that ids
// stay topological. A schedule read backwards from its makespan is a schedule of the mirrored project and vice
// versa, so both have the SAME optimum. The search is a different one: it meets the conflicts from the other end
// of the project, and its bound, dominance and branching differ. Inside RCPSP_CBS_RESTART a stage written
// "<sec>/<TBSPEC>/<T>/M" runs on the mirror (relative to this flag). A lower bound proven in one orientation is
// valid in the other (same optimum), so RCPSP_CBS_RESTART_KEEPLB carries across; RCPSP_CBS_LEARN counters are
// re-indexed with the activities.
inline bool g_cbs_mirror = false;
inline int  g_cbs_mirror_now = 0;       // per instance: orientation of the project currently loaded (1 = mirrored)
// RCPSP_CBS_DELAY (default 0): how a child REMEMBERS the delay that resolved a conflict. Experimental, not in the paper.
//   0        the delay is a number (start >= earliest finish of the other conflict members, taken once from the
//            parent's schedule). Nothing is remembered: a node is just its schedule.
//   1 "prec" minimal delaying modes (De Reyck & Herroelen 1998): one child per (MDA M, activity k of the rest);
//            every activity of M starts after k finishes - a precedence relation kept for the whole subtree.
//   2 "pair" precedence posting on a minimal conflict set (Laborie 2005; Bell & Park's first variant): the smallest
//            violating subset of the conflict, one child per ordered pair (u, v): v starts after u finishes.
//   3 "wait" waiting conditions (Igelmund & Radermacher 1983; AND/OR precedence, Moehring, Skutella & Stork 2004):
//            one child per MDA M as today, but the delay is kept as a condition - every activity of M starts no
//            earlier than the earliest finish among the rest - and is re-evaluated whenever start times move.
// In 1..3 a node is its schedule PLUS its remembered conditions: both are part of the node's identity, the low
// level computes the least schedule satisfying all of them, and dominance is restricted to nodes whose conditions
// over unscheduled activities coincide (Bell & Park's condition (2)). The DR5S table and the reverse
// (bidirectional) direction compare schedules only, so the driver switches both off.
// Coverage: for a feasible schedule p let F be the earliest finish in p among the conflict set and b0 an activity
// finishing at F. The activities starting at F or later contain an MDA M, and in its own start times p satisfies
// "M after b0" (modes) and "M after the earliest finish of the rest" (waiting). For pairs: the members of a
// violating set cannot all overlap in p, so two of them are disjoint (intervals on a line).
inline int  g_cbs_delay = 0;
inline int  g_cbs_delay_maxkids = 0;    // RCPSP_CBS_DELAY_MAXKIDS, mode prec: an MDA whose rest has more activities than this gets the plain delay child instead (sound: it covers all its modes); 0 = no limit
inline long g_rel_infeasible = 0;       // per instance: children dropped because their conditions admit no schedule within the horizon
inline long g_rel_fallback   = 0;       // per instance: MDAs that fell back to the plain delay child (MAXKIDS)
inline long g_rel_maxlen     = 0;       // per instance: most conditions held by one node
inline int  g_rel_horizon    = 32000;   // per instance: sum of all durations - an optimal schedule never needs more
//   4 "far"  delay further where that is safe. One child per MDA as in 0, and a node is still just its schedule:
//            nothing is remembered and every dominance rule stays as it is. e1 = earliest finish of the rest (the
//            plain delay). far(x) = the first finish of the rest at which x fits, on the conflict resource, beside
//            the rest activities still running. A feasible schedule p that the plain child of the MDA M covers has
//            p(x) >= far(x) for every x in M unless some rest activity q, still running after e1 in this node, is
//            started in p AFTER x ("q postponed"): if every rest activity in x's way starts no later than x, all of
//            them run when x starts, so x must fit beside them. In the other case p >= (M at e1, q at e1 + lag),
//            lag = duration of x if x and q can never overlap on this resource, else 1. M is given the further delay
//            only if, for every such q, another MDA M' inside M + {q} has a child that is no later than that vector
//            in its own delays (checked to a fixpoint, because M' may itself be delayed further); p is then covered
//            by the child of M'. Otherwise M keeps the plain delay. So every feasible schedule >= the node's
//            schedule is >= some child: the property the search and the dominance rules rely on. RCPSP_SETDELAY and
//            RCPSP_NMD apply the further delay without this test, which is where they lose optima.
inline long g_far_blocked    = 0;       // per instance: MDA children for which a further delay exists
inline long g_far_applied    = 0;       // per instance: MDA children that were given it (the test passed)
// RCPSP_UB_SEED=0: with RCPSP_UB=1 the upper bound starts at infinity and is set only by feasible schedules the
// search itself generates (Descendants lines 4-6); a generated state with makespan >= the bound is discarded.
// Default 1 = the old behaviour, a seed read from the datasheet, which is NOT valid for reported runs.
inline bool g_ub_seed = true;
// RCPSP_LEFTSHIFT=2: the left-shift rule with ANY earlier start ("can be started earlier without violating
// precedence or resource constraints and without adjusting the scheduled start times of other activities", p. 64),
// where RCPSP_LEFTSHIFT=1 tests a one-unit shift only. Scheduled-set activities only, as in the paper; the window
// lies before RVST, where usage can only fall in descendants, so the shift stays possible in all of them.
inline bool g_leftshift_global = false;
// RCPSP_DOM_BP_SETKEY=1: Bell & Park compare states with the same SCHEDULED SET - their CLOSED table is keyed by the
// scheduled-set bit vector (p. 78). The table here also required the same RVST, which is stricter than the paper.
// Sound without it: take a feasible schedule F' below the dominated state and keep the dominating state's start
// times on the scheduled set. Before the dominating state's RVST every unscheduled activity running in F' is also
// running in the dominating state (it starts no later there and ends after RVST), so the result is feasible, lies
// above the dominating state and has F''s makespan. Scheduled sets grow strictly along a path of stored states,
// so a state is never compared with its own ancestor.
inline bool g_dom_bp_setkey = false;
// Diagnostic (always on, CSV lbRaiseExp): the expansion count at which the proven lower bound last rose.
// expandNumber - lbRaiseExp = expansions spent on the FINAL f-layer (the f = C* plateau when solved).
inline long  g_lb_raise_exp  = 0;
// RCPSP_CBS_PATHMAX=1: a child starts from its parent's final f. Branching only ADDS delays, so every schedule
// reachable below a child is reachable below its parent (CV(child) is a subset of CV(parent)); the parent's
// f - HCBS plus everything RCPSP_CBS_TT refuted - is therefore a lower bound for the child too. Without it a
// child re-refutes, one full propagation per deadline, what its parent already proved.
inline bool  g_cbs_pathmax   = false;
// RCPSP_CBS_PATHMAX_INSERT=1: also QUEUE a child at its parent's f (lazy insertion), not only floor it when it is
// evaluated. Implies the evaluation-time floor.
inline bool  g_cbs_pathmax_insert = false;
// RCPSP_CBS_GATE_DEPTH=1: the expensive-bound gates (subset / min-cut / single-res / hierRS) measure depth as
// the node's CT depth instead of added_precedences.size(), which is ALWAYS 0 under MDA branching (a resolution is
// a start time, not an arc) - so without this those bounds run at every node regardless of *_MAXDEPTH.
inline bool  g_cbs_gate_depth = false;
// RCPSP_CBS_SEL: ordering among STRONG conflicts under the hybrid rule (see RCPSPState.cpp compute_h_and_RVS).
//   0 = highest cardinal score, then earliest   1 = fewest free MDAs, then fewest MDAs, then earliest
//   2 = highest score, then fewest MDAs, then earliest
inline int   g_cbs_sel = 0;
// RCPSP_CBS_ADAPT_BRANCH=k: singular branching (one minimal-delay child per conflict member) whenever the branching
// conflict has more than k*|G| MDAs; 0 = always MDA branching. Caps the branching factor on dense J90 conflicts.
inline int   g_cbs_adapt_branch = 0;
inline long  g_cbs_adapt_singular = 0;   // nodes branched singularly (per instance; CSV adaptSingular)
inline long  g_cbs_pathmax_raised = 0;   // nodes whose h the parent floor raised (per instance; CSV pathmaxRaised)
inline bool  g_use_dr4       = false;  // RCPSP_DR4=1: DR4 (Liu non-delaying rule) reduced to the CBS partial schedule == the active-schedule / left-shift dominance (drives the same verified-sound left_shift_prunable() prune). Recorded as useDR4; prunes counted in leftshiftPruned.
inline bool  g_use_bidir     = false;  // RCPSP_BIDIR=1: new states also kill dominated stored states
inline bool  g_use_hybrid    = false;  // RCPSP_HYBRID=1: prio falls back to earliest conflict when best score < 1
inline float g_hybrid_threshold = 0.5f; // RCPSP_HYBRID_T: cardinality score below which we prefer first-conflict
inline bool  g_use_lean      = false;  // RCPSP_LEAN=1: strip conflict vectors from OPEN/CLOSED copies (memory for time)
inline bool  g_use_inline    = false;  // RCPSP_INLINE=1: expand intermediate children in place (B&P Descendants recursion)
inline bool  g_dom_skyline   = false;  // RCPSP_SKYLINE=1: keep dominance buckets as a Pareto frontier (retire stored entries dominated by a newer one). Search-IDENTICAL for pruning (transitivity), only shrinks buckets => faster checks + less memory. Unlike BIDIR it does NOT feed the kill-set, so it never changes which nodes are expanded.
inline bool  g_use_lazy      = false;  // RCPSP_LAZY=1: use LazyAStarCBS (deferred-heuristic A*) instead of TemplateAStar — compute the expensive compute_h_and_RVS only when a node is POPPED, not at insertion, so never-expanded OPEN nodes skip it. Optimal (h=0 is an admissible lower bound + lazy re-insertion).
inline bool  g_tt2_dr5       = false;  // RCPSP_TT2_DR5=1: cutset (DR5) dominance for the TT2/TTPNR search (DominanceTT2.h), checked+inserted in RCPSP_TT2::GetSuccessors. Includes always-on skyline thinning.
inline int   g_tt2_dom_side  = 0;      // bidirectional DR table selector (0=forward, 1=backward); DominanceTT2.h get_tt2_dominance_table() picks the side's table.
inline bool  g_tt2_dr4       = false;  // RCPSP_TT2_DR4=1: DR4 delayed-start dominance in TT2 successor generation (Liu et al. rule 4, ported from new_herustic). Prune firing l at delta d_l if some other available transition i has d_i < d_l and d_i + p_i <= d_l (sound left-shift; min-delta child never pruned).
inline thread_local long  g_tt2_dr4_pruned = 0;     // successors skipped by DR4 (per instance; CSV: dr4Pruned)
inline int   g_tt2_kill      = 0;      // RCPSP_TT2_KILL (needs RCPSP_TT2_DR5): DR5 entries retired by a later dominating state are recorded as DEAD (DominanceTT2.h). 1 = count expansions of dead nodes only (search unchanged), 2 = also skip them (no children). Forward TT2 A* (solveRCPSP_TT2) only.
inline bool  g_tt2_hdz_fix   = false;  // RCPSP_TT2_HDZFIX=1 (forced on in tt2hda): the zero-delay h shortcut in RCPSP_TT2::HCost recomputes when the inherited parent h is 0. TemplateAStar's decrease-key/reopen replaces a node's stored state with the new copy, whose h member was never computed (0), so that node's zero-delay children would inherit h=0.
inline thread_local bool g_tt2_hda_worker = false;   // set inside HDA* worker threads (HDAStarTT2.h): RCPSP_TT2::GetSuccessors then leaves DR5 to the owning worker
inline int   g_tt2_kill_armed = 0;     // g_tt2_kill while solveRCPSP_TT2's A* runs, 0 otherwise (keeps the other TT2 drivers unaffected)
inline thread_local long  g_tt2_dead_expanded = 0;  // RCPSP_TT2_KILL: expansions of dead nodes (per instance; with =2 these emitted no children)
inline bool  g_tt2_dupprobe  = false;  // RCPSP_TT2_DUPPROBE=1: classify every child that reaches A* against its open/closed list (measurement only; one extra lookup per child)
inline thread_local long  g_tt2_dup[5]    = {0, 0, 0, 0, 0};   // per instance: new / on OPEN with lower g / on OPEN, not lower / on CLOSED with lower g / on CLOSED, not lower
inline int   g_tt2_precheck  = 0;      // RCPSP_TT2_PRECHECK (needs RCPSP_TT2_DR5): RCPSP_TT2::GetSuccessors tests a firing child against DR5 on its DR5 view (scheduled set, g, active releases) BEFORE building it, so the children DR5 prunes are never constructed. Same tests in the same order, so the search is identical. 2 = also build every such child and compare the view with it (exits on a mismatch).
inline thread_local long  g_tt2_precheck_cut = 0;  // RCPSP_TT2_PRECHECK: children DR5 pruned before construction (per instance)
inline bool  g_tt2_wait      = false;  // RCPSP_TT2_WAIT=1: explicit-wait model. A transition fires only when all its input tokens are ready NOW, and a separate wait edge advances the clock one unit (RCPSP_TT2::GetSuccessors). Every integer schedule is then a path on the forward and the reversed net, so bidirectional frontiers always share cuts. DR4 becomes a horizon on the wait chain; DR5 exempts a state's own wait ancestors (DominanceTT2.h).
inline thread_local long  g_tt2_wait_edges = 0;     // explicit-wait model: wait children generated (per instance; CSV: waitEdges)
inline thread_local long  g_tt2_wait_cut   = 0;     // explicit-wait model: wait edges cut by the DR4 horizon (per instance; CSV: waitCut)
inline bool  g_tt2_waitjump   = true;  // RCPSP_TT2_WAITJUMP (default 1, wait model only): a wait chain where nothing can start jumps in one edge to the first time something becomes ready; HBAE re-creates the skipped nodes as closed meet-only nodes
inline thread_local long  g_tt2_wait_jumped = 0;    // wait-model units skipped by those jumps (per instance; CSV: waitJumped)
inline long  g_tt2_wait_meetonly = 0;  // HBAE: skipped wait nodes re-created for the meet (per instance; CSV: waitMeetOnly)
inline thread_local long  g_tt2_wait_pass  = 0;     // explicit-wait model: wait edges from a node where nothing could start (its only successor; CSV: waitPass)
inline bool  g_tt2_immsel    = false;  // RCPSP_TT2_IMMSEL=1: immediate selection (D&H / Hartmann Remark 1) — when every available transition is fireable NOW and one of them cannot be co-processed with any not-yet-started activity, it is forced: emit only that single firing (a left-shift; no optimum lost). Branching reduction, not a state prune.
inline thread_local long  g_tt2_immsel_fired = 0;   // nodes where immediate selection collapsed the branching (per instance)
inline bool  g_tt2_freefire  = false;  // RCPSP_TT2_FREEFIRE=1 (SUB-SOLVE ONLY): force-fire an available activity with 0 demand on the constrained resource k that is ready NOW (delta==0). A left-shift of "free" activities (they never conflict), pure branching reduction. Gated to g_in_subsolve; the single-res relaxation zeroes 3/4 of demands so most activities become free.
inline thread_local long  g_tt2_freefire_fired = 0; // nodes where a free activity was force-fired (per instance)
inline bool  g_tt2_batch     = false;  // RCPSP_TT2_BATCH=1: batch/macro expansion — each successor starts a resource-feasible SUBSET of the now-available activities in one edge (plus the time-advancing single firings), trading depth for branching. Superset of single-firing (singletons included) => still optimal.
inline bool  g_tt2_hfresh    = false;  // always compute h fresh: RCPSP_TT2::HCost skips the zero-delay shortcut (h = predessesor_h). HBAE sets it per instance (RCPSP_BI_HFRESH, default 1) because its stored states keep h = 0, so the shortcut scored zero-delay children h = 0 (f = g): admissible but inconsistent. RCPSP_TT2_HFRESH=1 sets it for any mode (e.g. tt2consist).
inline int   g_tt2_compact   = 0;      // RCPSP_TT2_COMPACT (plain TT2 search only: tt2_<size>, tt2rev_<size>, tt2one, tt2rev). 1 = stored search nodes do not keep the token lists (resource_nodes, activity_nodes): the lists follow from the finished set and the active list (tt2CompactRebuild, RCPSPState.cpp), so RCPSP_TT2::GetSuccessors rebuilds them for the node it expands and releases them from the children. Same search, less memory per stored node. 2 = check: nothing is released, the lists are rebuilt as well and compared with the kept ones, exit(7) on a difference.
inline int   g_tt2_compact_on = 0;     // the RCPSP_TT2_COMPACT value in force for the instance being solved: 0 if the net fails the structural check of tt2CompactPrepare (CSV: tt2Compact)
inline thread_local long  g_tt2_compact_rebuilds = 0;   // expanded nodes whose token lists were rebuilt (per instance; CSV: compactRebuilds)
inline thread_local long  g_tt2_compact_checked  = 0;   // check mode: states whose rebuilt lists were compared with the kept ones (per instance; CSV: compactChecked)
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
inline thread_local long  g_tt2_sym_pruned = 0;     // successors skipped by the canonical-order rule
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
inline thread_local long  g_tt2_theta_better = 0;     // times the Θ-tree bound strictly beat the existing bound at a state (per instance; CSV: thetaBoundBetter)
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
inline long g_cbs_singleres_expand   = 0;     // RCPSP_CBS_SINGLERES_EXPAND: per single-resource sub-solve expand cap (0 = UNCAPPED, solve to optimum)
inline int  g_cbs_singleres_maxsize  = 45;    // RCPSP_CBS_SINGLERES_MAXSIZE: skip if residual larger than this (cost guard)
// Single interchangeable sub-solver selector (RCPSP_SUBSOLVER = ext|cbs|tt2, or 0|1|2),
// applies to whichever main solver is running. The nested engine uses only its base
// heuristic (max(CP,RC)/HCBS) — the expensive bounds are recursion-guarded off inside.
//   0 = extension mini-solver (subsetRcpspLB)   1 = nested CBS   2 = nested TT2
inline int  g_subsolver              = 0;
inline bool g_subsolve_hcbs_off      = false; // RCPSP_SUBSOLVE_HCBS_OFF=1: nested CBS sub-solve uses a CONSISTENT h (0) instead of the inconsistent HCBS, so the future exact-distance cache stays sound
inline bool g_hcbs_off_now           = false; // runtime: true only while inside a nested CBS sub-solve (set by singleResViaCBS_CBS)
inline bool g_singleres_via_cbs      = false; // (legacy alias; kept so old scripts still map to g_subsolver=1)
inline long g_singleres_viacbs_capped= 0;     // times a nested CBS sub-solve returned no goal (timed out / no schedule) — the "unsolvable sub-problem" signal

// Cutset/skyline dominance inside the subset mini-solver (subsetRcpspLB). Default
// ON — it's the workhorse RCPSP pruning and the sub-search was missing it (only had
// exact-state dedup). Sound: same scheduled set + pointwise-<= starts => dominates.
// Toggle for A/B soundness tests (RCPSP_SUBSET_DOM=0).
inline bool g_subset_dom = true;

// ── Hierarchical RS-relaxation LB (RCPSP_CBS_HIERRS=1), CBS ───────────────────
// For a LOW-RS (tight) instance, bound the remaining makespan by solving the SAME
// residual subproblem at the NEXT-HIGHER RS level — every resource capacity inflated
// to the value giving that RS (Kmin + RS*(Kmax-Kmin), clamped >= real cap; same
// formula as the warm-start block). More capacity => the relaxed optimum <= the real
// residual optimum, so it is an admissible ABSOLUTE makespan LB. Unlike single-res
// (keep ONE resource exact, others infinite) this keeps ALL resources but LOOSER —
// a different, complementary relaxation. Solved once per node with the subset mini-
// solver (reusing subsetRcpspLB) and memoized by (residual set + releases), since the
// wide tree revisits the same residual heavily. Relaxed schedule is a LOWER bound only
// (never restricts the search) so the optimum is preserved. Recurses conceptually up
// to RS=1 (where hCP is optimal); the flat one-level bound is implemented here.
inline bool g_cbs_hierrs        = false; // RCPSP_CBS_HIERRS=1: enable the hierarchical RS-relaxation LB
inline long g_cbs_hierrs_better = 0;     // times it strictly beat h (per instance; CSV: hierRsBetter)
inline long g_cbs_hierrs_calls  = 0;     // times the (gated) bound was computed (per instance; CSV: hierRsCalls)
inline int  g_cbs_hierrs_maxdepth = 3;   // RCPSP_CBS_HIERRS_MAXDEPTH: run if depth <= this (shallow arm)
inline int  g_cbs_hierrs_gap    = 8;     // RCPSP_CBS_HIERRS_GAP: also run if 0 < UB-(g+h) <= this (near-incumbent arm)
inline long g_cbs_hierrs_expand = 0;     // RCPSP_CBS_HIERRS_EXPAND: per relaxed sub-solve expand cap (0 = UNCAPPED)
inline double g_cbs_hierrs_target = -1.0;// resolved target RS (next level up) for this instance; <0 => bound inert
inline double g_hierrs_time_sec = 0.0;   // time inside the hier-RS bound (CSV: hierRsTimeSec)
inline long g_cbs_hierrs_cache_hits = 0; // memo hits (per instance; CSV: hierRsCacheHits)
inline std::unordered_map<uint64_t,double> g_cbs_hierrs_cache; // memo: (residual set + releases) -> LB
// Per-instance RS-inflation profile (filled by precomputeRSInflation): per resource,
// Kmin = max single-activity demand, Kmax = peak of the CPM earliest-start demand
// profile. Inflated cap for a target RS is Kmin + RS*(Kmax-Kmin), clamped >= real cap.
inline std::vector<int> g_rs_Kmin;       // [R]
inline std::vector<int> g_rs_Kmax;       // [R]

// ── Always-on instrumentation (no flag; reset per instance) ──────────────────
// Recorded in every CSV regardless of feature flags: root heuristic value, total
// time spent computing the heuristic, call count, and per-feature time for the
// expensive bounds so we can see where runtime actually goes.
#include <chrono>
inline thread_local double g_heur_time_sec      = 0.0;  // cumulative wall time inside HCost compute (CSV: heurTimeSec)
inline thread_local long   g_heur_calls         = 0;    // HCost computations, excl. cache hits (CSV: heurCalls)
inline thread_local double g_root_h             = -1.0; // heuristic value at the root/start node (CSV: rootH)
// ── TT2 PROFILING (RCPSP_TT2_PROF=1) ─────────────────────────────────────────
// Per-instance wall time by search phase, reset in solveRCPSP_TT2 and written to the TT2 CSV.
// Timers run only when g_tt2_prof is on (a steady_clock read is ~20-30ns). Counters and the
// f-bound checkpoints are always on: an increment, plus one clock read every 1024 expansions.
#include <cstdio>
inline bool   g_tt2_prof            = false;
inline thread_local double g_prof_succ_sec       = 0.0;  // whole RCPSP_TT2::GetSuccessors
inline thread_local double g_prof_avail_sec      = 0.0;  // unstarted scan + getAvailableTransitionIndices_TT2
inline thread_local double g_prof_gen_sec        = 0.0;  // child construction: firing, DR4, symmetry, batch subsets
inline thread_local double g_prof_dr5_sec        = 0.0;  // DR5 check_and_insert over the children
inline thread_local double g_prof_hash_sec       = 0.0;  // RCPSP_TT2::GetStateHash
inline thread_local double g_prof_hcp_sec        = 0.0;  // heuristic, critical-path part (getForwardHcost)
inline thread_local double g_prof_hres_sec       = 0.0;  // heuristic, resource part (getforwardResource)
inline double g_prof_astar_sec      = 0.0;  // TemplateAStar::GetPath total (set by the driver)
inline thread_local long   g_prof_succ_calls     = 0;    // GetSuccessors calls (= expansions incl. re-expansions)
inline thread_local long   g_prof_hash_calls     = 0;
inline thread_local long   g_prof_children_raw   = 0;    // children after DR4/symmetry, before DR5
inline thread_local long   g_prof_children_kept  = 0;    // children after DR5 (and UB if on)
inline thread_local long   g_prof_h_calls_all    = 0;    // HCost entries, including the two shortcuts below
inline thread_local long   g_prof_h_known        = 0;    // HCost returned a fused/cached h
inline thread_local long   g_prof_h_deltazero    = 0;    // HCost reused the parent h (zero-time firing)
inline thread_local int    g_prof_maxf           = -1;   // max f expanded so far: the proven lower bound (consistent h)
inline thread_local int    g_prof_f_at[5]        = {-1, -1, -1, -1, -1};  // g_prof_maxf at 1 / 10 / 60 / 120 / 240 s
inline std::chrono::steady_clock::time_point g_prof_t0;      // A* start
inline std::FILE* g_prof_trace      = nullptr;  // RCPSP_TT2_PROF_TRACE=<file>: a snapshot every 5 s
inline thread_local double g_prof_next_trace     = 5.0;
inline int    g_prof_group = 0, g_prof_exam = 0;
inline const char* g_prof_size      = "";
inline double g_singleres_time_sec = 0.0;  // time inside the single-resource bound (CSV: singleResTimeSec)
inline double g_mincut_time_sec    = 0.0;  // time inside the min-cut bound (CSV: minCutTimeSec)
inline double secs_since(std::chrono::steady_clock::time_point t0){
  return std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
}

// ── Per-instance heuristic deadline ──────────────────────────────────────────
// The main A* checks its wall clock only BETWEEN expansions; a single HCost that
// runs an uncapped nested sub-solve can overshoot the 300s budget badly. Set this
// ONCE per instance (= start + astar_timeout_seconds) so every sub-solve honors the
// SAME absolute deadline instead of re-arming a fresh 300s each call. Aborting a
// sub-solve early is sound: the returned min-f on OPEN is still <= opt (a valid LB).
inline std::chrono::steady_clock::time_point g_instance_deadline{};
inline bool g_instance_deadline_set = false;
inline long long heur_time_left_s(){
  if(!g_instance_deadline_set) return 1LL<<30;                 // no deadline => unlimited
  auto now = std::chrono::steady_clock::now();
  if(now >= g_instance_deadline) return 0;
  return (long long)std::chrono::duration_cast<std::chrono::seconds>(g_instance_deadline - now).count();
}
inline bool heur_deadline_hit(){
  return g_instance_deadline_set && std::chrono::steady_clock::now() >= g_instance_deadline;
}
// Which sub-solver engine is active (for the self-documenting CSV column).
inline const char* subsolver_name(){
  switch(g_subsolver){ case 1: return "cbs"; case 2: return "tt2"; default: return "ext"; }
}

// ── Shadow cache estimator (RCPSP_SHADOW=1; measure-only, default off) ────────
// Estimates the hit rate the future exact-distance sub-solve cache WOULD get,
// without storing any solutions. g_shadow_set = per-instance union of state hashes
// on PRIOR sub-solves' solution paths (exactly what the real cache would hold). While
// inside a sub-solve (g_in_subsolve), every generated sub-state is probed against it:
// already present => a hit the cache would serve. Cleared per instance. Reported to
// stdout ("SHADOW ..."). Adds one set lookup per sub-state only when g_shadow_cache.
inline bool g_shadow_cache = false;      // RCPSP_SHADOW=1
inline bool g_in_subsolve  = false;      // true only while a nested sub-solve runs
inline int  g_subsolve_k   = 0;          // which resource-k relaxation is being solved (part of the cache key:
                                         // successors + h* depend on k because caps differ per relaxation)

// ── Successor (transposition) cache for the TT2 sub-solve (RCPSP_SUBSOLVE_SUCC_CACHE=1) ──
// If a (marking,k) was expanded in a PRIOR sub-solve this instance, its RAW children
// (post-DR4/symmetry, PRE stored-state dominance) are Markovian on the marking, so we
// splice them in and skip the available-scan + batch enumeration. Release-dependent
// filters (order-swap, DR5) are ALWAYS re-applied fresh => sound. Capped + cleared per
// instance to bound memory (the entry stores the key state + child states).
inline bool g_subsolve_succ_cache = false;   // RCPSP_SUBSOLVE_SUCC_CACHE=1 (PARKED: memory-heavy, net-neg)
inline long g_succ_cache_cap      = 0;       // RCPSP_SUCC_CACHE_CAP: max entries; 0 = UNLIMITED (server has memory).
                                             // Only a speed knob: a very large map costs rehash/lookup/alloc time,
                                             // so cap it only if profiling shows map growth outweighs the hits it buys.
inline thread_local long g_succ_probes = 0;               // expansions probed (per instance)
inline thread_local long g_succ_hits   = 0;               // served from cache (per instance)

// ── h-value cache (RCPSP_SUBSOLVE_H_CACHE=1) — the light lever ────────────────
// Memoize h=max(CP,RC) per (state,k) across sub-solves (deterministic, so sound; 128-bit
// key guards collisions). One double/entry => tens of MB, not GB. TT2 sub-solve is eager,
// so a hit short-circuits HCost; in the CBS LAZY sub-solve a hit sets h immediately + flags
// the node so lazy skips its deferred recompute.
inline bool g_h_cache      = false;          // RCPSP_SUBSOLVE_H_CACHE=1
inline bool g_h_cache_solved = false;        // RCPSP_SUBSOLVE_H_CACHE_SOLVED=1: cache ONLY solved (proven-path) nodes with
                                             // EXACT h*=M-g (back-filled after each sub-solve), not every open node's heuristic h
inline thread_local long g_hcache_probes= 0;              // per instance
inline thread_local long g_hcache_hits  = 0;              // per instance
inline std::unordered_set<uint64_t> g_shadow_set;
inline long g_shadow_probes = 0;
inline long g_shadow_hits   = 0;

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
inline long g_tt2_singleres_expand  = 0;     // RCPSP_TT2_SINGLERES_EXPAND: per sub-solve expand cap (0 = UNCAPPED)
inline bool g_tt2_singleres_rel     = false; // RCPSP_TT2_SINGLERES_REL=1: build the TT2 single-res residual in RELATIVE time from the
                                             // marking (running: dur=remaining,release=0; unstarted: dur=full,release=0) instead of
                                             // from abs_start. Admissible + lets us delete abs_start. Validate makespans unchanged first.
inline bool g_tt2_singleres_via_cbs = false; // RCPSP_TT2_SINGLERES_VIACBS=1: solve TT2's single-resource sub-problem with the nested CBS engine (translate TT2 node -> CBS state)
inline bool g_tt2_singleres_via_tt2 = false; // RCPSP_TT2_SINGLERES_VIATT2=1: solve TT2's single-resource sub-problem with the nested TT2 engine (batch) from the node's own marking
inline int  g_tt2_singleres_maxsize = 45;    // RCPSP_TT2_SINGLERES_MAXSIZE: skip if residual larger than this

// ── TT2 upper-bound (incumbent) pruning (RCPSP_TT2_UB=1) ─────────────────────
// TT2 has no free internal incumbent (an internal marking is a PARTIAL schedule),
// so seed g_incumbent once from a real feasible primal (serialSGS_makespan — NOT the
// datasheet-optima cheat) and drop any child whose f = g + h STRICTLY exceeds it,
// before it enters OPEN. Sound with TT2's CONSISTENT heuristic: on any optimal path
// f <= opt <= incumbent, so strict '>' never removes the optimum. Purpose = shrink
// peak memory (and skip expanding provably-suboptimal nodes). Reuses g_incumbent /
// g_ub_pruned (CBS and TT2 never run in the same process).
inline bool g_tt2_ub = false;                // RCPSP_TT2_UB=1: enable SGS-seeded incumbent pruning on TT2
// RCPSP_TT2_UB_GE=1: prune f >= incumbent instead of f > incumbent. The incumbent is a REAL schedule, so a
// node with f = U can only lead to a solution of length U, which we already hold - dropping it loses nothing.
// What it buys is the STOP: OPEN now empties exactly when no schedule shorter than U exists, which PROVES U
// optimal. With f > U the f = C* plateau still has to be expanded to reach a goal, which is where A* dies on
// instances whose bound already sits at the optimum. Needs RCPSP_TT2_UB=1 and an honest primal (SGS/root seed).
inline bool g_tt2_ub_ge = false;             // RCPSP_TT2_UB_GE=1: >= pruning + "OPEN exhausted => incumbent optimal"
inline bool g_tt2_ub_closed = false;         // set when the search ended by exhausting OPEN under >= pruning

// ── TT2 port of the hierarchical RS-relaxation LB (RCPSP_TT2_HIERRS=1) ────────
// Same bound as the CBS hierRSBound: residual = unfinished activities, releases from
// abs_start/g, every resource cap inflated to the next RS level up, solve once via
// subsetRcpspLB, max into base h. Admissible; reuses g_cbs_hierrs_target / _cache /
// _cache_hits / g_rs_Kmin/Kmax / inflatedCapForRS (instance-scoped; TT2 and CBS never
// run in the same process). On TT2 the payoff is a TIGHTER h => fewer expands to solve.
inline bool g_tt2_hierrs        = false; // RCPSP_TT2_HIERRS=1: enable the hierarchical RS-relaxation LB on TT2
inline long g_tt2_hierrs_better = 0;     // times it beat base h (per instance; CSV: tt2HierRsBetter)
inline long g_tt2_hierrs_calls  = 0;     // times computed (per instance; CSV: tt2HierRsCalls)
inline long g_tt2_hierrs_expand = 0;     // RCPSP_TT2_HIERRS_EXPAND: per relaxed sub-solve expand cap (0 = UNCAPPED)

// RCPSP_HIERRS_TARGET (shared CBS+TT2): if >0, override the target RS for BOTH hier-RS
// bounds. Default (<0) = next ladder step up. Set to the instance's own RS (e.g. 0.2)
// to solve the residual at ~real caps — a MUCH tighter bound that can beat max(CP,RC),
// unlike the loose next-level-up default. Tighter => costlier per solve (cache offsets).
inline double g_hierrs_target_override = -1.0;

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
    if (g_cbs_hierrs)    s += "+HierRS";
    if (g_cbs_subset)    s += "+Subset";
  }
  return s;
}
inline std::string tt2_heuristic_name(bool includeGated) {
  std::string s = "max(CP/RC)";                 // TT2 base bound
  if (includeGated) {
    if (g_tt2_theta)     s += "+Theta";
    if (g_tt2_singleres) s += "+SingleRes";
    if (g_tt2_hierrs)    s += "+HierRS";
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
inline thread_local long  g_orderswap_cand = 0;      // children kept by DR5 that ARE order-swap reducible (per instance)

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