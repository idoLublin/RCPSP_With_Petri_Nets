# LBCS (Lower Bound Critical Sequence) Implementation Guide

## Overview

This document describes how to implement the **LBCS heuristic** (Lower Bound Critical Sequence) in the RCPSP solver codebase. The LBCS heuristic improves upon the basic Critical Path (CP) heuristic by considering **resource constraints** in addition to precedence constraints.

**Reference**: Stinson, J.P., Davis, E.W., & Khumawala, B.M. (1978). Multiple resource-constrained scheduling using branch and bound. As described in Coelho, J. & Vanhoucke, M. (2018), "An exact composite lower bound strategy for the resource-constrained project scheduling problem."

**Current state**: The codebase already has `HeuristicType::LBCS` in the enum and CLI parsing (`--heuristic lbcs`). Selecting LBCS currently prints a warning and falls back to CP. Your job is to implement the actual computation.

---

## 1. Algorithm Description

### Core Idea

The Critical Path (CP) lower bound only considers precedence constraints and ignores resources. LBCS extends CP by asking: **"For each non-critical task, can it actually execute within its time window given resource availability? If not, how much must the schedule extend?"**

### Steps

Given a partial state (some activities finished, some in progress, some not started):

1. **Calculate CPM (Critical Path Method)**: Compute the critical path length for all remaining (unfinished) activities. This is the baseline lower bound. The existing `getForwardHcostDP_TT` / `getForwardHcostDP` functions already do this.

2. **Compute ES/LF for remaining activities**: For each remaining activity `i`:
   - `ES_i` = Earliest Start time (from forward pass of CPM)
   - `LF_i` = Latest Finish time (from backward pass, where project deadline = CPM length)
   - `LS_i` = Latest Start = `LF_i - duration_i`
   - Time window = `[ES_i, LF_i]`
   - An activity is **critical** if `ES_i + duration_i == LF_i` (i.e., zero slack)

3. **Identify Non-Critical Tasks**: Tasks where `LF_i - ES_i > duration_i` (positive slack).

4. **Sort Non-Critical Tasks**: Sort in **descending order of duration** (longest tasks first, as they are hardest to fit).

5. **Greedy Resource Check**: For each non-critical task `i` (in sorted order):
   - Look at its time window `[ES_i, LF_i)`
   - For each resource `k` that task `i` requires:
     - Check how many time units within `[ES_i, LF_i)` have enough available capacity for task `i`'s demand
     - Let `e_i` = the maximum number of time units where task `i` **can** execute (resource is available)
   - If `e_i < duration_i`: the task cannot fit in its window
     - Extension: `a_i = duration_i - e_i`

6. **Update Lower Bound**:
   ```
   LBCS = CPM + max(a_i) over all non-critical tasks i
   ```
   If no task has a positive extension, LBCS = CPM.

### Mathematical Formulation (from Coelho & Vanhoucke 2018, Section 2.1)

For each resource type `k` with capacity `R_k`:

```
For each non-critical activity i:
  For each resource k where r_{i,k} > 0:
    e_{i,k} = number of time periods t in [ES_i, LF_i - 1] where:
              R_k - sum(r_{j,k} for all critical activities j active at time t) >= r_{i,k}
    e_i = min over all resources k of e_{i,k}
    a_i = max(0, duration_i - e_i)

LBCS = CPM + max(a_i for all non-critical i)
```

Key insight: We only check resource availability against **critical tasks** (those on the critical path). This is a conservative check that provides a valid lower bound.

---

## 2. Codebase Integration Points

### Files to modify

| File | What to add |
|------|-------------|
| `HOG2/RCPSP/RCPSPState.cpp` | New functions: `computeLBCS()`, `computeLBCS_TT()` |
| `HOG2/RCPSP/RCPSP.h` | Forward declarations for new functions, wire into `HCost` |
| `HOG2/RCPSP/Driver.cpp` | Remove the warning fallback for LBCS (lines ~487-492) |

### Key data structures you'll use

**Problem data** (global, thread-local in `RCPSPState.cpp`):
```cpp
// RCPSPState.cpp:53-54
thread_local PetriExample petri;    // petri.Transitions = vector of Transition objects
thread_local RCPSP_example RCPSPex; // The RCPSP problem instance
```

