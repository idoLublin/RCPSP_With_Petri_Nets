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
#include <algorithm>
#include <string>
#include <fstream>

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

template<short N>
class CBSDominanceTable {
public:
    struct Entry {
        std::array<short, N> start_times;                 // B&P condition (3); also identity
        std::vector<std::pair<short, short>> arcs_u;      // B&P condition (2); empty under weak
        short t_star = -1;                                // DR5
        std::vector<std::pair<short, short>> running;     // DR5: (activity, finish), sorted
    };

    long checks = 0;   // states consulted
    long pruned = 0;   // states pruned as dominated
    long stored = 0;   // states inserted

    void clear() { bp_buckets.clear(); dr5_buckets.clear(); checks = pruned = stored = 0; }

    // True if `state` is dominated by a stored state (=> prune); else stores it.
    // Requires compute_h_and_RVS() to have run (sets t_first). Rule per g_dom_rule;
    // in DOM_BOTH both Bell & Park and DR5S are consulted and the union prunes.
    template<class State, class Acts>
    bool check_and_insert(const State& state, const Acts& activities) {
        ++checks;
        if (state.t_first < 0) return false;   // no violation => nothing to compare

        const bool useBP  = (g_dom_rule == DOM_BP  || g_dom_rule == DOM_BOTH);
        const bool useDR5 = (g_dom_rule == DOM_DR5 || g_dom_rule == DOM_DR5S || g_dom_rule == DOM_BOTH);
        const bool sound  = (g_dom_rule == DOM_DR5S || g_dom_rule == DOM_BOTH);  // DR5S vs plain DR5

        // Prune if EITHER active rule dominates. Check both before storing, so a
        // state never dominates itself across the two tables.
        if (useBP  && bp_dominated(state, activities))         { ++pruned; return true; }
        if (useDR5 && dr5_dominated(state, activities, sound)) { ++pruned; return true; }

        if (useBP)  bp_store(state, activities);
        if (useDR5) dr5_store(state, activities);
        ++stored;
        return false;
    }

private:
    std::unordered_map<CBSCutsetKey<N>, std::vector<Entry>, CBSCutsetKeyHash<N>> bp_buckets;
    std::unordered_map<CBSCutsetKey<N>, std::vector<Entry>, CBSCutsetKeyHash<N>> dr5_buckets;

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

    template<class State, class Acts>
    bool bp_dominated(const State& state, const Acts& activities) {
        CBSCutsetKey<N> key; std::vector<short> Au; std::vector<std::pair<short, short>> arcs_u;
        bp_build(state, activities, key, Au, arcs_u);
        auto it = bp_buckets.find(key);
        if (it == bp_buckets.end()) return false;
        for (const Entry& e : it->second) {
            if (e.start_times == state.start_times) continue;   // never dominate an identical state
            if (e.arcs_u != arcs_u) continue;                   // (2)
            bool all_no_later = true;                           // (3)
            for (short a : Au)
                if (e.start_times[a] > state.start_times[a]) { all_no_later = false; break; }
            if (all_no_later) { dump(state.t_first, e, state.start_times, (int)activities.size()); return true; }
        }
        return false;
    }

    template<class State, class Acts>
    bool dr5_dominated(const State& state, const Acts& activities, bool sound) {
        CBSCutsetKey<N> key; std::vector<std::pair<short, short>> running; std::vector<short> nonCut;
        dr5_build(state, activities, key, running, nonCut);
        auto it = dr5_buckets.find(key);
        if (it == dr5_buckets.end()) return false;
        const short rvst = state.t_first;
        for (const Entry& e : it->second) {
            if (e.start_times == state.start_times) continue;
            if (e.t_star > rvst) continue;                      // stored must be at t*' <= t*
            // Same cutset can split into finished/running differently across states,
            // so the running vectors may differ; only compare matching member sets.
            if (e.running.size() != running.size()) continue;
            bool ok = true;
            for (size_t k = 0; k < running.size(); k++) {
                if (e.running[k].first != running[k].first)  { ok = false; break; }
                if (e.running[k].second > running[k].second) { ok = false; break; }
            }
            // DR5S: also require no-later starts outside the cutset — the release
            // bounds DR5's abstraction drops, which is why plain DR5 loses optima.
            if (ok && sound)
                for (short a : nonCut)
                    if (e.start_times[a] > state.start_times[a]) { ok = false; break; }
            if (ok) { dump(rvst, e, state.start_times, (int)activities.size()); return true; }
        }
        return false;
    }

    template<class State, class Acts>
    void bp_store(const State& state, const Acts& activities) {
        CBSCutsetKey<N> key; std::vector<short> Au; std::vector<std::pair<short, short>> arcs_u;
        bp_build(state, activities, key, Au, arcs_u);
        auto& bucket = bp_buckets[key];
        for (const Entry& e : bucket) if (e.start_times == state.start_times) return;  // no dup
        bucket.push_back(Entry{state.start_times, std::move(arcs_u), -1, {}});
    }

    template<class State, class Acts>
    void dr5_store(const State& state, const Acts& activities) {
        CBSCutsetKey<N> key; std::vector<std::pair<short, short>> running; std::vector<short> nonCut;
        dr5_build(state, activities, key, running, nonCut);
        auto& bucket = dr5_buckets[key];
        for (const Entry& e : bucket) if (e.start_times == state.start_times) return;  // no dup
        bucket.push_back(Entry{state.start_times, {}, state.t_first, std::move(running)});
    }
};

// One table per N, cleared per instance by the driver.
template<short N>
inline CBSDominanceTable<N>& get_cbs_dominance_table() {
    static CBSDominanceTable<N> table;
    return table;
}
