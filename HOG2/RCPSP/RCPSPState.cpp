//
// Created by idolu on 06/01/2025.

#include "RCPSPState.h"
#include "petriclasses.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <climits>
#include <iostream>
#include <set>
#include <thread>
#include <unordered_set>
#include <vector>
using namespace P_RCPSP;

// std::chrono::duration<double> generateTIME;
// std::chrono::duration<double> avelableTIME;
// std::chrono::duration<double> HTIME;
// std::chrono::duration<double>hashTIME;
//
// std::chrono::duration<double> comperTime;
// std::chrono::duration<double>secssesorTIME;

// Forward declarations for LBcc
void initializeLBcc();
double computeCriticalCapacityLB_Full();
double computeCriticalCapacityLB(
    const std::vector<short> &unfinishedActivities,
    const std::vector<std::pair<short, short>> &activeTransitions,
    int currentMakespan);

// std::atomic<bool> stop_printing(false); // Flag to stop the printing thread
//
// void printNetworkSize(const std::vector<RCPSPState>& network) {
//   while (!stop_printing) {
//     std::this_thread::sleep_for(std::chrono::seconds(1)); // Wait for a
//     second std::cout << "Current network size: " << network.size() <<
//     std::endl;
//   }
// }
//std::vector<Transition> getAvilableTransitions(std::map<std::string, int> marking);
std::vector<P_RCPSP::Transition> getAvilableTransitions(const std::unordered_map<std::string, int>& marking);

double getBackwardHcost2(
    const std::set<int> &startedActivities,
    const std::set<int> &finishedActivities,
    const std::vector<std::pair<int, int>> &activeTransitionIndices);

void GetNabor(std::vector<RCPSPState> &NodeList, int chosenNode, int &count);
// int ChooseExpansion(std::vector<RCPSPState> network);
thread_local PetriExample petri;
thread_local RCPSP_example RCPSPex;

// ============================================================================
// Dynamic Programming Heuristic Cache
// ============================================================================
// Precomputed heuristic values using DP to avoid recalculating from scratch
// heuristicDP[i] = earliest finish time for activity i (1-based index)
thread_local std::vector<int>
    heuristicDP; // Earliest finish time for each activity
thread_local bool heuristicDPInitialized = false;

// ============================================================================
// LBcc Pre-computation Cache
// ============================================================================
// Critical Capacity Lower Bound (LBcc) data structures
thread_local std::vector<int> estValues; // EST for each activity (1-based)
thread_local std::map<int, std::vector<int>>
    estToActivities;                      // EST -> activity IDs
thread_local std::vector<int> sortedESTs; // Unique EST values, sorted
thread_local std::map<std::string, double>
    workContentByResource; // Total work per resource
thread_local double precomputedLBcc =
    0.0; // Initial LBcc (all activities unfinished)
thread_local bool lbccInitialized = false;

// Note: MAX_ACTIVITIES is defined in RCPSPState.h

// Initialize the DP heuristic table once when problem is loaded
// Uses topological order (activities are already sorted by dependencies in
// RCPSP)
void initializeHeuristicDP() {
  int n = RCPSPex.activities.size();

  // Bounds check: ensure we don't exceed maximum supported activities
  if (n > MAX_ACTIVITIES) {
    throw std::runtime_error(
        "Error: Problem has " + std::to_string(n) +
        " activities, but maximum supported is " +
        std::to_string(MAX_ACTIVITIES) +
        ". Increase MAX_ACTIVITIES constant in RCPSPState.h.");
  }

  heuristicDP.assign(n + 1, 0); // 1-based indexing

  // Forward pass: Calculate earliest finish time for each activity
  // Activities are processed in topological order (1, 2, 3, ... n)
  for (int activityId = 1; activityId <= n; activityId++) {
    int maxPredecessorFinish = 0;

    // Check all predecessors (backward dependencies)
    for (int dep : RCPSPex.backword_dependencies[activityId - 1]) {
      // dep is the predecessor activity ID
      int predecessorFinish = heuristicDP[dep];
      maxPredecessorFinish = std::max(maxPredecessorFinish, predecessorFinish);
    }

    // Earliest finish = max predecessor finish + own duration
    int duration = RCPSPex.activities[activityId - 1].duration;
    heuristicDP[activityId] = maxPredecessorFinish + duration;
  }

  heuristicDPInitialized = true;
}

// ============================================================================
// LBcc Initialization
// ============================================================================
/**
 * Initialize the Critical Capacity Lower Bound (LBcc) data structures.
 * Pre-computes EST values, EST-to-activity mappings, and total work content per
 * resource. This should be called once per problem before using
 * computeCriticalCapacityLB().
 */
void initializeLBcc() {
  if (!heuristicDPInitialized) {
    initializeHeuristicDP();
  }
  if (lbccInitialized)
    return;

  int n = RCPSPex.activities.size();

  // Step 1: Compute EST values and build EST -> activities mapping
  estValues.assign(n + 1, 0); // 1-based indexing
  estToActivities.clear();

  for (int i = 1; i <= n; i++) {
    int duration = RCPSPex.activities[i - 1].duration;
    int est = heuristicDP[i] - duration; // EST = EFT - duration
    estValues[i] = est;
    estToActivities[est].push_back(i);
  }

  // Step 2: Sort unique EST values
  sortedESTs.clear();
  sortedESTs.reserve(estToActivities.size());
  for (const auto &kv : estToActivities) {
    sortedESTs.push_back(kv.first);
  }
  std::sort(sortedESTs.begin(), sortedESTs.end());

  // Step 3: Pre-compute total work content per resource
  workContentByResource.clear();
  for (int i = 0; i < n; i++) {
    const auto &activity = RCPSPex.activities[i];
    for (const auto &rd : activity.resource_demands) {
      workContentByResource[rd.first] +=
          static_cast<double>(rd.second) * activity.duration;
    }
  }

  lbccInitialized = true;
}

// ============================================================================
// Critical Capacity Lower Bound (LBcc) - Full Problem
// ============================================================================
/**
 * Compute LBcc for the full problem (all activities unfinished).
 * Uses the "tank model": each resource is modeled as a tank that fills when
 * activities become available (at their EST) and drains at constant rate
 * equal to resource capacity.
 *
 * @return Lower bound on makespan for the complete problem
 */
double computeCriticalCapacityLB_Full() {
  if (!lbccInitialized) {
    initializeLBcc();
  }

  int finalLB = 0;

  // Process each resource type
  for (const auto &res : RCPSPex.resources) {
    const std::string &resName = res.first;
    int capacity = res.second;
    int workContentTank = 0;
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

      // Step 4: Fill the tank (inflow) - add work content of activities at this
      // EST
      for (int activityId : estToActivities[est]) {
        const auto &activity = RCPSPex.activities[activityId - 1];
        auto it = activity.resource_demands.find(resName);
        if (it != activity.resource_demands.end()) {
          workContentTank += activity.duration * it->second;
        }
      }
    }

    // Step 5: Final drain - integer ceiling division
    int resourceLB = currentTime + (workContentTank + capacity - 1) / capacity;

    // Update global maximum
    finalLB = std::max(finalLB, resourceLB);
  }

  return static_cast<double>(finalLB);
}

// ============================================================================
// Critical Capacity Lower Bound (LBcc) - Partial Problem
// ============================================================================
/**
 * Compute LBcc for a partial problem (some activities already finished).
 * Called during search to evaluate states.
 *
 * @param unfinishedActivities Vector of activity IDs not yet finished (1-based)
 * @param activeTransitions Currently active transitions with remaining
 * durations
 * @param currentMakespan Current g-value (time elapsed)
 * @return Lower bound on remaining makespan
 */
