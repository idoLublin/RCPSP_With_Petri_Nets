//
// DominanceTT2.h — cutset-style dominance pruning for the TT2 (TTPNR) A* search.
//
// Implements the inequality-based dominance relation (Demeulemeester &
// Herroelen 1992 cutset rule; Liu, Jin, Zhou & Hu, C&OR 151 (2023) 106097,
// dominance rule 5) adapted to TTPNR states:
//
//   For states A, B with the same scheduled set S = finished ∪ active-ids,
//   define the release vector r_X(i) = g_X + rem_X(i) for i active in X,
//   and r_X(i) = g_X for i finished in X. Then
//
//       A ⪰ B  ⇔  g_A ≤ g_B  and  r_A(i) ≤ r_B(i) for all i in S.
//
//   If A ⪰ B, every completion of B can be replayed from A under the eager
//   firing semantics with componentwise no-later finish times, so the best
//   makespan reachable from A is ≤ that of B and B can be pruned.
//
// The reduced check (used below): g_A ≤ g_B, and for every i ACTIVE in A,
//   g_A + rem_A(i) ≤ (i active in B ? g_B + rem_B(i) : g_B).
// Activities finished in A need no per-activity check (r_A = g_A ≤ g_B).
//
// Mutual domination between DISTINCT states is impossible (it forces equal
// g, equal actives and equal finished sets — i.e. the same state, which the
// hash-based duplicate detection merges before dominance is ever consulted),
// so ⪰ is a strict partial order on distinct stored states and pruning can
// never discard both of two mutually-dominating nodes.
//
// The table stores element INDICES into the search's open/closed list and
// reads the dominator's g live at check time: a stored node's g can only
// decrease (better path / re-opening), which only strengthens dominance, so
// prior prune decisions remain valid.
//
#pragma once
#ifndef DOMINANCE_TT2_H
#define DOMINANCE_TT2_H

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "RCPSPState.h"

class TT2DominanceTable {
public:
    // Scheduled-set bitset packed into two 64-bit words (MAX_ACTIVITIES ≤ 128).
    struct Key {
        uint64_t lo, hi;
        bool operator==(const Key &o) const { return lo == o.lo && hi == o.hi; }
    };
    struct KeyHash {
        size_t operator()(const Key &k) const {
            // splitmix64-style mix of both words
            uint64_t h = k.lo + 0x9e3779b97f4a7c15ULL;
            h = (h ^ (h >> 30)) * 0xbf58476d1ce4e5b9ULL;
            h ^= k.hi + 0x9e3779b97f4a7c15ULL;
            h = (h ^ (h >> 27)) * 0x94d049bb133111ebULL;
            return (size_t)(h ^ (h >> 31));
        }
    };

    // Counters (reported per instance)
    uint64_t checks = 0;        // IsDominated calls
    uint64_t comparisons = 0;   // pairwise Dominates() evaluations
    uint64_t pruned = 0;        // candidates pruned at generation
    uint64_t popChecks = 0;     // IsDominatedAtPop calls
    uint64_t popPruned = 0;     // nodes skipped at expansion
    uint64_t inserts = 0;       // nodes recorded in the table
    uint64_t thinned = 0;       // stored entries removed by Pareto thinning
    size_t maxBucket = 0;       // largest bucket seen

    static Key MakeKey(const RCPSPState_TT2 &s) {
        std::bitset<MAX_ACTIVITIES> sched = s.finishedActivitiys;
        for (const auto &a : s.activeTransitionIndices)
            sched.set(a.first);
        Key k;
        if constexpr (MAX_ACTIVITIES > 64) {
            static const std::bitset<MAX_ACTIVITIES> low64(~0ULL);
            k.lo = (sched & low64).to_ullong();
            k.hi = (sched >> 64).to_ullong();
        } else {
            k.lo = sched.to_ullong();
            k.hi = 0;
        }
        return k;
    }

    // True iff stored A (with live cost gA) dominates candidate B (cost gB).
    // Precondition: same scheduled set (same bucket). Active lists are sorted
    // by activity id (state-constructor invariant), so a single merge scan
    // suffices. Non-strict ≤ throughout; see header comment for tie safety.
    static bool Dominates(const RCPSPState_TT2 &A, double gA,
                          const RCPSPState_TT2 &B, double gB) {
        if (gA > gB) return false;
        const auto &a = A.activeTransitionIndices;
        const auto &b = B.activeTransitionIndices;
        size_t ib = 0;
        for (size_t ia = 0; ia < a.size(); ++ia) {
            while (ib < b.size() && b[ib].first < a[ia].first) ++ib;
            const double bound = (ib < b.size() && b[ib].first == a[ia].first)
                                     ? gB + b[ib].second   // active in both
                                     : gB;                 // finished in B
            if (gA + a[ia].second > bound) return false;
        }
        return true;
    }

    // Generation-time check: is the (not-yet-stored) candidate dominated by
    // any stored node with the same scheduled set?
    template <class OpenClosedList>
    bool IsDominated(const RCPSPState_TT2 &cand, double gCand,
                     const OpenClosedList &list) {
        ++checks;
        auto it = buckets.find(MakeKey(cand));
        if (it == buckets.end()) return false;
        for (uint64_t idx : it->second) {
            const auto &e = list.Lookat(idx);
            ++comparisons;
            if (Dominates(e.data, e.g, cand, gCand)) {
                ++pruned;
                return true;
            }
        }
        return false;
    }

    // Pop-time check (Liu et al. apply their rule at generation AND at
    // branching): is the node being expanded dominated by a DIFFERENT stored
    // node? Catches dominators inserted after this node was generated. Uses
    // live g values, so it stays consistent with re-opening.
    template <class OpenClosedList>
    bool IsDominatedAtPop(uint64_t elementId, const OpenClosedList &list) {
        ++popChecks;
        const auto &self = list.Lookat(elementId);
        auto it = buckets.find(MakeKey(self.data));
        if (it == buckets.end()) return false;
        for (uint64_t idx : it->second) {
            if (idx == elementId) continue;
            const auto &e = list.Lookat(idx);
            ++comparisons;
            if (Dominates(e.data, e.g, self.data, self.g)) {
                ++popPruned;
                return true;
            }
        }
        return false;
    }

    // Record a node that was just added to open. Pareto-thins the bucket:
    // stored entries dominated by the new node are dropped from the TABLE
    // only (they stay in open/closed; transitivity preserves pruning power).
    template <class OpenClosedList>
    void Insert(uint64_t elementId, const OpenClosedList &list) {
        const auto &self = list.Lookat(elementId);
        auto &bucket = buckets[MakeKey(self.data)];
        for (size_t i = 0; i < bucket.size();) {
            const auto &e = list.Lookat(bucket[i]);
            ++comparisons;
            if (Dominates(self.data, self.g, e.data, e.g)) {
                bucket[i] = bucket.back();
                bucket.pop_back();
                ++thinned;
            } else {
                ++i;
            }
        }
        bucket.push_back(elementId);
        ++inserts;
        if (bucket.size() > maxBucket) maxBucket = bucket.size();
    }

    size_t BucketCount() const { return buckets.size(); }

private:
    std::unordered_map<Key, std::vector<uint64_t>, KeyHash> buckets;
};

#endif // DOMINANCE_TT2_H
