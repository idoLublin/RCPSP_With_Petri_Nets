#pragma once
#ifndef DOMINANCE_TT2_H
#define DOMINANCE_TT2_H
// ─────────────────────────────────────────────────────────────────────────────
// DominanceTT2.h — cutset (DR5) dominance for the TT2 / TTPNR A* search, adapted
// for the MAIN repo (self-contained: stores the active vector + g per entry, so it
// needs no A* library hooks — checked/inserted directly inside
// RCPSP_TT2::GetSuccessors, exactly like DominanceCBS.h does for CBS). Env-gated by
// RCPSP_TT2_DR5=1 (g_tt2_dr5). Ported from the students' new_heuristic/.../DominanceTT2.h
// (Demeulemeester & Herroelen 1992 cutset rule; Liu et al. C&OR 151 (2023) rule 5).
//
// Rule. Two states share a bucket iff they have the same scheduled set
// S = finished ∪ active-ids. Define the release of activity i in state X:
//   r_X(i) = g_X + rem_X(i)  if i is ACTIVE in X,   r_X(i) = g_X  if i is finished.
// Then  A ⪰ B  (A dominates B, prune B)  iff  g_A ≤ g_B  and  r_A(i) ≤ r_B(i) ∀ i∈S.
// Since A,B share S, an id active in A is either active in B (bound g_B+rem_B) or
// finished in B (bound g_B). Finished-in-A ids need no check (r_A = g_A ≤ g_B).
// Every completion of B replays from A with componentwise no-later finishes, so
// A's best makespan ≤ B's. Active lists are sorted by id (state ctor invariant,
// RCPSPState.cpp:5438) => one merge scan suffices.
//
// Self-contained caveat vs the students' index version: we store g at INSERT time.
// In A* g can only DECREASE later (better path / reopen), and a smaller true g_A
// only makes A dominate MORE — so a stale (too-high) stored g can only make us miss
// a prune, never make a wrong one. Sound, marginally weaker.
//
// Skyline thinning: on insert, stored entries dominated by the new state are dropped
// from the TABLE (transitivity keeps pruning power) — the same Pareto-min trick as
// RCPSP_SKYLINE on the CBS side, here always-on because it is pure upside.
// ─────────────────────────────────────────────────────────────────────────────

#include <cstdint>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include "RCPSPState.h"

class TT2DominanceTable {
public:
    // Scheduled-set bitset (finished ∪ active ids) packed into two 64-bit words.
    struct Key {
        uint64_t lo, hi;
        bool operator==(const Key& o) const { return lo == o.lo && hi == o.hi; }
    };
    struct KeyHash {
        size_t operator()(const Key& k) const {
            uint64_t h = k.lo + 0x9e3779b97f4a7c15ULL;
            h = (h ^ (h >> 30)) * 0xbf58476d1ce4e5b9ULL;
            h ^= k.hi + 0x9e3779b97f4a7c15ULL;
            h = (h ^ (h >> 27)) * 0x94d049bb133111ebULL;
            return (size_t)(h ^ (h >> 31));
        }
    };

    struct Entry {
        std::vector<std::pair<short, short>> active;   // (id, remaining), sorted by id
        short g;
    };

    // Per-instance counters (reported by the driver).
    uint64_t checks = 0, comparisons = 0, pruned = 0, inserts = 0, thinned = 0;
    size_t   maxBucket = 0;

    void clear() {
        buckets.clear();
        checks = comparisons = pruned = inserts = thinned = 0;
        maxBucket = 0;
    }

    static Key MakeKey(const RCPSPState_TT2& s) {
        std::bitset<128> sched = s.finishedActivitiys;
        for (const auto& a : s.activeTransitionIndices) sched.set(a.first);
        static const std::bitset<128> low64(~0ULL);
        Key k;
        k.lo = (sched & low64).to_ullong();
        k.hi = (sched >> 64).to_ullong();
        return k;
    }

    // Does (active a, cost gA) dominate (active b, cost gB)? Same scheduled set
    // assumed (same bucket). Non-strict ≤ throughout.
    static bool dominates(const std::vector<std::pair<short, short>>& a, short gA,
                          const std::vector<std::pair<short, short>>& b, short gB) {
        if (gA > gB) return false;
        size_t ib = 0;
        for (size_t ia = 0; ia < a.size(); ++ia) {
            while (ib < b.size() && b[ib].first < a[ia].first) ++ib;
            const short bound = (ib < b.size() && b[ib].first == a[ia].first)
                                    ? (short)(gB + b[ib].second)   // active in both
                                    : gB;                          // finished in b
            if (gA + a[ia].second > bound) return false;
        }
        return true;
    }

    // True if `s` is dominated by a stored entry (=> prune). Otherwise Pareto-thins
    // the bucket by `s` and inserts `s`. One call per generated child, in GetSuccessors.
    bool check_and_insert(const RCPSPState_TT2& s) {
        ++checks;
        auto& bucket = buckets[MakeKey(s)];
        for (const Entry& e : bucket) {
            ++comparisons;
            if (dominates(e.active, e.g, s.activeTransitionIndices, s.g)) { ++pruned; return true; }
        }
        // skyline: drop stored entries dominated by s (table-only; transitivity-safe)
        for (size_t i = 0; i < bucket.size();) {
            ++comparisons;
            if (dominates(s.activeTransitionIndices, s.g, bucket[i].active, bucket[i].g)) {
                bucket[i] = std::move(bucket.back());
                bucket.pop_back();
                ++thinned;
            } else ++i;
        }
        bucket.push_back(Entry{s.activeTransitionIndices, s.g});
        ++inserts;
        if (bucket.size() > maxBucket) maxBucket = bucket.size();
        return false;
    }

    size_t bucketCount() const { return buckets.size(); }

private:
    std::unordered_map<Key, std::vector<Entry>, KeyHash> buckets;
};

// One table, cleared per instance by the driver.
inline TT2DominanceTable& get_tt2_dominance_table() {
    static TT2DominanceTable t;
    return t;
}

#endif // DOMINANCE_TT2_H