double computeCriticalCapacityLB(
    const std::vector<short> &unfinishedActivities,
    const std::vector<std::pair<short, short>> &activeTransitions,
    int currentMakespan) {
  if (!lbccInitialized) {
    initializeLBcc();
  }

  // Edge case: no unfinished activities
  if (unfinishedActivities.empty()) {
    // Only active transitions remain
    int maxRemaining = 0;
    for (const auto &at : activeTransitions) {
      maxRemaining = std::max(maxRemaining, static_cast<int>(at.second));
    }
    return maxRemaining;
  }

  // Build set of active activity IDs for quick lookup
  std::unordered_set<int> activeSet;
  for (const auto &at : activeTransitions) {
    activeSet.insert(at.first);
  }

  // Filter to get truly unstarted activities (not active)
  std::vector<short> unstartedActivities;
  unstartedActivities.reserve(unfinishedActivities.size());
  for (short id : unfinishedActivities) {
    if (!activeSet.count(id)) {
      unstartedActivities.push_back(id);
    }
  }

  // Compute relative EST values (adjusted for current time = 0)
  // For unstarted activities: relative EST = original EST - currentMakespan
  // (clamped to 0) For active activities: they start at time 0 with remaining
  // duration

  std::map<int, std::vector<int>> relativeESTToActivities;

  // Add active transitions (EST = 0, use remaining duration)
  if (!activeTransitions.empty()) {
    for (const auto &at : activeTransitions) {
      relativeESTToActivities[0].push_back(at.first);
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
  for (const auto &kv : relativeESTToActivities) {
    sortedRelativeESTs.push_back(kv.first);
  }
  std::sort(sortedRelativeESTs.begin(), sortedRelativeESTs.end());

  int finalLB = 0;

  // Process each resource type
  for (const auto &res : RCPSPex.resources) {
    const std::string &resName = res.first;
    int capacity = res.second;
    int workContentTank = 0;
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

        int duration = 0;
        if (isActive) {
          // Use remaining duration for active transitions
          for (const auto &at : activeTransitions) {
            if (at.first == activityId) {
              duration = at.second;
              break;
            }
          }
        } else {
          duration = RCPSPex.activities[activityId - 1].duration;
        }

        const auto &activity = RCPSPex.activities[activityId - 1];
        auto it = activity.resource_demands.find(resName);
        if (it != activity.resource_demands.end()) {
          workContentTank += duration * it->second;
        }
      }
    }

    // Final drain - integer ceiling division
    int resourceLB = currentTime + (workContentTank + capacity - 1) / capacity;

    finalLB = std::max(finalLB, resourceLB);
  }

  return static_cast<double>(finalLB);
}

// ============================================================================
// LBip0 (Klein & Scholl 1999, "Precedence bound 1" / LB10)
// ============================================================================
// For every incompatible pair (i, j) not already ordered by transitive
// precedence, both orientations i->j and j->i are tested. Per Remark 3, the
// longest path through arc (i, j) equals ES[i] + d[i] + d[j] + tail[j], where
// tail[k] = LB1 - LF[k]. The pair bound is min(LB(i,j), LB(j,i)), and LBip0
// is the max over all incompatible pairs. The bound is static: computed once
// from the project network and reused at every search node with a g-subtract.

thread_local int lbip0Value = 0;
thread_local bool lbip0Initialized = false;

void initializeLBIP0() {
  if (!heuristicDPInitialized) initializeHeuristicDP();
  if (lbip0Initialized) return;

  int n = static_cast<int>(RCPSPex.activities.size());

  // LB1 = critical path length = max EFT over all activities
  int LB1 = 0;
  for (int i = 1; i <= n; i++) LB1 = std::max(LB1, heuristicDP[i]);
  lbip0Value = LB1;

  if (n <= 1) { lbip0Initialized = true; return; }

  // ES[i] = EFT[i] - d[i]
  std::vector<int> ES(n + 1, 0);
  for (int i = 1; i <= n; i++) {
    ES[i] = heuristicDP[i] - RCPSPex.activities[i - 1].duration;
  }

  // Backward pass for LF[i] (latest finish given makespan LB1)
  // LF[i] = min over successors s of (LF[s] - d[s]); sinks default to LB1
  std::vector<int> LF(n + 1, LB1);
  for (int i = n; i >= 1; i--) {
    int minLS = LB1;
    for (short s : RCPSPex.dependencies[i - 1]) {
      int LS_s = LF[s] - RCPSPex.activities[s - 1].duration;
      minLS = std::min(minLS, LS_s);
    }
    LF[i] = minLS;
  }

  // tail[i] = LB1 - LF[i]
  std::vector<int> tail(n + 1, 0);
  for (int i = 1; i <= n; i++) tail[i] = LB1 - LF[i];

  // Transitive closure via Floyd-Warshall on precedence graph.
  // reach[i][j] = true iff j is a (transitive) successor of i.
  // Activities are 1-based; use n+1 x n+1 packed in 1D for cache locality.
  int N = n + 1;
  std::vector<unsigned char> reach(static_cast<size_t>(N) * N, 0);
  auto R = [&](int i, int j) -> unsigned char & {
    return reach[static_cast<size_t>(i) * N + j];
  };
  for (int i = 1; i <= n; i++) {
    for (short s : RCPSPex.dependencies[i - 1]) R(i, s) = 1;
  }
  for (int k = 1; k <= n; k++) {
    for (int i = 1; i <= n; i++) {
      if (!R(i, k)) continue;
      for (int j = 1; j <= n; j++) {
        if (R(k, j)) R(i, j) = 1;
      }
    }
  }

  // Enumerate unordered pairs. Skip pairs already ordered by transitive
  // precedence — they are incompatible by construction, but the original
  // critical path already captures their sequencing, so they add nothing.
  for (int i = 1; i < n; i++) {
    const auto &actI = RCPSPex.activities[i - 1];
    if (actI.duration == 0) continue;
    for (int j = i + 1; j <= n; j++) {
      if (R(i, j) || R(j, i)) continue;
      const auto &actJ = RCPSPex.activities[j - 1];
      if (actJ.duration == 0) continue;

      // Resource incompatibility: ∃ resource k with r_ik + r_jk > a_k
      bool incompatible = false;
      for (const auto &res : RCPSPex.resources) {
        auto itI = actI.resource_demands.find(res.first);
        auto itJ = actJ.resource_demands.find(res.first);
        int rI = (itI != actI.resource_demands.end()) ? itI->second : 0;
        int rJ = (itJ != actJ.resource_demands.end()) ? itJ->second : 0;
        if (rI + rJ > res.second) { incompatible = true; break; }
      }
      if (!incompatible) continue;

      // Length of longest path through arc i->j and through arc j->i
      int lenIJ = ES[i] + actI.duration + actJ.duration + tail[j];
      int lenJI = ES[j] + actJ.duration + actI.duration + tail[i];
      int lbIJ = std::max(LB1, lenIJ);
      int lbJI = std::max(LB1, lenJI);
      int pairBound = std::min(lbIJ, lbJI);
      if (pairBound > lbip0Value) lbip0Value = pairBound;
    }
  }

  lbip0Initialized = true;
}

double computeLBIP0(int currentMakespan) {
  if (!lbip0Initialized) initializeLBIP0();
  int remaining = lbip0Value - currentMakespan;
  return remaining > 0 ? static_cast<double>(remaining) : 0.0;
}