**Activity info** (`petriclasses.h`):
```cpp
RCPSPex.activities[i]                   // Activity object (0-indexed, activity IDs are 1-based)
RCPSPex.activities[i].duration           // short: task duration
RCPSPex.activities[i].resource_demands   // map<string, short>: e.g., {"R1": 3, "R2": 1}
RCPSPex.backword_dependencies[i]         // vector<short>: predecessor IDs (1-based) of activity i+1
RCPSPex.dependencies[i]                  // vector<short>: successor IDs (1-based) of activity i+1
RCPSPex.resources                        // vector<pair<string, short>>: e.g., [("R1", 10), ("R2", 8), ...]
```

**DP heuristic cache** (`RCPSPState.cpp:61-62`):
```cpp
thread_local std::vector<int> heuristicDP;  // heuristicDP[activityId] = earliest finish time (1-based)
thread_local bool heuristicDPInitialized;
```
- `heuristicDP` is populated by `initializeHeuristicDP()` (lines 68-99). This gives you ES for the full (initial) problem. For a partial state, you need to recompute ES relative to remaining activities (or reuse the DP logic).

**State (TP method)** (`RCPSPState.h`):
```cpp
class RCPSPState {
    vector<short> marking;
    vector<pair<short, short>> activeTransitionIndices;  // (transitionID, remainingDuration)
    array<short, 128> startedActivitiys;   // -1 = not started
    array<short, 128> finishedActivitiys;  // -1 = not finished, else = finish time
    short g;  // current time
    mutable short h;  // heuristic value (cached)
};
```

**State (TT method)** (`RCPSPState.h`):
```cpp
class RCPSPState_TT {
    array<vector<pair<short, short>>, 4> resource_nodes;  // per resource: (activityID, remainingDuration)
    vector<pair<short, short>> activity_nodes;  // (activityID, remainingDuration)
    array<short, 128> finishedActivitiys;  // -1 = not finished
    short g;  // current time
};
```

**Existing heuristic entry points** (`RCPSP.h`):
- `RCPSP::HCost` (line 248): TP method - builds `tempUnstarted` list, calls `getForwardHcostDP` or `getForwardHcost`
- `RCPSP_TT::HCost` (line 610): TT method - same pattern, calls `getForwardHcostDP_TT` or `getForwardHcost_TT`

**Heuristic type check** (`Driver.cpp:35`, `RCPSP.h:27`):
```cpp
extern P_RCPSP::HeuristicType activeHeuristic;  // Driver.cpp:35
extern bool useDPHeuristic;                       // Driver.cpp:38
```

### Constants

```cpp
constexpr int MAX_ACTIVITIES = 128;  // RCPSPState.h:14
// Activity IDs are 1-based (1 to N). Index into arrays/vectors as [activityId] or [activityId-1].
// Resources are typically 4 for j30 problems: R1, R2, R3, R4
```

---

## 3. Pseudocode

