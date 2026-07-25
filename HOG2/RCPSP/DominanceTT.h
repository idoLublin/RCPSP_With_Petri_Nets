//
// DominanceTT.h — cutset-style dominance pruning for the TT (absolute-date) A* search.
//
// Implements the inequality-based dominance relation (Demeulemeester &
// Herroelen 1992 cutset rule; Liu, Jin, Zhou & Hu, C&OR 151 (2023) 106097,
// dominance rule 5) directly on TT's ABSOLUTE-date states — no relative-delay
// reconstruction is needed, unlike the TT2 adaptation (DominanceTT2.h).
//
//   For two TT states A, B with the same scheduled (= finished) set S, A ⪰ B iff
//   A finishes every activity no later than B AND frees every resource unit no
//   later than B:
//
//       A ⪰ B  ⇔  finishedActivitiys_A[i] ≤ finishedActivitiys_B[i]  ∀ i∈S
//                 and, per resource r, sorted per-unit availability times of A
//                 are componentwise ≤ those of B.
//
//   If A ⪰ B, every completion of B replays from A with componentwise no-later
//   activity starts (precedence timing) and no-later resource releases, so the
//   best makespan reachable from A is ≤ that of B and B can be pruned.
//
//   The finish-time test alone subsumes g_A ≤ g_B (g = max finish) and, since
//   start = finish − duration, also A's SM1 last-start ≤ B's — so the DR1/DR2
//   canonical-order constraint stays consistent under domination.
//
//   Both tests are required: earlier finishes imply earlier starts, so a
//   finish-dominant A can occupy a resource EARLIER and thus be LESS available
//   at early times — finish dominance does not imply resource dominance.
//
//   activity_nodes (control-flow tokens) are NOT compared: they are derived from
//   the finish times (a dominator's are strictly earlier), so comparing them
//   would wrongly reject valid dominators.
//
// Soundness / no-double-discard (differs from TT2): TT's resource_nodes are not
// kept in canonical order, so the TT2 argument "mutual domination ⇒ identical
// state ⇒ hash-merged" does NOT transfer here. Instead: generation-time pruning
// only discards a candidate against an already-STORED dominator, and A ⪰ B means
// every completion of B is reproducible from A at no-later times, so dropping B
// loses nothing even if B ⪰ A. Insert() Pareto-thins the TABLE only (thinned
// entries stay in open/closed), and by transitivity a representative of every
// mutually-dominating class always remains stored — so pruning can never discard
// all representatives of a class.
//
// The table stores a compact, state-intrinsic signature per node (finish times
// + sorted resource-release times) rather than a live open-list reference: these
// fields are fixed at node creation and are exactly what the dominance relation
// reads, so no open/closed lookup is needed on the hot path.
//
#pragma once
#ifndef DOMINANCE_TT_H
#define DOMINANCE_TT_H

#include <algorithm>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "RCPSPState.h"