// Fast DP-based heuristic lookup
// Given the set of unstarted activities, return the max "time to finish"
// using precomputed values
double getForwardHcostDP(
    const std::vector<short> &unstartedTransitions,
    const std::vector<std::pair<short, short>> &activeTransitionIndices) {
  if (!heuristicDPInitialized) {
    initializeHeuristicDP();
  }

  if (unstartedTransitions.empty()) {
    return 0;
  }

  // For each unstarted activity, we need to calculate how much time remains
  // Taking into account which activities are already finished

  // Create a lookup for quick access to active transition remaining times
  // Note: Uses fixed-size array for performance; bounds checked at
  // initialization
  std::array<short, MAX_ACTIVITIES> activeRemaining;
  activeRemaining.fill(-1);
  for (const auto &[transIdx, remaining] : activeTransitionIndices) {
    activeRemaining[transIdx] = remaining;
  }

  // Calculate the earliest finish time for unstarted activities
  // using DP with memoization for this specific state
  // Note: Uses fixed-size array for performance; bounds checked at
  // initialization
  std::array<int, MAX_ACTIVITIES> earlyFinish;
  earlyFinish.fill(-1); // -1 means not computed yet

  // Mark finished activities as having earlyFinish = 0 (already done)
  // This is implicit: if an activity is not in unstartedTransitions,
  // we treat its contribution as 0

  double maxH = 0;

  // Process unstarted activities - they're already in topological order
  for (short activityId : unstartedTransitions) {
    int maxPredecessorFinish = 0;

    // Check all predecessors
    for (int dep : RCPSPex.backword_dependencies[activityId - 1]) {
      // Check if predecessor is in unstarted list
      if (earlyFinish[dep] != -1) {
        // Predecessor is unstarted and we've computed its value
        maxPredecessorFinish = std::max(maxPredecessorFinish, earlyFinish[dep]);
      }
      // If predecessor is finished (not in list), its contribution is 0
      // If predecessor is active, use remaining time
      else if (activeRemaining[dep] != -1) {
        maxPredecessorFinish =
            std::max(maxPredecessorFinish, (int)activeRemaining[dep]);
      }
    }

    // Get duration (check if active with remaining time)
    int duration;
    if (activeRemaining[activityId] != -1) {
      duration = activeRemaining[activityId];
    } else {
      duration = RCPSPex.activities[activityId - 1].duration;
    }

    earlyFinish[activityId] = maxPredecessorFinish + duration;
    maxH = std::max(maxH, (double)earlyFinish[activityId]);
  }

  return maxH;
}

// Fast DP-based heuristic for TT method
double getForwardHcostDP_TT(const std::vector<short> &unstartedTransitions) {
  if (!heuristicDPInitialized) {
    initializeHeuristicDP();
  }

  if (unstartedTransitions.empty()) {
    return 0;
  }

  // Calculate the earliest finish time for unstarted activities
  // Note: Uses fixed-size array for performance; bounds checked at
  // initialization
  std::array<int, MAX_ACTIVITIES> earlyFinish;
  earlyFinish.fill(-1);

  double maxH = 0;

  // Process unstarted activities in topological order
  for (short activityId : unstartedTransitions) {
    int maxPredecessorFinish = 0;

    for (int dep : RCPSPex.backword_dependencies[activityId - 1]) {
      if (earlyFinish[dep] != -1) {
        maxPredecessorFinish = std::max(maxPredecessorFinish, earlyFinish[dep]);
      }
      // Finished predecessors contribute 0
    }

    int duration = RCPSPex.activities[activityId - 1].duration;
    earlyFinish[activityId] = maxPredecessorFinish + duration;
    maxH = std::max(maxH, (double)earlyFinish[activityId]);
  }

  return maxH;
}

// ============================================================================
// LBCS: Lower Bound Critical Sequence
// Based on Stinson, Davis & Khumawala (1978), as described in
// Coelho & Vanhoucke (2018), Section 2.1
// ============================================================================

/**
 * Compute LBCS for TT method.
 * Only considers unstarted activities (TT has no "active" transitions concept in HCost).
 *
 * @param unstartedTransitions  Activity IDs (1-based) that are not yet finished, in ascending order
 * @return  LBCS lower bound value
 */
