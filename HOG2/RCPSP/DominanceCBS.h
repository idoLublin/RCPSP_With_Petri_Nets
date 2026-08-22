#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// State dominance for the CBS search — Bell & Park (1990), "Solving resource-
// constrained project scheduling problems by A* search", Naval Research
// Logistics 37:61-84, Section 5 (pp. 67-69).
//
// Their search space IS this one: a state is a set of precedence constraints,
// its schedule is the earliest-start schedule under them, and children are made
// by repairing the earliest resource violation. Their RVST (resource violation
// start time) is our t_first; their MRVS is our conflict set.
//
// Given a state's RVST, activities split into (p. 67):
//     scheduled set   A_s = { a : a finishes at or before RVST }
//     unscheduled set A_u = { a : a finishes after RVST }
// NOTE: A_u deliberately CONTAINS the activities running at RVST ("the
// unscheduled set may contain activities which start before RVST"). Actives are
// not excluded and not dropped — they are compared as part of A_u.
//
// S weakly dominates S' (so S' may be pruned) if:
//   (1) S and S' have the same scheduled set A_s (hence the same A_u);
//   (2) the precedence networks of S and S' have the same subsets of arcs
//       connecting pairs of activities in A_u;
//   (3) S's start times for activities in A_u are all <= those of S'.
// Start times of A_s are deliberately ignored — only arcs within A_u can ever
// propagate changes in any descendant.
//
// CONDITION (2) AND WEAK vs STRONG CONSTRAINTS
// Bell & Park: under the weakly constrained approach condition (2) is
// superfluous, because a delay is imposed as a release-time constant (their
// dummy activity z) rather than as a real arc a->b, so no arc ever connects two
// unscheduled activities and (1) implies (2).
//   - MDA branching (RCPSPState_CBS ctor taking mda_activities) is the weak
//     form: it pins start_times[delayed] = new_start and never touches
//     added_precedences, which therefore stays empty. (2) is automatic.
//   - Arc branching (ctor taking from/to) is the STRONG form: it pushes a real
//     (from,to) into added_precedences and re-propagates it, so such an arc can
//     move A_u members in descendants. There (2) genuinely bites — dropping it
//     prunes optima (measured: 11-14 wrong of 40 on j30 cfg1).
// So (2) is checked only when added_precedences is non-empty. In weak/MDA runs
// that test is a single .empty() check and never fires.
// ─────────────────────────────────────────────────────────────────────────────

#include <array>
#include <vector>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <string>
#include <fstream>
#include "Globals.h"   // g_use_bidir (and friends)

// DIAGNOSTIC: when non-empty, every prune appends "rvst;<dominating starts>;<pruned starts>"
// here, so each claimed dominance can be re-checked offline by solving from both
// states exactly (Driver mode "verifydom"). Empty in normal runs.
inline std::string g_dom_dump_path;

// Which dominance rule the table applies (env RCPSP_DOM_RULE=bp|dr5).
//   BP  — Bell & Park (1990): same A_s = {finish <= RVST}, same A_u arcs, A_u starts <=.
//   DR5 — Demeulemeester & Herroelen cutset dominance, in the form specified for
//         this project: cutset = {finished by t*} U {running at t*, NOT a conflict
//         participant}; dominated when the same cutset was stored at t*' <= t* with
//         every running member finishing no later.
// Both share the reduction time t* = t_first and the Descendants Line-8 gate.
//   DR5S — DR5's cutset, plus the comparison DR5 is missing in a CBS context.
//         DR5 comes from a B&B where unscheduled activities are genuinely free, so
//         the cutset summarises the node. Here every activity carries a release-time
//         lower bound in start_times, and DR5 ignores those outside the cutset —
//         which is measurably why plain DR5 loses optima. DR5S additionally requires
//         the stored state to be no-later on every activity outside the cutset.
//   BOTH — Bell & Park AND DR5S run together, against separate tables: a state is
//         pruned if EITHER rule dominates it. They key and compare differently, so
//         neither subsumes the other and the union prunes strictly more. Only the
//         SOUND DR5S half is used — adding pruning rules can only prune more, so
//         unioning plain (unsound) DR5 with B&P would still lose optima.
enum CBSDomRule { DOM_BP = 0, DOM_DR5 = 1, DOM_DR5S = 2, DOM_BOTH = 3 };
inline int g_dom_rule = DOM_BP;

// Store cap per table (env RCPSP_DOM_CAP). Above the cap the table still CHECKS
// against what it holds but stops STORING new entries — strictly weaker pruning,
// never unsound, and it bounds memory on 300s-timeout instances that would
// otherwise accumulate millions of entries.
inline long g_dom_store_cap = 4'000'000;

