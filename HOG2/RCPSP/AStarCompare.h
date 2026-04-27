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

        // prefer more finished activities
        if (i1.data.finishedActivitiys.count() != i2.data.finishedActivitiys.count())
            return i1.data.finishedActivitiys.count() < i2.data.finishedActivitiys.count();

        // prefer fewer active transitions
        return i1.data.activeTransitionIndices.size() > i2.data.activeTransitionIndices.size();
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
template<>
struct AStarCompareWithF<RCPSPState_CBS> {
    bool operator()(const AStarOpenClosedDataWithF<RCPSPState_CBS>& i1,
                    const AStarOpenClosedDataWithF<RCPSPState_CBS>& i2) const {
        if (i1.f != i2.f) return i1.f > i2.f;//less
        if (i1.g != i2.g) return i1.g < i2.g;//more
        // if (i1.data.best_score != i2.data.best_score)
        //     return i1.data.best_score > i2.data.best_score;
        // return i1.data.depth < i2.data.depth;
        return i1.data.conflict_number > i2.data.conflict_number;
        return i1.g < i2.g;
    }
};

#endif // ASTARCOMPARE_H