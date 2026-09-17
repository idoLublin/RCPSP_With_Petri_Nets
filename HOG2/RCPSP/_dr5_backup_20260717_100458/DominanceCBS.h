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

template<short N>
class CBSDominanceTable {
public:
    struct Entry {
        std::array<short, N> start_times;                 // for condition (3)
        std::vector<std::pair<short, short>> arcs_u;      // for condition (2); empty under weak
    };

    long checks = 0;   // states consulted
    long pruned = 0;   // states pruned as dominated
    long stored = 0;   // states inserted

    void clear() { buckets.clear(); checks = pruned = stored = 0; }

    // True if `state` is dominated by a stored state (=> prune); else stores it.
    // Requires compute_h_and_RVS() to have run (sets t_first).
    template<class State, class Acts>
    bool check_and_insert(const State& state, const Acts& activities) {
        ++checks;
        const short rvst = state.t_first;      // Bell & Park's RVST
        if (rvst < 0) return false;            // no violation => nothing to compare

        const int n = (int)activities.size();

        // (1) scheduled set A_s = finishes at or before RVST; A_u is the rest,
        //     including anything merely running at RVST.
        CBSCutsetKey<N> key;
        key.rvst = rvst;
        std::vector<short> unscheduled;
        unscheduled.reserve(n);
        for (int i = 0; i < n; i++) {
            if (state.start_times[i] + activities[i].duration <= rvst) key.set(i);
            else unscheduled.push_back((short)i);
        }

        // (2) arcs of the added network with BOTH endpoints in A_u. Empty under the
        //     weak (MDA) form, where added_precedences is never populated.
        std::vector<std::pair<short, short>> arcs_u;
        if (!state.added_precedences.empty()) {
            for (const auto& [f, t] : state.added_precedences)
                if (std::binary_search(unscheduled.begin(), unscheduled.end(), f) &&
                    std::binary_search(unscheduled.begin(), unscheduled.end(), t))
                    arcs_u.emplace_back(f, t);
            std::sort(arcs_u.begin(), arcs_u.end());
        }

        auto& bucket = buckets[key];
        bool self_present = false;
        for (const Entry& e : bucket) {
            // HCost() runs more than once per node in TemplateAStar, so a node can
            // meet its own entry; it must never dominate itself.
            if (e.start_times == state.start_times) { self_present = true; continue; }
            if (e.arcs_u != arcs_u) continue;                       // (2)
            bool all_no_later = true;                               // (3)
            for (short a : unscheduled)
#ifdef DOM_DUPLICATES_ONLY
                // DIAGNOSTIC: exact equality instead of <=. Under weak branching
                // added_precedences is always empty, so a state IS its start_times;
                // same A_s + equal A_u start times therefore means an exact duplicate,
                // and pruning duplicates is sound no matter what. If this loses an
                // optimum, the fault is in the harness, not in Bell & Park's rule.
                if (e.start_times[a] != state.start_times[a]) { all_no_later = false; break; }
#else
                if (e.start_times[a] > state.start_times[a]) { all_no_later = false; break; }
#endif
            if (all_no_later) { ++pruned; return true; }
        }

        if (!self_present) {
            bucket.push_back(Entry{state.start_times, std::move(arcs_u)});
            ++stored;
        }
        return false;
    }

private:
    std::unordered_map<CBSCutsetKey<N>, std::vector<Entry>, CBSCutsetKeyHash<N>> buckets;
};

// One table per N, cleared per instance by the driver.
template<short N>
inline CBSDominanceTable<N>& get_cbs_dominance_table() {
    static CBSDominanceTable<N> table;
    return table;
}
