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

#include <functional>
#include <cstdint>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <memory>
#include <cstdlib>
#include <string>
#include <cstring>
#include <unordered_set>
#include "RCPSPState.h"


// Optional observer on PRUNED states (bidirectional meet). A DR-dominated state is still a
// real state reached by a real path, so its (meet key, g) is a legitimate cut even though the
// search will never expand it. Recording it lets the forward side keep full DR while still
// offering the reversed side every cut it might match. Sound for the incumbent/UB direction:
// g is an actual path cost, so g_f + g_b is an actual schedule length. Null by default.
inline std::function<void(const RCPSPState_TT2&)> g_tt2_dr_prune_observer;

// RCPSP_TT2_DUPPROBE: set by solveRCPSP_TT2 to look a child up in its A*'s open/closed list.
// Returns the g_tt2_dup index (0 new, 1/2 open lower/not, 3/4 closed lower/not). Null by default.
inline std::function<int(const RCPSPState_TT2&)> g_tt2_dup_probe;

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
        short side = 0;   // which search inserted it (cross-side guard)
    };

    // ── FAST STORAGE (RCPSP_DR5_FAST, default 1) ─────────────────────────────
    // Same rule, same decisions, same bucket order as the vector version below (so even the
    // comparison counter matches). Differences are representation only:
    //  * each entry keeps its active ids and RELEASES r(i) = g + rem(i) INLINE (no heap block per
    //    entry, which cost a cache miss per comparison); more than K actives spill to one array;
    //  * two necessary conditions reject most pairs before the merge scan:
    //      A dominates B  =>  g_A <= g_B  and  Rmax_A <= Rmax_B,
    //    where Rmax_X = max over the scheduled set of r_X(i) (= g_X if nothing is active): every
    //    active i of A needs r_A(i) <= r_B(i) <= Rmax_B.
    static constexpr int K = 12;
    struct FEntry {
        short g = 0, side = 0, n = 0, R = 0;
        short id[K]; short rel[K];
        std::unique_ptr<short[]> big;             // n > K: ids in [0,n), releases in [n,2n)
        const short* ids()  const { return n <= K ? id  : big.get(); }
        const short* rels() const { return n <= K ? rel : big.get() + n; }
    };
    // RCPSP_DR5_SORTED (default 0, fast storage only): each bucket is kept ordered by g. Measured 2026-09-10:
    // 24-30% fewer comparisons, identical decisions, but NO faster (keeping the order costs what the skipped
    // comparisons save once the quick reject makes a comparison cheap), so it is off.
    // Mechanism: Only entries with
    // g <= g_s can dominate s and only entries with g >= g_s can be dominated by s, so each scan touches just
    // its side of the order. The SET of entries is exactly what the unsorted table holds, so every prune /
    // thin / insert decision is identical; only the comparison counter (and which dominator is met first) changes.
    static bool SortedOn() {
        static const bool on = [](){ const char* e = std::getenv("RCPSP_DR5_SORTED"); return e ? (std::atoi(e) != 0) : false; }();
        return on;
    }
    // RCPSP_DR5_MTF (default 1, fast unsorted storage): an entry that prunes a state is swapped to the front
    // of its bucket, so repeat dominators are met first. Prune / thin / insert decisions depend only on the SET
    // of entries, so they are identical; only the comparison counter (and bucket order) changes.
    static bool MtfOn() {
        static const bool on = [](){ const char* e = std::getenv("RCPSP_DR5_MTF"); return e ? (std::atoi(e) != 0) : true; }();
        return on;
    }
    static bool FastOn() {
        static const bool on = [](){ const char* e = std::getenv("RCPSP_DR5_FAST"); return e ? (std::atoi(e) != 0) : true; }();
        return on;
    }
    // does A (gA, releases relA over ids idA) dominate B? Same scheduled set assumed.
    static bool domFast(int gA, int nA, int RA, const short* idA, const short* relA,
                        int gB, int nB, int RB, const short* idB, const short* relB) {
        if (gA > gB || RA > RB) return false;
        int ib = 0;
        for (int ia = 0; ia < nA; ++ia) {
            while (ib < nB && idB[ib] < idA[ia]) ++ib;
            const int bound = (ib < nB && idB[ib] == idA[ia]) ? (int)relB[ib] : gB;   // active in both : finished in B
            if (relA[ia] > bound) return false;
        }
        return true;
    }

    // ── EXPLICIT-WAIT MODEL (RCPSP_TT2_WAIT=1) ───────────────────────────────
    // A wait edge keeps the scheduled set and every release r(i) = g + rem(i) and raises g by one, so a state is
    // dominated by its own parent, and by every earlier state of its wait chain. Pruning there would cut every
    // wait. Exception: an entry E with g_E < g_s whose releases equal those of s on every activity still running
    // in s (the other E-actives end by g_s, which domFast already checked) IS s after waiting g_s - g_E units:
    // the parent, the grandparent, ... of the chain, or a state with exactly that future. s is kept and not
    // inserted, since E prunes whatever s would. Strict dominators still prune; equal states at equal g still merge.
    static bool waitTwin(int nA, const short* idA, const short* relA, int nB, const short* idB, const short* relB) {
        int ia = 0;
        for (int ib = 0; ib < nB; ++ib) {
            while (ia < nA && idA[ia] < idB[ib]) ++ia;
            if (ia == nA || idA[ia] != idB[ib] || relA[ia] != relB[ib]) return false;
        }
        return true;
    }
    static bool waitTwinVec(const std::vector<std::pair<short, short>>& a, short gA,
                            const std::vector<std::pair<short, short>>& b, short gB) {
        size_t ia = 0;
        for (const auto& pb : b) {
            while (ia < a.size() && a[ia].first < pb.first) ++ia;
            if (ia == a.size() || a[ia].first != pb.first || gA + a[ia].second != gB + pb.second) return false;
        }
        return true;
    }

    // ── DEAD NODES (RCPSP_TT2_KILL) ─────────────────────────────────────────
    // An entry retired by skyline thinning is a state that a later state dominates; its A* node
    // may still sit on OPEN. Recording it lets RCPSP_TT2::GetSuccessors recognise that node when
    // it is expanded (count it, or skip it with =2: the dominating state covers its subtree, the
    // same argument every DR5 prune already rests on). The record is exact -- scheduled set, g,
    // active ids and releases -- so no hash collision can kill a live node. A node whose g was
    // lowered by a better path was re-inserted with that g, so it never matches its old record.
    static std::string deadKey(const Key& k, int g, int n, const short* ids, const short* rels) {
        std::string r(sizeof(Key) + (2 + 2 * n) * sizeof(short), '\0');
        char* p = &r[0];
        std::memcpy(p, &k, sizeof(Key)); p += sizeof(Key);
        const short hdr[2] = {(short)g, (short)n};
        std::memcpy(p, hdr, sizeof(hdr)); p += sizeof(hdr);
        std::memcpy(p, ids, n * sizeof(short)); p += n * sizeof(short);
        std::memcpy(p, rels, n * sizeof(short));
        return r;
    }
    void markDead(const Key& k, int g, int n, const short* ids, const short* rels) {
        if (g_tt2_kill_armed && !g_in_subsolve) dead.insert(deadKey(k, g, n, ids, rels));
    }
    void markDeadVec(const Key& k, const Entry& e) {
        if (!g_tt2_kill_armed || g_in_subsolve) return;
        const int n = (int)e.active.size();
        std::vector<short> ids(n), rels(n);
        for (int j = 0; j < n; ++j) { ids[j] = e.active[j].first; rels[j] = (short)(e.g + e.active[j].second); }
        dead.insert(deadKey(k, e.g, n, ids.data(), rels.data()));
    }
    bool is_dead(const RCPSPState_TT2& s) const {
        if (dead.empty()) return false;
        const int n = (int)std::min<size_t>(s.activeTransitionIndices.size(), 128);
        short ids[128], rels[128];
        for (int j = 0; j < n; ++j) {
            ids[j]  = s.activeTransitionIndices[j].first;
            rels[j] = (short)(s.g + s.activeTransitionIndices[j].second);
        }
        return dead.count(deadKey(MakeKey(s), s.g, n, ids, rels)) != 0;
    }
    size_t deadCount() const { return dead.size(); }

    // Per-instance counters (reported by the driver).
    uint64_t crossSide = 0;   // prunes where the entry came from the OTHER search
    uint64_t checks = 0, comparisons = 0, pruned = 0, inserts = 0, thinned = 0;
    uint64_t waitExempt = 0;  // explicit-wait model: states kept because their only dominators were themselves before waiting
    size_t   maxBucket = 0;

    void clear() {
        buckets.clear(); fbuckets.clear(); dead.clear();
        checks = comparisons = pruned = inserts = thinned = waitExempt = 0;
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
        if (!FastOn()) return check_and_insert_vec(s);
        const int n = (int)std::min<size_t>(s.activeTransitionIndices.size(), 128);
        short sid[128], srel[128];
        for (int j = 0; j < n; ++j) {
            sid[j]  = s.activeTransitionIndices[j].first;
            srel[j] = (short)(s.g + s.activeTransitionIndices[j].second);
        }
        return check_and_insert_raw(MakeKey(s), s.g, n, sid, srel, &s);
    }

    // The same check on the state's DR5 view only: scheduled set k, g, and the active ids (ascending) with their
    // releases g + rem. RCPSP_TT2::GetSuccessors uses it to test a child BEFORE building it (RCPSP_TT2_PRECHECK).
    // `s` is the built state when there is one; the prune observer needs it, so callers without one must not run
    // while an observer is set. Fast storage only.
    bool check_and_insert_raw(const Key& k, short g, int n, const short* sid, const short* srel, const RCPSPState_TT2* s) {
        ++checks;
        int R = g;
        for (int j = 0; j < n; ++j) if (srel[j] > R) R = srel[j];
        auto& bucket = fbuckets[k];
        if (SortedOn()) return checkSorted(k, g, s, bucket, n, R, sid, srel);
        bool twin = false;
        for (size_t i = 0; i < bucket.size(); ++i) {
            const FEntry& e = bucket[i];
            ++comparisons;
            if (domFast(e.g, e.n, e.R, e.ids(), e.rels(), g, n, R, sid, srel)) {
                if (g_tt2_wait && e.g < g && waitTwin(e.n, e.ids(), e.rels(), n, sid, srel)) { twin = true; continue; }
                ++pruned;
                if (e.side != (short)g_tt2_dom_side) {
                    ++crossSide;
                    if (crossSide == 1)
                        std::cerr << "[DR5 CROSS-SIDE PRUNE] entry.side=" << e.side
                                  << " current side=" << g_tt2_dom_side << std::endl;
                }
                if (g_tt2_dr_prune_observer && s) g_tt2_dr_prune_observer(*s);
                if (i > 0 && MtfOn()) std::swap(bucket[0], bucket[i]);
                return true; }
        }
        if (twin) { ++waitExempt; return false; }
        for (size_t i = 0; i < bucket.size();) {                 // skyline, same order as the vector version
            ++comparisons;
            const FEntry& e = bucket[i];
            if (domFast(g, n, R, sid, srel, e.g, e.n, e.R, e.ids(), e.rels())) {
                markDead(k, e.g, e.n, e.ids(), e.rels());
                if (i + 1 != bucket.size()) bucket[i] = std::move(bucket.back());
                bucket.pop_back();
                ++thinned;
            } else ++i;
        }
        bucket.emplace_back();
        FEntry& ne = bucket.back();
        ne.g = g; ne.side = (short)g_tt2_dom_side; ne.n = (short)n; ne.R = (short)R;
        if (n <= K) { for (int j = 0; j < n; ++j) { ne.id[j] = sid[j]; ne.rel[j] = srel[j]; } }
        else { ne.big.reset(new short[2 * n]); for (int j = 0; j < n; ++j) { ne.big[j] = sid[j]; ne.big[n + j] = srel[j]; } }
        ++inserts;
        if (bucket.size() > maxBucket) maxBucket = bucket.size();
        return false;
    }

    bool checkSorted(const Key& k, short gs, const RCPSPState_TT2* s, std::vector<FEntry>& bucket, int n, int R,
                     const short* sid, const short* srel) {
        bool twin = false;
        for (const FEntry& e : bucket) {                        // candidates that may dominate s: g <= g_s
            if (e.g > gs) break;
            ++comparisons;
            if (domFast(e.g, e.n, e.R, e.ids(), e.rels(), gs, n, R, sid, srel)) {
                if (g_tt2_wait && e.g < gs && waitTwin(e.n, e.ids(), e.rels(), n, sid, srel)) { twin = true; continue; }
                ++pruned;
                if (e.side != (short)g_tt2_dom_side) {
                    ++crossSide;
                    if (crossSide == 1)
                        std::cerr << "[DR5 CROSS-SIDE PRUNE] entry.side=" << e.side
                                  << " current side=" << g_tt2_dom_side << std::endl;
                }
                if (g_tt2_dr_prune_observer && s) g_tt2_dr_prune_observer(*s);
                return true; }
        }
        if (twin) { ++waitExempt; return false; }
        // skyline on the g >= g_s part, order-preserving compaction
        size_t lo = std::lower_bound(bucket.begin(), bucket.end(), gs,
                                     [](const FEntry& e, short g) { return e.g < g; }) - bucket.begin();
        size_t w = lo;
        for (size_t i = lo; i < bucket.size(); ++i) {
            ++comparisons;
            if (domFast(gs, n, R, sid, srel, bucket[i].g, bucket[i].n, bucket[i].R, bucket[i].ids(), bucket[i].rels())) {
                markDead(k, bucket[i].g, bucket[i].n, bucket[i].ids(), bucket[i].rels());
                ++thinned; continue;
            }
            if (w != i) bucket[w] = std::move(bucket[i]);
            ++w;
        }
        bucket.resize(w);
        size_t pos = std::upper_bound(bucket.begin(), bucket.end(), gs,
                                      [](short g, const FEntry& e) { return g < e.g; }) - bucket.begin();
        bucket.emplace(bucket.begin() + pos);
        FEntry& ne = bucket[pos];
        ne.g = gs; ne.side = (short)g_tt2_dom_side; ne.n = (short)n; ne.R = (short)R;
        if (n <= K) { for (int j = 0; j < n; ++j) { ne.id[j] = sid[j]; ne.rel[j] = srel[j]; } }
        else { ne.big.reset(new short[2 * n]); for (int j = 0; j < n; ++j) { ne.big[j] = sid[j]; ne.big[n + j] = srel[j]; } }
        ++inserts;
        if (bucket.size() > maxBucket) maxBucket = bucket.size();
        return false;
    }

    // The original vector-based version (RCPSP_DR5_FAST=0).
    bool check_and_insert_vec(const RCPSPState_TT2& s) {
        ++checks;
        auto& bucket = buckets[MakeKey(s)];
        bool twin = false;
        for (const Entry& e : bucket) {
            ++comparisons;
            if (dominates(e.active, e.g, s.activeTransitionIndices, s.g)) {
                if (g_tt2_wait && e.g < s.g && waitTwinVec(e.active, e.g, s.activeTransitionIndices, s.g)) { twin = true; continue; }
                ++pruned;
                // CROSS-SIDE GUARD (bidirectional). Forward and reversed states live on
                // DIFFERENT nets; an entry inserted by one search says nothing about the
                // other, so pruning across sides is unsound. Tables are meant to be split by
                // g_tt2_dom_side -- this catches it if they are not.
                if (e.side != (short)g_tt2_dom_side) {
                    ++crossSide;
                    if (crossSide == 1)
                        std::cerr << "[DR5 CROSS-SIDE PRUNE] entry.side=" << e.side
                                  << " current side=" << g_tt2_dom_side << std::endl;
                }
                if (g_tt2_dr_prune_observer) g_tt2_dr_prune_observer(s);
                return true; }
        }
        if (twin) { ++waitExempt; return false; }
        // skyline: drop stored entries dominated by s (table-only; transitivity-safe)
        for (size_t i = 0; i < bucket.size();) {
            ++comparisons;
            if (dominates(s.activeTransitionIndices, s.g, bucket[i].active, bucket[i].g)) {
                markDeadVec(MakeKey(s), bucket[i]);
                bucket[i] = std::move(bucket.back());
                bucket.pop_back();
                ++thinned;
            } else ++i;
        }
        { Entry ne{s.activeTransitionIndices, s.g}; ne.side = (short)g_tt2_dom_side;
          bucket.push_back(std::move(ne)); }
        ++inserts;
        if (bucket.size() > maxBucket) maxBucket = bucket.size();
        return false;
    }

    size_t bucketCount() const { return FastOn() ? fbuckets.size() : buckets.size(); }

private:
    std::unordered_map<Key, std::vector<Entry>, KeyHash> buckets;
    std::unordered_map<Key, std::vector<FEntry>, KeyHash> fbuckets;
    std::unordered_set<std::string> dead;   // RCPSP_TT2_KILL records (deadKey)
};

// One table, cleared per instance by the driver. For bidirectional search the active
// side is selected by g_tt2_dom_side (0=forward, 1=backward) so the two directions keep
// SEPARATE dominance tables while interleaving.
extern int g_tt2_dom_side;
inline TT2DominanceTable& get_tt2_dominance_table() {
    static thread_local TT2DominanceTable t0, t1;   // per thread: each HDA* worker (HDAStarTT2.h) owns its own
    return (g_tt2_dom_side == 0) ? t0 : t1;
}

#endif // DOMINANCE_TT2_H
