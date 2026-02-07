# Critical Capacity Lower Bound (LBcc) Implementation Guide

This document provides a comprehensive guide for implementing the Critical Capacity Lower Bound (LBcc) algorithm in the RCPSP solver codebase.

## Table of Contents

1. [Algorithm Overview](#algorithm-overview)
2. [Codebase Architecture](#codebase-architecture)
3. [Integration Strategy](#integration-strategy)
4. [Using the Existing CPM Algorithm](#using-the-existing-cpm-algorithm)
5. [Making Heuristics Configurable](#making-heuristics-configurable)
6. [Expanding Pre-computation](#expanding-pre-computation)
7. [Implementation Steps](#implementation-steps)
8. [Testing and Validation](#testing-and-validation)

---

## Algorithm Overview

The Critical Capacity Lower Bound (LBcc) merges two fundamental constraints:
- **Network precedence constraints** (via Critical Path Method - EST values)
- **Resource capacity constraints** (via Work Content consumption model)

### Core Concept: The Tank Model

The algorithm models each resource as a "tank" that:
1. **Fills** when activities become available (at their EST)
2. **Drains** at a constant rate equal to resource capacity
3. **Cannot go negative** (idle time cannot be banked)

### Mathematical Formulation

```
For each resource k:
    LB_k = FinalTime + (RemainingWorkContent / Capacity_k)

LBcc = ceil(max(LB_k for all k))
```

---

## Codebase Architecture

### Key Files and Their Roles

| File | Purpose | Relevant Functions |
|------|---------|-------------------|
| `RCPSPState.cpp` | Heuristic computations, lower bounds | `initializeHeuristicDP()`, `getForwardHcostDP()`, existing LB functions |
| `RCPSPState.h` | State definitions | `RCPSPState`, `RCPSPState_TT`, `MAX_ACTIVITIES` |
| `RCPSP.h` | Environment classes | `RCPSP::HCost()`, `RCPSP_TT::HCost()` |
| `petriclasses.h` | Data structures | `Activity`, `RCPSP_example` (resources, dependencies) |
| `Driver.cpp` | Configuration, main loop | `Config`, `useCS` flag, `runSolver()` |

### Global Data Structures

```cpp
// Thread-local storage (RCPSPState.cpp)
thread_local PetriExample petri;           // Petri net structure
thread_local RCPSP_example RCPSPex;        // RCPSP problem data

// Heuristic cache
thread_local std::vector<int> heuristicDP; // Earliest finish times
thread_local bool heuristicDPInitialized;  // Initialization flag

// Configuration
bool useCS;                                 // Global heuristic flag
```

### Existing Lower Bound Functions (RCPSPState.cpp)

```cpp
double computeResourceCapacityLowerBound(...)   // Lines 1048-1088
double computeWorkloadLowerBoundWithMax(...)    // Lines 970-1045
double computeSequenceLowerBoundWithMax(...)    // Lines 878-968
double computeCoreTimeLowerBoundWithMax(...)    // Lines 778-876
```

---

## Integration Strategy

### Overview

The LBcc algorithm will be integrated as a new lower bound function that:
1. **Reuses** the existing CPM computation (`initializeHeuristicDP()`)
2. **Accesses** resource data from `RCPSPex.resources` and activity data from `RCPSPex.activities`
3. **Integrates** into the heuristic selection system via the `useCS` mechanism

### Integration Points

```
Driver.cpp (Config)
    |
    v
RCPSPState.cpp (LBcc computation)
    |
    +---> initializeHeuristicDP()     [EST calculation - existing]
    +---> initializeLBcc()            [NEW: Pre-compute LBcc]
    +---> computeCriticalCapacityLB() [NEW: Runtime computation]
    |
    v
RCPSP.h (HCost methods)
    |
    +---> RCPSP::HCost()      [Use LBcc in TP method]
    +---> RCPSP_TT::HCost()   [Use LBcc in TT method]
```

---

## Using the Existing CPM Algorithm

### Current CPM Implementation

The CPM algorithm is already implemented in `initializeHeuristicDP()` (lines 110-141):

```cpp
void initializeHeuristicDP() {
    int n = RCPSPex.activities.size();
    heuristicDP.assign(n + 1, 0);  // 1-based indexing

    // Forward pass: Calculate earliest finish time for each activity
    for (int activityId = 1; activityId <= n; activityId++) {
        int maxPredecessorFinish = 0;

        // Check all predecessors (backward dependencies)
        for (int dep : RCPSPex.backword_dependencies[activityId - 1]) {
            int predecessorFinish = heuristicDP[dep];
            maxPredecessorFinish = std::max(maxPredecessorFinish, predecessorFinish);
        }

        // Earliest finish = max predecessor finish + own duration
        int duration = RCPSPex.activities[activityId - 1].duration;
        heuristicDP[activityId] = maxPredecessorFinish + duration;
    }

    heuristicDPInitialized = true;
}
```

### Extracting EST for LBcc

The existing `heuristicDP[]` array stores **Earliest Finish Times (EFT)**. To get **Earliest Start Times (EST)** for LBcc:

```cpp
// EST[i] = EFT[i] - duration[i]
int getEST(int activityId) {
    return heuristicDP[activityId] - RCPSPex.activities[activityId - 1].duration;
}
```

### Creating a Dedicated EST Array

For LBcc, create a companion array to store EST values:

```cpp
// Add to RCPSPState.cpp global section
thread_local std::vector<int> estValues;  // Earliest start times
thread_local bool estInitialized = false;

void initializeEST() {
    if (!heuristicDPInitialized) {
        initializeHeuristicDP();
    }

    int n = RCPSPex.activities.size();
    estValues.assign(n + 1, 0);

    for (int i = 1; i <= n; i++) {
        estValues[i] = heuristicDP[i] - RCPSPex.activities[i - 1].duration;
    }

    estInitialized = true;
}
```

---

## Making Heuristics Configurable

### Current Configuration System

The codebase uses a hierarchical configuration system:

1. **Command-line flags** (highest priority)
2. **Environment variables** (medium priority)
3. **Hardcoded defaults** (lowest priority)

### Existing `useCS` Flag

Currently, there's a single `useCS` flag (Driver.cpp:52, RCPSPState.cpp:31):

```cpp
// Driver.cpp Config struct
struct Config {
    bool useCS = true;  // Enable CS optimization
    // ...
};

// RCPSPState.cpp global
bool useCS;

// Set in runSolver()
useCS = config.useCS;
```

### Proposed Enhancement: Heuristic Selection Enum

Replace the boolean flag with an enumeration for fine-grained control:

```cpp
// ============== ADD TO RCPSPState.h ==============
enum class HeuristicType {
    CPM_ONLY,           // Critical Path Method only (baseline)
    RESOURCE_CAPACITY,  // LBrc - simple resource capacity
    WORKLOAD,           // Workload-based lower bound
    SEQUENCE,           // Sequence-based lower bound
    CORE_TIME,          // Core time lower bound
    CRITICAL_CAPACITY,  // NEW: LBcc algorithm
    COMPOSITE_ALL       // Max of all available bounds
};

// ============== ADD TO Driver.cpp Config ==============
struct Config {
    // ... existing fields ...

    // Replace useCS with heuristic selection
    HeuristicType heuristic = HeuristicType::COMPOSITE_ALL;

    // Individual toggles for composite mode
    bool useCPM = true;
    bool useLBrc = true;
    bool useLBcc = true;      // NEW
    bool useWorkload = false;  // Optional, expensive
    bool useSequence = false;  // Optional, expensive
};
```

### Command-Line Flags for Heuristic Selection

```cpp
// Add to parseArgs() in Driver.cpp
else if (arg == "--heuristic" && i + 1 < argc) {
    std::string h = argv[++i];
    if (h == "cpm") config.heuristic = HeuristicType::CPM_ONLY;
    else if (h == "lbrc") config.heuristic = HeuristicType::RESOURCE_CAPACITY;
    else if (h == "lbcc") config.heuristic = HeuristicType::CRITICAL_CAPACITY;
    else if (h == "workload") config.heuristic = HeuristicType::WORKLOAD;
    else if (h == "sequence") config.heuristic = HeuristicType::SEQUENCE;
    else if (h == "all") config.heuristic = HeuristicType::COMPOSITE_ALL;
}
else if (arg == "--enable-lbcc") config.useLBcc = true;
else if (arg == "--disable-lbcc") config.useLBcc = false;
```

### Environment Variable Support

```cpp
// Add to parseArgs() environment variable section
config.useLBcc = getEnvOrDefault("RCPSP_USE_LBCC", config.useLBcc);

std::string heuristicEnv = getEnvOrDefault("RCPSP_HEURISTIC", "");
if (!heuristicEnv.empty()) {
    // Parse heuristic type from environment
}
```

---

## Expanding Pre-computation

### Current Pre-computation

The codebase currently pre-computes:
- `heuristicDP[]`: Earliest finish times (CPM forward pass)

### Proposed Pre-computation Additions

Add pre-computed values for LBcc efficiency:

```cpp
// ============== ADD TO RCPSPState.cpp ==============

// Pre-computed LBcc values
thread_local std::vector<int> estValues;              // EST for each activity
thread_local std::map<int, std::vector<int>> estToActivities;  // EST -> activity IDs
thread_local std::vector<int> sortedESTs;             // Unique EST values, sorted
thread_local std::map<std::string, double> workContentByResource;  // Total work per resource
thread_local double precomputedLBcc = 0.0;            // Initial LBcc (all activities unfinished)
thread_local bool lbccInitialized = false;

// ============== INITIALIZATION FUNCTION ==============
void initializeLBcc() {
    if (!heuristicDPInitialized) {
        initializeHeuristicDP();
    }

    int n = RCPSPex.activities.size();

    // Step 1: Compute EST values
    estValues.assign(n + 1, 0);
    estToActivities.clear();

    for (int i = 1; i <= n; i++) {
        int est = heuristicDP[i] - RCPSPex.activities[i - 1].duration;
        estValues[i] = est;
        estToActivities[est].push_back(i);
    }

    // Step 2: Sort unique EST values
    sortedESTs.clear();
    for (const auto& [est, _] : estToActivities) {
        sortedESTs.push_back(est);
    }
    std::sort(sortedESTs.begin(), sortedESTs.end());

    // Step 3: Pre-compute total work content per resource
    workContentByResource.clear();
    for (int i = 0; i < n; i++) {
        const auto& activity = RCPSPex.activities[i];
        for (const auto& [resName, demand] : activity.resource_demands) {
            workContentByResource[resName] += demand * activity.duration;
        }
    }

    // Step 4: Compute initial LBcc (full problem)
    precomputedLBcc = computeCriticalCapacityLB_Full();

    lbccInitialized = true;
}
```

### Pre-computation Summary Table

| Variable | Type | Purpose | Computed When |
|----------|------|---------|---------------|
| `heuristicDP[]` | `vector<int>` | Earliest finish times | Problem load |
| `estValues[]` | `vector<int>` | Earliest start times | Problem load |
| `estToActivities` | `map<int, vector<int>>` | Activities at each EST | Problem load |
| `sortedESTs` | `vector<int>` | Unique EST values sorted | Problem load |
| `workContentByResource` | `map<string, double>` | Total work per resource | Problem load |
| `precomputedLBcc` | `double` | Initial LBcc bound | Problem load |

---

## Implementation Steps

### Step 1: Add LBcc Data Structures

Add to `RCPSPState.cpp` after line 104:

```cpp
// ============================================================================
// LBcc Pre-computation Cache
// ============================================================================
thread_local std::vector<int> estValues;
thread_local std::map<int, std::vector<int>> estToActivities;
thread_local std::vector<int> sortedESTs;
thread_local std::map<std::string, double> workContentByResource;
thread_local double precomputedLBcc = 0.0;
thread_local bool lbccInitialized = false;
```

### Step 2: Implement LBcc Initialization

Add the initialization function after `initializeHeuristicDP()`:

```cpp
// ============================================================================
// LBcc Initialization
// ============================================================================
void initializeLBcc() {
    if (!heuristicDPInitialized) {
        initializeHeuristicDP();
    }
    if (lbccInitialized) return;

    int n = RCPSPex.activities.size();

    // Compute EST values and build EST -> activities mapping
    estValues.assign(n + 1, 0);
    estToActivities.clear();

    for (int i = 1; i <= n; i++) {
        int duration = RCPSPex.activities[i - 1].duration;
        int est = heuristicDP[i] - duration;
        estValues[i] = est;
        estToActivities[est].push_back(i);
    }

    // Sort unique EST values
    sortedESTs.clear();
    sortedESTs.reserve(estToActivities.size());
    for (const auto& [est, _] : estToActivities) {
        sortedESTs.push_back(est);
    }
    std::sort(sortedESTs.begin(), sortedESTs.end());

    // Pre-compute total work content per resource
    workContentByResource.clear();
    for (int i = 0; i < n; i++) {
        const auto& activity = RCPSPex.activities[i];
        for (const auto& [resName, demand] : activity.resource_demands) {
            workContentByResource[resName] +=
                static_cast<double>(demand) * activity.duration;
        }
    }

    lbccInitialized = true;
}
```

### Step 3: Implement the Core LBcc Algorithm

```cpp
// ============================================================================
// Critical Capacity Lower Bound (LBcc)
// ============================================================================

/**
 * Compute LBcc for the full problem (all activities unfinished).
 * Called once during initialization.
 */
double computeCriticalCapacityLB_Full() {
    if (!lbccInitialized && !heuristicDPInitialized) {
        initializeLBcc();
    }

    double finalLB = 0.0;

    // Process each resource type
    for (const auto& [resName, capacity] : RCPSPex.resources) {
        double workContentTank = 0.0;
        int currentTime = 0;

        // Process each EST event in order
        for (int est : sortedESTs) {
            // Step 1: Drain the tank (outflow)
            int deltaT = est - currentTime;
            workContentTank -= deltaT * capacity;

            // Step 2: Apply idle time constraint (tank cannot go negative)
            if (workContentTank < 0) {
                workContentTank = 0;
            }

            // Step 3: Update current time
            currentTime = est;

            // Step 4: Fill the tank (inflow) - add work content of activities at this EST
            for (int activityId : estToActivities[est]) {
                const auto& activity = RCPSPex.activities[activityId - 1];
                auto it = activity.resource_demands.find(resName);
                if (it != activity.resource_demands.end()) {
                    double workContent =
                        static_cast<double>(activity.duration) * it->second;
                    workContentTank += workContent;
                }
            }
        }

        // Step 5: Final drain calculation
        double remainingTime = workContentTank / capacity;
        double resourceLB = currentTime + remainingTime;

        // Update global maximum
        finalLB = std::max(finalLB, resourceLB);
    }

    return std::ceil(finalLB);
}

/**
 * Compute LBcc for a partial problem (some activities already finished).
 * Called during search to evaluate states.
 *
 * @param unfinishedActivities Vector of activity IDs not yet finished (1-based)
 * @param activeTransitions Currently active transitions with remaining durations
 * @param currentMakespan Current g-value (time elapsed)
 * @return Lower bound on remaining makespan
 */
double computeCriticalCapacityLB(
    const std::vector<short>& unfinishedActivities,
    const std::vector<std::pair<short, short>>& activeTransitions,
    int currentMakespan
) {
    if (!lbccInitialized) {
        initializeLBcc();
    }

    // Edge case: no unfinished activities
    if (unfinishedActivities.empty()) {
        // Only active transitions remain
        int maxRemaining = 0;
        for (const auto& [id, remaining] : activeTransitions) {
            maxRemaining = std::max(maxRemaining, static_cast<int>(remaining));
        }
        return maxRemaining;
    }

    // Build set of active activity IDs for quick lookup
    std::unordered_set<int> activeSet;
    for (const auto& [id, _] : activeTransitions) {
        activeSet.insert(id);
    }

    // Filter to get truly unstarted activities (not active)
    std::vector<short> unstartedActivities;
    unstartedActivities.reserve(unfinishedActivities.size());
    for (short id : unfinishedActivities) {
        if (activeSet.find(id) == activeSet.end()) {
            unstartedActivities.push_back(id);
        }
    }

    // Compute relative EST values (adjusted for current time = 0)
    // For unstarted activities: relative EST = original EST - currentMakespan (clamped to 0)
    // For active activities: they start at time 0 with remaining duration

    std::map<int, std::vector<int>> relativeESTToActivities;

    // Add active transitions (EST = 0, duration = remaining time)
    if (!activeTransitions.empty()) {
        for (const auto& [id, remaining] : activeTransitions) {
            relativeESTToActivities[0].push_back(id);
        }
    }

    // Add unstarted activities with adjusted EST
    for (short activityId : unstartedActivities) {
        int originalEST = estValues[activityId];
        int relativeEST = std::max(0, originalEST - currentMakespan);
        relativeESTToActivities[relativeEST].push_back(activityId);
    }

    // Sort relative EST values
    std::vector<int> sortedRelativeESTs;
    sortedRelativeESTs.reserve(relativeESTToActivities.size());
    for (const auto& [est, _] : relativeESTToActivities) {
        sortedRelativeESTs.push_back(est);
    }
    std::sort(sortedRelativeESTs.begin(), sortedRelativeESTs.end());

    double finalLB = 0.0;

    // Process each resource type
    for (const auto& [resName, capacity] : RCPSPex.resources) {
        double workContentTank = 0.0;
        int currentTime = 0;

        // Process each EST event
        for (int relEST : sortedRelativeESTs) {
            // Drain
            int deltaT = relEST - currentTime;
            workContentTank -= deltaT * capacity;

            // Idle time constraint
            if (workContentTank < 0) {
                workContentTank = 0;
            }

            currentTime = relEST;

            // Fill - add work content
            for (int activityId : relativeESTToActivities[relEST]) {
                // Check if this is an active transition
                bool isActive = activeSet.find(activityId) != activeSet.end();

                int duration;
                if (isActive) {
                    // Use remaining duration for active transitions
                    for (const auto& [id, rem] : activeTransitions) {
                        if (id == activityId) {
                            duration = rem;
                            break;
                        }
                    }
                } else {
                    duration = RCPSPex.activities[activityId - 1].duration;
                }

                const auto& activity = RCPSPex.activities[activityId - 1];
                auto it = activity.resource_demands.find(resName);
                if (it != activity.resource_demands.end()) {
                    double workContent = static_cast<double>(duration) * it->second;
                    workContentTank += workContent;
                }
            }
        }

        // Final drain
        double remainingTime = workContentTank / capacity;
        double resourceLB = currentTime + remainingTime;

        finalLB = std::max(finalLB, resourceLB);
    }

    return std::ceil(finalLB);
}
```

### Step 4: Add Forward Declaration

Add to `RCPSPState.cpp` near the top (around line 80):

```cpp
// Forward declarations for LBcc
void initializeLBcc();
double computeCriticalCapacityLB_Full();
double computeCriticalCapacityLB(
    const std::vector<short>& unfinishedActivities,
    const std::vector<std::pair<short, short>>& activeTransitions,
    int currentMakespan
);
```

### Step 5: Integrate into HCost

Modify `RCPSP::HCost()` in `RCPSP.h` (around line 240):

```cpp
inline double RCPSP::HCost(const RCPSPState &state1, const RCPSPState &state2) const {
    if (state1.status) {
        return state1.h;  // Status=1: h is identical to predecessor
    }

    // Build list of unfinished activities
    std::vector<short> tempUnstarted;
    tempUnstarted.reserve(petri.Transitions.size());

    for (int i = 0; i < petri.Transitions.size(); i++) {
        short taskID = i + 1;
        if (state1.finishedActivitiys[taskID] == -1) {
            tempUnstarted.push_back(taskID);
        }
    }

    // Compute CPM-based heuristic
    double cpH = getForwardHcostDP(tempUnstarted, state1.activeTransitionIndices);

    // Compute LBcc if enabled
    double lbccH = 0;
    if (useLBcc) {  // NEW: Check configuration flag
        lbccH = computeCriticalCapacityLB(
            tempUnstarted,
            state1.activeTransitionIndices,
            state1.g
        );
    }

    // Return maximum of all bounds
    state1.h = std::max(cpH, lbccH);
    return state1.h;
}
```

### Step 6: Update Configuration

Add to `Driver.cpp` Config struct:

```cpp
struct Config {
    // ... existing fields ...
    bool useLBcc = true;  // Enable Critical Capacity Lower Bound
};
```

Add global flag and command-line parsing:

```cpp
// Global (in RCPSPState.cpp or separate config file)
bool useLBcc;

// In parseArgs()
else if (arg == "--use-lbcc") config.useLBcc = true;
else if (arg == "--no-lbcc") config.useLBcc = false;

// In runSolver()
useLBcc = config.useLBcc;
```

### Step 7: Reset LBcc Cache Per Problem

Update the problem reset in `Driver.cpp` `runSolver()`:

```cpp
for (int group = config.groupStart; group <= config.groupEnd; group++) {
    for (int exam = config.examStart; exam <= config.examEnd; exam++) {
        // Reset state before each problem
        petri.reset();
        RCPSPex.reset();
        heuristicDPInitialized = false;
        lbccInitialized = false;  // ADD THIS LINE

        // ... solving code ...
    }
}
```

---

## Testing and Validation

### Unit Test: Basic LBcc Calculation

```cpp
void testLBcc_Basic() {
    // Setup: Resource Limit = 10
    // Activities 1 & 2: EST=0, Total Work=62
    // Activities 3 & 4: EST=10, Total Work=88

    // Expected trace:
    // T=0: Inflow 62. Tank=62.
    // T=10: Drain 100, Tank=-38 -> 0. Inflow 88. Tank=88.
    // Final: 88/10 = 8.8. Result: 10 + 8.8 = 18.8
    // LBcc = ceil(18.8) = 19

    double result = computeCriticalCapacityLB_Full();
    assert(result == 19.0);
}
```

### Validation Against Known Bounds

Compare LBcc with existing bounds:
- LBcc >= LBcp (Critical Path)
- LBcc >= LBrc (Resource Capacity)

```cpp
void validateLBccDominance() {
    double lbcp = /* CPM result */;
    double lbrc = computeResourceCapacityLowerBound(...);
    double lbcc = computeCriticalCapacityLB_Full();

    assert(lbcc >= lbcp);
    assert(lbcc >= lbrc);
}
```

### Benchmark Configuration

Run comparisons with different heuristic settings:

```bash
# CPM only (baseline)
./solver --heuristic cpm --problem-type j30 --group-end 48

# With LBcc enabled
./solver --use-lbcc --problem-type j30 --group-end 48

# Full composite (all bounds)
./solver --heuristic all --problem-type j30 --group-end 48
```

---

## Summary: Files to Modify

| File | Changes |
|------|---------|
| `RCPSPState.cpp` | Add LBcc cache variables, `initializeLBcc()`, `computeCriticalCapacityLB()` |
| `RCPSPState.h` | Add `HeuristicType` enum (optional), extern declarations |
| `RCPSP.h` | Modify `HCost()` to use LBcc, add extern for `useLBcc` |
| `Driver.cpp` | Add `useLBcc` to Config, command-line parsing, cache reset |

## Summary: New Functions

| Function | Location | Purpose |
|----------|----------|---------|
| `initializeLBcc()` | RCPSPState.cpp | Pre-compute EST values, mappings, work content |
| `computeCriticalCapacityLB_Full()` | RCPSPState.cpp | LBcc for full problem (initialization) |
| `computeCriticalCapacityLB()` | RCPSPState.cpp | LBcc for partial problem (during search) |

## Summary: New Variables

| Variable | Type | Location | Purpose |
|----------|------|----------|---------|
| `estValues` | `vector<int>` | RCPSPState.cpp | EST per activity |
| `estToActivities` | `map<int, vector<int>>` | RCPSPState.cpp | Activities at each EST |
| `sortedESTs` | `vector<int>` | RCPSPState.cpp | Sorted unique EST values |
| `workContentByResource` | `map<string, double>` | RCPSPState.cpp | Total work per resource |
| `lbccInitialized` | `bool` | RCPSPState.cpp | Cache validity flag |
| `useLBcc` | `bool` | Global | Configuration flag |

---

## Appendix: Complete LBcc Implementation Reference

The LBcc algorithm as a standalone function for reference:

```cpp
/**
 * Critical Capacity Lower Bound (LBcc)
 *
 * Combines network precedence (EST) with resource capacity constraints.
 * Models resources as "tanks" that fill at EST events and drain at constant rate.
 *
 * Time Complexity: O(n * k) where n = activities, k = resource types
 * Space Complexity: O(n) for EST mappings
 */
double computeLBcc(
    const std::vector<Activity>& activities,
    const std::vector<std::pair<std::string, int>>& resources,
    const std::vector<int>& estValues  // Pre-computed from CPM
) {
    int n = activities.size();

    // Build EST -> activities mapping
    std::map<int, std::vector<int>> estToActs;
    for (int i = 0; i < n; i++) {
        estToActs[estValues[i + 1]].push_back(i + 1);  // 1-based
    }

    // Extract sorted EST values
    std::vector<int> sortedESTs;
    for (const auto& [est, _] : estToActs) {
        sortedESTs.push_back(est);
    }
    std::sort(sortedESTs.begin(), sortedESTs.end());

    double finalLB = 0.0;

    // Process each resource
    for (const auto& [resName, capacity] : resources) {
        double tank = 0.0;
        int currentTime = 0;

        for (int est : sortedESTs) {
            // Drain
            tank -= (est - currentTime) * capacity;
            tank = std::max(0.0, tank);  // Idle time constraint
            currentTime = est;

            // Fill
            for (int actId : estToActs[est]) {
                const auto& act = activities[actId - 1];
                auto it = act.resource_demands.find(resName);
                if (it != act.resource_demands.end()) {
                    tank += static_cast<double>(act.duration) * it->second;
                }
            }
        }

        // Final drain
        double resourceLB = currentTime + (tank / capacity);
        finalLB = std::max(finalLB, resourceLB);
    }

    return std::ceil(finalLB);
}
```
