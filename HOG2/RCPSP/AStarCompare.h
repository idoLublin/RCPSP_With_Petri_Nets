#ifndef ASTARCOMPARE_H
#define ASTARCOMPARE_H

#include "Globals.h"
#include "RCPSPState.h"

// ── RCPSPState ────────────────────────────────────────────────────
template<>
struct AStarCompareWithF<RCPSPState> {
    bool operator()(const AStarOpenClosedDataWithF<RCPSPState>& i1,
                    const AStarOpenClosedDataWithF<RCPSPState>& i2) const {
        if (i1.f != i2.f) return i1.f > i2.f;
        if (i1.g != i2.g) return i1.g < i2.g;//more

        // prefer more started activities
        size_t s1 = 0, s2 = 0;
        for (int t : i1.data.startedActivitiys) if (t != -1) s1++;
        for (int t : i2.data.startedActivitiys) if (t != -1) s2++;
        if (s1 != s2) return s1 < s2;

        // prefer more finished activities
        size_t f1 = 0, f2 = 0;
        for (int t : i1.data.finishedActivitiys) if (t != -1) f1++;
        for (int t : i2.data.finishedActivitiys) if (t != -1) f2++;
        return f1 < f2;
    }
};

// ── RCPSPState_TT ─────────────────────────────────────────────────
template<>
struct AStarCompareWithF<RCPSPState_TT> {
    bool operator()(const AStarOpenClosedDataWithF<RCPSPState_TT>& i1,
                    const AStarOpenClosedDataWithF<RCPSPState_TT>& i2) const {
        if (i1.f != i2.f) return i1.f > i2.f;
        if (i1.g != i2.g) return i1.g < i2.g;//more

        // prefer more finished activities
        size_t f1 = 0, f2 = 0;
        for (int t : i1.data.finishedActivitiys) if (t != -1) f1++;
        for (int t : i2.data.finishedActivitiys) if (t != -1) f2++;
        return f1 < f2;
    }
};

// ── RCPSPState_Bi ─────────────────────────────────────────────────
template<>
struct AStarCompareWithF<RCPSPState_Bi> {
    bool operator()(const AStarOpenClosedDataWithF<RCPSPState_Bi>& i1,
                    const AStarOpenClosedDataWithF<RCPSPState_Bi>& i2) const {
        if (i1.f != i2.f) return i1.f > i2.f;
        if (i1.g != i2.g) return i1.g < i2.g;//more

        // prefer more finished activities
        size_t f1 = 0, f2 = 0;
        for (int t : i1.data.finishedActivitiys) if (t != -1) f1++;
        for (int t : i2.data.finishedActivitiys) if (t != -1) f2++;
        return f1 < f2;
    }
};

// ── RCPSPState_TT2 ────────────────────────────────────────────────
template<>
struct AStarCompareWithF<RCPSPState_TT2> {
    bool operator()(const AStarOpenClosedDataWithF<RCPSPState_TT2>& i1,
                    const AStarOpenClosedDataWithF<RCPSPState_TT2>& i2) const {
        if (i1.f != i2.f) return i1.f > i2.f;
        // RCPSP_TT2_TBSPEC (RCPSPState.h): explicit key order among equal-f nodes. `true` = i1 is popped later.
        if (!tt2TbSpec().empty()) return tt2TbCompare(i1.data, i1.g, i2.data, i2.g) > 0;
        if (i1.g != i2.g) return i1.g < i2.g;//more

        // prefer more finished activities
        if (i1.data.finishedActivitiys.count() != i2.data.finishedActivitiys.count())
            return i1.data.finishedActivitiys.count() < i2.data.finishedActivitiys.count();

        // prefer fewer active transitions
        if (i1.data.activeTransitionIndices.size() != i2.data.activeTransitionIndices.size())
            return i1.data.activeTransitionIndices.size() < i2.data.activeTransitionIndices.size();

        // RCPSP_TT2_SYMTB=1: deterministic final tie-break — prefer the SMALLER
        // lastTransitionId (expand the canonical parent first). This makes symmetry
        // breaking remove only rediscoveries, never a first-discovery edge.
        if (g_tt2_sym_tiebreak && i1.data.lastTransitionId != i2.data.lastTransitionId)
            return i1.data.lastTransitionId > i2.data.lastTransitionId;  // smaller id popped first
        return false;
    }
};

// ── RCPSPState_BAP ────────────────────────────────────────────────
template<>
struct AStarCompareWithF<RCPSPState_BAP> {
    bool operator()(const AStarOpenClosedDataWithF<RCPSPState_BAP>& i1,
                    const AStarOpenClosedDataWithF<RCPSPState_BAP>& i2) const {
        if (i1.f != i2.f) return i1.f > i2.f;

        // prefer lower makespan
        short m1 = i1.data.start_times[g_sink_id] + RCPSPex.activities[g_sink_id].duration;
        short m2 = i2.data.start_times[g_sink_id] + RCPSPex.activities[g_sink_id].duration;
        if (m1 != m2) return m1 > m2;

        // prefer fewer conflicts
        return i1.data.rvs_activities_pool.size() > i2.data.rvs_activities_pool.size();
    }
};