// Key = condition (1): the scheduled set A_s as a bitmask, PLUS the RVST it was
// taken at. The existing sibling-level Bell & Park pruning in RCPSP.h requires
// a.t == b.t, and comparing two states whose A_s happens to coincide but whose
// RVSTs differ is not the same comparison the rule is stated for.
template<short N>
struct CBSCutsetKey {
    static constexpr int W = (N + 63) / 64;
    std::array<uint64_t, W> bits{};
    short rvst = -1;
    void set(int i) { bits[i >> 6] |= (uint64_t(1) << (i & 63)); }
    bool operator==(const CBSCutsetKey& o) const { return bits == o.bits && rvst == o.rvst; }
};

template<short N>
struct CBSCutsetKeyHash {
    size_t operator()(const CBSCutsetKey<N>& k) const {
        size_t h = 1469598103934665603ULL;              // FNV-1a
        for (uint64_t w : k.bits) { h ^= (size_t)w; h *= 1099511628211ULL; }
        h ^= (size_t)k.rvst; h *= 1099511628211ULL;
        return h;
    }
};

// Build a state's (A_s, RVST) key — condition (1) plus the RVST it was taken at.
// Exposed so GetSuccessors can apply Bell & Park's Descendants Line 8 test:
// a child is a real descendant worth remembering only if its RVST AND its
// scheduled set both differ from its parent's. Otherwise it is an INTERMEDIATE
// state, which must not be dominance-checked — a child is always >= its parent
// componentwise, so an intermediate child is trivially dominated by its own
// parent and pruning it would kill the entire subtree.
template<short N, class State, class Acts>
inline CBSCutsetKey<N> cbs_cutset_key(const State& state, const Acts& activities) {
    CBSCutsetKey<N> key;
    key.rvst = state.t_first;
    if (state.t_first < 0) return key;
    for (int i = 0; i < (int)activities.size(); i++)
        if (state.start_times[i] + activities[i].duration <= state.t_first) key.set(i);
    return key;
}

// Exact-match hash for start_times arrays (used by the bidirectional kill set;
// membership is by full array equality, so hash collisions can never kill a
// wrong state).
template<short N>
struct CBSStateArrHash {
    size_t operator()(const std::array<short, N>& a) const {
        size_t h = 1469598103934665603ULL;
        for (short v : a) { h ^= (size_t)(uint16_t)v; h *= 1099511628211ULL; }
        return h;
    }
};

template<short N>
class CBSDominanceTable {
public:
    struct Entry {
        std::array<short, N> start_times;                 // B&P condition (3); also identity
        std::vector<std::pair<short, short>> arcs_u;      // B&P condition (2); empty under weak
        short t_star = -1;                                // DR5
        std::vector<std::pair<short, short>> running;     // DR5: (activity, finish), sorted
    };

    long checks = 0;      // states consulted
    long pruned = 0;      // states pruned as dominated
    long stored = 0;      // states inserted (into at least one table)
    long bp_count  = 0;   // entries held by the BP table
    long dr5_count = 0;   // entries held by the DR5 table

    long killed_count = 0;   // states retired by bidirectional domination

    void clear() {
        bp_buckets.clear(); dr5_buckets.clear(); killed.clear();
        checks = pruned = stored = bp_count = dr5_count = killed_count = 0;
    }

    // Diagnostic: bucket-size distribution of the BP table (the skyline opportunity).
    // nb = #buckets, maxb = largest bucket, avgb = mean entries/bucket.
    void bp_bucket_stats(size_t& nb, size_t& maxb, double& avgb) const {
        nb = bp_buckets.size(); maxb = 0; size_t tot = 0;
        for (const auto& kv : bp_buckets) { maxb = std::max(maxb, kv.second.size()); tot += kv.second.size(); }
        avgb = nb ? (double)tot / nb : 0.0;
    }

    // Bidirectional dominance (B&P: "pruning of existing states by a new state
    // S as well"): a state whose stored entry was later dominated is dead — its
    // subtree is covered by the dominating state's. GetSuccessors consults this
    // at expansion and emits no children for killed states.
    template<class State>
    bool is_killed(const State& state) const {
        return !killed.empty() && killed.count(state.start_times) != 0;
    }