```
function computeLBCS(unstartedActivities, activeTransitions, state_g):
    N = total number of activities

    // === STEP 1: Forward pass (ES computation) for remaining activities ===
    // Similar to getForwardHcostDP but we need ES (start) and EF (finish) separately

    ES[1..N] = -1   // -1 means finished/irrelevant
    EF[1..N] = -1

    // For active transitions, their "remaining work" starts now (time 0 relative to current state)
    for each (actId, remainingDur) in activeTransitions:
        EF[actId] = remainingDur
        ES[actId] = 0  // already started

    // Process unstarted activities in topological order
    for each actId in unstartedActivities (in ascending order = topological):
        if actId is active: continue  // already handled

        maxPredFinish = 0
        for each predecessor dep of actId:
            if EF[dep] != -1:
                maxPredFinish = max(maxPredFinish, EF[dep])

        ES[actId] = maxPredFinish
        EF[actId] = maxPredFinish + duration[actId]

    CPM = max(EF[actId] for all remaining actId)

    // === STEP 2: Backward pass (LF computation) ===
    LF[1..N] = -1

    // Process in reverse topological order (descending ID)
    for each actId in unstartedActivities (in descending order):
        minSuccStart = CPM   // default: can finish at project end
        for each successor succ of actId:
            if ES[succ] != -1:  // succ is remaining
                // LF of actId must be <= ES[succ] in the latest schedule
                // But for LF computation: LF[actId] = min(LF[succ] - duration[succ] + duration[succ])
                // Actually: LF[actId] = min over successors of (LF[succ] - duration[succ])
                // Wait - standard backward pass: LF[actId] = min over successors of LS[succ]
                // where LS[succ] = LF[succ] - duration[succ]
                if LF[succ] != -1:
                    minSuccStart = min(minSuccStart, LF[succ] - duration[succ])

        LF[actId] = minSuccStart  // This is actually LS of successors = LF of current
        // Correction: LF[actId] should equal the minimum LS of all successors
        // But if actId has no successors, LF[actId] = CPM

    // Fix: Proper backward pass
    // LF[actId] = min over all successors s of (LF[s] - duration[s])   ... no
    // Standard: LF[actId] = min over successors s of LS[s]
    //           LS[s] = LF[s] - duration[s]
    // For the last activity (sink): LF[sink] = CPM, LS[sink] = CPM - duration[sink]

    // Correct backward pass:
    for each actId in reverse topological order:
        LF[actId] = CPM  // default
        for each successor succ of actId:
            if LF[succ] != -1:
                LS_succ = LF[succ] - duration[succ]
                LF[actId] = min(LF[actId], LS_succ)
        // Now: LF[actId] is the latest time actId can FINISH
        // Wait - that gives us the latest START of actId
        // Let me re-derive:
        //   LS[actId] = LF[actId] - duration[actId]
        //   LF[actId] = min over successors of (LS[succ]) = min over succ of (LF[succ] - dur[succ])
        // Hmm, that's not right either. Standard CPM backward pass:
        //   LF[sink] = CPM
        //   LF[actId] = min over successors succ of ES[succ]  ... no
        //
        // CORRECT standard backward pass:
        //   LF[last] = CPM
        //   For each activity in reverse topological order:
        //     if activity has successors:
        //       LF[act] = min over successors s of (LF[s] - duration[s])  <<< THIS IS LS[s]
        //     else:
        //       LF[act] = CPM
        //   LS[act] = LF[act] - duration[act]
    // ACTUALLY the standard is:
    //   LF[act] = min over successors s of LS[s]
    //   LS[act] = LF[act] - duration[act]
    // Where LS[s] = LF[s] - duration[s]
    // So: LF[act] = min over s of (LF[s] - duration[s])

    // I'll use the correct version in the implementation below.

    // === STEP 3: Identify critical vs non-critical ===
    slack[actId] = LF[actId] - EF[actId]   // total float
    // Critical if slack == 0

    criticalSet = {actId : slack[actId] == 0}
    nonCriticalSet = {actId : slack[actId] > 0 and actId is not active}

    // === STEP 4: Sort non-critical by duration descending ===
    sort nonCriticalSet by duration[actId] descending

    // === STEP 5: Compute resource profile of critical activities ===
    // For each time t in [0, CPM), compute resource usage by critical activities
    // resourceUsage[k][t] = sum of r_{j,k} for all critical activities j where ES[j] <= t < EF[j]

    for each resource k:
        for t = 0 to CPM-1:
            resourceUsage[k][t] = 0
            for each critical activity j:
                if ES[j] <= t < EF[j]:
                    resourceUsage[k][t] += demand[j][k]

    // === STEP 6: Greedy extension check ===
    maxExtension = 0

    for each non-critical activity i (sorted by duration desc):
        e_i = infinity  // available execution slots

        for each resource k where demand[i][k] > 0:
            available_slots = 0
            for t = ES[i] to LF[i] - 1:
                remaining_capacity = capacity[k] - resourceUsage[k][t]
                if remaining_capacity >= demand[i][k]:
                    available_slots += 1
            e_i = min(e_i, available_slots)

        if e_i == infinity:  // no resource demands (shouldn't happen, but just in case)
            e_i = duration[i]

        a_i = max(0, duration[i] - e_i)
        maxExtension = max(maxExtension, a_i)

    // === RESULT ===
    return CPM + maxExtension
```