class TTDominanceTable {
public:
    // Scheduled-set bitset packed into two 64-bit words (MAX_ACTIVITIES ≤ 128).
    struct Key {
        uint64_t lo, hi;
        bool operator==(const Key &o) const { return lo == o.lo && hi == o.hi; }
    };
    struct KeyHash {
        size_t operator()(const Key &k) const {
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

    // Scheduled set = finished activities (TT has no in-progress list; each edge
    // runs a task to completion). Bit i set iff finishedActivitiys[i] != -1.
    static Key MakeKey(const RCPSPState_TT &s) {
        Key k{0, 0};
        for (int i = 0; i < MAX_ACTIVITIES; ++i) {
            if (s.finishedActivitiys[i] == -1) continue;
            if (i < 64) k.lo |= (1ULL << i);
            else        k.hi |= (1ULL << (i - 64));
        }
        return k;
    }

    // Compact per-node "signature": the finish time of each scheduled activity
    // (aligned to the bucket's fixed activity-id layout) followed by the sorted
    // per-unit resource-availability times. Two nodes in the same bucket share an
    // equal-length layout, so dominance A ⪰ B reduces to ONE contiguous
    // componentwise ≤ over the signature: the finish-time prefix means "A
    // finishes each activity no later" (which subsumes g_A ≤ g_B, as g = max
    // finish) and the resource suffix means "A frees each unit no later". No
    // open-list lookup, per-comparison expansion, or 128-slot scan on the hot path.
    struct Bucket {
        std::vector<short> ids;                 // scheduled activity ids, ascending
        std::vector<uint64_t> elems;            // element ids, parallel to sigs
        std::vector<std::vector<short>> sigs;   // one signature per stored node
    };

    // Ascending scheduled-activity id list for a state (the bucket's layout).
    static void CollectIds(const RCPSPState_TT &s, std::vector<short> &ids) {
        ids.clear();
        for (int i = 0; i < MAX_ACTIVITIES; ++i)
            if (s.finishedActivitiys[i] != -1) ids.push_back((short)i);
    }

    // Build the signature of state s under a bucket's id layout into `sig`.
    static void BuildSig(const RCPSPState_TT &s, const std::vector<short> &ids,
                         std::vector<short> &sig) {
        sig.clear();
        for (short id : ids) sig.push_back(s.finishedActivitiys[id]);
        for (int r = 0; r < 4; ++r) {
            size_t start = sig.size();
            for (const auto &[amt, t] : s.resource_nodes[r])
                for (short c = 0; c < amt; ++c) sig.push_back(t);
            std::sort(sig.begin() + (long)start, sig.end());
        }
    }

    // A ⪰ B: every signature component of A is ≤ B's (equal length by construction).
    static bool SigDominates(const std::vector<short> &a, const std::vector<short> &b) {
        if (a.size() != b.size()) return false;   // defensive; equal in practice
        for (size_t k = 0; k < a.size(); ++k)
            if (a[k] > b[k]) return false;
        return true;
    }

    static std::vector<short> &scratchSig() {
        static thread_local std::vector<short> buf; return buf;
    }

    // Generation-time check: is the candidate dominated by any stored node with
    // the same scheduled set?
    template <class OpenClosedList>
    bool IsDominated(const RCPSPState_TT &cand, double /*gCand*/,
                     const OpenClosedList & /*list*/) {
        ++checks;
        auto it = buckets.find(MakeKey(cand));
        if (it == buckets.end()) return false;
        Bucket &b = it->second;
        auto &sig = scratchSig();
        BuildSig(cand, b.ids, sig);
        for (const auto &stored : b.sigs) {
            ++comparisons;
            if (SigDominates(stored, sig)) { ++pruned; return true; }
        }
        return false;
    }

    // Pop-time check: is the node being expanded dominated by a DIFFERENT stored
    // node? Catches dominators inserted after this node was generated.
    template <class OpenClosedList>
    bool IsDominatedAtPop(uint64_t elementId, const OpenClosedList &list) {
        ++popChecks;
        const auto &self = list.Lookat(elementId);
        auto it = buckets.find(MakeKey(self.data));
        if (it == buckets.end()) return false;
        Bucket &b = it->second;
        auto &sig = scratchSig();
        BuildSig(self.data, b.ids, sig);
        for (size_t i = 0; i < b.sigs.size(); ++i) {
            if (b.elems[i] == elementId) continue;
            ++comparisons;
            if (SigDominates(b.sigs[i], sig)) { ++popPruned; return true; }
        }
        return false;
    }

    // Record a node just added to open, Pareto-thinning the bucket: stored
    // entries dominated by the new node are dropped from the TABLE only (they
    // remain in open/closed; transitivity preserves pruning power).
    template <class OpenClosedList>
    void Insert(uint64_t elementId, const OpenClosedList &list) {
        const auto &self = list.Lookat(elementId);
        Bucket &b = buckets[MakeKey(self.data)];
        if (b.ids.empty()) CollectIds(self.data, b.ids);   // fixed per scheduled set
        std::vector<short> sig;
        BuildSig(self.data, b.ids, sig);
        for (size_t i = 0; i < b.sigs.size();) {
            ++comparisons;
            if (SigDominates(sig, b.sigs[i])) {
                b.sigs[i] = std::move(b.sigs.back()); b.sigs.pop_back();
                b.elems[i] = b.elems.back();          b.elems.pop_back();
                ++thinned;
            } else {
                ++i;
            }
        }
        b.sigs.push_back(std::move(sig));
        b.elems.push_back(elementId);
        ++inserts;
        if (b.sigs.size() > maxBucket) maxBucket = b.sigs.size();
    }

    size_t BucketCount() const { return buckets.size(); }

private:
    std::unordered_map<Key, Bucket, KeyHash> buckets;
};

#endif // DOMINANCE_TT_H