    // True if `state` is dominated by a stored state (=> prune); else stores it.
    // Requires compute_first_conflict() or compute_h_and_RVS() to have run (sets
    // t_first / first_conflict_pool). Rule per g_dom_rule; in DOM_BOTH both
    // Bell & Park and DR5S are consulted and the union prunes.
    //
    // Each rule's key/artifacts are built exactly ONCE and shared between the
    // check and the store (the old code rebuilt them for each), and duplicate
    // detection is folded into the check scan.
    template<class State, class Acts>
    bool check_and_insert(const State& state, const Acts& activities) {
        ++checks;
        if (state.t_first < 0) return false;   // no violation => nothing to compare

        const bool useBP  = (g_dom_rule == DOM_BP  || g_dom_rule == DOM_BOTH);
        const bool useDR5 = (g_dom_rule == DOM_DR5 || g_dom_rule == DOM_DR5S || g_dom_rule == DOM_BOTH);
        const bool sound  = (g_dom_rule == DOM_DR5S || g_dom_rule == DOM_BOTH);  // DR5S vs plain DR5
        const short rvst  = state.t_first;
        const int n = (int)activities.size();

        // ── Bell & Park: build once, check, remember bucket for the store ──
        CBSCutsetKey<N> bp_key;
        std::vector<short> Au;
        std::vector<std::pair<short, short>> arcs_u;
        std::vector<Entry>* bp_bucket = nullptr;
        bool bp_dup = false;
        if (useBP) {
            bp_build(state, activities, bp_key, Au, arcs_u);
            bp_bucket = &bp_buckets[bp_key];
            for (const Entry& e : *bp_bucket) {
                if (e.start_times == state.start_times) { bp_dup = true; continue; }  // never self-dominate
                if (e.arcs_u != arcs_u) continue;                   // (2)
                bool all_no_later = true;                           // (3)
                for (short a : Au)
                    if (e.start_times[a] > state.start_times[a]) { all_no_later = false; break; }
                if (all_no_later) { dump(rvst, e, state.start_times, n); ++pruned; return true; }
            }
        }

        // ── DR5/DR5S: build once, check, remember bucket for the store ──
        CBSCutsetKey<N> d_key;
        std::vector<std::pair<short, short>> running;
        std::vector<short> nonCut;
        std::vector<Entry>* d_bucket = nullptr;
        bool d_dup = false;
        if (useDR5) {
            dr5_build(state, activities, d_key, running, nonCut);
            d_bucket = &dr5_buckets[d_key];
            for (const Entry& e : *d_bucket) {
                if (e.start_times == state.start_times) { d_dup = true; continue; }
                if (e.t_star > rvst) continue;                      // stored must be at t*' <= t*
                if (e.running.size() != running.size()) continue;   // member sets must match
                bool ok = true;
                for (size_t k = 0; k < running.size(); k++) {
                    if (e.running[k].first  != running[k].first)  { ok = false; break; }
                    if (e.running[k].second > running[k].second)  { ok = false; break; }
                }
                if (ok && sound)   // DR5S: release bounds outside the cutset
                    for (short a : nonCut)
                        if (e.start_times[a] > state.start_times[a]) { ok = false; break; }
                if (ok) { dump(rvst, e, state.start_times, n); ++pruned; return true; }
            }
        }

        // ── Bidirectional (RCPSP_BIDIR): the new state survived, so it may now
        // retire stored entries IT dominates. Order matters for soundness: a
        // state that would be dominated by an entry was already pruned above,
        // so mutual kills are impossible. Domination is transitive within a
        // bucket, so removing a retired entry loses no pruning power — the new
        // entry covers everything it covered.
        // g_dom_skyline enables the SAME entry-retirement but WITHOUT the kill-set,
        // so it is search-identical for pruning (an entry retired here was dominated
        // by the new one, and by transitivity anything it would have pruned the new
        // entry prunes too) — it only shrinks buckets. g_use_bidir additionally
        // records the retired state in `killed`, which stops its expansion and DOES
        // change the search.
        if (g_use_bidir || g_dom_skyline) {
            const bool feed_kill = g_use_bidir;
            if (bp_bucket) {
                for (size_t k = 0; k < bp_bucket->size(); ) {
                    Entry& e = (*bp_bucket)[k];
                    bool dominated = (e.start_times != state.start_times) && (e.arcs_u == arcs_u);
                    if (dominated)
                        for (short a : Au)
                            if (state.start_times[a] > e.start_times[a]) { dominated = false; break; }
                    if (dominated) {
                        if (feed_kill) { killed.insert(e.start_times); ++killed_count; }
                        e = std::move(bp_bucket->back()); bp_bucket->pop_back(); --bp_count;
                    } else k++;
                }
            }
            if (d_bucket) {
                for (size_t k = 0; k < d_bucket->size(); ) {
                    Entry& e = (*d_bucket)[k];
                    bool dominated = (e.start_times != state.start_times)
                                  && (rvst <= e.t_star)
                                  && (e.running.size() == running.size());
                    if (dominated)
                        for (size_t m = 0; m < running.size(); m++) {
                            if (e.running[m].first != running[m].first ||
                                running[m].second > e.running[m].second) { dominated = false; break; }
                        }
                    if (dominated && sound)
                        for (short a : nonCut)
                            if (state.start_times[a] > e.start_times[a]) { dominated = false; break; }
                    if (dominated) {
                        if (feed_kill) { killed.insert(e.start_times); ++killed_count; }
                        e = std::move(d_bucket->back()); d_bucket->pop_back(); --dr5_count;
                    } else k++;
                }
            }
        }

        // ── Not dominated: store into each active table (dup-skip, capped) ──
        if (bp_bucket && !bp_dup && bp_count < g_dom_store_cap) {
            bp_bucket->push_back(Entry{state.start_times, std::move(arcs_u), -1, {}});
            ++bp_count;
        }
        if (d_bucket && !d_dup && dr5_count < g_dom_store_cap) {
            d_bucket->push_back(Entry{state.start_times, {}, rvst, std::move(running)});
            ++dr5_count;
        }
        ++stored;
        return false;
    }

private:
    std::unordered_map<CBSCutsetKey<N>, std::vector<Entry>, CBSCutsetKeyHash<N>> bp_buckets;
    std::unordered_map<CBSCutsetKey<N>, std::vector<Entry>, CBSCutsetKeyHash<N>> dr5_buckets;
    // Bidirectional kill set: start_times of states retired by a later
    // dominating state. Exact array equality — no false kills via collisions.
    std::unordered_set<std::array<short, N>, CBSStateArrHash<N>> killed;