---

## 4. C++ Implementation Skeleton

Add the following to `RCPSPState.cpp`:

```cpp
#include "HeuristicTypes.h"

// Global heuristic type (defined in Driver.cpp)
extern P_RCPSP::HeuristicType activeHeuristic;

// ============================================================================
// LBCS: Lower Bound Critical Sequence
// ============================================================================

/**
 * Compute LBCS for TT method.
 *
 * @param unstartedTransitions  Activity IDs (1-based) that are not yet finished
 * @return  LBCS lower bound value
 */
double computeLBCS_TT(const std::vector<short>& unstartedTransitions) {
    if (unstartedTransitions.empty()) return 0;

    int n = RCPSPex.activities.size(); // total activities

    // --- Step 1: Forward pass (ES, EF) for remaining activities ---
    std::array<int, MAX_ACTIVITIES> ES, EF;
    ES.fill(-1);
    EF.fill(-1);

    // Process in topological order (ascending ID)
    for (short actId : unstartedTransitions) {
        int maxPredFinish = 0;
        for (int dep : RCPSPex.backword_dependencies[actId - 1]) {
            if (EF[dep] != -1) {
                maxPredFinish = std::max(maxPredFinish, EF[dep]);
            }
            // Finished predecessors: contribute 0 (already done)
        }
        ES[actId] = maxPredFinish;
        EF[actId] = maxPredFinish + RCPSPex.activities[actId - 1].duration;
    }

    // CPM = max earliest finish
    int CPM = 0;
    for (short actId : unstartedTransitions) {
        CPM = std::max(CPM, EF[actId]);
    }

    if (CPM == 0) return 0;

    // --- Step 2: Backward pass (LF) ---
    std::array<int, MAX_ACTIVITIES> LF;
    LF.fill(-1);

    // Process in reverse topological order (descending ID)
    // unstartedTransitions is in ascending order, so iterate backwards
    for (int idx = (int)unstartedTransitions.size() - 1; idx >= 0; idx--) {
        short actId = unstartedTransitions[idx];
        int minSuccLS = CPM; // default: latest finish = project deadline

        // Check all successors
        for (int succ : RCPSPex.dependencies[actId - 1]) {
            if (LF[succ] != -1) {
                int LS_succ = LF[succ] - RCPSPex.activities[succ - 1].duration;
                minSuccLS = std::min(minSuccLS, LS_succ);
            }
        }

        LF[actId] = minSuccLS;
    }

    // --- Step 3: Identify critical vs non-critical ---
    std::vector<short> criticalActivities;
    std::vector<short> nonCriticalActivities;

    for (short actId : unstartedTransitions) {
        int slack = LF[actId] - EF[actId];
        if (slack == 0) {
            criticalActivities.push_back(actId);
        } else if (slack > 0) {
            nonCriticalActivities.push_back(actId);
        }
    }

    if (nonCriticalActivities.empty()) {
        return CPM; // All critical, no extension possible
    }

    // --- Step 4: Sort non-critical by duration descending ---
    std::sort(nonCriticalActivities.begin(), nonCriticalActivities.end(),
              [](short a, short b) {
                  return RCPSPex.activities[a - 1].duration > RCPSPex.activities[b - 1].duration;
              });

    // --- Step 5: Build resource usage profile of critical activities ---
    // Resource indices: use RCPSPex.resources which is vector<pair<string, short>>
    // resourceUsage[k][t] = usage at time t for resource k
    int numResources = RCPSPex.resources.size();

    // Use a 2D vector: resourceUsage[resourceIdx][time]
    std::vector<std::vector<int>> resourceUsage(numResources, std::vector<int>(CPM, 0));

    for (short actId : criticalActivities) {
        int es = ES[actId];
        int ef = EF[actId];
        for (int k = 0; k < numResources; k++) {
            const std::string& resName = RCPSPex.resources[k].first;
            auto it = RCPSPex.activities[actId - 1].resource_demands.find(resName);
            if (it != RCPSPex.activities[actId - 1].resource_demands.end() && it->second > 0) {
                for (int t = es; t < ef && t < CPM; t++) {
                    resourceUsage[k][t] += it->second;
                }
            }
        }
    }

    // --- Step 6: Greedy extension check ---
    int maxExtension = 0;

    for (short actId : nonCriticalActivities) {
        int dur = RCPSPex.activities[actId - 1].duration;
        int es = ES[actId];
        int lf = LF[actId];

        int minAvailableSlots = INT_MAX; // min over all resources

        for (int k = 0; k < numResources; k++) {
            const std::string& resName = RCPSPex.resources[k].first;
            auto it = RCPSPex.activities[actId - 1].resource_demands.find(resName);
            if (it == RCPSPex.activities[actId - 1].resource_demands.end() || it->second == 0) {
                continue; // No demand for this resource
            }

            int demand = it->second;
            int capacity = RCPSPex.resources[k].second;

            int availableSlots = 0;
            for (int t = es; t < lf && t < CPM; t++) {
                int remainingCap = capacity - resourceUsage[k][t];
                if (remainingCap >= demand) {
                    availableSlots++;
                }
            }

            minAvailableSlots = std::min(minAvailableSlots, availableSlots);
        }

        if (minAvailableSlots == INT_MAX) {
            minAvailableSlots = dur; // No resource constraints
        }

        int extension = std::max(0, dur - minAvailableSlots);
        maxExtension = std::max(maxExtension, extension);
    }

    return CPM + maxExtension;
}

/**
 * Compute LBCS for TP method (similar but accounts for active transitions).
 */
double computeLBCS(const std::vector<short>& unstartedTransitions,
                   const std::vector<std::pair<short, short>>& activeTransitionIndices) {
    // Same algorithm as computeLBCS_TT but active transitions
    // have remaining durations instead of full durations.
    //
    // For active transitions:
    //   ES[actId] = 0 (already started)
    //   EF[actId] = remainingDuration
    //
    // The rest of the algorithm is identical.

    if (unstartedTransitions.empty()) return 0;

    // Build active transition lookup
    std::array<short, MAX_ACTIVITIES> activeRemaining;
    activeRemaining.fill(-1);
    for (const auto& [transIdx, remaining] : activeTransitionIndices) {
        activeRemaining[transIdx] = remaining;
    }

    // --- Forward pass ---
    std::array<int, MAX_ACTIVITIES> ES, EF;
    ES.fill(-1);
    EF.fill(-1);

    for (short actId : unstartedTransitions) {
        if (activeRemaining[actId] != -1) {
            // Active: already started, remaining time only
            ES[actId] = 0;
            EF[actId] = activeRemaining[actId];
            continue;
        }

        int maxPredFinish = 0;
        for (int dep : RCPSPex.backword_dependencies[actId - 1]) {
            if (EF[dep] != -1) {
                maxPredFinish = std::max(maxPredFinish, EF[dep]);
            }
            else if (activeRemaining[dep] != -1) {
                maxPredFinish = std::max(maxPredFinish, (int)activeRemaining[dep]);
            }
        }
        ES[actId] = maxPredFinish;
        EF[actId] = maxPredFinish + RCPSPex.activities[actId - 1].duration;
    }

    int CPM = 0;
    for (short actId : unstartedTransitions) {
        CPM = std::max(CPM, EF[actId]);
    }

    if (CPM == 0) return 0;

    // --- Backward pass ---
    std::array<int, MAX_ACTIVITIES> LF;
    LF.fill(-1);

    for (int idx = (int)unstartedTransitions.size() - 1; idx >= 0; idx--) {
        short actId = unstartedTransitions[idx];
        int minSuccLS = CPM;

        for (int succ : RCPSPex.dependencies[actId - 1]) {
            if (LF[succ] != -1) {
                int dur_succ = (activeRemaining[succ] != -1)
                    ? activeRemaining[succ]
                    : RCPSPex.activities[succ - 1].duration;
                int LS_succ = LF[succ] - dur_succ;
                minSuccLS = std::min(minSuccLS, LS_succ);
            }
        }

        LF[actId] = minSuccLS;
    }

    // --- Identify critical vs non-critical ---
    std::vector<short> criticalActivities;
    std::vector<short> nonCriticalActivities;

    for (short actId : unstartedTransitions) {
        int slack = LF[actId] - EF[actId];
        if (slack == 0) {
            criticalActivities.push_back(actId);
        } else if (slack > 0) {
            nonCriticalActivities.push_back(actId);
        }
    }

    if (nonCriticalActivities.empty()) return CPM;

    // --- Sort non-critical by duration descending ---
    std::sort(nonCriticalActivities.begin(), nonCriticalActivities.end(),
              [&activeRemaining](short a, short b) {
                  int durA = (activeRemaining[a] != -1) ? activeRemaining[a] : RCPSPex.activities[a - 1].duration;
                  int durB = (activeRemaining[b] != -1) ? activeRemaining[b] : RCPSPex.activities[b - 1].duration;
                  return durA > durB;
              });

    // --- Resource profile + extension check (same as TT version) ---
    int numResources = RCPSPex.resources.size();
    std::vector<std::vector<int>> resourceUsage(numResources, std::vector<int>(CPM, 0));

    for (short actId : criticalActivities) {
        int es = ES[actId];
        int ef = EF[actId];
        for (int k = 0; k < numResources; k++) {
            const std::string& resName = RCPSPex.resources[k].first;
            auto it = RCPSPex.activities[actId - 1].resource_demands.find(resName);
            if (it != RCPSPex.activities[actId - 1].resource_demands.end() && it->second > 0) {
                for (int t = es; t < ef && t < CPM; t++) {
                    resourceUsage[k][t] += it->second;
                }
            }
        }
    }

    int maxExtension = 0;
    for (short actId : nonCriticalActivities) {
        int dur = (activeRemaining[actId] != -1) ? activeRemaining[actId] : RCPSPex.activities[actId - 1].duration;
        int es = ES[actId];
        int lf = LF[actId];

        int minAvailableSlots = INT_MAX;

        for (int k = 0; k < numResources; k++) {
            const std::string& resName = RCPSPex.resources[k].first;
            auto it = RCPSPex.activities[actId - 1].resource_demands.find(resName);
            if (it == RCPSPex.activities[actId - 1].resource_demands.end() || it->second == 0) continue;

            int demand = it->second;
            int capacity = RCPSPex.resources[k].second;

            int availableSlots = 0;
            for (int t = es; t < lf && t < CPM; t++) {
                if (capacity - resourceUsage[k][t] >= demand) availableSlots++;
            }

            minAvailableSlots = std::min(minAvailableSlots, availableSlots);
        }

        if (minAvailableSlots == INT_MAX) minAvailableSlots = dur;

        int extension = std::max(0, dur - minAvailableSlots);
        maxExtension = std::max(maxExtension, extension);
    }

    return CPM + maxExtension;
}
```