// ── RCPSPState_CBS ────────────────────────────────────────────────
template<short N>
struct AStarCompareWithF<RCPSPState_CBS<N>> {
    bool operator()(const AStarOpenClosedDataWithF<RCPSPState_CBS<N>>& i1,
                    const AStarOpenClosedDataWithF<RCPSPState_CBS<N>>& i2) const {
        double f1 = i1.data.start_times[g_sink_id] + i1.h;
        double f2 = i2.data.start_times[g_sink_id] + i2.h;

        if (f1 != f2) return f1 > f2; // lower f first

        // RCPSP_CBS_TBSPEC (Globals.h): explicit key order among equal-f nodes. `true` = i1 is popped later.
        if (!g_cbs_tb_spec.empty()) {
            for (const char c : g_cbs_tb_spec) {
                long a, b;
                switch (c | 0x20) {
                    case 'g': a = i1.data.start_times[g_sink_id]; b = i2.data.start_times[g_sink_id]; break;
                    case 'd': a = i1.data.tb_depth;  b = i2.data.tb_depth;  break;
                    case 't': a = i1.data.tb_key();  b = i2.data.tb_key();  break;
                    case 's': a = i1.data.tb_sum();  b = i2.data.tb_sum();  break;
                    case 'c': a = i1.data.tb_nconf;  b = i2.data.tb_nconf;  break;
                    case 'o': a = i1.data.tb_over;   b = i2.data.tb_over;   break;
                    case 'n': a = (long)i1.data.tb_seq; b = (long)i2.data.tb_seq; break;
                    case 'x': a = i1.data.tb_x;      b = i2.data.tb_x;      break;
                    case 'k': a = i1.data.tb_k;      b = i2.data.tb_k;      break;
                    case 'm': a = i1.data.tb_m;      b = i2.data.tb_m;      break;
                    default:  continue;
                }
                if (a != b) return (c >= 'A' && c <= 'Z') ? a < b : a > b;
            }
            return false;
        }

        // tiebreak: prefer higher g (higher makespan = less h inflation)
        short g1 = i1.data.start_times[g_sink_id];
        short g2 = i2.data.start_times[g_sink_id];
        if (g1 != g2) return g1 < g2; // higher g first = lower h
        // RCPSP_CBS_TB (Globals.h): among equal f and g, prefer the schedule that is feasible further out,
        // then the deeper node. `true` = i1 is WORSE (popped later).
        if (g_cbs_tb == 1 || g_cbs_tb == 3 || g_cbs_tb == 5 || g_cbs_tb == 7) {
            const short k1 = i1.data.tb_key(), k2 = i2.data.tb_key();
            if (k1 != k2) return k1 < k2;
        }
        if (g_cbs_tb >= 2) {
            if (i1.data.tb_depth != i2.data.tb_depth) return i1.data.tb_depth < i2.data.tb_depth;
        }
        // modes 4..7: among siblings / equal depth, order by total delay (4, 5: least first; 6, 7: most first)
        if (g_cbs_tb >= 4) {
            const int s1 = i1.data.tb_sum(), s2 = i2.data.tb_sum();
            if (s1 != s2) return (g_cbs_tb <= 5) ? s1 > s2 : s1 < s2;
        }
        return i1.data.conflict_number > i2.data.conflict_number;

        return false;



        // if (i1.data.best_score != i2.data.best_score)
        //     return i1.data.best_score > i2.data.best_score;
        // return i1.data.depth < i2.data.depth;
        return i1.data.conflict_number > i2.data.conflict_number;
        return i1.g < i2.g;
    }
};

template<short N>
struct AStarCompareWithF<oldRCPSPState<N>> {
    bool operator()(const AStarOpenClosedDataWithF<oldRCPSPState<N>>& i1,
                    const AStarOpenClosedDataWithF<oldRCPSPState<N>>& i2) const {
        if (i1.f != i2.f) return i1.f > i2.f;
        if (i1.g != i2.g) return i1.g < i2.g;//more

        // prefer more started activities
        size_t s1 = 0, s2 = 0;
        for (int t : i1.data.startedActivitiys) if (t != -1) s1++;
        for (int t : i2.data.startedActivitiys) if (t != -1) s2++;
        if (s1 != s2) return s1 < s2;

        // prefer more finished activities
        size_t f1 = 0, f2 = 0;
        for (int t : i1.data.finishedActivitiys) if (t != -1) f1++;
        for (int t : i2.data.finishedActivitiys) if (t != -1) f2++;
        return f1 < f2;
    }
};
#endif // ASTARCOMPARE_H