double computeLBCS_TT(const std::vector<short>& unstartedTransitions) {
    if (unstartedTransitions.empty()) return 0;

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
    // LF[act] = min over successors s of (LF[s] - duration[s])
    // For activities with no remaining successors: LF = CPM
    std::array<int, MAX_ACTIVITIES> LF;
    LF.fill(-1);

    // Process in reverse topological order (descending ID)
    for (int idx = (int)unstartedTransitions.size() - 1; idx >= 0; idx--) {
        short actId = unstartedTransitions[idx];
        int minSuccLS = CPM; // default: can finish at project end

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
    int numResources = RCPSPex.resources.size();

    // resourceUsage[resourceIdx][time] = usage at time t by critical activities
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
 * Compute LBCS for TP method.
 * Accounts for active transitions that have remaining durations.
 *
 * @param unstartedTransitions      Activity IDs (1-based) not yet finished, in ascending order
 * @param activeTransitionIndices   Currently active transitions: (transitionID, remainingDuration)
 * @return  LBCS lower bound value
 */
double computeLBCS(const std::vector<short>& unstartedTransitions,
                   const std::vector<std::pair<short, short>>& activeTransitionIndices) {
    if (unstartedTransitions.empty()) return 0;

    // Build active transition lookup
    std::array<short, MAX_ACTIVITIES> activeRemaining;
    activeRemaining.fill(-1);
    for (const auto& [transIdx, remaining] : activeTransitionIndices) {
        activeRemaining[transIdx] = remaining;
    }

    // --- Step 1: Forward pass (ES, EF) ---
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
        }
        ES[actId] = maxPredFinish;
        EF[actId] = maxPredFinish + RCPSPex.activities[actId - 1].duration;
    }

    int CPM = 0;
    for (short actId : unstartedTransitions) {
        CPM = std::max(CPM, EF[actId]);
    }

    if (CPM == 0) return 0;

    // --- Step 2: Backward pass (LF) ---
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

    // --- Step 3: Identify critical vs non-critical ---
    std::vector<short> criticalActivities;
    std::vector<short> nonCriticalActivities;

    for (short actId : unstartedTransitions) {
        // Skip active transitions for non-critical check (they're already committed)
        if (activeRemaining[actId] != -1) {
            // Active transitions are treated as critical (fixed schedule)
            criticalActivities.push_back(actId);
            continue;
        }
        int slack = LF[actId] - EF[actId];
        if (slack == 0) {
            criticalActivities.push_back(actId);
        } else if (slack > 0) {
            nonCriticalActivities.push_back(actId);
        }
    }

    if (nonCriticalActivities.empty()) return CPM;

    // --- Step 4: Sort non-critical by duration descending ---
    std::sort(nonCriticalActivities.begin(), nonCriticalActivities.end(),
              [](short a, short b) {
                  return RCPSPex.activities[a - 1].duration > RCPSPex.activities[b - 1].duration;
              });

    // --- Step 5: Resource profile of critical activities ---
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

    // --- Step 6: Greedy extension check ---
    int maxExtension = 0;

    for (short actId : nonCriticalActivities) {
        int dur = RCPSPex.activities[actId - 1].duration;
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

 int main2() {
   return 0;
 }
// std::vector<Transition> getAvilableDetransitions(const std::unordered_map<std::string, int>& marking) {
//    std::vector<Transition> availableDetransitions;
//    availableDetransitions.reserve(petri.Transitions.size());
//
//    for (const auto& transition : petri.Transitions) {
//      bool canUndo = true;
//
//      for (const auto& arc : transition.arcs_out) {  // Instead of arcs_in, we
//      check arcs_out
//        auto it = marking.find(arc.first);
//        int tokenCount = (it != marking.end()) ? it->second : 0;
//
//        if (tokenCount < arc.second) {
//          canUndo = false;  // Not enough tokens in the output place to undo
//          break;
//        }
//      }
//
//      if (canUndo) {
//        availableDetransitions.push_back(transition);
//      }
//    }
//
//    return availableDetransitions;
//
//
// //  }
// std::vector<Transition> getAvilableTransitions(const
// std::unordered_map<std::string, int>& marking) {
//    auto startS1 = std::chrono::high_resolution_clock::now();
//
//    std::vector<Transition> avilableTransitions;
//    avilableTransitions.reserve(petri.Transitions.size());  // Reserve memory
//    to avoid multiple reallocations
//
//    for (const auto& transition : petri.Transitions) {
//      int avilable = 0, requirment = 0;
//      bool canFire = true;
//
//      for (const auto& arc : transition.arcs_in) {
//        auto it = marking.find(arc.first);
//        int tokenCount = (it != marking.end()) ? it->second : 0;
//
//        if (tokenCount < arc.second) {
//          canFire = false;  // Not enough tokens to fire
//          break;            // Stop checking further
//        }
//        avilable += std::min(tokenCount, arc.second);
//        requirment += arc.second;
//      }
//
//      if (canFire) {
//        avilableTransitions.push_back(transition);
//      }
//    }
//
//     auto endS1 = std::chrono::high_resolution_clock::now();
//     avelableTIME += endS1 - startS1;
//
//    return avilableTransitions;
//  }

std::vector<int>
getAvilableTransitionIndices(const std::vector<short> &marking);

std::vector<int> getAvilableDetransitionIndices(
    const std::unordered_map<std::string, int> &marking);

double
getForwardHcost(std::set<short> unstartedTransitions,
                std::vector<std::pair<short, short>> activeTransitionIndices) {
  // auto startS3 = std::chrono::high_resolution_clock::now();

  std::map<int, int>
      earlyfinishMap2; // Map to store activity IDs and their early finish times
  // std::map<int, int> visitmap; // Map to store activity IDs and their early
  // finish times
  double h;
  std::set<int> processedDependencies;
  // Iterate over unstarted activitiesint lastElementEarlyFinish = 0;
  // int lastElementEarlyFinish = 0;
  for (int activityId : unstartedTransitions) {
    int maxFinishTime = 0;
    std::set<int> processedDependencies;

    for (int dep :
         RCPSPex.backword_dependencies[activityId - 1]) { // Changed to int
      int depId = dep - 1;                                // No more std::stoi

      if (std::find(unstartedTransitions.begin(), unstartedTransitions.end(),
                    depId + 1) != unstartedTransitions.end()) {
        short duration = getTransitionDuration2(
            activeTransitionIndices, dep); // Pass dep directly (no std::stoi)
        if (duration != -1) {
          maxFinishTime =
              std::max(maxFinishTime, earlyfinishMap2[depId + 1] + duration);
        } else {
          maxFinishTime =
              std::max(maxFinishTime, earlyfinishMap2[depId + 1] +
                                          RCPSPex.activities[depId].duration);
        }
      } else {
        maxFinishTime = std::max(maxFinishTime, earlyfinishMap2[depId + 1]);
      }
    }
    earlyfinishMap2[activityId] = maxFinishTime;
  }
  if (earlyfinishMap2.size() == 0) {
    h = 0;
  } else {
    h = earlyfinishMap2.rbegin()->second;
    ;
  }

  // auto endS3 = std::chrono::high_resolution_clock::now();
  // HTIME += endS3 - startS3;

  return h;
}

double
getForwardHcost(std::vector<short> unstartedTransitions,
                std::vector<std::pair<short, short>> activeTransitionIndices
                //,std::vector<int> finishedActivitiys

) {
  // auto startS3 = std::chrono::high_resolution_clock::now();

  std::map<int, int>
      earlyfinishMap2; // Map to store activity IDs and their early finish times
  // std::map<int, int> earlyfinishMap3; // Map to store activity IDs and their
  // early finish times

  double h;
  std::set<int> processedDependencies;

  for (int activityId : unstartedTransitions) {
    int maxFinishTime = 0;
    std::set<int> processedDependencies;

    for (int dep :
         RCPSPex.backword_dependencies[activityId - 1]) { // Changed to int
      int depId = dep - 1;                                // No more std::stoi

      if (std::find(unstartedTransitions.begin(), unstartedTransitions.end(),
                    depId + 1) != unstartedTransitions.end()) {
        int duration = getTransitionDuration2(activeTransitionIndices,
                                              dep); // Pass dep directly
        if (duration != -1) {
          maxFinishTime =
              std::max(maxFinishTime, earlyfinishMap2[depId + 1] + duration);
          // earlyfinishMap3[depId+1] = earlyfinishMap2[depId+1] + duration;
        } else {
          maxFinishTime =
              std::max(maxFinishTime, earlyfinishMap2[depId + 1] +
                                          RCPSPex.activities[depId].duration);
          //   earlyfinishMap3[depId+1] = earlyfinishMap2[depId+1] +
          //   RCPSPex.activities[depId].duration;
        }
      } else {
        maxFinishTime = std::max(maxFinishTime, earlyfinishMap2[depId + 1]);
        //  earlyfinishMap3[depId+1] = earlyfinishMap2[depId+1];
      }
    }
    earlyfinishMap2[activityId] = maxFinishTime;
    // earlyfinishMap3[activityId] = maxFinishTime;
  }
  if (earlyfinishMap2.size() == 0) {
    h = 0;
  } else {
    h = earlyfinishMap2.rbegin()->second;
    ;
  }

  return h;
}

double getBackwardHcost2(
    const std::set<int> &startedActivities,
    const std::set<int> &finishedActivities,
    const std::vector<std::pair<int, int>> &activeTransitionIndices) {
  std::map<int, int> earlyFinishMap;
  std::set<int> allRelevant;

  for (int id : startedActivities)
    allRelevant.insert(id);
  for (const auto &[id, _] : activeTransitionIndices)
    allRelevant.insert(id);

  for (int actId : allRelevant) {
    int maxDepFinish = 0;
    // for (const std::string& depStr : RCPSPex.backword_dependencies[actId -
    // 1]) {
    //   int depId = std::stoi(depStr);
    //   if (earlyFinishMap.count(depId))
    //     maxDepFinish = std::max(maxDepFinish, earlyFinishMap[depId]);
    // }

    int duration = RCPSPex.activities[actId - 1].duration;
    int remaining = 0;
    for (const auto &[id, remain] : activeTransitionIndices) {
      if (id == actId) {
        remaining = remain;
        break;
      }
    }

    int effectiveDuration = duration - remaining;
    earlyFinishMap[actId] = maxDepFinish + effectiveDuration;
  }

  int maxSoFar = 0;
  for (const auto &[_, finishTime] : earlyFinishMap)
    maxSoFar = std::max(maxSoFar, finishTime);

  return static_cast<double>(maxSoFar);
}




// double getBackwordsHcost(std::set<int>startedTransitions,
// std::vector<std::pair<int, int>>activeTransitionIndices)
// {
//  // auto startS3 = std::chrono::high_resolution_clock::now();
//
//   std::map<int, int> earlyfinishMap2; // Map to store activity IDs and their
//   early finish times double h;
//
//   // Iterate over started activities
//   for (int activityId: startedTransitions) {
//     int maxFinishTime = 0;
//
//     // Check all backward dependencies
//     for (const auto &dep: RCPSPex.backword_dependencies[activityId - 1]) {
//       int depId = std::stoi(dep) - 1;
//
//       // If dependency is in started transitions
//       if (std::find(startedTransitions.begin(), startedTransitions.end(),
//       depId + 1) != startedTransitions.end()) {
//         // Find if dependency is in active transitions to get its duration
//         int duration = -1;
//         for (const auto &pair : activeTransitionIndices) {
//           if (pair.first == depId + 1) {
//             duration = pair.second;
//             break;
//           }
//         }
//
//         if (duration != -1) {
//           // Use duration from active transitions
//           maxFinishTime = std::max(maxFinishTime, earlyfinishMap2[depId+1] +
//           duration);
//         } else {
//           // Use default duration
//           maxFinishTime = std::max(maxFinishTime, earlyfinishMap2[depId+1] +
//           RCPSPex.activities[depId].duration);
//         }
//       } else {
//         // If dependency is not in started transitions, just use its finish
//         time maxFinishTime = std::max(maxFinishTime,
//         earlyfinishMap2[depId+1]);
//       }
//     }
//
//     if (std::find(startedTransitions.begin(), startedTransitions.end(),
//     activityId + 1) != startedTransitions.end()) {
//       int duration = -1;
//       for (const auto &pair : activeTransitionIndices) {
//         if (pair.first == activityId + 1) {
//           duration = pair.second;
//           break;
//         }
//       }
//
//       if (duration != -1) {
//         // Use duration from active transitions
//         maxFinishTime = std::max(maxFinishTime, earlyfinishMap2[activityId+1]
//         + duration);
//       } else {
//         // Use default duration
//         maxFinishTime = std::max(maxFinishTime, earlyfinishMap2[activityId+1]
//         + RCPSPex.activities[activityId].duration);
//       }
//     }
//     earlyfinishMap2[activityId] = maxFinishTime;
//   }
//
//   // Find the maximum finish time
//   h = 0;
//   if (!earlyfinishMap2.empty()) {
//     h = std::max_element(
//       earlyfinishMap2.begin(),
//       earlyfinishMap2.end(),
//       [](const auto& p1, const auto& p2) { return p1.second < p2.second; }
//     )->second;
//   }
//
// //  auto endS3 = std::chrono::high_resolution_clock::now();
//   //HTIME += endS3 - startS3;
//   return h;
// /*
//
//  auto startS3 = std::chrono::high_resolution_clock::now();
//
//    std::map<int, int> earlyfinishMap2; // Map to store activity IDs and their
//    early finish times
//   //std::map<int, int> visitmap; // Map to store activity IDs and their early
//   finish times double h; std::set<int> processedDependencies;
//   // Iterate over unstarted activitiesint lastElementEarlyFinish = 0;
//   //int lastElementEarlyFinish = 0;
//   for (int activityId: startedTransitions) {
//     int maxFinishTime = 0;
//     std::set<int> processedDependencies;
//
//     for (const auto &dep: RCPSPex.backword_dependencies[activityId - 1]) {
//       int depId = std::stoi(dep) - 1;
//       // if (processedDependencies.count(depId) > 0) continue;
//       // processedDependencies.insert(depId);
//       if (std::find(startedTransitions.begin(), startedTransitions.end(),
//       depId + 1) != startedTransitions.end()) {
//         int duration = getTransitionDuration2(activeTransitionIndices,
//         std::stoi(dep)); if (duration !=-1) {
//           maxFinishTime = std::max(maxFinishTime, earlyfinishMap2[depId+1] +
//           duration);
//           //if (RCPSPex.activities[depId].duration !=duration) {
//           //  std::cout<<name<<":"<<dep<<" "<<activityId<<"
//           "<<RCPSPex.activities[depId].duration-duration<<std::endl;
//           //}
//         }
//         else {
//           maxFinishTime = std::max(maxFinishTime, earlyfinishMap2[depId+1] +
//           RCPSPex.activities[depId].duration);
//
//         }
//       }
//       else {
//         maxFinishTime = std::max(maxFinishTime, earlyfinishMap2[depId+1]);
//       }
//     }
//
//     earlyfinishMap2[activityId] = maxFinishTime;
//     //std::cout <<activityId<<":"<<
//     earlyfinishMap[activityId]+RCPSPex.activities[activityId-1].duration <<
//     std::endl;
//     // For last element with duration 0, just use the max finish time of
//     dependencies
//   }
//   h = 0;
//   if (earlyfinishMap2.size()==0) {
//
//   }
//   else {
//     // Find the maximum value in the map
//     h = std::max_element(
//       earlyfinishMap2.begin(),
//       earlyfinishMap2.end(),
//       [](const auto& p1, const auto& p2) { return p1.second < p2.second; }
//     )->second;
//   }
//   // if (h != newH) {
//   //   int asd;
//   //   asd++;
//   // }
//    auto endS3 = std::chrono::high_resolution_clock::now();
//    HTIME += endS3 - startS3;
//   return h;
//   */
// }

RCPSPState::RCPSPState() {
  // 1. Safety Valve
  // auto startS1 = std::chrono::high_resolution_clock::now();
  // direction = true;
  // startedActivitiys[0] = 0;

  // 2. Initialize Marking Vector (Fast!)
  marking.resize(petri.places.size(), 0);
  // Allocating 'size + 1' allows you to use index 'N' without crashing.
  startedActivitiys.fill(-1);
  finishedActivitiys.fill(-1);
  // 3. LOOP 1: Setup Places
  for (int i = 0; i < petri.places.size(); i++) {

    // Check Final Node (Just for bookkeeping)
    if (petri.places[i].arcs_out.empty()) {
      finalstatename = petri.places[i].name;
    }

    // Check Initial Node (The ONLY one that should be 1)
    if (petri.places[i].arcs_in.empty()) {
      initialstatename = petri.places[i].name;
      marking[i] = 1; // <--- INSIDE THE IF. ONLY HAPPENS ONCE.
    } else {
      // For everyone else, check the JSON state
      // Usually this is 0, so this block barely runs.
      if (!petri.places[i].state.empty() && !petri.places[i].state[0].empty()) {
        int val = petri.places[i].state[0][0];
        if (val > 0) {
          marking[i] = (short)val;
        }
      }
    }
  }
  // 4. LOOP 2: Setup Resources (CRITICAL MISSING PIECE)
  // This sets R1=4, R2=2, etc. Without this, resources stay 0.
  for (const auto &[resName, capacity] : RCPSPex.resources) {
    int resID = petri.place_name_to_id.at(resName);
    marking[resID] = (short)capacity;
  }

  // 5. Initialize Unstarted Transitions
  // unfinishedTransitions.reserve(petri.Transitions.size());
  // for (int i = 1; i < petri.Transitions.size(); i++) {
  //   unfinishedTransitions.push_back(i + 1);
  // }

  // auto endS1 = std::chrono::high_resolution_clock::now();
  // generateTIME += endS1 - startS1;

  // 6. GET AVAILABLE (Do this ONCE at the end)
  // Now that marking is fully built, calculate what can run.

  g = 0;
}
// not working
RCPSPState_bi::RCPSPState_bi() : nodestatus(false) {
  // auto startS1 = std::chrono::high_resolution_clock::now();
  // startedActivitiys.insert(0);
  direction = true;

  // Find initial and final places
  for (int i = 0; i < petri.places.size(); i++) {
    if (petri.places[i].arcs_out.size() == 0) {
      finalstatename = petri.places[i].name;
    }
    if (petri.places[i].arcs_in.size() == 0) {
      initialstatename = petri.places[i].name;
    }
  }

  // Initialize unstartedTransitions
  for (int i = 1; i < petri.Transitions.size(); i++) {
    unstartedTransitions.insert(i + 1);
    // unstartedTransitions.insert(i);
  }

  // Initialize marking
  for (int i = 0; i < petri.places.size(); i++) {
    if (petri.places[i].name == initialstatename) {
      marking[petri.places[i].name] = 1;
    } else {
      marking[petri.places[i].name] = petri.places[i].state[0][0];
    }
  }

  // auto endS1 = std::chrono::high_resolution_clock::now();
  // generateTIME += endS1 - startS1;

  // Change: Get indices of available transitions instead of full Transition
  // objects
  // avilableTransitionIndices = getAvilableTransitionIndices(marking);

  // avilableDeTransitionIndices = getAvilableDetransitionIndices(marking);
  g_b = 0;
  g_f = 0;
  name = 0;
  // h_f=getForwardHcost(unstartedTransitions,activeTransitionIndices);
  h_b = 0;
  f = h_f;
}

RCPSPState::RCPSPState(const RCPSPState &predecesor,
                       const P_RCPSP::Transition &active, bool status1,
                       short location, uint64_t &count) {

  // 1. FAST COPY (Vectors are contiguous, so this is basically a memcpy)
  startedActivitiys = predecesor.startedActivitiys;
  finishedActivitiys = predecesor.finishedActivitiys;
  marking = predecesor.marking;
  activeTransitionIndices = predecesor.activeTransitionIndices;

  // Copy Scalars
  g = predecesor.g;
  status = status1;
  h = predecesor.h;

  // 2. FORWARD LOGIC
  if (1) {
    if (status) { // --- STARTING A TASK ---

      // Apply arcs_in (Consume resources)
      // Note: active.name is 1-based, so we subtract 1 to access Petri net
      // transitions
      for (const auto &arc :
           petri.Transitions[active.name - 1].arcs_in_indices) {
        marking[arc.first] -= arc.second;
      }

      // Store index and duration
      activeTransitionIndices.push_back({active.name, active.duration});

      // [VECTOR UPDATE]
      // Access index directly. Ensure vector size is at least active.name + 1
      startedActivitiys[active.name] = g;

      if (active.duration == 0) {
        status = 0; // Immediate finish if duration is 0
      }
    }

    if (!status) { // --- FINISHING A TASK ---
      g += active.duration;

      // [VECTOR UPDATE]
      // Mark the current active task as finished
      finishedActivitiys[active.name] = g;

      // 1. Update remaining time for ALL active tasks
      for (auto &[id, remain] : activeTransitionIndices) {
        remain -= active.duration;
        if (remain < 0)
          remain = 0;
      }

      // 2. Collect ALL tasks that finished exactly at this moment
      std::vector<int> finishedNow;
      for (const auto &[id, remain] : activeTransitionIndices) {
        if (remain == 0) {
          finishedNow.push_back(id);
        }
      }

      // 3. Process all finished tasks (Chain Reaction)
      for (int id : finishedNow) {
        // [VECTOR UPDATE]
        // Check if already marked to avoid double-processing (optional
        // optimization)
        if (finishedActivitiys[id] == -1) {
          finishedActivitiys[id] = g;
        }

        // Release resources (Produce tokens)
        // Note: id is 1-based, so id - 1 for Petri net access
        for (const auto &arc : petri.Transitions[id - 1].arcs_out_indices) {
          marking[arc.first] += arc.second;
        }
      }

      // 4. Remove finished tasks from active list
      // Optimization: Remove requires shifting elements.
      // Since order doesn't matter, Swap-and-Pop is faster, but remove_if is
      // safer for now.
      activeTransitionIndices.erase(
          std::remove_if(activeTransitionIndices.begin(),
                         activeTransitionIndices.end(),
                         [](const std::pair<int, int> &p) {
                           return p.second == 0;
                         }), // Remove if remaining time is 0
          activeTransitionIndices.end());
    }
  }
  // Backward logic omitted for brevity (mirror the changes above if needed)
}

RCPSPState_bi::RCPSPState_bi(RCPSPState_bi predecesor, Transition active,
                             bool status, int location, uint64_t &count) {
  // auto startS4 = std::chrono::high_resolution_clock::now();

  // Copy basic properties
  direction = predecesor.direction;
  name = count;
  nodestatus = status;
  unstartedTransitions = predecesor.unstartedTransitions;
  startedActivitiys = predecesor.startedActivitiys;
  finishedActivitiys = predecesor.finishedActivitiys;
  marking = predecesor.marking;

  // Copy indices instead of full Transition objects
  activeTransitionIndices = predecesor.activeTransitionIndices;
  avilableTransitionIndices = predecesor.avilableTransitionIndices;
  g_b = predecesor.g_b;
  g_f = predecesor.g_f;
  h_b = predecesor.h_b;
  h_f = predecesor.h_f;

  if (direction) {
    // g_f = predecesor.g_f;

    if (status) {
      // h_f = predecesor.h_f;

      // Apply arcs_in from the transition
      for (const auto &arc :
           petri.Transitions[active.name - 1].arcs_in_indices) {

        // arc.first  is now the integer Place ID (e.g., 5)
        // arc.second is the token count (e.g., 1)

        // This is a direct array access. 1 CPU cycle.
        // marking[arc.first] -= arc.second;
      }

      // Store index and duration instead of full Transition
      activeTransitionIndices.push_back({active.name, active.duration});
      startedActivitiys.insert(active.name);
      if (active.duration == 0) {
        status = 0;
      }
      //  auto endS1 = std::chrono::high_resolution_clock::now();
      //   generateTIME += endS1-startS4;
    }
    if (!status) {
      g_f += active.duration;
      finishedActivitiys.insert(active.name);

      // Remove from unstarted
      unstartedTransitions.erase(active.name);

      // Update durations and remove completed transitions
      for (int i = activeTransitionIndices.size() - 1; i >= 0; --i) {
        activeTransitionIndices[i].second -= active.duration;
        if (activeTransitionIndices[i].second < 0) {
          activeTransitionIndices[i].second = 0;
        }
        if (activeTransitionIndices[i].first == active.name) {
          // Apply arcs_out from the transition
          // for (const auto& arc : petri.Transitions[active.name-1].arcs_out) {
          //   marking[arc.first] += arc.second;
          // }
          activeTransitionIndices.erase(activeTransitionIndices.begin() + i);
        }
      }

      // auto endS1 = std::chrono::high_resolution_clock::now();
      // generateTIME += endS1-startS4;

      // h_f=getForwardHcost(unstartedTransitions,activeTransitionIndices);
    }
    f = g_f + h_f;
    h_b = getBackwardHcost2(startedActivitiys, finishedActivitiys,
                            activeTransitionIndices);
    f = 2 * g_f + h_f - h_b;
    // f=2*g_f+h_f;

    // avilableTransitionIndices = getAvilableTransitionIndices(marking);
  } else {

    // Similar transformation for the backward direction
    if (status) {
      // h_b = predecesor.h_b;

      // for (const auto& arc : petri.Transitions[active.name-1].arcs_out) {
      //   marking[arc.first] -= arc.second;
      // }

      activeTransitionIndices.push_back({active.name, 0});
      auto it = std::find(finishedActivitiys.begin(), finishedActivitiys.end(),
                          active.name);
      if (it != finishedActivitiys.end()) {
        finishedActivitiys.erase(it);
      }
      if (active.duration == 0) {
        status = 0;
      }
      // auto endS1 = std::chrono::high_resolution_clock::now();
      // generateTIME += endS1-startS4;
    }
    if (!status) {
      g_b += (petri.Transitions[active.name - 1].duration - active.duration);

      auto it = std::find(startedActivitiys.begin(), startedActivitiys.end(),
                          active.name);
      if (it != startedActivitiys.end()) {
        startedActivitiys.erase(it);
      }
      unstartedTransitions.insert(active.name);

      for (int i = activeTransitionIndices.size() - 1; i >= 0; --i) {
        activeTransitionIndices[i].second +=
            (petri.Transitions[active.name - 1].duration - active.duration);
        if (activeTransitionIndices[i].second >
            petri.Transitions[activeTransitionIndices[i].first - 1].duration) {
          activeTransitionIndices[i].second =
              petri.Transitions[activeTransitionIndices[i].first - 1].duration;
        }
        if (activeTransitionIndices[i].first == active.name) {
          // for (const auto& arc : petri.Transitions[active.name-1].arcs_in) {
          //   marking[arc.first] += arc.second;
          // }
          activeTransitionIndices.erase(activeTransitionIndices.begin() + i);
        }
      }
      // auto endS1 = std::chrono::high_resolution_clock::now();
      // generateTIME += endS1-startS4;
      h_b = getBackwardHcost2(startedActivitiys, finishedActivitiys,
                              activeTransitionIndices);
    }
    // h_f=getForwardHcost(unstartedTransitions,activeTransitionIndices);

    avilableDeTransitionIndices = getAvilableDetransitionIndices(marking);
    f = 2 * g_b + h_b - h_f;
    // f=g_b;
    // f=g_b+h_b;
  }

  // You'll need to modify these functions to return indices instead of
  // Transitions
  //   if (direction) {
  // }
  // else {
  //   }
  int asdasd;
  asdasd++;
}

// CHANGE 1: Input is now a fast vector, not a slow map

std::vector<int> getAvilableDetransitionIndices(
    const std::unordered_map<std::string, int> &marking) {
  std::vector<int> availableIndices;

  // Similar implementation for detransitions
  for (int i = 0; i < petri.Transitions.size(); i++) {
    const Transition &t = petri.Transitions[i];
    bool available = true;

    // Check output arcs instead of input arcs for detransitions
    // for (const auto& arc : t.arcs_out) {
    //   auto it = marking.find(arc.first);
    //   if (it == marking.end() || it->second < arc.second) {
    //     available = false;
    //     break;
    //   }
    // }

    if (available) {
      availableIndices.push_back(i + 1);
    }
  }

  return availableIndices;
}

bool RCPSPState::operator==(const RCPSPState &other) const {
  // 1. Fast checks
  // if (name != other.name) return false;
  // if (direction != other.direction) return false;

  // 2. Vector Comparison
  // This is only safe if you GUARANTEE every vector is initialized
  // exactly the same way in the constructor (e.g., size + 5, filled with -1).
  if (marking != other.marking)
    return false;
  if (finishedActivitiys != other.finishedActivitiys)
    return false;
  if (startedActivitiys != other.startedActivitiys)
    return false;

  return true;
}
bool RCPSPState_bi::operator==(const RCPSPState_bi &other) const {
  // if (this->expanded != other.expanded) {
  //   return false;
  // }
  if (this->activeTransitionIndices != other.activeTransitionIndices) {
    return false;
  }
  if (this->startedActivitiys != other.startedActivitiys) {
    return false;
  }
  // if (this->avilableTransition != other.avilableTransition) {
  //   return false;
  // }
  // if (this->activeTransitions != other.activeTransitions) {
  //   return false;
  // }
  if (this->finishedActivitiys != other.finishedActivitiys) {
    return false;
  }
  // if (this->marking != other.marking) {
  //   return false;
  // }
  return true;
}

RCPSPState_TT::RCPSPState_TT() {
  // Build mapping: resource name -> index (0-3)
  std::unordered_map<int, int> place_to_resource_idx;
  int res_count = 0;
  for (const auto &[resName, cap] : RCPSPex.resources) {
    int resID = petri.place_name_to_id.at(resName);
    place_to_resource_idx[resID] = res_count++;
  }

  // Count activity nodes
  int num_activities = petri.places.size() - res_count;
  activity_nodes.resize(num_activities);

  finishedActivitiys.fill(-1);
  int activity_counter = 0;
  for (int i = 0; i < petri.places.size(); ++i) {
    const auto &place = petri.places[i];

    // Identify Start/End Names
    if (place.arcs_out.empty())
      finalstatename = place.name;
    if (place.arcs_in.empty())
      initialstatename = place.name;

    // Check if resource node
    auto it = place_to_resource_idx.find(i);

    if (it != place_to_resource_idx.end()) {
      // This is a resource node
      int res_idx = it->second;

      if (place.name == initialstatename) {
        resource_nodes[res_idx].push_back({1, 0});
      } else if (!place.state.empty() && !place.state[0].empty()) {
        int val = place.state[0][0];
        if (val > 0)
          resource_nodes[res_idx].push_back({val, 0});
      }
    } else {
      // This is an activity node
      if (place.name == initialstatename) {
        activity_nodes[activity_counter] = {1, 0};
      } else if (!place.state.empty() && !place.state[0].empty()) {
        int val = place.state[0][0];
        activity_nodes[activity_counter] =
            (val > 0) ? std::make_pair<short, short>(val, 0)
                      : std::make_pair<short, short>(0, 0);
      } else {
        activity_nodes[activity_counter] = {0, 0};
      }
      activity_counter++;
    }
  }

  // Add resource capacities
  for (const auto &[resName, cap] : RCPSPex.resources) {
    if (cap > 0) {
      int resID = petri.place_name_to_id.at(resName);
      int res_idx = place_to_resource_idx[resID];

      if (resource_nodes[res_idx].empty()) {
        resource_nodes[res_idx].push_back({cap, 0});
      }
    }
  }

  g = 0;
}

std::vector<std::pair<short, short>>
consumeResourceList(const std::vector<std::pair<short, short>> &resource,
                    int amount, int currentTime) {
  if (amount < 1)
    return resource;

  // Check if already sorted to avoid unnecessary sorting
  std::vector<std::pair<short, short>> resourceCopy = resource;

  // Only sort if not already sorted (you could maintain sorted invariant)
  std::sort(resourceCopy.begin(), resourceCopy.end(),
            [](const auto &a, const auto &b) {
              return a.second > b.second; // DESCENDING by time
            });

  int remainingAmount = amount;

  for (auto &[qty, time] : resourceCopy) {
    if (time <= currentTime && remainingAmount > 0) {
      if (qty >= remainingAmount) {
        qty -= remainingAmount;
        remainingAmount = 0;
        break;
      } else {
        remainingAmount -= qty;
        qty = 0;
      }
    }
  }

  // Use erase-remove idiom efficiently
  resourceCopy.erase(std::remove_if(resourceCopy.begin(), resourceCopy.end(),
                                    [](const auto &p) { return p.first <= 0; }),
                     resourceCopy.end());

  return resourceCopy;
}

std::vector<std::pair<int, int>>
return_resource(const std::vector<std::pair<int, int>> &resource, int amount,
                int return_time) {

  // Create a copy of the input resource vector
  std::vector<std::pair<int, int>> resource_copy = resource;

  // If amount is less than 1, just return the copy without changes
  if (amount < 1) {
    return resource_copy;
  }

  // Check if there is already an entry with the return_time
  bool found = false;
  for (auto &item : resource_copy) {
    if (item.second == return_time) {
      // Found an existing entry with the same return time, add to it
      item.first += amount;
      found = true;
      break;
    }
  }

  // If no entry with the return_time was found, create a new one
  if (!found) {
    resource_copy.push_back({amount, return_time});
  }

  return resource_copy;
}



//!!!i changed map3 (early finish) havent chack correctness
double getForwardHcost_TT(std::vector<short> unstartedTransitions) {
  std::map<int, int>
      earlyfinishMap2; // Map to store activity IDs and their early finish times
  double h;
  std::set<int> processedDependencies;
  for (int activityId : unstartedTransitions) {
    int maxFinishTime = 0;
    std::set<int> processedDependencies;

    for (int dep :
         RCPSPex.backword_dependencies[activityId -
                                       1]) { // Changed from const auto& to int
      int depId = dep - 1;                   // No more std::stoi!

      if (std::find(unstartedTransitions.begin(), unstartedTransitions.end(),
                    depId + 1) != unstartedTransitions.end()) {
        maxFinishTime =
            std::max(maxFinishTime, earlyfinishMap2[depId + 1] +
                                        RCPSPex.activities[depId].duration);
      } else {
        maxFinishTime = std::max(maxFinishTime, earlyfinishMap2[depId + 1]);
      }
    }

    earlyfinishMap2[activityId] = maxFinishTime;
  }

  if (earlyfinishMap2.size() == 0) {
    h = 0;
  } else {
    h = earlyfinishMap2.rbegin()->second;
  }

  return h; // Added return statement
}
RCPSPState_TT::RCPSPState_TT(const RCPSPState_TT &prev, short transitionId,
                             short firingTime) {
  finishedActivitiys = prev.finishedActivitiys;
  resource_nodes = prev.resource_nodes;
  activity_nodes = prev.activity_nodes;

  const Transition &transition = petri.Transitions[transitionId - 1];
  const Activity &act = RCPSPex.activities[transitionId - 1];
  const short duration = act.duration;
  const short activityFinishTime = firingTime + duration;

  // Add output tokens (NO SORTING HERE)
  for (const auto &[placeID, outAmount] : transition.arcs_out_indices) {
    if (placeID < 4) {
      resource_nodes[placeID].push_back(
          {outAmount, firingTime + transition.duration});
    } else {
      short activity_idx = placeID - 4;
      activity_nodes[activity_idx] = {outAmount,
                                      firingTime + transition.duration};
    }
  }

  finishedActivitiys[transitionId] = activityFinishTime;

  // Consume resources (NO SORTING HERE)
  for (const auto &[resName, demand] : act.resource_demands) {
    if (demand > 0) {
      short resID = petri.place_name_to_id.at(resName);
      resource_nodes[resID] =
          consumeResourceList(resource_nodes[resID], demand, firingTime);
    }
  }

  // Calculate g
  g = 0;
  for (short finishTime : finishedActivitiys) {
    if (finishTime > g) {
      g = finishTime;
    }
  }
}

// std::vector<std::pair<short, short>> getAvailableTransitionIndices_TT(
//     const std::vector<short> &unstartedTransitions,
//     //const std::map<int, int> &finishedActivities,
//     const     std::vector<short> &finishedActivitiys,
//     const std::vector<std::vector<std::pair<short, short>>> &marking
// ) {
//     std::vector<std::pair<short, short>> available;
//
//     for (short transId : unstartedTransitions) {
//
//       const auto& dependencies = RCPSPex.backword_dependencies[transId - 1];
//
//       // DEBUG: Check if Task 2 (usually the first real task) has
//       dependencies if (transId == 2 && dependencies.empty()) {
//         std::cout << "❌ CRITICAL ERROR: Task 2 has NO dependencies! (JSON
//         Loading Failed)" << std::endl; exit(1); // Stop immediately so you
//         see this
//       }
//
//
//
//         const Activity &act = RCPSPex.activities[transId - 1];
//
//         // 1. Precedence constraints
//         bool allPredsFinished = true;
//         int maxPredFinishTime = 0;
//
//         for (const std::string &predStr :
//         RCPSPex.backword_dependencies[transId - 1]) {
//             int predId = std::stoi(predStr);
//
//             // THE FIX: Direct Vector Access (O(1))
//             // Get the time directly. If it is -1, it means "not finished".
//             int finishTime = finishedActivitiys[predId];
//
//             if (finishTime == -1) {
//                 allPredsFinished = false;
//                 break;
//             }
//             else {
//                 maxPredFinishTime = std::max(maxPredFinishTime, finishTime);
//             }
//         }
//
//         if (!allPredsFinished)
//             continue;
//
//         // 2. Resource availability
//         bool resourcesOK = true;
//         int maxResourceTime = maxPredFinishTime;
//       for (const auto &[res, demand] : act.resource_demands) {
//
//         // 1. Get ID
//         int resID = petri.place_name_to_id.at(res);
//
//         // 2. Check if resource has any tokens (Vector check)
//         if (marking[resID].empty()) {
//           resourcesOK = false;
//           break;
//         }
//
//         // 3. GET DATA (The Fix)
//         // We make a local copy 'tokens' because we are about to sort it.
//         // DO NOT use 'it->second'. Use 'marking[resID]'.
//         auto tokens = marking[resID];
//
//         // ... The rest of your logic is perfect ...
//         std::sort(tokens.begin(), tokens.end(),
//                   [](auto &a, auto &b) { return a.second < b.second; });
//
//         int totalAvailable = 0;
//         int resourceReadyTime = -1;
//
//         for (const auto &[amt, time] : tokens) {
//           if (time <= maxPredFinishTime) {
//             totalAvailable += amt;
//           }
//         }
//
//         if (totalAvailable >= demand) {
//           resourceReadyTime = maxPredFinishTime;
//         } else {
//           for (const auto &[amt, time] : tokens) {
//             if (time > maxPredFinishTime) {
//               totalAvailable += amt;
//               if (totalAvailable >= demand) {
//                 resourceReadyTime = time;
//                 break;
//               }
//             }
//           }
//         }
//
//         if (resourceReadyTime == -1) {
//           resourcesOK = false;
//           break;
//         }
//
//         maxResourceTime = std::max(maxResourceTime, resourceReadyTime);
//       }
//
//       if (resourcesOK) {
//         available.emplace_back(transId, maxResourceTime);
//       }
//     }
//
//     return available;
// }

std::vector<std::pair<short, short>> getAvailableTransitionIndices_TT(
    const std::vector<short> &unstartedTransitions,
    const std::vector<short> &finishedActivitiys,
    const std::array<std::vector<std::pair<short, short>>, 4> &resource_nodes,
    const std::vector<std::pair<short, short>> &activity_nodes);

std::vector<std::pair<short, short>> getAvailableTransitionIndices_TT(
    const std::vector<short> &unstartedTransitions,
    const std::array<short, MAX_ACTIVITIES> &finishedActivitiys,
    const std::array<std::vector<std::pair<short, short>>, 4> &resource_nodes,
    const std::vector<std::pair<short, short>> &activity_nodes) {
  std::vector<std::pair<short, short>> available;

  for (short transId : unstartedTransitions) {
    const auto &dependencies = RCPSPex.backword_dependencies[transId - 1];

    // DEBUG: Check if Task 2 has dependencies
    if (transId == 2 && dependencies.empty()) {
      std::cout << "❌ CRITICAL ERROR: Task 2 has NO dependencies! (JSON "
                   "Loading Failed)"
                << std::endl;
      exit(1);
    }

    const Activity &act = RCPSPex.activities[transId - 1];

    // 1. Precedence constraints
    bool allPredsFinished = true;
    int maxPredFinishTime = 0;

    for (int predId : RCPSPex.backword_dependencies[transId - 1]) {
      int finishTime = finishedActivitiys[predId];

      if (finishTime == -1) {
        allPredsFinished = false;
        break;
      } else {
        maxPredFinishTime = std::max(maxPredFinishTime, finishTime);
      }
    }

    if (!allPredsFinished)
      continue;

    // 2. Resource availability
    bool resourcesOK = true;
    int maxResourceTime = maxPredFinishTime;

    // Sort resources only once per transition check
    std::array<std::vector<std::pair<short, short>>, 4> sorted_resources;
    bool resources_sorted[4] = {false, false, false, false};

    for (const auto &[res, demand] : act.resource_demands) {
      int resID = petri.place_name_to_id.at(res);

      if (resource_nodes[resID].empty()) {
        resourcesOK = false;
        break;
      }

      // Sort only once per resource, only if needed
      if (!resources_sorted[resID]) {
        sorted_resources[resID] = resource_nodes[resID];
        std::sort(
            sorted_resources[resID].begin(), sorted_resources[resID].end(),
            [](const auto &a, const auto &b) { return a.second < b.second; });
        resources_sorted[resID] = true;
      }

      const auto &tokens = sorted_resources[resID];

      int totalAvailable = 0;
      int resourceReadyTime = -1;

      // Count tokens available at maxPredFinishTime
      for (const auto &[amt, time] : tokens) {
        if (time <= maxPredFinishTime) {
          totalAvailable += amt;
        }
      }

      if (totalAvailable >= demand) {
        resourceReadyTime = maxPredFinishTime;
      } else {
        // Check future tokens
        for (const auto &[amt, time] : tokens) {
          if (time > maxPredFinishTime) {
            totalAvailable += amt;
            if (totalAvailable >= demand) {
              resourceReadyTime = time;
              break;
            }
          }
        }
      }

      if (resourceReadyTime == -1) {
        resourcesOK = false;
        break;
      }

      maxResourceTime = std::max(maxResourceTime, resourceReadyTime);
    }

    if (resourcesOK) {
      available.emplace_back(transId, maxResourceTime);
    }
  }

  return available;
}

bool RCPSPState_TT::operator==(const RCPSPState_TT &other) const {
  return finishedActivitiys == other.finishedActivitiys &&
         activity_nodes == other.activity_nodes &&
         resource_nodes == other.resource_nodes;
}