---

## 5. Wiring Into HCost

### In `RCPSP.h`, modify `RCPSP::HCost` (line ~248):

Add forward declarations at the top of the file (near line 22):
```cpp
// Forward declarations for LBCS heuristic
double computeLBCS(const std::vector<short>& unstartedTransitions,
                   const std::vector<std::pair<short, short>>& activeTransitionIndices);
double computeLBCS_TT(const std::vector<short>& unstartedTransitions);
```

Then modify the HCost functions to branch on `activeHeuristic`:

```cpp
// In RCPSP::HCost (TP method), after building tempUnstarted:
#include "HeuristicTypes.h"
extern P_RCPSP::HeuristicType activeHeuristic;

// Replace the existing heuristic call block with:
if (activeHeuristic == P_RCPSP::HeuristicType::LBCS) {
    state1.h = computeLBCS(tempUnstarted, state1.activeTransitionIndices);
} else {
    // Critical Path (default)
    if (useDPHeuristic) {
        state1.h = getForwardHcostDP(tempUnstarted, state1.activeTransitionIndices);
    } else {
        state1.h = getForwardHcost(tempUnstarted, state1.activeTransitionIndices);
    }
}
```

Similarly for `RCPSP_TT::HCost` (line ~610):
```cpp
if (activeHeuristic == P_RCPSP::HeuristicType::LBCS) {
    return computeLBCS_TT(tempUnstarted);
    // Note: TT HCost also has the independent set / unkTime logic.
    // You may want to apply LBCS only to the final return value,
    // or rethink how LBCS interacts with the independent set optimization.
    // Simplest approach: replace the entire return with computeLBCS_TT.
} else {
    // ... existing CP logic with DP branching ...
}
```

