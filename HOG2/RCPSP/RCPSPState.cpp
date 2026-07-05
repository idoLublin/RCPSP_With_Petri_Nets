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
#ifdef USE_ORTOOLS
#include "ortools/sat/cp_model.h" // EER/GER 0-1 IP feasibility tests (depth >=5)
#endif
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

  double finalLB = 0.0;

  // Process each resource type
  for (const auto &res : RCPSPex.resources) {
    const std::string &resName = res.first;
    short capacity = res.second;
    double workContentTank = 0.0;
    int currentTime = 0;

    // Process each EST event in order
    for (int est : sortedESTs) {
      // Step 1: Drain the tank (outflow)
      int deltaT = est - currentTime;
      workContentTank -= deltaT * capacity;

      // Step 2: Apply idle time constraint (tank cannot go negative)
      if (workContentTank < 0.0) {
        workContentTank = 0.0;
      }

      // Step 3: Update current time
      currentTime = est;

      // Step 4: Fill the tank (inflow) - add work content of activities at this
      // EST
      for (int activityId : estToActivities[est]) {
        const auto &activity = RCPSPex.activities[activityId - 1];
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

  double finalLB = 0.0;

  // Process each resource type
  for (const auto &res : RCPSPex.resources) {
    const std::string &resName = res.first;
    short capacity = res.second;
    double workContentTank = 0.0;
    int currentTime = 0;

    // Process each EST event
    for (int relEST : sortedRelativeESTs) {
      // Drain
      int deltaT = relEST - currentTime;
      workContentTank -= deltaT * capacity;

      // Idle time constraint
      if (workContentTank < 0.0) {
        workContentTank = 0.0;
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

// ============================================================================
// Enhanced Energetic Reasoning Lower Bound (LBER)
// Haouari, Kooli, Neron, "Enhanced energetic reasoning-based lower bounds for
// the resource constrained project scheduling problem", Computers & Operations
// Research, 2012.
//
// Destructive lower bound: for a trial (remaining) makespan C, derive per-
// activity release r_j / deadline d_j from the precedence longest paths and run
// energetic feasibility tests on the O(n^2) relevant intervals. If some
// resource is overloaded, C is infeasible and the bound is C+1; the smallest C
// that is not proven infeasible is a valid lower bound.
//
// Phase 1 implements the classical energetic reasoning test (CER). The cascade
// is depth-parameterised by cost (cheapest-first, short-circuiting). The inner
// per-interval feasibility test strengthens with depth: depth>=2 DFF,
// >=4 RER, >=5 EER, >=6 GER. SHV (depth>=3) is the efficient shaving procedure
// applied by the root pipeline as a wrap around these tests (not inside
// erInfeasible). All of these are added in later phases and are no-ops here.
// Applied to the RESIDUAL problem at a node, the result is an admissible lower
// bound on the REMAINING makespan.
// ============================================================================

// Precomputation cache (thread-local; reset per problem in runSolver()).
thread_local std::vector<int> tailAfter; // longest path from finish of j to sink (1-based)
thread_local std::vector<std::vector<int>> erDemand; // erDemand[k][j] = b_jk (1-based j)
thread_local std::vector<int> erCapacity;            // capacity B_k per resource index
thread_local bool energeticInitialized = false;

// Reusable per-call scratch (thread-local) so the per-node hot path performs no
// heap allocation. Indexed by activity id (1-based); only the current residual
// ids are read, so stale entries are harmless.
thread_local std::array<int, MAX_ACTIVITIES + 2> erP{}; // residual processing time
thread_local std::array<int, MAX_ACTIVITIES + 2> erR{}; // residual release date
thread_local std::array<int, MAX_ACTIVITIES + 2> erD{}; // deadline at trial C
thread_local std::vector<int> erO1, erO2;               // interval endpoints
thread_local std::vector<int> erEST; // global earliest start (1-based), tightened by root shaving

// Root-mode (SHV) state: the destructive + shaving bound is computed once per
// problem and tightens erEST[]/tailAfter[] for the per-node bound.
thread_local int lberRootBound = 0;
thread_local bool lberRootComputed = false;

void initializeEnergetic() {
  if (energeticInitialized)
    return;
  if (!lbccInitialized)
    initializeLBcc(); // ensures estValues[] (EST) is populated

  int n = RCPSPex.activities.size();

  // tailAfter[j] = longest path (in durations) from the FINISH of activity j to
  // the project end, i.e. max over successors s of (p_s + tailAfter[s]).
  // Activities are in topological order, so process them in reverse.
  tailAfter.assign(n + 2, 0);
  for (int j = n; j >= 1; j--) {
    int best = 0;
    for (int s : RCPSPex.dependencies[j - 1]) {
      int cand = RCPSPex.activities[s - 1].duration + tailAfter[s];
      best = std::max(best, cand);
    }
    tailAfter[j] = best;
  }

  // Dense per-resource demand matrix + capacities (avoids std::map lookups in
  // the O(n^2) interval hot loop).
  int K = RCPSPex.resources.size();
  erCapacity.assign(K, 0);
  erDemand.assign(K, std::vector<int>(n + 1, 0));
  for (int k = 0; k < K; k++) {
    const std::string &resName = RCPSPex.resources[k].first;
    erCapacity[k] = RCPSPex.resources[k].second;
    for (int j = 1; j <= n; j++) {
      const auto &dem = RCPSPex.activities[j - 1].resource_demands;
      auto it = dem.find(resName);
      if (it != dem.end())
        erDemand[k][j] = it->second;
    }
  }

  // Global earliest starts (copy of EST); root shaving tightens these in place.
  erEST.assign(n + 2, 0);
  for (int j = 1; j <= n; j++)
    erEST[j] = estValues[j];

  energeticInitialized = true;
}

// Mandatory work of activity j (demand b, processing p, release r, deadline d)
// inside interval [t1,t2]: min of its "left work" and "right work".
static inline long long erMandatoryWork(int b, int p, int r, int d, int t1,
                                        int t2) {
  if (b == 0)
    return 0;
  int span = t2 - t1;
  int wl = std::min(std::min(span, p), std::max(0, r + p - t1));
  int wr = std::min(std::min(span, p), std::max(0, t2 - d + p));
  int w = std::min(wl, wr);
  return (w > 0) ? (long long)b * w : 0;
}

// Reduced processing time of activity j inside [t1,t2] (= W_jk / b_jk):
// p_j(t1,t2) = min(t2-t1, p, max(0,r+p-t1), max(0,t2-d+p)).
static inline int erReducedP(int p, int r, int d, int t1, int t2) {
  int span = t2 - t1;
  int v = std::min(std::min(span, p),
                   std::min(std::max(0, r + p - t1), std::max(0, t2 - d + p)));
  return v > 0 ? v : 0;
}

// Fekete-Schepers DFF f^2_s applied to value x with capacity B (s in {1,2}):
// f(x) = x if x(s+1) mod B == 0, else floor(x(s+1)/B) * (B/s). f(B) = B.
static inline long long dff2(long long x, long long B, int s) {
  if (x <= 0 || B <= 0)
    return 0;
  long long num = x * (s + 1);
  if (num % B == 0)
    return x;
  return (num / B) * (B / s);
}

// 0-1 knapsack via DP: maximize total value with total weight <= cap.
// items = (weight, value) pairs. Reuses a thread-local DP buffer.
thread_local std::vector<long long> erKnap;
static long long erKnapsackMax(const std::vector<std::pair<int, long long>> &items,
                               int cap) {
  if (cap <= 0)
    return 0;
  erKnap.assign(cap + 1, 0);
  for (const auto &it : items) {
    int w = it.first;
    long long v = it.second;
    if (w <= 0 || w > cap || v <= 0)
      continue;
    for (int c = cap; c >= w; c--)
      erKnap[c] = std::max(erKnap[c], erKnap[c - w] + v);
  }
  return erKnap[cap];
}

// RER (relaxed revisited energetic reasoning), depth>=4. For resource k over
// interval [t1,t2], compute the relaxed work estimate W~_k via the 7-subset
// partition + two independent knapsacks (P2 with constraints (11),(12) relaxed),
// and test Property 4: W~_k > m_k*(t2-t1) => infeasible. Uses the current
// windows erR/erD/erP. m^l_k/m^r_k use the admissible over-approximation
// min(placeable-demand, m_k) (a larger left/right capacity only weakens the
// bound, never breaks admissibility). W~_k <= the exact min work, so it is a
// valid lower bound on the work.
thread_local std::vector<std::pair<int, long long>> erLeftItems, erRightItems;
static bool rerInfeasibleInterval(const std::vector<short> &ids, int t1, int t2,
                                  int k) {
  long long span = t2 - t1;
  int B = erCapacity[k];
  const std::vector<int> &dem = erDemand[k];

  // m_k = B_k - sum of demands of A_0 (activities spanning the whole interval).
  long long a0 = 0;
  for (short j : ids) {
    int b = dem[j];
    if (b == 0)
      continue;
    int rj = erR[j], dj = erD[j], pj = erP[j];
    if (!((rj + pj > t1) && (dj - pj < t2)))
      continue; // not in A(t1,t2)
    if ((rj + pj >= t2) && (dj - pj <= t1))
      a0 += b; // A_0: whole-interval
  }
  long long mk = (long long)B - a0;
  if (mk < 0)
    return true; // forced whole-interval demand already exceeds capacity

  long long baseline = 0, sumL = 0, sumR = 0;
  erLeftItems.clear();
  erRightItems.clear();
  for (short j : ids) {
    int b = dem[j];
    if (b == 0)
      continue;
    int rj = erR[j], dj = erD[j], pj = erP[j];
    if (!((rj + pj > t1) && (dj - pj < t2)))
      continue;
    if ((rj + pj >= t2) && (dj - pj <= t1))
      continue; // A_0 already counted in m_k
    long long Wi = (long long)b * pj;
    long long Wl = (long long)b * std::min(std::min((int)span, pj), std::max(0, rj + pj - t1));
    long long Wr = (long long)b * std::min(std::min((int)span, pj), std::max(0, t2 - dj + pj));

    bool Lonly = (rj + pj < t2) && (dj - pj <= t1);
    bool Ronly = (rj + pj >= t2) && (dj - pj > t1);
    bool Ionly = (t1 < rj) && (dj < t2);
    if (Lonly) {
      baseline += Wl;
    } else if (Ronly) {
      baseline += Wr;
    } else if (Ionly) {
      baseline += Wi;
    } else {
      // mixed (A_LI / A_RI / A_LR / A_LIR): baseline = full inside work W^i,
      // and offer left/right "savings" items to the knapsacks per placeability.
      baseline += Wi;
      bool aLI = (rj <= t1) && (dj < t2) && (dj - pj > t1);
      bool aRI = (rj > t1) && (dj >= t2) && (rj + pj < t2);
      bool common = (rj <= t1) && (dj >= t2) && (rj + pj < t2) && (dj - pj > t1);
      bool plL = aLI || common;           // A_LI, A_LR, A_LIR
      bool plR = aRI || common;           // A_RI, A_LR, A_LIR
      if (plL) { erLeftItems.push_back({b, Wi - Wl}); sumL += b; }
      if (plR) { erRightItems.push_back({b, Wi - Wr}); sumR += b; }
    }
  }
  long long mlk = std::min(sumL, mk);
  long long mrk = std::min(sumR, mk);
  long long Ek = erKnapsackMax(erLeftItems, (int)mlk) +
                 erKnapsackMax(erRightItems, (int)mrk);
  long long Wtilde = baseline - Ek;
  return Wtilde > mk * span;
}

#ifdef USE_ORTOOLS
namespace ors = operations_research::sat;

// Classify a mixed (left/right/inside-flexible) activity for interval [t1,t2].
// The partition is resource-independent. Returns true if j is "mixed" and sets
// placeability flags; false if j is forced (or not in A(t1,t2)\A_0).
static inline bool erClassifyMixed(int rj, int dj, int pj, int t1, int t2,
                                   bool &plL, bool &plR, bool &plI) {
  long long span = t2 - t1;
  bool aLI = (rj <= t1) && (dj < t2) && (dj - pj > t1);
  bool aRI = (rj > t1) && (dj >= t2) && (rj + pj < t2);
  bool common = (rj <= t1) && (dj >= t2) && (rj + pj < t2) && (dj - pj > t1);
  if (!(aLI || aRI || common))
    return false;
  plL = aLI || common;                      // A_LI, A_LR, A_LIR
  plR = aRI || common;                      // A_RI, A_LR, A_LIR
  plI = aLI || aRI || (common && pj < span - 1); // A_LI, A_RI, A_LIR (not A_LR)
  return true;
}

// EER (exact revisited ER), depth>=5: solve the per-resource 0-1 IP (P1) for the
// minimum inside-work E_k via CP-SAT, then W_bar_k = E_k + forced work
// (Property 3: W_bar_k > m_k*(t2-t1) => infeasible). Only acts on a proven
// OPTIMAL solve; otherwise returns false (cannot prove -> stay admissible).
static bool eerInfeasibleInterval(const std::vector<short> &ids, int t1, int t2,
                                  int k) {
  long long span = t2 - t1;
  const std::vector<int> &dem = erDemand[k];
  long long a0 = 0, forced = 0, sumL = 0, sumR = 0;
  struct Mx { long long b, Wl, Wr, Wi; bool plL, plR, plI; };
  std::vector<Mx> mixed;
  for (short j : ids) {
    int b = dem[j];
    if (b == 0)
      continue;
    int rj = erR[j], dj = erD[j], pj = erP[j];
    if (!((rj + pj > t1) && (dj - pj < t2)))
      continue;
    if ((rj + pj >= t2) && (dj - pj <= t1)) { a0 += b; continue; }
    long long Wi = (long long)b * pj;
    long long Wl = (long long)b * std::min(std::min((int)span, pj), std::max(0, rj + pj - t1));
    long long Wr = (long long)b * std::min(std::min((int)span, pj), std::max(0, t2 - dj + pj));
    bool Lonly = (rj + pj < t2) && (dj - pj <= t1);
    bool Ronly = (rj + pj >= t2) && (dj - pj > t1);
    bool Ionly = (t1 < rj) && (dj < t2);
    if (Lonly) { forced += Wl; }
    else if (Ronly) { forced += Wr; }
    else if (Ionly) { forced += Wi; }
    else {
      bool plL, plR, plI;
      erClassifyMixed(rj, dj, pj, t1, t2, plL, plR, plI);
      mixed.push_back({b, Wl, Wr, Wi, plL, plR, plI});
      if (plL) sumL += b;
      if (plR) sumR += b;
    }
  }
  long long mk = (long long)erCapacity[k] - a0;
  if (mk < 0)
    return true;
  if (mixed.empty())
    return forced > mk * span;
  if ((int)mixed.size() > 30)
    return false; // gate large IPs for tractability
  long long mlk = std::min(sumL, mk), mrk = std::min(sumR, mk);

  ors::CpModelBuilder cp;
  ors::LinearExpr obj, capL, capR;
  for (const auto &a : mixed) {
    ors::BoolVar x = cp.NewBoolVar(), y = cp.NewBoolVar(), z = cp.NewBoolVar();
    if (!a.plL) cp.AddEquality(x, 0);
    if (!a.plR) cp.AddEquality(y, 0);
    if (!a.plI) cp.AddEquality(z, 0);
    cp.AddEquality(x + y + z, 1);
    obj += a.Wl * x; obj += a.Wr * y; obj += a.Wi * z;
    capL += a.b * x; capR += a.b * y;
  }
  cp.AddLessOrEqual(capL, mlk);
  cp.AddLessOrEqual(capR, mrk);
  cp.Minimize(obj);
  ors::SatParameters params;
  params.set_max_time_in_seconds(1.0);
  params.set_num_search_workers(1);
  const ors::CpSolverResponse resp = ors::SolveWithParameters(cp.Build(), params);
  if (resp.status() != ors::CpSolverStatus::OPTIMAL)
    return false;
  long long Ek = (long long)(resp.objective_value() + 0.5);
  return (Ek + forced) > mk * span;
}

// GER (global revisited ER), depth>=6: one IP (P3) over ALL resources with
// shared position vars and per-resource slack e_k; min sum e_k; Property 5:
// optimum > 0 => infeasible. Partition is resource-independent. OPTIMAL-only.
static bool gerInfeasibleInterval(const std::vector<short> &ids, int t1, int t2) {
  long long span = t2 - t1;
  int K = erCapacity.size();
  std::vector<long long> mk(K), forced(K, 0);
  for (int k = 0; k < K; k++) mk[k] = erCapacity[k];
  std::vector<short> mixedIds;
  std::vector<std::array<bool, 3>> place; // plL, plR, plI per mixed activity
  for (short j : ids) {
    int rj = erR[j], dj = erD[j], pj = erP[j];
    if (!((rj + pj > t1) && (dj - pj < t2)))
      continue;
    bool whole = (rj + pj >= t2) && (dj - pj <= t1);
    if (whole) { for (int k = 0; k < K; k++) mk[k] -= erDemand[k][j]; continue; }
    bool Lonly = (rj + pj < t2) && (dj - pj <= t1);
    bool Ronly = (rj + pj >= t2) && (dj - pj > t1);
    bool Ionly = (t1 < rj) && (dj < t2);
    if (Lonly || Ronly || Ionly) {
      for (int k = 0; k < K; k++) {
        int b = erDemand[k][j];
        if (!b) continue;
        long long Wi = (long long)b * pj;
        long long Wl = (long long)b * std::min(std::min((int)span, pj), std::max(0, rj + pj - t1));
        long long Wr = (long long)b * std::min(std::min((int)span, pj), std::max(0, t2 - dj + pj));
        forced[k] += Lonly ? Wl : (Ronly ? Wr : Wi);
      }
    } else {
      bool plL, plR, plI;
      erClassifyMixed(rj, dj, pj, t1, t2, plL, plR, plI);
      mixedIds.push_back(j);
      place.push_back({plL, plR, plI});
    }
  }
  for (int k = 0; k < K; k++)
    if (mk[k] < 0) return true;
  if (mixedIds.empty()) {
    for (int k = 0; k < K; k++)
      if (forced[k] > mk[k] * span) return true;
    return false;
  }
  if ((int)mixedIds.size() > 30)
    return false;

  ors::CpModelBuilder cp;
  size_t M = mixedIds.size();
  std::vector<ors::BoolVar> X(M), Y(M), Z(M);
  for (size_t i = 0; i < M; i++) {
    X[i] = cp.NewBoolVar(); Y[i] = cp.NewBoolVar(); Z[i] = cp.NewBoolVar();
    if (!place[i][0]) cp.AddEquality(X[i], 0);
    if (!place[i][1]) cp.AddEquality(Y[i], 0);
    if (!place[i][2]) cp.AddEquality(Z[i], 0);
    cp.AddEquality(X[i] + Y[i] + Z[i], 1);
  }
  ors::LinearExpr objSlack;
  for (int k = 0; k < K; k++) {
    ors::LinearExpr capL, capR, energy;
    for (size_t i = 0; i < M; i++) {
      short j = mixedIds[i];
      int b = erDemand[k][j];
      if (!b) continue;
      int rj = erR[j], dj = erD[j], pj = erP[j];
      long long Wi = (long long)b * pj;
      long long Wl = (long long)b * std::min(std::min((int)span, pj), std::max(0, rj + pj - t1));
      long long Wr = (long long)b * std::min(std::min((int)span, pj), std::max(0, t2 - dj + pj));
      capL += (long long)b * X[i];
      capR += (long long)b * Y[i];
      energy += Wl * X[i]; energy += Wr * Y[i]; energy += Wi * Z[i];
    }
    cp.AddLessOrEqual(capL, mk[k]);
    cp.AddLessOrEqual(capR, mk[k]);
    long long Ek = mk[k] * span - forced[k]; // available work for mixed activities
    if (Ek < 0) Ek = 0;
    ors::IntVar ek = cp.NewIntVar(operations_research::Domain(0, mk[k] * span + 1));
    cp.AddLessOrEqual(energy - ek, Ek);
    objSlack += ek;
  }
  cp.Minimize(objSlack);
  ors::SatParameters params;
  params.set_max_time_in_seconds(2.0);
  params.set_num_search_workers(1);
  const ors::CpSolverResponse resp = ors::SolveWithParameters(cp.Build(), params);
  if (resp.status() != ors::CpSolverStatus::OPTIMAL)
    return false;
  return resp.objective_value() > 0.5; // xi > 0 => infeasible
}
#endif // USE_ORTOOLS

// CER overload test over the CURRENT windows erR[]/erD[]/erP[] for the given
// ids. Returns true iff some resource is overloaded on some relevant interval.
// Cheapest-first; short-circuits on the first overload. (Shaving calls this
// directly after mutating erR/erD; the destructive loop sets erD from C first.)
static bool erOverload(const std::vector<short> &ids, int depth) {
  // Interval endpoint sets O1 = {r_j} U {d_j - p_j}, O2 = {d_j} U {r_j + p_j}.
  erO1.clear();
  erO2.clear();
  for (short j : ids) {
    erO1.push_back(erR[j]);
    erO1.push_back(erD[j] - erP[j]);
    erO2.push_back(erD[j]);
    erO2.push_back(erR[j] + erP[j]);
  }
  std::sort(erO1.begin(), erO1.end());
  erO1.erase(std::unique(erO1.begin(), erO1.end()), erO1.end());
  std::sort(erO2.begin(), erO2.end());
  erO2.erase(std::unique(erO2.begin(), erO2.end()), erO2.end());

  int K = erCapacity.size();

  // ---- depth 1: CER (classical energetic reasoning), per resource ----
  for (int t1 : erO1) {
    for (int t2 : erO2) {
      if (t2 <= t1)
        continue;
      long long span = t2 - t1;
      for (int k = 0; k < K; k++) {
        const std::vector<int> &dem = erDemand[k];
        long long avail = (long long)erCapacity[k] * span;
        long long Wk = 0;
        for (short j : ids) {
          int b = dem[j];
          if (b == 0)
            continue;
          Wk += erMandatoryWork(b, erP[j], erR[j], erD[j], t1, t2);
          if (Wk > avail)
            return true; // overload -> infeasible (short-circuit)
        }
        // ---- depth >= 2: DFF capacity bound on the reduced instance. Apply the
        // Fekete-Schepers DFF f^2_s to demands; f(B_k)=B_k, so infeasibility
        // (Corollary 1, transformed reduced LB_C > t2-t1) is f-work > B_k*span.
        if (depth >= 2) {
          for (int s = 1; s <= 2; s++) {
            long long fwork = 0;
            for (short j : ids) {
              int b = dem[j];
              if (b == 0)
                continue;
              int pr = erReducedP(erP[j], erR[j], erD[j], t1, t2);
              if (pr == 0)
                continue;
              fwork += dff2(b, erCapacity[k], s) * pr;
            }
            if (fwork > (long long)erCapacity[k] * span)
              return true;
          }
        }
        // ---- depth >= 4: RER (relaxed revisited), a tighter per-resource test
        if (depth >= 4 && rerInfeasibleInterval(ids, t1, t2, k))
          return true;
#ifdef USE_ORTOOLS
        // ---- depth >= 5: EER (exact revisited), per-resource 0-1 IP
        if (depth >= 5 && eerInfeasibleInterval(ids, t1, t2, k))
          return true;
#endif
      }
#ifdef USE_ORTOOLS
      // ---- depth >= 6: GER (global revisited), single IP over all resources
      if (depth >= 6 && gerInfeasibleInterval(ids, t1, t2))
        return true;
#endif
    }
  }

  // Stronger inner tests added in later phases: depth >= 2 (DFF),
  // >= 5 (EER), >= 6 (GER). SHV (depth >= 3) wraps this test in the root
  // pipeline rather than running here.
  return false;
}

// Set deadlines d_j = C - tailAfter[j] for the given ids, then run the overload
// test. True iff trial makespan C is proven infeasible.
static bool erInfeasibleC(const std::vector<short> &ids, int C, int depth) {
  for (short j : ids)
    erD[j] = C - tailAfter[j];
  return erOverload(ids, depth);
}

// Energetic-reasoning lower bound on the REMAINING makespan from a search node.
// Builds the residual instance (unstarted + in-progress activities, origin
// shifted to currentMakespan) and runs the destructive loop. Admissible: never
// exceeds the true remaining makespan.
double computeEnergeticLB(
    const std::vector<short> &unfinishedActivities,
    const std::vector<std::pair<short, short>> &activeTransitions,
    int currentMakespan, int depth) {
  if (!energeticInitialized)
    initializeEnergetic();
  if (unfinishedActivities.size() < 2)
    return 0; // CER cannot overload with <2 activities; CP handles it

  // EER/GER (depth >= 5) solve a CP-SAT IP per interval — far too slow to run at
  // every A* node. They are used only as a one-shot ROOT bound (computeRootBound);
  // the per-node test is capped at the polynomial tiers (CER/DFF/RER).
  if (depth > 4)
    depth = 4;

  // Remaining processing time of in-progress activities.
  std::array<short, MAX_ACTIVITIES> activeRemaining;
  activeRemaining.fill(-1);
  for (const auto &[idx, rem] : activeTransitions)
    activeRemaining[idx] = rem;

  // Residual forward pass: earliest finish (origin = currentMakespan), giving a
  // valid (true) residual release r_j = earlyFinish_j - p_j for each activity.
  // Fills the thread-local scratch erP/erR (no allocation).
  std::array<int, MAX_ACTIVITIES> earlyFinish;
  earlyFinish.fill(-1);
  long long sumP = 0;
  int cInit = 0;
  for (short j : unfinishedActivities) {
    int maxPredFinish = 0;
    for (int dep : RCPSPex.backword_dependencies[j - 1]) {
      if (earlyFinish[dep] != -1)
        maxPredFinish = std::max(maxPredFinish, earlyFinish[dep]);
      else if (activeRemaining[dep] != -1)
        maxPredFinish = std::max(maxPredFinish, (int)activeRemaining[dep]);
    }
    int pj = (activeRemaining[j] != -1) ? activeRemaining[j]
                                        : RCPSPex.activities[j - 1].duration;
    erP[j] = pj;
    earlyFinish[j] = maxPredFinish + pj;
    // Residual release: the residual forward pass, tightened by the global
    // earliest start (erEST, possibly raised by root shaving), shifted into the
    // residual frame. Both are valid lower bounds on j's residual start.
    int rj = earlyFinish[j] - pj;
    int rGlobal = erEST[j] - currentMakespan;
    if (rGlobal > rj)
      rj = rGlobal;
    if (rj < 0)
      rj = 0;
    erR[j] = rj;
    sumP += pj;
    // residual critical-path bound: earliest finish of j + its downstream tail.
    cInit = std::max(cInit, earlyFinish[j] + tailAfter[j]);
  }

  // Common case: the critical-path bound is already feasible, so energetic
  // reasoning does not improve it -> a single feasibility test returns cInit.
  if (!erInfeasibleC(unfinishedActivities, cInit, depth))
    return (double)cInit;

  // Otherwise the bound is strictly above cInit. Binary search the smallest
  // C in (cInit, cMax] not proven infeasible (erInfeasible is monotone in C:
  // larger C widens windows -> less mandatory work -> no new infeasibility).
  int cMax = (int)sumP; // serial schedule is always feasible -> upper bound
  if (cMax < cInit)
    cMax = cInit;
  int lo = cInit + 1, hi = cMax;
  while (lo < hi) {
    int mid = lo + (hi - lo) / 2;
    if (erInfeasibleC(unfinishedActivities, mid, depth))
      lo = mid + 1;
    else
      hi = mid;
  }
  return (double)lo;
}

// Root-mode SHV: run the destructive loop + dichotomous shaving (Algorithm 1 of
// Haouari, Kooli, Neron 2012) ONCE on the full problem. Stores the root lower
// bound (lberRootBound) and tightens erEST[]/tailAfter[] so the per-node bound
// is stronger. Self-contained: uses only the CER overload test. depth>=3 enables
// the shaving inner loop; depth<3 yields the plain destructive CER bound.
void computeRootBound(int depth) {
  if (lberRootComputed)
    return;
  if (!energeticInitialized)
    initializeEnergetic();
  lberRootComputed = true;

  // The per-activity shaving inner loop runs many feasibility tests, so it uses
  // the cheap polynomial tiers (CER/RER). The expensive EER/GER (depth >= 5)
  // run only as the once-per-trial-C GLOBAL feasibility check below.
  int shaveDepth = std::min(depth, 4);

  int n = RCPSPex.activities.size();
  if (n < 2) {
    lberRootBound = 0;
    return;
  }

  std::vector<short> ids;
  ids.reserve(n);
  for (int j = 1; j <= n; j++)
    ids.push_back((short)j);

  long long sumP = 0;
  int cpLB = 0;
  for (int j = 1; j <= n; j++) {
    erP[j] = RCPSPex.activities[j - 1].duration;
    sumP += erP[j];
    cpLB = std::max(cpLB, erEST[j] + erP[j] + tailAfter[j]); // critical-path LB
  }
  int cMax = (int)sumP; // serial schedule is feasible -> upper bound on makespan
  if (cMax < cpLB)
    cMax = cpLB;

  int C = cpLB; // destructive: start from the critical-path bound
  bool solved = false;
  while (!solved && C <= cMax) {
    // (Re)initialise windows for trial C; shaving below only tightens them.
    for (short j : ids) {
      erR[j] = erEST[j];
      erD[j] = C - tailAfter[j];
    }

    // Line 2: global energetic feasibility at this C.
    if (erOverload(ids, depth)) {
      C++;
      continue;
    }
    if (depth < 3) { // no shaving requested -> plain destructive CER bound
      solved = true;
      break;
    }

    // Lines 3-13: dichotomous shaving to a fixpoint.
    bool changed = true, bumped = false;
    while (changed && !bumped) {
      changed = false;
      for (short j : ids) {
        int lo = erR[j];
        int hi = erD[j] - erP[j]; // latest start
        if (hi <= lo)
          continue; // start-time window too small to split
        int Mj = (lo + hi + 1) / 2; // ceil((r_j + d_j - p_j)/2)
        int savedR = erR[j], savedD = erD[j];

        // I^1_j: force j into the left half (finish by M_j+p_j-1).
        erD[j] = Mj + erP[j] - 1;
        bool leftInf = erOverload(ids, shaveDepth);
        erD[j] = savedD;

        // I^2_j: force j into the right half (start at/after M_j).
        erR[j] = Mj;
        bool rightInf = erOverload(ids, shaveDepth);
        erR[j] = savedR;

        if (leftInf && rightInf) {
          bumped = true; // neither half possible -> C infeasible
          break;
        } else if (leftInf) {
          erR[j] = Mj; // cannot be left -> r_j <- M_j
          changed = true;
        } else if (rightInf) {
          erD[j] = Mj + erP[j] - 1; // cannot be right -> d_j <- M_j+p_j-1
          changed = true;
        }
      }
    }
    if (bumped) {
      C++;
      continue;
    }
    solved = true; // fixpoint reached with no infeasibility -> C is feasible
  }

  lberRootBound = C;
  // Persist tightened windows (only ever tighter, hence still valid lower
  // bounds): erEST <- shaved release; tailAfter <- C - shaved deadline.
  for (short j : ids) {
    erEST[j] = erR[j];
    int tnew = C - erD[j];
    if (tnew > tailAfter[j])
      tailAfter[j] = tnew;
  }
}

// (defined below in this translation unit; declared here for the helper above it)
double getForwardHcostDP(
    const std::vector<short> &unstartedTransitions,
    const std::vector<std::pair<short, short>> &activeTransitionIndices);

// Resource work / capacity lower bound (the "lbrc" term). For each resource, the
// remaining work that must still be processed on it -- active tasks contribute
// (residual duration x demand), unstarted tasks contribute (full duration x
// demand) -- divided by the resource capacity. The max over resources is a valid
// lower bound on the remaining makespan (that much work cannot be packed into
// less time than work/capacity). It is a pure function of the state (no g), so
// it is admissible AND consistent: across a step of length delta the work on any
// resource drops by at most capacity*delta, so work/capacity drops by <= delta.
double computeResourceWorkLB(
    const std::vector<short> &unfinishedActivities,
    const std::vector<std::pair<short, short>> &activeTransitions) {
  std::array<short, MAX_ACTIVITIES> activeRemaining;
  activeRemaining.fill(-1);
  for (const auto &[id, remaining] : activeTransitions)
    activeRemaining[id] = remaining;

  double best = 0.0;
  for (const auto &[resName, cap] : RCPSPex.resources) {
    if (cap <= 0) continue;
    long long work = 0;
    for (short id : unfinishedActivities) {
      const auto &dem = RCPSPex.activities[id - 1].resource_demands;
      auto it = dem.find(resName);
      if (it == dem.end() || it->second <= 0) continue;
      int dur = (activeRemaining[id] != -1) ? (int)activeRemaining[id]
                                            : RCPSPex.activities[id - 1].duration;
      work += (long long)dur * it->second;
    }
    double b = (double)work / (double)cap;
    if (b > best) best = b;
  }
  return best;
}

// Consistent Class-1 LBER bound used by the TT2 search.
//
// Per problem (once): compute the full destructive LBER root bound
// `lberRootBound` (energetic reasoning + DFF + IP at depth>=5). It is a VALID
// lower bound on the project makespan C* (lberRootBound <= C*).
//
// Per node: h(s) = max( residual critical-path(s), lberRootBound - g(s) ).
//   - The critical-path term (getForwardHcostDP) is the longest-path bound of
//     the precedence relaxation: admissible and consistent (drops by <= the
//     step's time advance).
//   - The floor term lberRootBound - g(s): admissible because
//     lberRootBound - g(s) <= C* - g(s) (true remaining makespan); consistent
//     because (K - g) decreases by exactly the step cost delta = g(s')-g(s).
// max of two admissible+consistent terms is admissible+consistent, so A* stays
// optimal without node re-opening.
//
// We deliberately do NOT reuse the shaved erEST[]/tailAfter[] windows here: the
// paper uses shaving only destructively, and a window shaved at the trial
// horizon C = lberRootBound (a LOWER bound) is only valid for schedules with
// makespan <= lberRootBound, hence not an unconditional bound when C* exceeds
// it. Carrying only the scalar lberRootBound as a floor avoids that gap.
double computeConsistentLBER_floor(
    const std::vector<short> &unfinishedActivities,
    const std::vector<std::pair<short, short>> &activeTransitions,
    int currentMakespan, int depth) {
  computeRootBound(depth); // once per problem (guarded by lberRootComputed)
  double cpH = getForwardHcostDP(unfinishedActivities, activeTransitions);
  double floorH = (double)lberRootBound - (double)currentMakespan;
  double h = cpH;
  if (floorH > h)
    h = floorH;
  if (h < 0.0)
    h = 0.0;
  return h;
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

// ============================ TT2 (forward) ============================
// Ported from the `ido` branch (forward path only). The TT2 state uses a
// relative-time model: active activities carry a residual duration, a firing
// advances time by `firingTime`, and finished activities live in a bitset.

// Available transitions for the TT2 state: precedence- and resource-feasible
// unstarted tasks, each tagged with the earliest time it can start.
std::vector<std::pair<short, short>> getAvailableTransitionIndices_TT2(
    const std::vector<short> &unstartedTransitions,
    const std::bitset<128> &finishedActivitiys,
    const std::array<std::vector<std::pair<short, short>>, 4> &resource_nodes,
    const std::vector<std::pair<short, short>> &activeTransitionIndices) {
  std::vector<std::pair<short, short>> available;

  std::unordered_set<short> activeTasks;
  for (const auto &[taskID, _] : activeTransitionIndices) {
    activeTasks.insert(taskID);
  }

  for (short transId : unstartedTransitions) {
    if (activeTasks.count(transId) > 0) {
      continue;
    }

    const auto &dependencies = RCPSPex.backword_dependencies[transId - 1];
    const Activity &act = RCPSPex.activities[transId - 1];

    // 1. Precedence: every predecessor must be finished or active.
    bool allPredsFinished = true;
    int maxPredFinishTime = 0;
    for (int predId : dependencies) {
      if (finishedActivitiys.test(predId)) {
        continue;
      }
      bool isActive = false;
      int activeRemainingTime = 0;
      for (const auto &[activeID, remaining] : activeTransitionIndices) {
        if (activeID == predId) {
          isActive = true;
          activeRemainingTime = remaining;
          break;
        }
      }
      if (isActive) {
        maxPredFinishTime = std::max(maxPredFinishTime, activeRemainingTime);
      } else {
        allPredsFinished = false;
        break;
      }
    }
    if (!allPredsFinished)
      continue;

    // 2. Resources: find the earliest time enough capacity is available.
    bool resourcesOK = true;
    int maxResourceTime = maxPredFinishTime;
    std::array<std::vector<std::pair<short, short>>, 4> sorted_resources;
    bool resources_sorted[4] = {false, false, false, false};

    for (const auto &[res, demand] : act.resource_demands) {
      int resID = petri.place_name_to_id.at(res);
      if (resource_nodes[resID].empty()) {
        resourcesOK = false;
        break;
      }
      if (!resources_sorted[resID]) {
        sorted_resources[resID] = resource_nodes[resID];
        std::sort(sorted_resources[resID].begin(), sorted_resources[resID].end(),
                  [](const auto &a, const auto &b) { return a.second < b.second; });
        resources_sorted[resID] = true;
      }
      const auto &tokens = sorted_resources[resID];
      int totalAvailable = 0;
      int resourceReadyTime = -1;
      for (const auto &[amt, time] : tokens) {
        if (time <= maxPredFinishTime) {
          totalAvailable += amt;
        }
      }
      if (totalAvailable >= demand) {
        resourceReadyTime = maxPredFinishTime;
      } else {
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
      available.emplace_back(transId, (short)maxResourceTime);
    }
  }

  return available;
}

// Reconstruct the per-resource token lists from the slim state (finished/active).
// Renewable invariant: a resource's free tokens now = capacity - amount held by
// active tasks; each active task returns its demand when it finishes (at its
// remaining time). Indexed by raw resource place id (place_name_to_id), matching
// what getAvailableTransitionIndices_TT2 expects. Canonicalized (sort+merge) so
// it equals the value the old incremental ctor produced.
std::array<std::vector<std::pair<short, short>>, 4> reconstructResourceNodes(
    const std::vector<std::pair<short, short>> &activeTransitionIndices) {
  std::array<std::vector<std::pair<short, short>>, 4> resource_nodes;
  for (const auto &[resName, cap] : RCPSPex.resources) {
    if (cap <= 0) continue;
    int resID = petri.place_name_to_id.at(resName); // 0..3
    auto &resVec = resource_nodes[resID];
    int used = 0;
    for (const auto &[taskID, remaining] : activeTransitionIndices) {
      const auto &dem = RCPSPex.activities[taskID - 1].resource_demands;
      auto it = dem.find(resName);
      if (it != dem.end() && it->second > 0) {
        resVec.push_back({(short)it->second, remaining});
        used += it->second;
      }
    }
    int freeNow = cap - used;
    if (freeNow > 0) resVec.push_back({(short)freeNow, 0});

    // Same canonical form as the firing ctor: sort by (time, amount), merge ties.
    std::sort(resVec.begin(), resVec.end(), [](const auto &a, const auto &b) {
      if (a.second != b.second) return a.second < b.second;
      return a.first < b.first;
    });
    if (!resVec.empty()) {
      auto rit = resVec.begin();
      while (rit != resVec.end() - 1) {
        auto next = rit + 1;
        if (rit->second == next->second) {
          rit->first += next->first;
          resVec.erase(next);
        } else {
          ++rit;
        }
      }
    }
  }
  return resource_nodes;
}

RCPSPState_TT2::RCPSPState_TT2() {
  finishedActivitiys.reset();
  // Set the global start/end place names (used elsewhere); no token state stored.
  for (int i = 0; i < (int)petri.places.size(); ++i) {
    const auto &place = petri.places[i];
    if (place.arcs_out.empty()) finalstatename = place.name;
    if (place.arcs_in.empty())  initialstatename = place.name;
  }
  h = 0;
  predessesor_h = 0;
  isDeltaZero = false;
  g = 0;
}

// Slim firing step: only (finished, active) + scalars evolve. resource_nodes is
// NOT stored — it is reconstructed on demand at expansion. The chosen move
// (transitionId, firingTime) already encodes the resource-feasible start time,
// computed by getAvailableTransitionIndices_TT2 on the reconstructed resources.
RCPSPState_TT2::RCPSPState_TT2(const RCPSPState_TT2 &prev, short transitionId,
                              short firingTime) {
  isDeltaZero = (firingTime == 0);
  g = prev.g + firingTime;
  predessesor_h = prev.h; // reused as h on delta-zero steps (consistency-safe)
  lastTransitionId = transitionId;

  finishedActivitiys = prev.finishedActivitiys;
  activeTransitionIndices = prev.activeTransitionIndices;

  // 1. Advance time by `firingTime`: active tasks lose that time; finish at <= 0.
  if (firingTime > 0) {
    auto it = activeTransitionIndices.begin();
    while (it != activeTransitionIndices.end()) {
      it->second -= firingTime;
      if (it->second <= 0) {
        finishedActivitiys[it->first] = 1;
        it = activeTransitionIndices.erase(it);
      } else {
        ++it;
      }
    }
  }

  // 2. Start the activity: add to the active list (or finish if zero-duration).
  short duration = RCPSPex.activities[transitionId - 1].duration;
  if (duration > 0) {
    activeTransitionIndices.emplace_back(transitionId, duration);
  } else {
    finishedActivitiys[transitionId] = 1;
  }

  // 3. Canonicalize the active list (order-independent dedup; hash uses it).
  std::sort(activeTransitionIndices.begin(), activeTransitionIndices.end());
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