    // ── Bell & Park keying: key = (A_s, RVST); A_u = everything not finished by RVST.
    template<class State, class Acts>
    static void bp_build(const State& state, const Acts& activities,
                         CBSCutsetKey<N>& key, std::vector<short>& Au,
                         std::vector<std::pair<short, short>>& arcs_u) {
        const short rvst = state.t_first;
        const int n = (int)activities.size();
        key.rvst = rvst;
        Au.reserve(n);
        for (int i = 0; i < n; i++) {
            if (state.start_times[i] + activities[i].duration <= rvst) key.set(i);   // A_s
            else Au.push_back((short)i);                                             // A_u
        }
        // (2) arcs of the added network with both endpoints in A_u. Empty under the
        //     weak (MDA) form; a single .empty() check when so.
        if (!state.added_precedences.empty()) {
            for (const auto& [f, t] : state.added_precedences)
                if (std::binary_search(Au.begin(), Au.end(), f) &&
                    std::binary_search(Au.begin(), Au.end(), t))
                    arcs_u.emplace_back(f, t);
            std::sort(arcs_u.begin(), arcs_u.end());
        }
    }

    // ── DR5 keying: key = cutset {finished by t*} U {running at t*, not a conflict
    //    participant}; running = (act, finish) for the running members; nonCut = the
    //    activities outside the cutset (which DR5S additionally compares by start).
    template<class State, class Acts>
    static void dr5_build(const State& state, const Acts& activities,
                          CBSCutsetKey<N>& key, std::vector<std::pair<short, short>>& running,
                          std::vector<short>& nonCut) {
        const short rvst = state.t_first;
        const int n = (int)activities.size();
        key.rvst = -1;   // t* deliberately NOT keyed; DR5 compares across t*' <= t*
        const auto& pool = state.first_conflict_pool;
        for (int i = 0; i < n; i++) {
            const short start  = state.start_times[i];
            const short finish = start + activities[i].duration;
            if (finish <= rvst) { key.set(i); continue; }
            if (start <= rvst && std::find(pool.begin(), pool.end(), (short)i) == pool.end()) {
                key.set(i);
                running.emplace_back((short)i, finish);
                continue;
            }
            nonCut.push_back((short)i);
        }
        std::sort(running.begin(), running.end());
    }

    static void dump(short rvst, const Entry& e, const std::array<short, N>& s, int n) {
        if (g_dom_dump_path.empty()) return;
        std::ofstream f(g_dom_dump_path, std::ios::app);
        f << rvst << ";";
        for (int i = 0; i < n; i++) f << e.start_times[i] << (i + 1 < n ? "," : "");
        f << ";";
        for (int i = 0; i < n; i++) f << s[i] << (i + 1 < n ? "," : "");
        f << "\n";
    }

};

// One table per N, cleared per instance by the driver.
template<short N>
inline CBSDominanceTable<N>& get_cbs_dominance_table() {
    static CBSDominanceTable<N> table;
    return table;
}