### In `Driver.cpp`, remove the warning:

Delete or comment out lines ~487-492:
```cpp
// Remove this block:
if (activeHeuristic != P_RCPSP::HeuristicType::CRITICAL_PATH) {
    std::cerr << "WARNING: Heuristic '"
              << P_RCPSP::heuristicTypeToString(activeHeuristic)
              << "' is not yet implemented. Falling back to Critical Path (CP).\n";
}
```

---

## 6. Important Notes

### Activity ID Convention
- Activities are **1-based** (ID 1 to N)
- `RCPSPex.activities` is **0-indexed** (activity ID `i` is at index `i-1`)
- `RCPSPex.backword_dependencies[i]` gives predecessors of activity `i+1`
- `RCPSPex.dependencies[i]` gives successors of activity `i+1`
- `finishedActivitiys[actId]` uses 1-based indexing directly

### Dependencies Direction
- `backword_dependencies[i]` = predecessors of activity `i+1` (used in forward pass)
- `dependencies[i]` = successors of activity `i+1` (used in backward pass)

### Performance Considerations
- The resource usage profile is `O(numResources * CPM)` in memory
- The extension check is `O(numNonCritical * numResources * CPM)` in time
- For j30 problems (32 activities, 4 resources, CPM ~50-100), this is fast
- For j120 problems (122 activities), consider optimizing if too slow
- The function is called for **every node** expanded by A*, so it must be efficient

### Admissibility
- LBCS is **admissible** (never overestimates) because:
  - CPM is a valid lower bound (ignoring resources can only underestimate)
  - The extension `max(a_i)` only counts the minimum amount a single task must extend beyond the critical path due to resource conflicts
  - We only check against critical tasks' resource usage, which is a subset of all resource usage

### DP Integration
- The `--dp` / `--no-dp` flag currently toggles between `getForwardHcostDP` and `getForwardHcost`
- For LBCS, the DP distinction is less relevant since LBCS does its own forward/backward pass
- You can ignore the DP flag for LBCS, or use the DP table to speed up the forward pass

---

## 7. Testing Plan

### Unit Test Approach
1. **Compile**: `clang++ -std=c++17 -O3 -march=native -DNDEBUG -flto HOG2/RCPSP/Driver.cpp -o build/Driver`
2. **Run single problem**: `./build/Driver --group-start 10 --group-end 10 --exam-start 8 --exam-end 8 --method tt --heuristic lbcs`
3. **Verify**: The solver should find the optimal makespan (validated automatically against `data/j30opt.sm`)

### Validation Strategy
- LBCS >= CP for every state (it's at least as good as CP)
- LBCS <= optimal makespan (admissibility)
- Run all 480 j30 instances: `./build/Driver --group-start 1 --group-end 48 --method tt --heuristic lbcs --tag lbcs_test`
- Compare nodes expanded: LBCS should expand **fewer or equal** nodes than CP
- All solved instances must match optimal makespans in `data/j30opt.sm`

### Debugging Tips
- Add `std::cerr` output for a specific problem to print ES, EF, LF, slack, critical set, extensions
- Compare LBCS value against CP value for the initial state — LBCS should be >= CP
- If a problem fails validation, LBCS is overestimating (not admissible) — check the algorithm

---

## 8. LBCC (Future Work)

The **LBCC** (Lower Bound Critical Capacity) heuristic is more complex. From Coelho & Vanhoucke (2018):

- LBCC simulates "work arrival" at each time period using earliest start times
- It checks if the work content arriving at time `t` can be processed within the remaining time window
- Formula involves cumulative work content and resource capacity

This can be implemented after LBCS is working and validated. The same integration pattern applies: add `computeLBCC_TT` / `computeLBCC` functions and wire them into HCost with an `activeHeuristic == HeuristicType::LBCC` branch.
