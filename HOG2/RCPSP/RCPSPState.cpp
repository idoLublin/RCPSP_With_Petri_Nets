//
// Created by idolu on 06/01/2025.

#include <iostream>
#include <vector>
#include <set>
#include <queue>
#include <unordered_set>
#include "petriclasses.h"
#include "RCPSPState.h"
#include "RCPSPState.h"
#include <thread>
#include <chrono>
#include <atomic>
#include <algorithm>
#include <climits>
#include <array> // <--- DON'T FORGET THIS
#include <climits>

#include "RCPSP.h"
using namespace P_RCPSP;


// std::chrono::duration<double> generateTIME;
// std::chrono::duration<double> avelableTIME;
// std::chrono::duration<double> HTIME;
// std::chrono::duration<double>hashTIME;
//
// std::chrono::duration<double> comperTime;
// std::chrono::duration<double>secssesorTIME;



// thread_local PetriExample petri;
// thread_local RCPSP_example RCPSPex;
bool useCS;


// std::atomic<bool> stop_printing(false); // Flag to stop the printing thread
//
// void printNetworkSize(const std::vector<RCPSPState>& network) {
//   while (!stop_printing) {
//     std::this_thread::sleep_for(std::chrono::seconds(1)); // Wait for a second
//     std::cout << "Current network size: " << network.size() << std::endl;
//   }
// }
//std::vector<Transition> getAvilableTransitions(std::map<std::string, int> marking);
std::vector<P_RCPSP::Transition> getAvilableTransitions(const std::unordered_map<std::string, int>& marking);
double computeWorkloadLowerBoundWithMax(
    const std::vector<short>& unfinishedTransitions,
    const std::vector<std::pair<short, short>>& activeTransitionIndices,
    const std::map<int, int>& earlyStartTimes,
    double criticalPathEstimate
);
// std::vector<std::pair<int, int>> getAvailableTransitionIndices_TT(
//     const std::vector<int> &unstartedTransitions,
//     const std::map<int, int> &finishedActivities,
//     const std::vector<std::vector<std::pair<int, int>>>&marking
// );
std::vector<int> getCriticalPath(const std::map<int, int>& earlyStartTimes,
                                 int lastActivityId,
                                 const std::vector<std::vector<std::string>>& backword_dependencies);


double computeSequenceLowerBoundWithMax(
    const std::vector<short>& unfinishedTransitions,
    const std::vector<std::pair<short, short>>& activeTransitionIndices,
    const std::map<int, int>& earlyStartTimes,
    double criticalPathEstimate
);
double computeSequenceLowerBoundWithMax2(
  const std::vector<short>& unfinishedTransitions,
const std::vector<std::pair<short, short>>& activeTransitionIndices,
 std::map<int, int>& earlyStartTimes,
 std::map<int, int>& earlyfinishTimes,
double criticalPathEstimate,
std::map<int, int> finishedActivities
);
double computeCoreTimeLowerBoundWithMax(
    const std::vector<int>& unfinishedTransitions,
    const std::vector<std::pair<int, int>>& activeTransitionIndices,
    const std::map<int, int>& earlyStartTimes,
    double criticalPathEstimate
);
double computeResourceCapacityLowerBound(
    const std::vector<int>& unfinishedTransitions,
    const std::vector<std::pair<int, int>>& activeTransitionIndices,
    double criticalPathEstimate
);

double getBackwardHcost2(
    const std::set<int>& startedActivities,
    const std::set<int>& finishedActivities,
    const std::vector<std::pair<int, int>>& activeTransitionIndices
);


void GetNabor(std::vector<RCPSPState> &NodeList,int chosenNode,int &count);
//int ChooseExpansion(std::vector<RCPSPState> network);

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
//      for (const auto& arc : transition.arcs_out) {  // Instead of arcs_in, we check arcs_out
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
// std::vector<Transition> getAvilableTransitions(const std::unordered_map<std::string, int>& marking) {
//    auto startS1 = std::chrono::high_resolution_clock::now();
//
//    std::vector<Transition> avilableTransitions;
//    avilableTransitions.reserve(petri.Transitions.size());  // Reserve memory to avoid multiple reallocations
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

std::vector<int> getAvilableTransitionIndices(const std::vector<short>& marking);

std::vector<int> getAvilableDetransitionIndices(const std::unordered_map<std::string, int>& marking);

double getForwardHcost(std::set<short>unstartedTransitions, std::vector<std::pair<short, short>>activeTransitionIndices) {
  //auto startS3 = std::chrono::high_resolution_clock::now();

   std::map<int, int> earlyfinishMap2; // Map to store activity IDs and their early finish times
  //std::map<int, int> visitmap; // Map to store activity IDs and their early finish times
  double h;
  std::set<int> processedDependencies;
  // Iterate over unstarted activitiesint lastElementEarlyFinish = 0;
  //int lastElementEarlyFinish = 0;
  for (int activityId: unstartedTransitions) {
    int maxFinishTime = 0;
    std::set<int> processedDependencies;

      for (int dep : RCPSPex.backword_dependencies[activityId - 1]) {  // Changed to int
          int depId = dep - 1;  // No more std::stoi

          if (std::find(unstartedTransitions.begin(), unstartedTransitions.end(), depId + 1) != unstartedTransitions.end()) {
              short duration = getTransitionDuration2(activeTransitionIndices, dep);  // Pass dep directly (no std::stoi)
              if (duration !=-1) {
                  maxFinishTime = std::max(maxFinishTime, earlyfinishMap2[depId+1] + duration);
              }
              else {
                  maxFinishTime = std::max(maxFinishTime, earlyfinishMap2[depId+1] + RCPSPex.activities[depId].duration);
              }
          }
          else {
              maxFinishTime = std::max(maxFinishTime, earlyfinishMap2[depId+1]);
          }
      }
    earlyfinishMap2[activityId] = maxFinishTime;

  }
  if (earlyfinishMap2.size()==0) {
    h = 0;
  }
  else {
    h = earlyfinishMap2.rbegin()->second;;

  }

   // auto endS3 = std::chrono::high_resolution_clock::now();
   // HTIME += endS3 - startS3;

 return h;

}

double getForwardHcost0Based(std::vector<short> unstartedTransitions,
                      std::vector<std::pair<short, short>> activeTransitionIndices) {

    //auto startS3 = std::chrono::high_resolution_clock::now();

    std::map<int, int> earlyfinishMap2; // Map to store activity IDs and their early finish times
    //std::map<int, int> visitmap; // Map to store activity IDs and their early finish times
    double h;
    std::set<int> processedDependencies;
    // Iterate over unstarted activitiesint lastElementEarlyFinish = 0;
    //int lastElementEarlyFinish = 0;
    for (int activityId: unstartedTransitions) {
        int maxFinishTime = 0;
        std::set<int> processedDependencies;

        for (int dep : RCPSPex.backword_dependencies[activityId - 1]) {  // Changed to int
            int depId = dep - 1;  // No more std::stoi

            if (std::find(unstartedTransitions.begin(), unstartedTransitions.end(), depId + 1) != unstartedTransitions.end()) {
                short duration = getTransitionDuration2(activeTransitionIndices, dep);  // Pass dep directly (no std::stoi)
                if (duration !=-1) {
                    maxFinishTime = std::max(maxFinishTime, earlyfinishMap2[depId+1] + duration);
                }
                else {
                    maxFinishTime = std::max(maxFinishTime, earlyfinishMap2[depId+1] + RCPSPex.activities[depId].duration);
                }
            }
            else {
                maxFinishTime = std::max(maxFinishTime, earlyfinishMap2[depId+1]);
            }
        }
        earlyfinishMap2[activityId] = maxFinishTime;

    }
    if (earlyfinishMap2.size()==0) {
        h = 0;
    }
    else {
        h = earlyfinishMap2.rbegin()->second;;

    }

    // auto endS3 = std::chrono::high_resolution_clock::now();
    // HTIME += endS3 - startS3;

    return h;

}

double getForwardHcost(std::vector<short>unstartedTransitions,
                      std::vector<std::pair<short, short>>activeTransitionIndices
                      //,std::vector<int> finishedActivitiys

                      ) {
  //auto startS3 = std::chrono::high_resolution_clock::now();

   std::map<int, int> earlyfinishMap2; // Map to store activity IDs and their early finish times
   //std::map<int, int> earlyfinishMap3; // Map to store activity IDs and their early finish times

  double h;
  std::set<int> processedDependencies;

  for (int activityId: unstartedTransitions) {
    int maxFinishTime = 0;
    std::set<int> processedDependencies;

      for (int dep : RCPSPex.backword_dependencies[activityId - 1]) {  // Changed to int
          int depId = dep - 1;  // No more std::stoi

          if (std::find(unstartedTransitions.begin(), unstartedTransitions.end(), depId + 1) != unstartedTransitions.end()) {
              int duration = getTransitionDuration2(activeTransitionIndices, dep);  // Pass dep directly
              if (duration !=-1) {
                  maxFinishTime = std::max(maxFinishTime, earlyfinishMap2[depId+1] + duration);
                  //earlyfinishMap3[depId+1] = earlyfinishMap2[depId+1] + duration;
              }
              else {
                  maxFinishTime = std::max(maxFinishTime, earlyfinishMap2[depId+1] + RCPSPex.activities[depId].duration);
               //   earlyfinishMap3[depId+1] = earlyfinishMap2[depId+1] + RCPSPex.activities[depId].duration;
              }
          }
          else {
              maxFinishTime = std::max(maxFinishTime, earlyfinishMap2[depId+1]);
            //  earlyfinishMap3[depId+1] = earlyfinishMap2[depId+1];
          }
      }
    earlyfinishMap2[activityId] = maxFinishTime;
   // earlyfinishMap3[activityId] = maxFinishTime;
  }
  if (earlyfinishMap2.size()==0) {
    h = 0;
  }
  else {
    h = earlyfinishMap2.rbegin()->second;;

  }

    //   std::array<int, 32> LF;
    // LF.fill(-1);
    //
    // // Process in reverse topological order (descending ID)
    // for (int idx = (int)unstartedTransitions.size() - 1; idx >= 0; idx--) {
    //     short actId = unstartedTransitions[idx];
    //     int minSuccLS = h;
    //
    //     for (int succ : RCPSPex.dependencies[actId - 1]) {
    //         if (std::find(unstartedTransitions.begin(), unstartedTransitions.end(), (short)succ) == unstartedTransitions.end())
    //             continue;
    //         if (LF[succ] == -1) continue;
    //         int duration = getTransitionDuration2(activeTransitionIndices, succ);
    //         int effectiveDuration = (duration != -1) ? duration : RCPSPex.activities[succ - 1].duration;
    //         minSuccLS = std::min(minSuccLS, LF[succ] - effectiveDuration);
    //     }
    //     LF[actId] = minSuccLS;
    // }
    //
    // // --- Step 3: Identify critical vs non-critical ---
    // // std::vector<short> criticalActivities;
    // // std::vector<short> nonCriticalActivities;
    //
    // for (short actId : unstartedTransitions) {
    //     int duration = getTransitionDuration2(activeTransitionIndices, actId);
    //     int effectiveDuration = (duration != -1) ? duration : RCPSPex.activities[actId - 1].duration;
    //     int EF = earlyfinishMap2[actId] + effectiveDuration;
    //     if (EF == 0) continue;
    //     int slack = LF[actId] - EF;
    //     if (slack == 0) {
    //         nextCritical=actId;
    //         break;
    //
    //         // } else if (slack > 0) {
    //     //     // nonCriticalActivities.push_back(actId);
    //      }
    // }



 return h;

}


double getForwardHcost(std::vector<short>unstartedTransitions,
                      std::vector<std::pair<short, short>>activeTransitionIndices,
                      short& nextCritical  // ← add this

                      ) {
  //auto startS3 = std::chrono::high_resolution_clock::now();

   std::map<int, int> earlyfinishMap2; // Map to store activity IDs and their early finish times
   //std::map<int, int> earlyfinishMap3; // Map to store activity IDs and their early finish times

  double h;
  std::set<int> processedDependencies;

  for (int activityId: unstartedTransitions) {
    int maxFinishTime = 0;
    std::set<int> processedDependencies;

      for (int dep : RCPSPex.backword_dependencies[activityId - 1]) {  // Changed to int
          int depId = dep - 1;  // No more std::stoi

          if (std::find(unstartedTransitions.begin(), unstartedTransitions.end(), depId + 1) != unstartedTransitions.end()) {
              int duration = getTransitionDuration2(activeTransitionIndices, dep);  // Pass dep directly
              if (duration !=-1) {
                  maxFinishTime = std::max(maxFinishTime, earlyfinishMap2[depId+1] + duration);
                  //earlyfinishMap3[depId+1] = earlyfinishMap2[depId+1] + duration;
              }
              else {
                  maxFinishTime = std::max(maxFinishTime, earlyfinishMap2[depId+1] + RCPSPex.activities[depId].duration);
               //   earlyfinishMap3[depId+1] = earlyfinishMap2[depId+1] + RCPSPex.activities[depId].duration;
              }
          }
          else {
              maxFinishTime = std::max(maxFinishTime, earlyfinishMap2[depId+1]);
            //  earlyfinishMap3[depId+1] = earlyfinishMap2[depId+1];
          }
      }
    earlyfinishMap2[activityId] = maxFinishTime;
   // earlyfinishMap3[activityId] = maxFinishTime;
  }
  if (earlyfinishMap2.size()==0) {
    h = 0;
  }
  else {
    h = earlyfinishMap2.rbegin()->second;;

  }

    // std::array<int, 128> LF;
    // LF.fill(-1);
    //
    // // Process in reverse topological order (descending ID)
    // for (int idx = (int)unstartedTransitions.size() - 1; idx >= 0; idx--) {
    //     short actId = unstartedTransitions[idx];
    //     int minSuccLS = h;
    //
    //     for (int succ : RCPSPex.dependencies[actId - 1]) {
    //         if (std::find(unstartedTransitions.begin(), unstartedTransitions.end(), (short)succ) == unstartedTransitions.end())
    //             continue;
    //         if (LF[succ] == -1) continue;
    //         int duration = getTransitionDuration2(activeTransitionIndices, succ);
    //         int effectiveDuration = (duration != -1) ? duration : RCPSPex.activities[succ - 1].duration;
    //         minSuccLS = std::min(minSuccLS, LF[succ] - effectiveDuration);
    //     }
    //     LF[actId] = minSuccLS;
    // }
    //
    // // --- Step 3: Identify critical vs non-critical ---
    // // std::vector<short> criticalActivities;
    // // std::vector<short> nonCriticalActivities;
    //
    // for (short actId : unstartedTransitions) {
    //     int duration = getTransitionDuration2(activeTransitionIndices, actId);
    //     int effectiveDuration = (duration != -1) ? duration : RCPSPex.activities[actId - 1].duration;
    //     int EF = earlyfinishMap2[actId] + effectiveDuration;
    //     if (EF == 0) continue;
    //     int slack = LF[actId] - EF;
    //     if (slack == 0) {
    //         nextCritical=actId;
    //         break;
    //
    //         // } else if (slack > 0) {
    //     //     // nonCriticalActivities.push_back(actId);
    //      }
    //}



 return h;

}

double getforwardResource(std::vector<short> tempUnfinished,
    std::vector<std::pair<short, short>> activeTransitionIndices) {

    double maxResourceBound = 0.0;

    for (auto& [resourceName, capacity] : RCPSPex.resources) {
        double totalDemand = 0.0;

        // Active activities: use residual delay as remaining duration
        // Active activities
        for (auto& [activityId, residualDelay] : activeTransitionIndices) {
            if (activityId == 1 || activityId == RCPSPex.activities.size()) continue;
            auto& activity = RCPSPex.activities[activityId - 1];
            auto it = activity.resource_demands.find(resourceName);
            if (it != activity.resource_demands.end()) {
                totalDemand += residualDelay * it->second;
            }
        }

        // Build a set of executing activity IDs for fast lookup
        std::unordered_set<short> executing;
        for (auto& [activityId, residualDelay] : activeTransitionIndices) {
            executing.insert(activityId);
        }

        for (short activityId : tempUnfinished) {
            if (activityId == 1 || activityId == RCPSPex.activities.size()) continue;
            if (executing.count(activityId)) continue; // skip executing
            auto& activity = RCPSPex.activities[activityId - 1];
            auto it = activity.resource_demands.find(resourceName);
            if (it != activity.resource_demands.end()) {
                totalDemand += activity.duration * it->second;
            }
        }
        double bound = totalDemand / capacity;
        maxResourceBound = std::max(maxResourceBound, bound);
    }
    return maxResourceBound;
}

double getBackwardHcost(std::vector<short> unstartedTransitions,
                        std::vector<std::pair<short, short>> activeTransitionIndices) {

    std::map<int, int> earlyfinishMap2;
    double h = 0;

    // 1. FILL THE MAP BASED ON CURRENT STATE
    for (int activityId = 1; activityId <= RCPSPex.activities.size(); ++activityId) {
        int maxParentFinish = 0;
        int idx = activityId - 1;

        // Standard Forward Pass
        for (int dep : RCPSPex.backword_dependencies[idx]) {
            maxParentFinish = std::max(maxParentFinish, earlyfinishMap2[dep]);
        }

        // --- THE DYNAMIC DURATION LOGIC ---
        int effectiveDuration = 0;

        // Is this task Active?
        int remaining = getTransitionDuration2(activeTransitionIndices, activityId);

        if (remaining != -1) {
            // Task is partially undone. Its "length" from the start is Total - Remaining.
           // effectiveDuration = RCPSPex.activities[idx].duration - remaining;
            effectiveDuration = remaining;
        }
        else if (std::find(unstartedTransitions.begin(), unstartedTransitions.end(), activityId) != unstartedTransitions.end()) {
            // Task is still fully Finished (1). Use full duration.
            effectiveDuration = RCPSPex.activities[idx].duration;
        }
        else {
            // Task is already Un-finished (0). It effectively has 0 duration now.
            effectiveDuration = 0;
        }

        earlyfinishMap2[activityId] = maxParentFinish + effectiveDuration;
    }

    // 2. RETURN THE HIGH-WATER MARK
    if (earlyfinishMap2.empty()) return 0;

    // We only care about the finish times of tasks that are still "in" the project
    for (int activityId : unstartedTransitions) {
        if (earlyfinishMap2[activityId] > h) {
            h = (double)earlyfinishMap2[activityId];
        }
    }

    // Also check active tasks specifically (though they should be in unstartedTransitions)
    for (const auto& [taskID, remainingTime] : activeTransitionIndices) {
        if (earlyfinishMap2[taskID] > h) {
            h = (double)earlyfinishMap2[taskID];
        }
    }

    return h;
}

std::vector<int> getCriticalPath(const std::map<int, int>& earlyfinishTimes,
                                 int lastActivityId) {
    std::vector<int> criticalPath;

    // Input validation
    if (earlyfinishTimes.empty() || lastActivityId <= 0) {
        return criticalPath;
    }

    // Check if lastActivityId exists in earlyStartTimes
    if (earlyfinishTimes.find(lastActivityId) == earlyfinishTimes.end()) {
        return criticalPath;
    }

    std::set<int> visited; // Prevent infinite loops
    int current = lastActivityId;

    while (current != -1) {
        // Check for cycles
        if (visited.count(current)) {
            break; // Cycle detected, stop to prevent infinite loop
        }

        visited.insert(current);
        criticalPath.push_back(current);

        // Get dependencies for current activity
        // Check bounds for RCPSPex.backword_dependencies array
        if (current < 1 || current > static_cast<int>(RCPSPex.backword_dependencies.size())) {
            break;
        }

        const auto& deps = RCPSPex.backword_dependencies[current - 1]; // Convert to 0-based

        if (deps.empty()) {
            break; // No more predecessors
        }

        int maxearlyfinish = -1;
        int nextActivity = -1;

        // Find predecessor with maximum early start time
        // for (const std::string& depStr : deps) {
        //     // Validate string conversion
        //     if (depStr.empty()) continue;
        //
        //     int depId = -1;
        //     try {
        //         depId = std::stoi(depStr);
        //     } catch (const std::exception&) {
        //         continue; // Skip invalid string
        //     }
        //
        //     // Validate depId
        //     if (depId <= 0) continue;
        //
        //     // Check if this dependency exists in earlyStartTimes
        //     auto it = earlyfinishTimes.find(depId);
        //     if (it == earlyfinishTimes.end()) continue;
        //
        //     int earlyfinish = it->second;
        //
        //     // Select predecessor with maximum early start time
        //     if (earlyfinish > maxearlyfinish) {
        //         maxearlyfinish = earlyfinish;
        //         nextActivity = depId;
        //     }
        //    if (earlyfinish==0) {
        //   //   visited.insert(current);
        //   //   criticalPath.push_back(current);
        //     break;
        //   }
        // }
        // Update current for next iteration
        current = nextActivity;
    }

    // Reverse to get path from start to end
    std::reverse(criticalPath.begin(), criticalPath.end());

    return criticalPath;
}

double getBackwardHcost2(
    const std::set<int>& startedActivities,
    const std::set<int>& finishedActivities,
    const std::vector<std::pair<int, int>>& activeTransitionIndices
) {
  std::map<int, int> earlyFinishMap;
  std::set<int> allRelevant;

  for (int id : startedActivities)
    allRelevant.insert(id);
  for (const auto& [id, _] : activeTransitionIndices)
    allRelevant.insert(id);

  for (int actId : allRelevant) {
    int maxDepFinish = 0;
    // for (const std::string& depStr : RCPSPex.backword_dependencies[actId - 1]) {
    //   int depId = std::stoi(depStr);
    //   if (earlyFinishMap.count(depId))
    //     maxDepFinish = std::max(maxDepFinish, earlyFinishMap[depId]);
    // }

    int duration = RCPSPex.activities[actId - 1].duration;
    int remaining = 0;
    for (const auto& [id, remain] : activeTransitionIndices) {
      if (id == actId) {
        remaining = remain;
        break;
      }
    }

    int effectiveDuration = duration - remaining;
    earlyFinishMap[actId] = maxDepFinish + effectiveDuration;
  }

  int maxSoFar = 0;
  for (const auto& [_, finishTime] : earlyFinishMap)
    maxSoFar = std::max(maxSoFar, finishTime);

  return static_cast<double>(maxSoFar);
}

#include <map>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>

// std::map<int, int> getlatefinish (int size,int finishTime) {
// std::map<int, int> latefinishTimes;
//
//     // Set the last activity's late finish time
//     latefinishTimes[size] = finishTime;
//
//     // Work backwards from the last activity
//     for (int i = size - 1; i > 0; i--) {
//         // Check bounds for forward_dependencies array
//         if (i < 1 || i > static_cast<int>(RCPSPex.dependencies.size())) {
//             continue;
//         }
//
//         // Get forward dependencies (successors) for current activity
//         const auto& successors = RCPSPex.dependencies[i - 1]; // Convert to 0-based
//
//         int minLateFinish = 999999;
//         bool hasValidSuccessor = false;
//
//         // Find the minimum late finish time among all successors
//         for (const std::string& succStr : successors) {
//             // Validate string conversion
//             if (succStr.empty()) continue;
//
//             int succId = -1;
//             try {
//                 succId = std::stoi(succStr);
//             } catch (const std::exception&) {
//                 continue; // Skip invalid string
//             }
//
//             // Validate succId
//             if (succId <= 0) continue;
//
//             // Check if this successor already has a late finish time calculated
//             auto it = latefinishTimes.find(succId);
//             if (it != latefinishTimes.end()) {
//                 minLateFinish = std::min(minLateFinish, it->second);
//                 hasValidSuccessor = true;
//             }
//         }
//
//         // If no valid successors found, this might be a terminal activity
//         // In that case, use the project finish time
//         if (!hasValidSuccessor) {
//             minLateFinish = finishTime;
//         }
//
//         // Get duration for current activity
//         int duration = 0;
//         if (i <= static_cast<int>(size) ){
//             duration = RCPSPex.activities[i-1].duration; // Convert to 0-based
//         }
//
//         // Calculate late finish time: min(successor late finish times) - duration
//         latefinishTimes[i] = minLateFinish - duration;
//     }
//
//     return latefinishTimes;
// }

double computeSequenceLowerBoundWithMax2(
const std::vector<short>& unfinishedTransitions,
const std::vector<std::pair<short, short>>& activeTransitionIndices,
 std::map<int, int>& earlyStartTimes,
 std::map<int, int>& earlyfinishTimes,
double criticalPathEstimate,
std::map<int, int> finishedActivities

) {
  std::vector<int> path =getCriticalPath(earlyfinishTimes,RCPSPex.activities.size());
  std::map<int, int> latestartTimes;//getlatefinish(RCPSPex.activities.size(),criticalPathEstimate);

  path.erase(
            std::remove_if(path.begin(), path.end(),
                [&finishedActivities](int activityId) {
                    return finishedActivities.find(activityId) != finishedActivities.end();
                }
            ),
            path.end()
        );

  // 1. Build active set
    std::unordered_set<int> activeSet;
    for (const auto& [id, _] : activeTransitionIndices)
        activeSet.insert(id);

    // 2. Build truly unstarted list
    std::vector<int> unstartedTransitions;
    for (int id : unfinishedTransitions) {
      if (!activeSet.count(id) &&
     std::find(path.begin(), path.end(), id) == path.end()) {
        unstartedTransitions.push_back(id);
     }

           // unstartedTransitions.push_back(id);
    }

    // 3. Build capacity map
    std::map<std::string, int> capacityMap;
    for (const auto& [resName, cap] : RCPSPex.resources)
        capacityMap[resName] = cap;

    // 4. Simulated resource usage timeline
    std::map<int, std::map<std::string, int>> resourceTimeline; // time -> resName -> usage

    // 5. Schedule active tasks at [0, remainingTime)
    for (const auto& [actId, remainingTime] : activeTransitionIndices) {
      path.erase(std::remove_if(path.begin(), path.end(),
          [&](int id) {
              for (const auto& [activeId, _] : activeTransitionIndices) {
                  if (id == activeId)
                    return true;
            }
            return false;
        }),
        path.end());
    }

    // 6. Sort unstarted activities by descending duration
    std::vector<std::pair<int, int>> unstartedSorted; // (actId, duration)
    for (int id : unstartedTransitions) {

        int dur = RCPSPex.activities[id - 1].duration;
        unstartedSorted.emplace_back(id, dur);
    }



  for (const auto& actId : path) {
    const auto& act = RCPSPex.activities[actId - 1];

    for (int t = earlyStartTimes[actId]; t < act.duration+earlyStartTimes[actId]; t++) {
      for (const auto& [res, demand] : act.resource_demands) {
        resourceTimeline[t][res] += demand;
      }
    }

  }

  for (auto it = unstartedSorted.begin(); it != unstartedSorted.end(); ) {
    int actId = it->first;
    bool shouldRemove = false;

    // Check if it's an active activity
    if (activeSet.count(actId)) {
      shouldRemove = true;
    }

    // Check if it's on the critical path
    if (!shouldRemove) {
      for (int pathId : path) {
        if (actId == pathId) {
          shouldRemove = true;
          break;
        }
      }
    }

    if (shouldRemove) {
      it = unstartedSorted.erase(it);
    } else {
      it++;
    }
  }

    // 7. Schedule unstarted one by one
  int maxBlockedSlotsOverall = 0;
  bool valide_resoucre;

  for (const auto& [actId, duration] : unstartedSorted) {
    const auto& act = RCPSPex.activities[actId - 1];
    if (duration == 0) {
      continue; // Skip to next activity
    }
    int startTime = earlyStartTimes[actId];

    int latefinish=latestartTimes[actId]+duration;

    int counter=0;
    // Try scheduling from est onward
    int blockedSlots = duration;

      for (int t = startTime; t <= latefinish; t++) {
        valide_resoucre=true;

        for (const auto& [res, demand] : act.resource_demands) {
          int used = resourceTimeline[t][res];
          int available = capacityMap[res];
          if (used + demand > available) {
            blockedSlots=std::min(blockedSlots,duration-counter);
            valide_resoucre=false;
            break;

          }
        }
        if (valide_resoucre){
          counter++;
          if (counter==duration) {
          blockedSlots=0;
          break;
        }
        }
        else {
          counter=0;
        }
      }

    blockedSlots=std::min(blockedSlots,duration-counter);
    maxBlockedSlotsOverall = std::max(maxBlockedSlotsOverall, blockedSlots);
  }

    return criticalPathEstimate+maxBlockedSlotsOverall;
}

double computeCoreTimeLowerBoundWithMax(
    const std::vector<int>& unfinishedTransitions,
    const std::vector<std::pair<int, int>>& activeTransitionIndices,
    const std::map<int, int>& earlyStartTimes,
    double criticalPathEstimate
) {
    // --- Step 1: filter active tasks out of unfinished
    std::unordered_set<int> activeSet;
    for (const auto& [id, _] : activeTransitionIndices)
        activeSet.insert(id);

    std::vector<int> unstartedTransitions;
    for (int id : unfinishedTransitions) {
        if (!activeSet.count(id))
            unstartedTransitions.push_back(id);
    }

    // --- Step 2: build capacity map and compute total work
    std::map<std::string, int> capacityMap;
    double totalWork = 0.0;
    int minCapacity = 9999999;

    for (const auto& [res, cap] : RCPSPex.resources) {
        capacityMap[res] = cap;
        //minCapacity = std::min(minCapacity, cap);
    }

    for (int id : unstartedTransitions) {
        const auto& act = RCPSPex.activities[id - 1];
        for (const auto& [res, demand] : act.resource_demands) {
            totalWork += demand * act.duration;
        }
    }

    // --- Step 3: active task demand into timeDemand
    std::map<int, std::map<std::string, int>> timeDemand;
    int activeMaxTime = 0;

    for (const auto& [id, remaining] : activeTransitionIndices) {
        const auto& act = RCPSPex.activities[id - 1];
        for (int t = 0; t < remaining; ++t) {
            for (const auto& [res, demand] : act.resource_demands) {
                timeDemand[t][res] += demand;
            }
        }
        activeMaxTime = std::max(activeMaxTime, remaining);
    }

    // --- Step 4: binary search range
    int low = static_cast<int>(std::ceil(criticalPathEstimate));
    int high = static_cast<int>(low + std::ceil(totalWork / std::max(1, minCapacity)));
    int bestFeasible = high;

    // --- Step 5: binary search
    while (low <= high) {
        int mid = (low + high) / 2;
        bool feasible = true;

        std::map<int, std::map<std::string, int>> tempDemand = timeDemand;

        for (int id : unstartedTransitions) {
            const auto& act = RCPSPex.activities[id - 1];
            int dur = act.duration;
            int est = earlyStartTimes.at(id);
            int lst = mid - dur;

            if (lst < est) {
                feasible = false;
                break;
            }

            // Proper core interval: where the activity *must* overlap if makespan is mid
            int coreStart = std::max(est, mid - dur);
            int coreEnd = std::min(mid - 1, est + dur - 1);

            for (int t = coreStart; t <= coreEnd; ++t) {
                for (const auto& [res, demand] : act.resource_demands) {
                    tempDemand[t][res] += demand;
                    if (tempDemand[t][res] > capacityMap[res]) {
                        feasible = false;
                        break;
                    }
                }
                if (!feasible) break;
            }

            if (!feasible) break;
        }

        if (feasible) {
            bestFeasible = mid;
            high = mid - 1;
        } else {
            low = mid + 1;
        }
    }

    return static_cast<double>(std::max(bestFeasible, activeMaxTime));
}

double computeSequenceLowerBoundWithMax(
    const std::vector<int>& unfinishedTransitions,
    const std::vector<std::pair<int, int>>& activeTransitionIndices,
    const std::map<int, int>& earlyStartTimes,
    double criticalPathEstimate
) {
    // 1. Build active set
    std::unordered_set<int> activeSet;
    for (const auto& [id, _] : activeTransitionIndices)
        activeSet.insert(id);

    // 2. Build truly unstarted list
    std::vector<int> unstartedTransitions;
    for (int id : unfinishedTransitions) {
        if (!activeSet.count(id))
            unstartedTransitions.push_back(id);
    }

    // 3. Build capacity map
    std::map<std::string, int> capacityMap;
    for (const auto& [resName, cap] : RCPSPex.resources)
        capacityMap[resName] = cap;

    // 4. Simulated resource usage timeline
    std::map<int, std::map<std::string, int>> resourceTimeline; // time -> resName -> usage

    // 5. Schedule active tasks at [0, remainingTime)
    for (const auto& [actId, remainingTime] : activeTransitionIndices) {
        const auto& act = RCPSPex.activities[actId - 1];
        for (int t = 0; t < remainingTime; ++t) {
            for (const auto& [res, demand] : act.resource_demands) {
                resourceTimeline[t][res] += demand;
            }
        }
    }

    // 6. Sort unstarted activities by descending duration
    std::vector<std::pair<int, int>> unstartedSorted; // (actId, duration)
    for (int id : unstartedTransitions) {
        int dur = RCPSPex.activities[id - 1].duration;
        unstartedSorted.emplace_back(id, dur);
    }
    std::sort(unstartedSorted.begin(), unstartedSorted.end(),
              [](auto& a, auto& b) { return a.second > b.second; });

    // 7. Schedule unstarted one by one
    std::map<int, int> taskEndTimes;
    for (const auto& [actId, duration] : unstartedSorted) {
        const auto& act = RCPSPex.activities[actId - 1];
        int est = earlyStartTimes.at(actId);
        int startTime = est;

        // Try to find first time slot where it can fit
        while (true) {
            bool fits = true;

            for (int t = startTime; t < startTime + duration; ++t) {
                for (const auto& [res, demand] : act.resource_demands) {
                    int used = resourceTimeline[t][res];
                    int available = capacityMap[res];
                    if (used + demand > available) {
                        fits = false;
                        break;
                    }
                }
                if (!fits) break;
            }

            if (fits) break;
            startTime++;
        }

        // Schedule task at startTime
        for (int t = startTime; t < startTime + duration; ++t) {
            for (const auto& [res, demand] : act.resource_demands) {
                resourceTimeline[t][res] += demand;
            }
        }

        taskEndTimes[actId] = startTime + duration;
    }

    // 8. Determine last finish time
    int simulatedEnd = 0;
    for (const auto& [actId, end] : taskEndTimes)
        simulatedEnd = std::max(simulatedEnd, end);
    for (const auto& [actId, remainingTime] : activeTransitionIndices)
        simulatedEnd = std::max(simulatedEnd, remainingTime);

    return std::max(criticalPathEstimate, static_cast<double>(simulatedEnd));
}

double computeWorkloadLowerBoundWithMax(
    const std::vector<int>& unfinishedTransitions,
    const std::vector<std::pair<int, int>>& activeTransitionIndices,
    const std::map<int, int>& earlyStartTimes,
    double criticalPathEstimate
) {
  // Build set of active IDs
  std::unordered_set<int> activeSet;
  for (const auto& [id, _] : activeTransitionIndices)
    activeSet.insert(id);

  // Filter out truly unstarted
  std::vector<int> unstartedTransitions;
  for (int id : unfinishedTransitions) {
    if (activeSet.count(id) == 0)
      unstartedTransitions.push_back(id);
  }
   // Step 1: Estimate total horizon needed
    int project_end_est = 0;

    for (const auto& [actId, est] : earlyStartTimes) {
        int duration = RCPSPex.activities[actId - 1].duration;
        project_end_est = std::max(project_end_est, est + duration);
    }

    for (const auto& [actId, startTime] : activeTransitionIndices) {
        int duration = RCPSPex.activities[actId - 1].duration;
        project_end_est = std::max(project_end_est, startTime + duration);
    }

    // Step 2: Aggregate workload across all time units
    std::map<std::string, double> workloadPerResource;

    for (int t = 0; t < project_end_est; ++t) {
        // --- From unstarted transitions ---
        for (int actId : unstartedTransitions) {
            int est = earlyStartTimes.at(actId);
            int duration = RCPSPex.activities[actId - 1].duration;

            if (t >= est && t < est + duration) {
                const auto& activity = RCPSPex.activities[actId - 1];
                for (const auto& [resName, demand] : activity.resource_demands) {
                    workloadPerResource[resName] += demand;
                }
            }
        }

        // --- From currently active transitions ---
      for (const auto& [actId, remainingTime] : activeTransitionIndices) {
        if (t < remainingTime) { // because they started at t = 0
          const auto& activity = RCPSPex.activities[actId - 1];
          for (const auto& [resName, demand] : activity.resource_demands) {
            workloadPerResource[resName] += demand;
          }
        }
      }
    }

    // Step 3: Get resource capacities
    std::map<std::string, int> capacityMap;
    for (const auto& [resName, capacity] : RCPSPex.resources) {
        capacityMap[resName] = capacity;
    }

    // Step 4: Compute workload lower bound per resource
    int workloadBound = 0;
    for (const auto& [resName, totalWork] : workloadPerResource) {
        int cap = capacityMap[resName];
        int timeRequired = static_cast<int>(std::ceil(totalWork / cap));
        workloadBound = std::max(workloadBound, timeRequired);
    }

    // Final result
    return std::max(criticalPathEstimate, static_cast<double>(workloadBound));
    //return static_cast<double>(workloadBound);
}


double computeResourceCapacityLowerBound(
    const std::vector<int>& unfinishedTransitions,
    const std::vector<std::pair<int, int>>& activeTransitionIndices,
    double criticalPathEstimate
) {
  // Step 1: Build set of active IDs
  std::unordered_set<int> activeSet;
  for (const auto& [id, _] : activeTransitionIndices)
    activeSet.insert(id);

  // Step 2: Filter out active tasks → get truly unstarted
  std::vector<int> unstartedTransitions;
  for (int id : unfinishedTransitions) {
    if (!activeSet.count(id))
      unstartedTransitions.push_back(id);
  }

  // Step 3: Build capacity map
  std::map<std::string, int> capacityMap;
  for (const auto& [resName, cap] : RCPSPex.resources)
    capacityMap[resName] = cap;

  // Step 4: Accumulate workload for each resource
  std::map<std::string, double> workloadPerResource;
  for (int id : unstartedTransitions) {
    const auto& act = RCPSPex.activities[id - 1];
    for (const auto& [res, demand] : act.resource_demands) {
      workloadPerResource[res] += demand * act.duration;
    }
  }

  // Step 5: Compute LB per resource
  double lb = 0.0;
  for (const auto& [res, workload] : workloadPerResource) {
    int cap = capacityMap[res];
    if (cap > 0)
      lb = std::max(lb, std::ceil(workload / cap));
  }

  return std::max(criticalPathEstimate, lb);
}


// double getBackwordsHcost(std::set<int>startedTransitions, std::vector<std::pair<int, int>>activeTransitionIndices)
// {
//  // auto startS3 = std::chrono::high_resolution_clock::now();
//
//   std::map<int, int> earlyfinishMap2; // Map to store activity IDs and their early finish times
//   double h;
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
//       if (std::find(startedTransitions.begin(), startedTransitions.end(), depId + 1) != startedTransitions.end()) {
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
//           maxFinishTime = std::max(maxFinishTime, earlyfinishMap2[depId+1] + duration);
//         } else {
//           // Use default duration
//           maxFinishTime = std::max(maxFinishTime, earlyfinishMap2[depId+1] + RCPSPex.activities[depId].duration);
//         }
//       } else {
//         // If dependency is not in started transitions, just use its finish time
//         maxFinishTime = std::max(maxFinishTime, earlyfinishMap2[depId+1]);
//       }
//     }
//
//     if (std::find(startedTransitions.begin(), startedTransitions.end(), activityId + 1) != startedTransitions.end()) {
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
//         maxFinishTime = std::max(maxFinishTime, earlyfinishMap2[activityId+1] + duration);
//       } else {
//         // Use default duration
//         maxFinishTime = std::max(maxFinishTime, earlyfinishMap2[activityId+1] + RCPSPex.activities[activityId].duration);
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
//    std::map<int, int> earlyfinishMap2; // Map to store activity IDs and their early finish times
//   //std::map<int, int> visitmap; // Map to store activity IDs and their early finish times
//   double h;
//   std::set<int> processedDependencies;
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
//       if (std::find(startedTransitions.begin(), startedTransitions.end(), depId + 1) != startedTransitions.end()) {
//         int duration = getTransitionDuration2(activeTransitionIndices, std::stoi(dep));
//         if (duration !=-1) {
//           maxFinishTime = std::max(maxFinishTime, earlyfinishMap2[depId+1] + duration);
//           //if (RCPSPex.activities[depId].duration !=duration) {
//           //  std::cout<<name<<":"<<dep<<" "<<activityId<<" "<<RCPSPex.activities[depId].duration-duration<<std::endl;
//           //}
//         }
//         else {
//           maxFinishTime = std::max(maxFinishTime, earlyfinishMap2[depId+1] + RCPSPex.activities[depId].duration);
//
//         }
//       }
//       else {
//         maxFinishTime = std::max(maxFinishTime, earlyfinishMap2[depId+1]);
//       }
//     }
//
//     earlyfinishMap2[activityId] = maxFinishTime;
//     //std::cout <<activityId<<":"<< earlyfinishMap[activityId]+RCPSPex.activities[activityId-1].duration << std::endl;
//     // For last element with duration 0, just use the max finish time of dependencies
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
  //auto startS1 = std::chrono::high_resolution_clock::now();
  //direction = true;
  //startedActivitiys[0] = 0;

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
    }
    else {
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
  for (const auto& [resName, capacity] : RCPSPex.resources) {
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
//not working
// RCPSPState_bi::RCPSPState_bi(): nodestatus(false) {
//   std::unordered_map<int, int> place_to_resource_idx;
//     int res_count = 0;
//     for (const auto& [resName, cap] : RCPSPex.resources) {
//         int resID = petri.place_name_to_id.at(resName);
//         place_to_resource_idx[resID] = res_count++;
//     }
//
//     // Count activity nodes
//     int num_activities = petri.places.size() - res_count;
//     activity_nodes.resize(num_activities);
//
//     finishedActivitiys.fill(-1);
//     int activity_counter = 0;
//     for (int i = 0; i < petri.places.size(); ++i) {
//         const auto& place = petri.places[i];
//
//         // Identify Start/End Names
//         if (place.arcs_out.empty()) finalstatename = place.name;
//         if (place.arcs_in.empty())  initialstatename = place.name;
//
//         // Check if resource node
//         auto it = place_to_resource_idx.find(i);
//
//         if (it != place_to_resource_idx.end()) {
//             // This is a resource node
//             int res_idx = it->second;
//
//             if (place.name == initialstatename) {
//                 resource_nodes[res_idx].push_back({1, 0});
//             } else if (!place.state.empty() && !place.state[0].empty()) {
//                 int val = place.state[0][0];
//                 if (val > 0) resource_nodes[res_idx].push_back({val, 0});
//             }
//         } else {
//             // This is an activity node
//             if (place.name == initialstatename) {
//                 activity_nodes[activity_counter] = {1, 0};
//             } else if (!place.state.empty() && !place.state[0].empty()) {
//                 int val = place.state[0][0];
//                 activity_nodes[activity_counter] = (val > 0) ? std::make_pair<short,short>(val, 0) : std::make_pair<short,short>(0, 0);
//             } else {
//                 activity_nodes[activity_counter] = {0, 0};
//             }
//             activity_counter++;
//         }
//     }
//
//     // Add resource capacities
//     for (const auto& [resName, cap] : RCPSPex.resources) {
//         if (cap > 0) {
//             int resID = petri.place_name_to_id.at(resName);
//             int res_idx = place_to_resource_idx[resID];
//
//             if (resource_nodes[res_idx].empty()) {
//                 resource_nodes[res_idx].push_back({cap, 0});
//             }
//         }
//     }
//
//     g = 0;
// }


RCPSPState::RCPSPState(const RCPSPState& predecesor, const P_RCPSP::Transition& active, bool status1, short location, uint64_t &count) {

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
            // Note: active.name is 1-based, so we subtract 1 to access Petri net transitions
            for (const auto& arc : petri.Transitions[active.name - 1].arcs_in_indices) {
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
            for (auto& [id, remain] : activeTransitionIndices) {
                remain -= active.duration;
                if (remain < 0) remain = 0;
            }

            // 2. Collect ALL tasks that finished exactly at this moment
            std::vector<int> finishedNow;
            for (const auto& [id, remain] : activeTransitionIndices) {
                if (remain == 0) {
                    finishedNow.push_back(id);
                }
            }

            // 3. Process all finished tasks (Chain Reaction)
            for (int id : finishedNow) {
                // [VECTOR UPDATE]
                // Check if already marked to avoid double-processing (optional optimization)
                if (finishedActivitiys[id] == -1) {
                    finishedActivitiys[id] = g;
                }

                // Release resources (Produce tokens)
                // Note: id is 1-based, so id - 1 for Petri net access
                for (const auto& arc : petri.Transitions[id - 1].arcs_out_indices) {
                    marking[arc.first] += arc.second;
                }
            }

            // 4. Remove finished tasks from active list
            // Optimization: Remove requires shifting elements.
            // Since order doesn't matter, Swap-and-Pop is faster, but remove_if is safer for now.
            activeTransitionIndices.erase(
                std::remove_if(
                    activeTransitionIndices.begin(),
                    activeTransitionIndices.end(),
                    [](const std::pair<int, int>& p) { return p.second == 0; }), // Remove if remaining time is 0
                activeTransitionIndices.end()
            );
        }
    }
    // Backward logic omitted for brevity (mirror the changes above if needed)
}


template<short N>
std::vector<short> getAvailableActivities(const oldRCPSPState<N>& state) {
    std::vector<short> available;

    // Calculate current resource usage from active transitions
    std::map<std::string, short> currentUsage;
    for (auto& [actIdx, remaining] : state.activeTransitionIndices) {
        for (auto& [resource, demand] : RCPSPex.activities[actIdx].resource_demands) {
            currentUsage[resource] += demand;
        }
    }

    // Check each activity
    for (short i = 0; i < RCPSPex.activity_len; i++) {
        // Skip if already started
        if (state.startedActivitiys[i] != -1) continue;
        // Check all predecessors are finished
        bool predecessorsDone = true;
        for (short pred : RCPSPex.backword_dependencies[i]) {
            if (state.finishedActivitiys[pred-1]==-1) {
                // std::cout << "Activity " << i << " skipped: pred " << pred << " not finished\n";

                predecessorsDone = false;
                break;
            }
        }
        if (!predecessorsDone) continue;

        // Check resources
        bool resourcesAvailable = true;
        for (auto& [resource, capacity] : RCPSPex.resources) {
            short demand = 0;
            if (RCPSPex.activities[i].resource_demands.count(resource)) {
                demand = RCPSPex.activities[i].resource_demands.at(resource);
            }
            if (currentUsage[resource] + demand > capacity) {
                // std::cout << "Activity " << i << " skipped: resource " << resource << " insufficient\n";

                resourcesAvailable = false;
                break;
            }
        }
        if (!resourcesAvailable) continue;

        available.push_back(i);
    }
    return available;
}


// In your header/cpp file, outside of any class
static std::vector<std::pair<short, short>> consumeResourceList(
    const std::vector<std::pair<short, short>>& resource,
    int amount,
    int currentTime
) {
    if (amount < 1)
        return resource;

    std::vector<std::pair<short, short>> resourceCopy = resource;
    std::sort(resourceCopy.begin(), resourceCopy.end(), [](const auto& a, const auto& b) {
        return a.second > b.second;
    });

    int remainingAmount = amount;

    for (auto& [qty, time] : resourceCopy) {
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

    resourceCopy.erase(
        std::remove_if(resourceCopy.begin(), resourceCopy.end(),
                      [](const auto& p) { return p.first <= 0; }),
        resourceCopy.end()
    );

    return resourceCopy;
}

static std::vector<std::pair<short, short>> removeResourceTokens(
    const std::vector<std::pair<short, short>>& resource,
    int amount,
    int targetTime
) {
    std::vector<std::pair<short, short>> result = resource;

    int remaining = amount;

    for (auto it = result.begin(); it != result.end() && remaining > 0; ) {
        if (it->second == targetTime) {
            if (it->first > remaining) {
                it->first -= remaining;
                remaining = 0;
                ++it;
            } else {
                remaining -= it->first;
                it = result.erase(it);
            }
        } else {
            ++it;
        }
    }

    return result;
}


RCPSPState_Bi::RCPSPState_Bi() {
    // Build mapping: resource name -> index (0-3)
    std::unordered_map<int, int> place_to_resource_idx;
    int res_count = 0;
    for (const auto& [resName, cap] : RCPSPex.resources) {
        int resID = petri.place_name_to_id.at(resName);
        place_to_resource_idx[resID] = res_count++;
    }

    // Count activity nodes
    int num_activities = petri.places.size() - res_count;
    activity_nodes.resize(num_activities);

    finishedActivitiys.fill(-1);

    int activity_counter = 0;
    for (int i = 0; i < petri.places.size(); ++i) {
        const auto& place = petri.places[i];

        if (place.arcs_out.empty()) finalstatename = place.name;
        if (place.arcs_in.empty())  initialstatename = place.name;

        auto it = place_to_resource_idx.find(i);

        if (it != place_to_resource_idx.end()) {
            int res_idx = it->second;
            if (place.name == initialstatename) {
                resource_nodes[res_idx].push_back({1, 0});
            } else if (!place.state.empty() && !place.state[0].empty()) {
                int val = place.state[0][0];
                if (val > 0) resource_nodes[res_idx].push_back({val, 0});
            }
        } else {
            if (place.name == initialstatename) {
                activity_nodes[activity_counter] = {1, 0};
            } else if (!place.state.empty() && !place.state[0].empty()) {
                int val = place.state[0][0];
                activity_nodes[activity_counter] = (val > 0) ?
                    std::make_pair<short,short>(val, 0) : std::make_pair<short,short>(0, 0);
            } else {
                activity_nodes[activity_counter] = {0, 0};
            }
            activity_counter++;
        }
    }

    for (const auto& [resName, cap] : RCPSPex.resources) {
        if (cap > 0) {
            int resID = petri.place_name_to_id.at(resName);
            int res_idx = place_to_resource_idx[resID];
            if (resource_nodes[res_idx].empty()) {
                resource_nodes[res_idx].push_back({cap, 0});
            }
        }
    }

    g = 0;
    direction = true;
    f = g_f = g_b = h_f = h_b = 0;
}


RCPSPState_Bi::RCPSPState_Bi(const RCPSPState_Bi& prev, short transitionId, short firingTime) {
    std::cout << "Creating successor: transId=" << transitionId
              << ", firingTime=" << firingTime
              << ", direction=" << prev.direction << std::endl;

    // Validate inputs
    if (transitionId < 1 || transitionId > petri.Transitions.size()) {
        std::cerr << "ERROR: Invalid transitionId " << transitionId << std::endl;
        throw std::runtime_error("Invalid transition ID");
    }
    finishedActivitiys = prev.finishedActivitiys;
    resource_nodes = prev.resource_nodes;
    activity_nodes = prev.activity_nodes;
    direction = prev.direction;

    const Transition& transition = petri.Transitions[transitionId - 1];
    const Activity& act = RCPSPex.activities[transitionId - 1];
    const short duration = act.duration;

    if (direction) {
        // FORWARD: same as TT
        const short activityFinishTime = firingTime + duration;

        // Add output tokens
        for (const auto& [placeID, outAmount] : transition.arcs_out_indices) {
            if (placeID < 4) {
                resource_nodes[placeID].push_back({outAmount, activityFinishTime});
            } else {
                short activity_idx = placeID - 4;
                activity_nodes[activity_idx] = {outAmount, activityFinishTime};
            }
        }

        finishedActivitiys[transitionId] = activityFinishTime;

        // Consume resources
        for (const auto& [resName, demand] : act.resource_demands) {
            if (demand > 0) {
                short resID = petri.place_name_to_id.at(resName);
                resource_nodes[resID] = consumeResourceList(resource_nodes[resID], demand, firingTime);
            }
        }

    } else {
        // BACKWARD: un-fire the activity
        const short activityFinishTime = firingTime + duration;

        // Remove output tokens
        for (const auto& [placeID, outAmount] : transition.arcs_out_indices) {
            if (placeID < 4) {
                // Remove resource tokens that were produced
                resource_nodes[placeID] = removeResourceTokens(resource_nodes[placeID], outAmount, activityFinishTime);
            } else {
                short activity_idx = placeID - 4;
                activity_nodes[activity_idx] = {0, 0};  // Remove token
            }
        }

        // Mark activity as not finished
        finishedActivitiys[transitionId] = activityFinishTime;
if (g_b<activityFinishTime){g_b=activityFinishTime;}
        // Return consumed resources
        for (const auto& [resName, demand] : act.resource_demands) {
            if (demand > 0) {
                short resID = petri.place_name_to_id.at(resName);
                resource_nodes[resID].push_back({demand, firingTime});
            }
        }
    }

    // Calculate g (max finish time of all activities)
    // At the end of RCPSPState_Bi::RCPSPState_Bi(const RCPSPState_Bi &prev, short transitionId, short firingTime)

    g = 0;
    for (short finishTime : finishedActivitiys) {
        if (finishTime > g) {
            g = finishTime;
        }
    }

    // Initialize BAE fields (will be set by search algorithm)
    f = g_f = g_b = h_f = h_b = 0;



    // auto startS4 = std::chrono::high_resolution_clock::now();
  //
  // // Copy basic properties
  // direction = predecesor.direction;
  // name = count;
  // nodestatus = status;
  // unstartedTransitions = predecesor.unstartedTransitions;
  // startedActivitiys = predecesor.startedActivitiys;
  // finishedActivitiys = predecesor.finishedActivitiys;
  // marking = predecesor.marking;
  //
  // // Copy indices instead of full Transition objects
  // activeTransitionIndices = predecesor.activeTransitionIndices;
  // avilableTransitionIndices = predecesor.avilableTransitionIndices;
  // g_b = predecesor.g_b;
  // g_f = predecesor.g_f;
  // h_b = predecesor.h_b;
  // h_f = predecesor.h_f;
  //
  //
  // if (direction) {
  //   //g_f = predecesor.g_f;
  //
  //   if (status) {
  //     //h_f = predecesor.h_f;
  //
  //     // Apply arcs_in from the transition
  //     for (const auto& arc : petri.Transitions[active.name-1].arcs_in_indices) {
  //
  //       // arc.first  is now the integer Place ID (e.g., 5)
  //       // arc.second is the token count (e.g., 1)
  //
  //       // This is a direct array access. 1 CPU cycle.
  //      // marking[arc.first] -= arc.second;
  //     }
  //
  //     // Store index and duration instead of full Transition
  //     activeTransitionIndices.push_back({active.name, active.duration});
  //     startedActivitiys.insert(active.name);
  //     if (active.duration==0) {
  //       status=0;
  //     }
  //   //  auto endS1 = std::chrono::high_resolution_clock::now();
  //  //   generateTIME += endS1-startS4;
  //   }
  //   if (!status) {
  //     g_f += active.duration;
  //     finishedActivitiys.insert(active.name);
  //
  //
  //     // Remove from unstarted
  //     unstartedTransitions.erase(active.name);
  //
  //     // Update durations and remove completed transitions
  //     for (int i = activeTransitionIndices.size() - 1; i >= 0; --i) {
  //       activeTransitionIndices[i].second -= active.duration;
  //       if (activeTransitionIndices[i].second <0) {
  //         activeTransitionIndices[i].second =0;
  //
  //       }
  //       if (activeTransitionIndices[i].first == active.name) {
  //         // Apply arcs_out from the transition
  //         // for (const auto& arc : petri.Transitions[active.name-1].arcs_out) {
  //         //   marking[arc.first] += arc.second;
  //         // }
  //         activeTransitionIndices.erase(activeTransitionIndices.begin() + i);
  //       }
  //
  //     }
  //
  //    // auto endS1 = std::chrono::high_resolution_clock::now();
  //     //generateTIME += endS1-startS4;
  //
  //     //h_f=getForwardHcost(unstartedTransitions,activeTransitionIndices);
  //
  //   }
  //   f=g_f+h_f;
  //   h_b=getBackwardHcost2(startedActivitiys,finishedActivitiys,activeTransitionIndices);
  //   f=2*g_f+h_f-h_b;
  //   //f=2*g_f+h_f;
  //
  //   //avilableTransitionIndices = getAvilableTransitionIndices(marking);
  // }
  // else {
  //
  //
  //   // Similar transformation for the backward direction
  //   if (status) {
  //     //h_b = predecesor.h_b;
  //
  //     // for (const auto& arc : petri.Transitions[active.name-1].arcs_out) {
  //     //   marking[arc.first] -= arc.second;
  //     // }
  //
  //     activeTransitionIndices.push_back({active.name, 0});
  //     auto it = std::find(finishedActivitiys.begin(), finishedActivitiys.end(), active.name);
  //     if (it != finishedActivitiys.end()) {
  //       finishedActivitiys.erase(it);
  //     }
  //     if (active.duration==0) {
  //       status=0;
  //     }
  //    // auto endS1 = std::chrono::high_resolution_clock::now();
  //     //generateTIME += endS1-startS4;
  //   }
  //   if (!status) {
  //    g_b += (petri.Transitions[active.name-1].duration-active.duration);
  //
  //     auto it = std::find(startedActivitiys.begin(), startedActivitiys.end(), active.name);
  //     if (it != startedActivitiys.end()) {
  //       startedActivitiys.erase(it);
  //
  //     }
  //     unstartedTransitions.insert(active.name);
  //
  //     for (int i = activeTransitionIndices.size() - 1; i >= 0; --i) {
  //       activeTransitionIndices[i].second += (petri.Transitions[active.name-1].duration-active.duration);
  //       if (activeTransitionIndices[i].second>petri.Transitions[activeTransitionIndices[i].first-1].duration) {
  //         activeTransitionIndices[i].second=petri.Transitions[activeTransitionIndices[i].first-1].duration;
  //       }
  //       if (activeTransitionIndices[i].first == active.name) {
  //         // for (const auto& arc : petri.Transitions[active.name-1].arcs_in) {
  //         //   marking[arc.first] += arc.second;
  //         // }
  //         activeTransitionIndices.erase(activeTransitionIndices.begin() + i);
  //       }
  //     }
  //     //auto endS1 = std::chrono::high_resolution_clock::now();
  //    // generateTIME += endS1-startS4;
  //     h_b=getBackwardHcost2(startedActivitiys,finishedActivitiys,activeTransitionIndices);
  //
  //   }
  //   //h_f=getForwardHcost(unstartedTransitions,activeTransitionIndices);
  //
  //   avilableDeTransitionIndices = getAvilableDetransitionIndices(marking);
  //   f=2*g_b+h_b-h_f;
  //   //f=g_b;
  //   //f=g_b+h_b;
  }




  // You'll need to modify these functions to return indices instead of Transitions
//   if (direction) {
// }
// else {
// //   }
// int asdasd;
//   asdasd++;
//}

// CHANGE 1: Input is now a fast vector, not a slow map

std::vector<int> getAvilableDetransitionIndices(const std::unordered_map<std::string, int>& marking) {
   std::vector<int> availableIndices;

   // Similar implementation for detransitions
   for (int i = 0; i < petri.Transitions.size(); i++) {
     const Transition& t = petri.Transitions[i];
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



bool RCPSPState::operator==(const RCPSPState& other) const {
  // 1. Fast checks
  // if (name != other.name) return false;
  // if (direction != other.direction) return false;

  // 2. Vector Comparison
  // This is only safe if you GUARANTEE every vector is initialized
  // exactly the same way in the constructor (e.g., size + 5, filled with -1).
  if (marking != other.marking) return false;
  if (finishedActivitiys != other.finishedActivitiys) return false;
  if (startedActivitiys != other.startedActivitiys) return false;

  return true;
}

RCPSPState_TT::RCPSPState_TT() {
    // Build mapping: resource name -> index (0-3)
    std::unordered_map<int, int> place_to_resource_idx;
    int res_count = 0;
    for (const auto& [resName, cap] : RCPSPex.resources) {
        int resID = petri.place_name_to_id.at(resName);
        place_to_resource_idx[resID] = res_count++;
    }

    // Count activity nodes
    int num_activities = petri.places.size() - res_count;
    activity_nodes.resize(num_activities);

    finishedActivitiys.fill(-1);
    int activity_counter = 0;
    for (int i = 0; i < petri.places.size(); ++i) {
        const auto& place = petri.places[i];

        // Identify Start/End Names
        if (place.arcs_out.empty()) finalstatename = place.name;
        if (place.arcs_in.empty())  initialstatename = place.name;

        // Check if resource node
        auto it = place_to_resource_idx.find(i);

        if (it != place_to_resource_idx.end()) {
            // This is a resource node
            int res_idx = it->second;

            if (place.name == initialstatename) {
                resource_nodes[res_idx].push_back({1, 0});
            } else if (!place.state.empty() && !place.state[0].empty()) {
                int val = place.state[0][0];
                if (val > 0) resource_nodes[res_idx].push_back({val, 0});
            }
        } else {
            // This is an activity node
            if (place.name == initialstatename) {
                activity_nodes[activity_counter] = {1, 0};
            } else if (!place.state.empty() && !place.state[0].empty()) {
                int val = place.state[0][0];
                activity_nodes[activity_counter] = (val > 0) ? std::make_pair<short,short>(val, 0) : std::make_pair<short,short>(0, 0);
            } else {
                activity_nodes[activity_counter] = {0, 0};
            }
            activity_counter++;
        }
    }

    // Add resource capacities
    for (const auto& [resName, cap] : RCPSPex.resources) {
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



std::vector<std::pair<int, int>> return_resource(
    const std::vector<std::pair<int, int>>& resource,
    int amount,
    int return_time) {

  // Create a copy of the input resource vector
  std::vector<std::pair<int, int>> resource_copy = resource;

  // If amount is less than 1, just return the copy without changes
  if (amount < 1) {
    return resource_copy;
  }

  // Check if there is already an entry with the return_time
  bool found = false;
  for (auto& item : resource_copy) {
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

double computeEarliestFinish(int activityId,
                             std::map<int, double>& earlyFinishMemo,
                             const std::vector<int>& unstartedTransitions,
                             const std::map<int, int>& finishedActivities) {
    // Memoization check
    if (earlyFinishMemo.count(activityId)) return earlyFinishMemo[activityId];

    // If activity is in finishedActivities, its finish time is 0
    if (finishedActivities.count(activityId)) {
        earlyFinishMemo[activityId] = 0.0;
        return 0.0;
    }

    int internalId = activityId - 1;
    double maxFinish = 0.0;

    // for (const std::string& dep : RCPSPex.backword_dependencies[internalId]) {
    //     int depId = std::stoi(dep); // 1-based
    //     int depInternal = depId - 1;
    //
    //     // Recurse only if it's in unstartedTransitions
    //     if (std::find(unstartedTransitions.begin(), unstartedTransitions.end(), depId) != unstartedTransitions.end()) {
    //         double depFinish = computeEarliestFinish(depId, earlyFinishMemo, unstartedTransitions, finishedActivities)
    //                          + RCPSPex.activities[depInternal].duration;
    //         maxFinish = std::max(maxFinish, depFinish);
    //     } else {
    //         // If dependency is finished, consider its finish time as stored in earlyFinishMemo
    //         maxFinish = std::max(maxFinish, earlyFinishMemo[depId]);
    //     }
   // }

    earlyFinishMemo[activityId] = maxFinish;
    return maxFinish;
}

//!!!i changed map3 (early finish) havent chack correctness

// double getForwardHcost_TT(std::vector<short> unstartedTransitions) {
//     std::map<int, int> earlyfinishMap2;
//
//     // BUILD DEPENDENCY GRAPH for unstarted activities
//     std::map<int, std::vector<int>> graph;
//     std::map<int, int> in_degree;
//
//     for (int activityId : unstartedTransitions) {
//         in_degree[activityId] = 0;
//         graph[activityId] = {};
//     }
//
//     // Build graph edges
//     for (int activityId : unstartedTransitions) {
//         for (int dep : RCPSPex.backword_dependencies[activityId - 1]) {
//             // If dependency is also unstarted, create edge
//             if (std::find(unstartedTransitions.begin(), unstartedTransitions.end(), dep) != unstartedTransitions.end()) {
//                 graph[dep].push_back(activityId);
//                 in_degree[activityId]++;
//             }
//         }
//     }
//
//     // TOPOLOGICAL SORT
//     std::queue<int> queue;
//     std::vector<int> topo_order;
//
//     for (auto& [node, degree] : in_degree) {
//         if (degree == 0) {
//             queue.push(node);
//         }
//     }
//
//     while (!queue.empty()) {
//         int node = queue.front();
//         queue.pop();
//         topo_order.push_back(node);
//
//         for (int neighbor : graph[node]) {
//             in_degree[neighbor]--;
//             if (in_degree[neighbor] == 0) {
//                 queue.push(neighbor);
//             }
//         }
//     }
//
//     // NOW calculate early finish times in TOPOLOGICAL ORDER
//     for (int activityId : topo_order) {
//         int maxFinishTime = 0;
//
//         for (int dep : RCPSPex.backword_dependencies[activityId - 1]) {
//             int depId = dep - 1;
//
//             if (std::find(unstartedTransitions.begin(), unstartedTransitions.end(), depId + 1) != unstartedTransitions.end()) {
//                 maxFinishTime = std::max(maxFinishTime, earlyfinishMap2[depId+1] + RCPSPex.activities[depId].duration);
//             }
//             else {
//                 maxFinishTime = std::max(maxFinishTime, earlyfinishMap2[depId+1]);
//             }
//         }
//
//         earlyfinishMap2[activityId] = maxFinishTime;
//     }
//
//     if (earlyfinishMap2.empty()) {
//         return 0;
//     }
//
//     return earlyfinishMap2.rbegin()->second;
// }
// פונקציית עזר למיון הרכיבים (אפשר לשים אותה כ-static בתוך המחלקה או מחוצה לה)
static void normalizeComponents(
    std::vector<std::pair<short, short>>& activity_nodes,
    std::array<std::vector<std::pair<short, short>>, 4>& resource_nodes,
    std::array<short, 128>& finishedActivitiys
) {
    // 1. מיון פעילויות
    std::sort(activity_nodes.begin(), activity_nodes.end());

    // 2. מיון משאבים (לולאה על המערך ומיון כל וקטור בנפרד)
    for (auto& res_vec : resource_nodes) {
        if (!res_vec.empty()) {
            std::sort(res_vec.begin(), res_vec.end());
        }
    }

    // 3. מיון הפעילויות שהסתיימו
    // שים לב: זה ממיין את כל ה-128 תאים.
    // אם המערך מלא ב-0 או -1 בחלקים הריקים, הם יצופו להתחלה או לסוף (תלוי בערך) וזה תקין לקנוניזציה.
   // std::sort(finishedActivitiys.begin(), finishedActivitiys.end());
}

RCPSPState_TT::RCPSPState_TT(const RCPSPState_TT &prev, short transitionId, short firingTime) {
    finishedActivitiys = prev.finishedActivitiys;
    resource_nodes = prev.resource_nodes;
    activity_nodes = prev.activity_nodes;
    predessesor_h = prev.h;

    const Transition& transition = petri.Transitions[transitionId - 1];
    const Activity& act = RCPSPex.activities[transitionId - 1];
    const short duration = act.duration;
    const short activityFinishTime = firingTime + duration;

    // Add output tokens (NO SORTING HERE)
    for (const auto& [placeID, outAmount] : transition.arcs_out_indices) {
        if (placeID < 4) {
            resource_nodes[placeID].push_back({outAmount, firingTime + transition.duration});
        } else {
            short activity_idx = placeID - 4;
            activity_nodes[activity_idx] = {outAmount, firingTime + transition.duration};
        }
    }

    finishedActivitiys[transitionId] = activityFinishTime;

    // Consume resources (NO SORTING HERE)
    for (const auto& [resName, demand] : act.resource_demands) {
        if (demand > 0) {
            short resID = petri.place_name_to_id.at(resName);
            resource_nodes[resID] = consumeResourceList(resource_nodes[resID], demand, firingTime);
        }
    }

    // Calculate g
    g = 0;
    for (short finishTime : finishedActivitiys) {
        if (finishTime > g) {
            g = finishTime;
        }
    }
  //  normalizeComponents(activity_nodes, resource_nodes, finishedActivitiys);
}

//setup to work only with bi directional
bool RCPSPState_TT::operator==(const RCPSPState_TT &other) const {
        // Check finished activities
        for (int i = 0; i < 128; i++) {
            bool this_finished = (finishedActivitiys[i] != -1);
            bool other_finished = (other.finishedActivitiys[i] != -1);
            if (this_finished != other_finished) return false;
        }

        // Check activity place markings (token counts, not times)
        if (activity_nodes.size() != other.activity_nodes.size()) return false;

        for (int i = 0; i < activity_nodes.size(); i++) {
            if (activity_nodes[i].first != other.activity_nodes[i].first) {
                return false;  // Different token counts
            }
            // Ignore .second (timestamps)
        }

        return true;
    }

RCPSPState_BI_TT2::RCPSPState_BI_TT2() {
     // Build mapping: resource name -> index (0-3)
    std::unordered_map<int, int> place_to_resource_idx;
    int res_count = 0;
    for (const auto& [resName, cap] : RCPSPex.resources) {
        int resID = petri.place_name_to_id.at(resName);
        place_to_resource_idx[resID] = res_count++;
    }

    // Count activity nodes
    int num_activities = petri.places.size() - res_count;
    activity_nodes.resize(num_activities);

    finishedActivitiys.reset();
    int activity_counter = 0;
    for (int i = 0; i < petri.places.size(); ++i) {
        const auto& place = petri.places[i];

        // Identify Start/End Names
        if (place.arcs_out.empty()) finalstatename = place.name;
        if (place.arcs_in.empty())  initialstatename = place.name;

        // Check if resource node
        auto it = place_to_resource_idx.find(i);

        if (it != place_to_resource_idx.end()) {
            // This is a resource node
            int res_idx = it->second;

            if (place.name == initialstatename) {
                resource_nodes[res_idx].push_back({1, 0});
            } else if (!place.state.empty() && !place.state[0].empty()) {
                int val = place.state[0][0];
                if (val > 0) resource_nodes[res_idx].push_back({val, 0});
            }
        } else {
            // This is an activity node
            if (place.name == initialstatename) {
                activity_nodes[activity_counter] = {1, 0};
            } else if (!place.state.empty() && !place.state[0].empty()) {
                int val = place.state[0][0];
                activity_nodes[activity_counter] = (val > 0) ? std::make_pair<short,short>(val, 0) : std::make_pair<short,short>(0, 0);
            } else {
                activity_nodes[activity_counter] = {0, 0};
            }
            activity_counter++;
        }
    }

    // Add resource capacities
    for (const auto& [resName, cap] : RCPSPex.resources) {
        if (cap > 0) {
            int resID = petri.place_name_to_id.at(resName);
            int res_idx = place_to_resource_idx[resID];

            if (resource_nodes[res_idx].empty()) {
                resource_nodes[res_idx].push_back({cap, 0});
            }
        }
    }
    std::vector<short> tempUnstarted;
    tempUnstarted.reserve(petri.Transitions.size());
    for (int i = 0; i < petri.Transitions.size(); i++) {
        short taskID = i + 1;
        if (finishedActivitiys[taskID] == 0) {
            tempUnstarted.push_back(taskID);
        }
    }


    h_f=getForwardHcost_TT2(tempUnstarted,activity_nodes,activeTransitionIndices,finishedActivitiys);
    predessesor_h_b=h_b=predessesor_h_f=h_f;

    g_f =g_b= 0;
}

RCPSPState_BI_TT2::RCPSPState_BI_TT2(const RCPSPState_BI_TT2 &prev, short transitionId, short firingTime, bool Direction) {

// 1. Update Global Cost (G)
    fireTime=firingTime;
    direction=Direction;
if (direction) {
    isDeltaZero = (firingTime == 0);
    g_f = prev.g_f + firingTime;
    g_b = prev.g_b;
    predessesor_h_f = prev.h_f;  // Store parent's h for isDeltaZero optimization
    predessesor_h_b = prev.h_b;  // Store parent's h for isDeltaZero optimization
    lastTransitionId=transitionId;
    // 2. Copy State
    finishedActivitiys = prev.finishedActivitiys;
    resource_nodes = prev.resource_nodes;
    activity_nodes = prev.activity_nodes;
    activeTransitionIndices = prev.activeTransitionIndices;  // Copy active list
    // ✅ CORRECT: Child gets fresh cache
    transitionsCached = false;
    AvailableTransitionIndices_TT2.clear(); // or just leave empty
    // 3. TIME SHIFT (Update "Remaining Time")
    if (firingTime > 0) {
        // Update resource tokens
        for (auto& resVec : resource_nodes) {
            for (auto& token : resVec) {
                token.second = std::max(0, token.second - firingTime);
            }
        }

        // Update activity node tokens
        for (auto& token : activity_nodes) {
            if (token.first > 0) {
                token.second = std::max(0, token.second - firingTime);
            }
        }

        // ========== UPDATE ACTIVE LIST ==========
        // Decrement remaining time for all active activities
        auto it = activeTransitionIndices.begin();
        while (it != activeTransitionIndices.end()) {
            it->second -= firingTime;  // Reduce remaining time

            if (it->second <= 0) {
                // Activity finished - mark it as complete
                short completedTaskID = it->first;
                finishedActivitiys[completedTaskID] = 1;

                // Produce output tokens for this completed activity
                const Transition& completedTransition = petri.Transitions[completedTaskID - 1];
                // for (const auto& [placeID, outAmount] : completedTransition.arcs_out_indices) {
                //     if (placeID < 4) {
                //         // Resource output
                //         resource_nodes[placeID].emplace_back(outAmount, 0);  // Available NOW
                //     } else {
                //         // Activity dependency output
                //         short idx = placeID - 4;
                //         activity_nodes[idx] = {outAmount, 0};  // Available NOW
                //     }
                // }

                // Remove from active list
                it = activeTransitionIndices.erase(it);
            } else {
                ++it;
            }
        }
    }

    // 4. Consume Resources (Standard Resources Only)
    const Transition& transition = petri.Transitions[transitionId - 1];
    const Activity& act = RCPSPex.activities[transitionId - 1];
    short duration = act.duration;

    for (const auto& [resName, demand] : act.resource_demands) {
        if (demand > 0) {
            short resID = petri.place_name_to_id.at(resName);
            auto& tokens = resource_nodes[resID];

            std::sort(tokens.begin(), tokens.end(),
                     [](const auto& a, const auto& b) { return a.second < b.second; });

            int remainingDemand = demand;
            auto it = tokens.begin();
            while (remainingDemand > 0 && it != tokens.end()) {
                if (it->first > remainingDemand) {
                    it->first -= remainingDemand;
                    remainingDemand = 0;
                } else {
                    remainingDemand -= it->first;
                    it = tokens.erase(it);
                }
            }
        }
    }

    // 5. Consume Input Dependency Token (Activity Nodes)
    for (const auto& [placeID, inAmount] : transition.arcs_in_indices) {
        if (placeID >= 4) {
            short idx = placeID - 4;
            activity_nodes[idx].first = 0;
            activity_nodes[idx].second = 0;
        }
    }

    // 6. ADD TO ACTIVE LIST (instead of marking as finished immediately)
    // 6. ADD TO ACTIVE LIST & GENERATE FUTURE EVENTS
    if (duration > 0) {
        // A. Track the Task (for "Finished" status logic)
        activeTransitionIndices.emplace_back(transitionId, duration);

        // B. THE FIX: Immediately return outputs as "Future Events"
        // We put resources back NOW, but marked as "Available in 'duration' seconds"
        const Transition& currentTrans = petri.Transitions[transitionId - 1];

        for (const auto& [placeID, outAmount] : currentTrans.arcs_out_indices) {
            if (placeID < 4) {
                // RESOURCE: Return it now with a delay
                resource_nodes[placeID].emplace_back(outAmount, duration);
            }
            else {
                // ACTIVITY TOKEN: Produce it now with a delay
                // (Be careful with index math: placeID 4 is index 0 in activity_nodes?)
                // Assuming your activity_nodes starts from Place 4:
                short idx = placeID - 4;
                // Only add if not already there (or handled by your specific logic)
                // For TT, usually we just set the availability time:
                if (idx < activity_nodes.size()) {
                    if (activity_nodes[idx].first == 0 || activity_nodes[idx].second > duration) {
                        activity_nodes[idx] = {outAmount, duration};
                    }
                }
            }
        }

        // Optional: Sort active list
        std::sort(activeTransitionIndices.begin(), activeTransitionIndices.end());

    } else {
        // Duration is 0 - finish immediately (Existing Logic)
        finishedActivitiys[transitionId] = 1;

        for (const auto& [placeID, outAmount] : petri.Transitions[transitionId-1].arcs_out_indices) {
            if (placeID < 4) {
                resource_nodes[placeID].emplace_back(outAmount, 0);
            } else {
                short idx = placeID - 4;
                if(idx < activity_nodes.size()) activity_nodes[idx] = {outAmount, 0};
            }
        }
    }
    // NOTE: If duration > 0, outputs are produced when activity completes (in TIME SHIFT section above)

    // 8. Canonical Sort & Merge (Only for Resources)
    // 8. Canonical Sort & Merge (CRITICAL FIX)
    for (auto& resVec : resource_nodes) {
        if (resVec.empty()) continue;

        // A. Sort by Time (Ascending)
        std::sort(resVec.begin(), resVec.end(), [](const auto& a, const auto& b) {
             if (a.second != b.second) return a.second < b.second;
             return a.first > b.first; // Optional: put larger chunks first
        });

        // B. Merge split groups with the same time
        // This converts [(5,0), (5,0)] -> [(10,0)]
        auto it = resVec.begin();
        while (it != resVec.end() - 1) {
            auto next = it + 1;
            if (it->second == next->second) {
                // Same time? Merge them!
                it->first += next->first;
                // Remove the second one
                resVec.erase(next);
                // Don't increment 'it', check the new neighbor
            } else {
                ++it;
            }
        }
    }
    if (!activeTransitionIndices.empty()) {
        std::sort(activeTransitionIndices.begin(), activeTransitionIndices.end());
        // std::pair default sort is (First, Second), which means (ID, Time). This is perfect.
    }
//**********cheack here***************
    // 2. SORT ACTIVITY TOKENS
    // Ensures tokens in "waiting places" are always in the same order
    // if (!activity_nodes.empty()) {
    //     std::sort(activity_nodes.begin(), activity_nodes.end());
    // }
//********************************************
    // 3. SORT & MERGE RESOURCES (Crucial for Heuristic Consistency)
    for (auto& resVec : resource_nodes) {
        if (resVec.empty()) continue;

        // Step A: Sort by Time (Availability Time)
        // If times are equal, sort by Amount (to be deterministic)
        std::sort(resVec.begin(), resVec.end(), [](const auto& a, const auto& b) {
            if (a.second != b.second) return a.second < b.second; // Earliest time first
            return a.first < b.first; // Then smallest amount
        });

        // Step B: Merge Adjacent Duplicates (The "Split Resource" Fix)
        // Converts [(5,0), (5,0)] -> [(10,0)]
        auto it = resVec.begin();
        while (it != resVec.end() - 1) {
            auto next = it + 1;
            // If they become available at the exact same time...
            if (it->second == next->second) {
                it->first += next->first; // Merge amounts
                resVec.erase(next);       // Delete the duplicate
                // Do not increment 'it', check the new neighbor
            } else {
                ++it;
            }
        }
    }
    // if (firingTime==7&&transitionId != 26) {
    //     int i;
    //     i++;
    // }
}

else {
    isDeltaZero = (firingTime == 0);
    g_b = prev.g_b + firingTime;
    g_f=prev.g_f;
    predessesor_h_b = prev.h_b;
    predessesor_h_f = prev.h_f;
    lastTransitionId=transitionId;

    // 2. Copy State
    finishedActivitiys = prev.finishedActivitiys;
    resource_nodes = prev.resource_nodes;
    activity_nodes = prev.activity_nodes;
    activeTransitionIndices = prev.activeTransitionIndices;

    transitionsCached = false;
    AvailableTransitionIndices_TT2.clear();

    // 3. TIME SHIFT (Update "Remaining Time")
    finishedActivitiys[transitionId] = 0;

    if (firingTime > 0) {
        // Update resource tokens
        for (auto& resVec : resource_nodes) {
            for (auto& token : resVec) {
                token.second = std::max(0, token.second - firingTime);
            }
        }

        // Update activity node tokens
        for (auto& token : activity_nodes) {
            if (token.first > 0) {
                token.second = std::max(0, token.second - firingTime);
            }
        }

        // Update active list
        auto it = activeTransitionIndices.begin();
        while (it != activeTransitionIndices.end()) {
            it->second -= firingTime;

            if (it->second <= 0) {
                short completedTaskID = it->first;
                it = activeTransitionIndices.erase(it);
            } else {
                ++it;
            }
        }
    }

    // 4. Consume OUTPUT Resources
    const Transition& transition = petri.Transitions[transitionId - 1];
    const Activity& act = RCPSPex.activities[transitionId - 1];
    short duration = act.duration;

    for (const auto& [placeID, outAmount] : transition.arcs_out_indices) {
        if (placeID < 4) {
            auto& tokens = resource_nodes[placeID];

            std::sort(tokens.begin(), tokens.end(),
                     [](const auto& a, const auto& b) { return a.second < b.second; });

            int remainingDemand = outAmount;
            auto it = tokens.begin();
            while (remainingDemand > 0 && it != tokens.end()) {
                if (it->first > remainingDemand) {
                    it->first -= remainingDemand;
                    remainingDemand = 0;
                } else {
                    remainingDemand -= it->first;
                    it = tokens.erase(it);
                }
            }
        }
    }

    // 5. Consume OUTPUT Activity Tokens
    for (const auto& [placeID, outAmount] : transition.arcs_out_indices) {
        if (placeID >= 4) {
            short idx = placeID - 4;
            activity_nodes[idx].first = 0;
            activity_nodes[idx].second = 0;
        }
    }

    // 6. Produce INPUT tokens (what forward consumed)
    // Step 6 - Produce INPUT tokens (what forward consumed)
    if (duration > 0) {
        activeTransitionIndices.emplace_back(transitionId, duration);

        // Produce inputs as future events
        for (const auto& [placeID, inAmount] : transition.arcs_in_indices) {
            if (placeID < 4) {
                resource_nodes[placeID].emplace_back(inAmount, duration);
            }
            else {
                short idx = placeID - 4;

                // ADD THIS DEBUG HERE:
                if (idx == 48 && idx < activity_nodes.size() && activity_nodes[idx].first > 0) {
                    std::cout << "Activity " << transitionId << " overwriting idx=48: old_time="
                              << activity_nodes[idx].second << ", new_duration=" << duration << std::endl;
                }

                if (idx < activity_nodes.size()) {
                    activity_nodes[idx] = {inAmount, duration};
                }
            }
        }
    }
    else {
        // Duration is 0 - finish immediately
        finishedActivitiys[transitionId] = 0;

        // BACKWARD FIX: Produce to arcs_IN, not arcs_OUT!
        for (const auto& [placeID, inAmount] : transition.arcs_in_indices) {
            if (placeID < 4) {
                resource_nodes[placeID].emplace_back(inAmount, 0);
            }
            else {
                short idx = placeID - 4;
                if(idx < activity_nodes.size()) {
                    activity_nodes[idx] = {inAmount, 0};
                }
            }
        }
    }

    // 7. Canonical Sort & Merge
    for (auto& resVec : resource_nodes) {
        if (resVec.empty()) continue;

        std::sort(resVec.begin(), resVec.end(), [](const auto& a, const auto& b) {
             if (a.second != b.second) return a.second < b.second;
             return a.first > b.first;
        });

        auto it = resVec.begin();
        while (it != resVec.end() - 1) {
            auto next = it + 1;
            if (it->second == next->second) {
                it->first += next->first;
                resVec.erase(next);
            } else {
                ++it;
            }
        }
    }

    if (!activeTransitionIndices.empty()) {
        std::sort(activeTransitionIndices.begin(), activeTransitionIndices.end());
    }

    // if (!activity_nodes.empty()) {
    //     std::sort(activity_nodes.begin(), activity_nodes.end());
    // }

    for (auto& resVec : resource_nodes) {
        if (resVec.empty()) continue;

        std::sort(resVec.begin(), resVec.end(), [](const auto& a, const auto& b) {
            if (a.second != b.second) return a.second < b.second;
            return a.first < b.first;
        });

        auto it = resVec.begin();
        while (it != resVec.end() - 1) {
            auto next = it + 1;
            if (it->second == next->second) {
                it->first += next->first;
                resVec.erase(next);
            } else {
                ++it;
            }
        }
    }
}




}
short computeMVC(std::vector<std::pair<short,short>>& edges) {
    if (edges.empty()) return 0;

    // Remove duplicate edges
    std::sort(edges.begin(), edges.end());
    edges.erase(std::unique(edges.begin(), edges.end()), edges.end());

    // Greedy upper bound — pick first uncovered edge, add both endpoints
    std::set<short> greedy_cover;
    for (auto& [u, v] : edges)
        if (!greedy_cover.count(u) && !greedy_cover.count(v)) {
            greedy_cover.insert(u);
            greedy_cover.insert(v);
        }
    short best = (short)greedy_cover.size();

    // Branch and bound
    std::function<void(std::vector<std::pair<short,short>>, short)>
    bnb = [&](std::vector<std::pair<short,short>> remaining, short current) {
        if (remaining.empty()) {
            best = std::min(best, current);
            return;
        }
        if (current >= best) return; // prune

        auto [u, v] = remaining[0];

        // Branch 1: cover u — remove all edges incident to u
        {
            std::vector<std::pair<short,short>> next;
            for (auto& [a, b] : remaining)
                if (a != u && b != u)
                    next.push_back({a, b});
            bnb(next, current + 1);
        }

        // Branch 2: cover v — remove all edges incident to v
        {
            std::vector<std::pair<short,short>> next;
            for (auto& [a, b] : remaining)
                if (a != v && b != v)
                    next.push_back({a, b});
            bnb(next, current + 1);
        }
    };

    bnb(edges, 0);
    return best;
}
template<short N>
void RCPSPState_CBS<N>::computeLatestStarts(std::array<short, N>& latest) const {
    short makespan = start_times[g_sink_id];

    // Initialize all to makespan
    for (int i = 0; i < RCPSPex.activities.size(); i++)
        latest[i] = makespan - RCPSPex.activities[i].duration;

    // Backward pass using direct forward dependencies
    for (int i = g_sink_id - 1; i >= 0; i--) {
        for (short succ : RCPSPex.dependencies[i]) {
            short succ_idx = succ - 1;
            latest[i] = std::min((int)latest[i],
                latest[succ_idx] - RCPSPex.activities[i].duration);
        }
    }
}

short computeSetCover(std::vector<std::vector<short>>& conflicts) {
    if (conflicts.empty()) return 0;

    // Collect all candidate jobs across all conflicts
    std::set<short> all_jobs;
    for (auto& conflict : conflicts)
        for (short job : conflict)
            all_jobs.insert(job);

    // Greedy upper bound
    // For each uncovered conflict, pick the job that covers the most conflicts
    std::vector<bool> covered(conflicts.size(), false);
    std::set<short> selected;
    short greedy = 0;

    while (true) {
        // Find first uncovered conflict
        int uncovered = -1;
        for (int i = 0; i < (int)conflicts.size(); i++)
            if (!covered[i]) { uncovered = i; break; }
        if (uncovered == -1) break;

        // Pick job from uncovered conflict that covers most other conflicts
        short best_job = conflicts[uncovered][0];
        int best_count = 0;

        for (short job : conflicts[uncovered]) {
            int count = 0;
            for (int i = 0; i < (int)conflicts.size(); i++)
                if (!covered[i])
                    for (short j : conflicts[i])
                        if (j == job) { count++; break; }
            if (count > best_count) {
                best_count = count;
                best_job = job;
            }
        }

        // Select best_job — mark all conflicts it covers
        selected.insert(best_job);
        greedy++;
        for (int i = 0; i < (int)conflicts.size(); i++)
            if (!covered[i])
                for (short j : conflicts[i])
                    if (j == best_job) { covered[i] = true; break; }
    }

    short best = greedy;

    // Branch and bound
    std::function<void(int, std::set<short>, short)>
    bnb = [&](int idx, std::set<short> current_selected, short current_size) {
        if (current_size >= best) return; // prune

        // Find first uncovered conflict from idx
        while (idx < (int)conflicts.size()) {
            bool is_covered = false;
            for (short job : conflicts[idx])
                if (current_selected.count(job)) { is_covered = true; break; }
            if (!is_covered) break;
            idx++;
        }

        if (idx == (int)conflicts.size()) {
            best = std::min(best, current_size);
            return;
        }

        // Branch on each job in this uncovered conflict
        for (short job : conflicts[idx]) {
            current_selected.insert(job);
            bnb(idx + 1, current_selected, current_size + 1);
            current_selected.erase(job);
        }
    };

    bnb(0, {}, 0);
    return best;
}

short computeWeightedSetCover(
    const std::vector<std::pair<std::vector<short>, short>>& conflicts) {

    if (conflicts.empty()) return 0;

    // 1. Find the maximum job ID to size our fast lookup vector
    short max_job_id = 0;
    for (const auto& conflict : conflicts) {
        for (short job : conflict.first) {
            max_job_id = std::max(max_job_id, job);
        }
    }

    // 2. Greedy Upper Bound (Fixed to minimize cost-per-coverage)
    std::vector<bool> covered(conflicts.size(), false);
    short greedy_cost = 0;

    while (true) {
        int uncovered = -1;
        for (int i = 0; i < (int)conflicts.size(); i++) {
            if (!covered[i]) { uncovered = i; break; }
        }
        if (uncovered == -1) break;

        short best_job = conflicts[uncovered].first[0];
        short best_job_cost = 0;
        double best_ratio = std::numeric_limits<double>::max();

        for (short job : conflicts[uncovered].first) {
            short max_forced = 0;
            int conflicts_covered = 0;

            for (int i = 0; i < (int)conflicts.size(); i++) {
                if (!covered[i]) {
                    for (short j : conflicts[i].first) {
                        if (j == job) {
                            max_forced = std::max(max_forced, conflicts[i].second);
                            conflicts_covered++;
                            break;
                        }
                    }
                }
            }

            // Ratio = Cost / Benefit (We want the lowest ratio)
            double ratio = (double)max_forced / conflicts_covered;
            if (ratio < best_ratio) {
                best_ratio = ratio;
                best_job_cost = max_forced;
                best_job = job;
            }
        }

        // Apply chosen job
        // --> CHANGE TO std::max(greedy_cost, best_job_cost) IF MAKESPAN
        greedy_cost += best_job_cost;

        for (int i = 0; i < (int)conflicts.size(); i++) {
            if (!covered[i]) {
                for (short j : conflicts[i].first) {
                    if (j == best_job) { covered[i] = true; break; }
                }
            }
        }
    }

    short best = greedy_cost;

    // 3. Optimized Branch & Bound
    // Using a single boolean vector passed by reference to eliminate allocations
    std::vector<bool> selected(max_job_id + 1, false);

    std::function<void(int, short)> bnb = [&](int idx, short current) {
        if (current >= best) return; // Prune

        // Find first uncovered conflict
        while (idx < (int)conflicts.size()) {
            bool is_covered = false;
            for (short job : conflicts[idx].first) {
                if (selected[job]) { is_covered = true; break; }
            }
            if (!is_covered) break;
            idx++;
        }

        if (idx == (int)conflicts.size()) {
            best = std::min(best, current);
            return;
        }

        // Branch on each job in the uncovered conflict
        for (short job : conflicts[idx].first) {
            short max_forced = 0;
            for (int i = idx; i < (int)conflicts.size(); i++) {
                bool covers = false;
                for (short j : conflicts[i].first) {
                    if (j == job) { covers = true; break; }
                }
                if (covers) {
                    max_forced = std::max(max_forced, conflicts[i].second);
                }
            }

            // Backtracking: Apply -> Recurse -> Undo
            selected[job] = true;

            // --> CHANGE TO std::max(current, max_forced) IF MAKESPAN
            bnb(idx + 1, current + max_forced);

            selected[job] = false;
        }
    };

    bnb(0, 0);
    return best;
}

template<short N>
oldRCPSPState<N>::oldRCPSPState() {
    g = 0;
    startedActivitiys.fill(-1);
    finishedActivitiys.fill(-1);
    startedActivitiys[0]=0;
    finishedActivitiys[0]=0;
    activeTransitionIndices.clear();
}

template<short N>
oldRCPSPState<N>::oldRCPSPState(const oldRCPSPState<N>& predecessor,
                              const std::vector<short>& subset,
                              uint64_t& count) {
    // 1. COPY
    startedActivitiys = predecessor.startedActivitiys;
    finishedActivitiys = predecessor.finishedActivitiys;
    activeTransitionIndices = predecessor.activeTransitionIndices;
    g = predecessor.g;

    // 2. START ALL ACTIVITIES IN SUBSET
    for (short actIdx : subset) {
        activeTransitionIndices.push_back({actIdx, RCPSPex.activities[actIdx].duration});
        startedActivitiys[actIdx] = g;
    }

    // 3. FIND MINIMUM REMAINING DURATION (time advance)
    if (activeTransitionIndices.empty()) return; // nothing active, nothing to advance

    short minRemain = activeTransitionIndices[0].second;
    for (const auto& [id, remain] : activeTransitionIndices) {
        if (remain < minRemain) minRemain = remain;
    }

    // 4. ADVANCE TIME
    g += minRemain;

    // 5. UPDATE ALL REMAINING DURATIONS
    for (auto& [id, remain] : activeTransitionIndices) {
        remain -= minRemain;
    }

    // 6. COLLECT FINISHED
    std::vector<short> finishedNow;
    for (const auto& [id, remain] : activeTransitionIndices) {
        if (remain == 0) {
            finishedNow.push_back(id);
        }
    }

    // 7. PROCESS FINISHED
    for (short id : finishedNow) {
        // if (finishedActivitiys[id] == -1) {
            finishedActivitiys[id] = g;
        // }
    }

    // 8. REMOVE FINISHED FROM ACTIVE
    activeTransitionIndices.erase(
        std::remove_if(
            activeTransitionIndices.begin(),
            activeTransitionIndices.end(),
            [](const std::pair<int, int>& p) { return p.second == 0; }
        ),
        activeTransitionIndices.end()
    );
}

template<short N>
bool oldRCPSPState<N>::operator==(const oldRCPSPState &other) const {
    if (finishedActivitiys != other.finishedActivitiys) return false;
    if (startedActivitiys != other.startedActivitiys) return false;
    if (activeTransitionIndices != other.activeTransitionIndices) return false;
    return true;

}

// short computeWeightedSetCover(
//     std::vector<std::pair<std::vector<short>, short>>& conflicts) {
//
//     if (conflicts.empty()) return 0;
//
//     // Greedy upper bound
//     std::vector<bool> covered(conflicts.size(), false);
//     short greedy = 0;
//
//     while (true) {
//         int uncovered = -1;
//         for (int i = 0; i < (int)conflicts.size(); i++)
//             if (!covered[i]) { uncovered = i; break; }
//         if (uncovered == -1) break;
//
//         // Pick job that covers most conflicts, weighted by max min_forced
//         short best_job = conflicts[uncovered].first[0];
//         short best_contribution = 0;
//
//         for (short job : conflicts[uncovered].first) {
//             // Find max min_forced across all conflicts this job covers
//             short max_forced = 0;
//             int count = 0;
//             for (int i = 0; i < (int)conflicts.size(); i++) {
//                 if (!covered[i]) {
//                     for (short j : conflicts[i].first) {
//                         if (j == job) {
//                             max_forced = std::max(max_forced, conflicts[i].second);
//                             count++;
//                             break;
//                         }
//                     }
//                 }
//             }
//             // Contribution = max_forced (cost of selecting this job)
//             // but we want to maximize conflicts covered per unit cost
//             if (max_forced > best_contribution) {
//                 best_contribution = max_forced;
//                 best_job = job;
//             }
//         }
//
//         // Select best_job
//         greedy += best_contribution;
//         for (int i = 0; i < (int)conflicts.size(); i++)
//             if (!covered[i])
//                 for (short j : conflicts[i].first)
//                     if (j == best_job) { covered[i] = true; break; }
//     }
//
//     short best = greedy;
//
//     // Branch and bound
//     std::function<void(int, std::map<short,short>, short)>
//     bnb = [&](int idx, std::map<short,short> selected, short current) {
//         if (current >= best) return;
//
//         // Find first uncovered conflict from idx
//         while (idx < (int)conflicts.size()) {
//             bool is_covered = false;
//             for (short job : conflicts[idx].first)
//                 if (selected.count(job)) { is_covered = true; break; }
//             if (!is_covered) break;
//             idx++;
//         }
//
//         if (idx == (int)conflicts.size()) {
//             best = std::min(best, current);
//             return;
//         }
//
//         // Branch on each job in uncovered conflict
//         for (short job : conflicts[idx].first) {
//             // Cost of selecting this job = max min_forced of all conflicts it covers
//             short max_forced = 0;
//             for (int i = idx; i < (int)conflicts.size(); i++) {
//                 bool covers = false;
//                 for (short j : conflicts[i].first)
//                     if (j == job) { covers = true; break; }
//                 if (covers)
//                     max_forced = std::max(max_forced, conflicts[i].second);
//             }
//
//             selected[job] = max_forced;
//             bnb(idx + 1, selected, current + max_forced);
//             selected.erase(job);
//         }
//     };
//     bnb(0, {}, 0);
//     return best;
// }



// Compute cost of a single MDA set given current_jobs and their start/duration info
// template<short N>
// short RCPSPState_CBS<N>::compute_mda_cost(
//     const std::vector<short>& mda_set,
//     const std::vector<short>& current_jobs,
//     const std::array<short, N>& latest_starts,
//     int res_idx) const
// {
//     // compute new_start once for the whole set
//     short new_start = std::numeric_limits<short>::max();
//     for (short other : current_jobs) {
//         if (std::find(mda_set.begin(), mda_set.end(), other) != mda_set.end()) continue;
//         short finish = start_times[other] + RCPSPex.activities[other].duration;
//         new_start = std::min(new_start, finish);
//     }
//
//     // then loop on set members only
//     short cost = 0;
//     for (short job : mda_set) {
//         short delta = (new_start > latest_starts[job])
//                       ? new_start - latest_starts[job]
//                       : 0;
//         cost = std::max(cost, delta);
//     }
//     return cost;
// }

// ConflictKey make_conflict_key(const std::vector<short>& current_jobs, short res_idx) {
//     ConflictKey key{0, 0, res_idx};
//     for (short job : current_jobs) {
//         if (job < 64) key.low  |= (1ULL << job);
//         else          key.high |= (1ULL << (job - 64));
//     }
//     return key;
// }
// std::unordered_map<ConflictKey, std::vector<std::vector<short>>, ConflictKeyHash>& get_mda_cache() {
//     static std::unordered_map<ConflictKey, std::vector<std::vector<short>>, ConflictKeyHash> cache;
//     return cache;
// }
template<short N>
ConflictKey<N> make_conflict_key(const std::vector<short>& current_jobs, short res_idx) {
    if constexpr (N <= 32) {
        ConflictKey<32> key{0, res_idx};
        for (short job : current_jobs)
            key.mask |= (1u << job);
        return key;
    } else if constexpr (N <= 62) {
        ConflictKey<62> key{0, res_idx};
        for (short job : current_jobs)
            key.mask |= (1ULL << job);
        return key;
    } else if constexpr (N <= 92) {
        ConflictKey<92> key{0, 0, res_idx};
        for (short job : current_jobs) {
            if (job < 64) key.low  |= (1ULL << job);
            else          key.high |= (1u << (job - 64));
        }
        return key;
    } else {
        ConflictKey<122> key{0, 0, res_idx};
        for (short job : current_jobs) {
            if (job < 64) key.low  |= (1ULL << job);
            else          key.high |= (1ULL << (job - 64));
        }
        return key;
    }
}
template<short N>
void RCPSPState_CBS<N>::enumerate_sets(
    const std::vector<short>& current_jobs,
    short excess_demand,
    int res_idx,
    std::vector<std::vector<short>>& sets) const
{
    int n = current_jobs.size();
    std::vector<short> min_size(n, std::numeric_limits<short>::max());
    std::vector<short> demands(n);
    for (int i = 0; i < n; i++)
        demands[i] = resource_info[res_idx].demand_lookup.at(current_jobs[i]);

    for (int size = 1; size <= n; size++) {
        // early exit — remaining eligible demand cant cover excess
        short remaining_demand = 0;
        for (int i = 0; i < n; i++) {
            if (min_size[i] >= size) remaining_demand += demands[i];
        }
        if (remaining_demand < excess_demand) break;

        std::vector<int> combo(size);
        std::iota(combo.begin(), combo.end(), 0);

        auto advance_combo = [&]() -> bool {
            int i = size - 1;
            while (i >= 0 && combo[i] == n - size + i) i--;
            if (i < 0) return false;
            combo[i]++;
            for (int j = i + 1; j < size; j++) combo[j] = combo[j-1] + 1;
            return true;
        };

        do {
            bool skip = false;
            for (const auto& existing : sets) {
                if (existing.size() >= size) continue; // can't be proper subset
                // check if existing is subset of current combo
                bool is_subset = true;
                for (short act : existing) {
                    bool found = false;
                    for (int idx : combo) {
                        if (current_jobs[idx] == act) { found = true; break; }
                    }
                    if (!found) { is_subset = false; break; }
                }
                if (is_subset) { skip = true; break; }
            }
            if (skip) continue;

            short demand = 0;
            for (int idx : combo) demand += demands[idx];
            if (demand < excess_demand) continue;

            std::vector<short> mda_set;
            mda_set.reserve(size);
            for (int idx : combo) {
                mda_set.push_back(current_jobs[idx]);
                // min_size[idx] = std::min(min_size[idx], (short)size);
            }
            sets.push_back(std::move(mda_set));

        } while (advance_combo());
    }
}

template<short N>
void RCPSPState_CBS<N>::enumerate_sets_bnb(const std::vector<short> &sorted_jobs, const std::vector<short> &demands,
    short excess_demand, int start_idx, short current_demand, std::vector<short> &current_set,
    std::vector<std::vector<short>> &sets) const{
    for (int i = start_idx; i < (int)sorted_jobs.size(); i++) {
        short new_demand = current_demand + demands[i];
        current_set.push_back(sorted_jobs[i]);

        if (new_demand >= excess_demand) {
            sets.push_back(current_set);
        } else {
            enumerate_sets_bnb(sorted_jobs, demands, excess_demand,
                i + 1, new_demand, current_set, sets);
        }

        current_set.pop_back();
    }
}

template<short N>
std::vector<MDA> RCPSPState_CBS<N>::compute_mdas(
    const std::vector<short>& current_jobs,
    short excess_demand,
    const std::array<short, N>& latest_starts,
    int res_idx,
    ConflictKey<N>& key) const
{
    // ConflictKey<N> key = make_conflict_key<N>(current_jobs, (short)res_idx);
    std::vector<std::vector<short>> local_sets;
    const std::vector<std::vector<short>>* sets_ptr = nullptr;

    if (setting.use_MDA_BAB) {
        if (setting.use_MDA_cache) {
            auto& cache = get_mda_cache<N>();
            auto it = cache.find(key);
            if (it == cache.end()) {
                // cache miss — run BAB
                std::vector<short> sorted_jobs = current_jobs;
                std::sort(sorted_jobs.begin(), sorted_jobs.end(), [&](short a, short b) {
                    return resource_info[res_idx].demand_lookup.at(a) >
                           resource_info[res_idx].demand_lookup.at(b);
                });
                std::vector<short> demands_sorted(sorted_jobs.size());
                for (int i = 0; i < (int)sorted_jobs.size(); i++)
                    demands_sorted[i] = resource_info[res_idx].demand_lookup.at(sorted_jobs[i]);


                // std::cout << "conflict jobs and demands:" << std::endl;
                // for (short job : current_jobs) {
                //     std::cout << "job=" << job
                //               << " demand=" << resource_info[res_idx].demand_lookup.at(job)
                //               << " start=" << start_times[job]
                //               << " finish=" << start_times[job] + RCPSPex.activities[job].duration
                //               << " duration=" << RCPSPex.activities[job].duration
                //               << std::endl;
                // }
                // std::cout << "excess=" << excess_demand << " capacity=" << resource_info[res_idx].capacity << std::endl;


                std::vector<short> current_set;
                current_set.reserve(sorted_jobs.size());
                enumerate_sets_bnb(sorted_jobs, demands_sorted, excess_demand, 0, 0, current_set, local_sets);
                cache[key] = std::move(local_sets);
                it = cache.find(key);
            }
            sets_ptr = &it->second;
        }
        else {
            // no cache — always run BAB
            std::vector<short> sorted_jobs = current_jobs;
            std::sort(sorted_jobs.begin(), sorted_jobs.end(), [&](short a, short b) {
                return resource_info[res_idx].demand_lookup.at(a) >
                       resource_info[res_idx].demand_lookup.at(b);
            });
            std::vector<short> demands_sorted(sorted_jobs.size());
            for (int i = 0; i < (int)sorted_jobs.size(); i++)
                demands_sorted[i] = resource_info[res_idx].demand_lookup.at(sorted_jobs[i]);

            std::vector<short> current_set;
            current_set.reserve(sorted_jobs.size());
            enumerate_sets_bnb(sorted_jobs, demands_sorted, excess_demand, 0, 0, current_set, local_sets);
            sets_ptr = &local_sets;
        }
    }
    else {
        if (setting.use_MDA_cache) {
            auto& cache = get_mda_cache<N>();
            auto it = cache.find(key);
            if (it == cache.end()) {
                enumerate_sets(current_jobs, excess_demand, res_idx, local_sets);
                cache[key] = std::move(local_sets);
                it = cache.find(key);
            }
            sets_ptr = &it->second;
        }
        else {
            enumerate_sets(current_jobs, excess_demand, res_idx, local_sets);
            sets_ptr = &local_sets;
        }
    }
 // // run the other method to compare
 //    std::vector<std::vector<short>> other_sets;
 //
 //    if (setting.use_MDA_BAB) {
 //        // we used BAB, now run enumerate to compare
 //        enumerate_sets(current_jobs, excess_demand, res_idx, other_sets);
 //    } else {
 //        // we used enumerate, now run BAB to compare
 //        std::vector<short> sorted_jobs = current_jobs;
 //        std::sort(sorted_jobs.begin(), sorted_jobs.end(), [&](short a, short b) {
 //            return resource_info[res_idx].demand_lookup.at(a) >
 //                   resource_info[res_idx].demand_lookup.at(b);
 //        });
 //        std::vector<short> demands_sorted(sorted_jobs.size());
 //        for (int i = 0; i < (int)sorted_jobs.size(); i++)
 //            demands_sorted[i] = resource_info[res_idx].demand_lookup.at(sorted_jobs[i]);
 //        std::vector<short> current_set;
 //        enumerate_sets_bnb(sorted_jobs, demands_sorted, excess_demand, 0, 0, current_set, other_sets);
 //    }
 //
 //    // normalize both for comparison — sort each set, then sort list of sets
 //    auto normalize = [](std::vector<std::vector<short>> s) {
 //        for (auto& set : s) std::sort(set.begin(), set.end());
 //        std::sort(s.begin(), s.end());
 //        return s;
 //    };
 //
 //    auto norm_sets = normalize(*sets_ptr);
 //    auto norm_other = normalize(other_sets);
 //
 //    if (norm_sets != norm_other) {
 //        std::cout << "=== MDA MISMATCH ===" << std::endl;
 //        std::cout << "conflict jobs: ";
 //        for (short j : current_jobs) std::cout << j << " ";
 //        std::cout << "\nexcess: " << excess_demand << std::endl;
 //
 //        std::cout << (setting.use_MDA_BAB ? "BAB" : "ENU") << " sets:" << std::endl;
 //        for (const auto& set : norm_sets) {
 //            std::cout << "{ ";
 //            for (short a : set) std::cout << a << " ";
 //            std::cout << "}" << std::endl;
 //        }
 //
 //        std::cout << (setting.use_MDA_BAB ? "ENU" : "BAB") << " sets:" << std::endl;
 //        for (const auto& set : norm_other) {
 //            std::cout << "{ ";
 //            for (short a : set) std::cout << a << " ";
 //            std::cout << "}" << std::endl;
 //        }
 //        std::cout << "===================" << std::endl;
 //    }
    const std::vector<std::vector<short>>& sets = *sets_ptr;

    std::vector<MDA> mdas;
    mdas.reserve(sets.size());
    for (const auto& set : sets) {
        MDA mda;
        mda.activities = set;
        mda.cost = compute_mda_cost(set, current_jobs, latest_starts, res_idx);
        mdas.push_back(std::move(mda));
    }
    return mdas;
}
template<short N>
short RCPSPState_CBS<N>::compute_mda_cost(
    const std::vector<short>& mda_set,
    const std::vector<short>& current_jobs,
    const std::array<short, N>& latest_starts,
    int res_idx) const
{
    // compute new_start once for all set members
    // = min finish of conflict members NOT in the set
    short new_start = std::numeric_limits<short>::max();
    for (short other : current_jobs) {
        if (std::find(mda_set.begin(), mda_set.end(), other) != mda_set.end()) continue;
        short finish = start_times[other] + RCPSPex.activities[other].duration;
        new_start = std::min(new_start, finish);
    }

    // max delta over set members
    short cost = 0;
    for (short job : mda_set) {
        short delta = (new_start > latest_starts[job])
                      ? new_start - latest_starts[job]
                      : 0;
        cost = std::max(cost, delta);
    }
    return cost;
}
template<short N>
short RCPSPState_CBS<N>::compute_h_and_RVS() const {

    if (!setting.use_conflict_prioritization &&setting.use_first_conflict) {
        rvs_activities_pool.clear();
        for (int resIdx = 0; resIdx < (int)resource_info.size(); resIdx++) {
            const ResourceInfo& res = resource_info[resIdx];

            std::vector<short> events;
            events.reserve(res.activity_indices.size());
            for (short actIdx : res.activity_indices)
                events.push_back(start_times[actIdx]);
            std::sort(events.begin(), events.end());
            events.erase(std::unique(events.begin(), events.end()), events.end());

            for (short t : events) {
                short total_demand = 0;
                std::vector<short> current_jobs;

                for (int j = 0; j < (int)res.activity_indices.size(); j++) {
                    short actIdx = res.activity_indices[j];
                    short start  = start_times[actIdx];
                    short finish = start + RCPSPex.activities[actIdx].duration;
                    if (start <= t && finish > t) {
                        total_demand += res.demands[j];
                        current_jobs.push_back(actIdx);
                    }
                }

                if (total_demand <= res.capacity) continue;

                // First conflict found — take it immediately
                rvs_activities_pool = std::move(current_jobs);
                this->t      = t;
                resourceType = resIdx;
                found_conflict = true;
                return 0;// h_cost = 0;

            }
        }
        // No conflict found
        found_conflict = false;

        return 0;//h_cost = 0;
    }

    // Compute latest starts fresh each time — no copy bug
    std::array<short, N> latest_starts = {};
    computeLatestStarts(latest_starts);
    rvs_activities_pool.clear();

                 // best_score = -1.0f;
    float        best_score = -1.0f;
    short        best_t        = -1;
    short        best_resource = -1;
    ConflictKey<N>        best_key;
    std::vector<short> best_jobs;
    bool         found_any     = false;
    std::vector<std::pair<std::vector<short>, short>> cardinal_conflicts;
    // std::vector<std::pair<short,short>> cardinal_pairs;
    std::vector<MDA> best_mdas;

    // conflict searching

    for (int resIdx = 0; resIdx < (int)resource_info.size(); resIdx++) {
        const ResourceInfo& res = resource_info[resIdx];

        std::vector<short> events;
        events.reserve(res.activity_indices.size());
        for (short actIdx : res.activity_indices)
            events.push_back(start_times[actIdx]);
        std::sort(events.begin(), events.end());
        events.erase(std::unique(events.begin(), events.end()), events.end());

        for (short t : events) {

            std::vector<short> current_jobs;
            short total_demand = 0;

            for (int j = 0; j < (int)res.activity_indices.size(); j++) {
                short actIdx = res.activity_indices[j];
                short start  = start_times[actIdx];
                short finish = start + RCPSPex.activities[actIdx].duration;

                if (start <= t && finish > t) {
                    total_demand += res.demands[j];
                    current_jobs.push_back(actIdx);
                }
            }

            if (total_demand <= res.capacity) continue;//conflict found
            // std::cout << resIdx <<":"<<t<< std::endl;
            //
            // for (short job: current_jobs){
            //     std::cout << job << " ";
            //     }
            // std::cout << std::endl;
            if (setting.use_MDA_sets) {
                ConflictKey<N> key = make_conflict_key<N>(current_jobs, (short)resIdx);
                short excess = total_demand - res.capacity;
                std::vector<MDA> mdas = compute_mdas(
                    current_jobs, excess, latest_starts, resIdx,key);

                // std::cout << "MDA:\n";
                // for (a : mdas)
                //     print(a, start[a], end[a]);
                //
                // std::cout << "Non-MDA conflict acts:\n";
                // for (a : rvs)
                //     if (!in_mda(a))
                //         print(a, start[a], end[a]);
                //
                // std::cout << "Computed new_start = " << new_start << "\n";
                //

                int cardinal_count = 0;
                short min_cardinal_cost = std::numeric_limits<short>::max();
                for (auto& mda : mdas) {
                    if (mda.cost > 0) {
                        cardinal_count++;
                        min_cardinal_cost = std::min(min_cardinal_cost, mda.cost);
                    }
                }
                float score = mdas.empty() ? 0.0f : (float)cardinal_count / (float)mdas.size();

                bool is_better = !found_any
                    || (score > best_score)
                    || (score == best_score && t < best_t);
                if (score == 1.0f) {
                    cardinal_conflicts.push_back({current_jobs, min_cardinal_cost});
                }
                if (is_better) {
                    best_t        = t;
                    best_resource = resIdx;
                    best_score    = score;
                    best_mdas =     std::move(mdas);
                    best_jobs     = std::move(current_jobs); // move last
                    best_key      = key;  // store key not solutions


                    // h_cost = (score == 1.0f) ? min_cardinal_cost : 0;
                    found_any     = true;
                }
                // if (setting.use_conflict_prioritization && setting.use_first_conflict) {
                //     if (best_score == 1) goto done;
                // }
            }
            else {
                // if ((short)current_jobs.size() > max_conflict_seen) {
                //     max_conflict_seen = current_jobs.size();
                //     std::cout << "New max conflict size: " << max_conflict_seen << "\n";
                // }
                std::array<short, N> individual_cost = {};
                short costly    = 0;
                short min_forced = std::numeric_limits<short>::max();
                short non_cardinal_demand = 0;

                for (short job : current_jobs) {
                    // Change from max to min in computeRVS classification
                    short new_start = std::numeric_limits<short>::max();
                    for (short other : current_jobs) {
                        if (other == job) continue;
                        new_start = std::min(new_start,
                            (short)(start_times[other] +
                                    RCPSPex.activities[other].duration));
                    }

                    if (new_start > latest_starts[job]) {
                        // short forced = new_start - latest_starts[job];
                        individual_cost[job] = new_start - latest_starts[job];
                        min_forced = std::min(min_forced, individual_cost[job]);
                        costly++;

                    }
                    else {
                        non_cardinal_demand += RCPSPex.activities[job].resource_demands[res.resource_nume];
                    }


                }

                // ConflictType type;
                // if      (costly == (short)current_jobs.size()) type = ConflictType::CARDINAL;
                // else if (costly > 0)                           type = ConflictType::SEMI_CARDINAL;
                // else                                           type = ConflictType::NON_CARDINAL;
                float score = (float)costly / (float)current_jobs.size();
                conflict_number++;
                bool is_better = !found_any
                    || (score > best_score)  // BUG: should be score > best_score!
                    || (score == best_score && t < best_t);
                if (is_better) {
                    best_jobs     = current_jobs;
                    best_t        = t;
                    best_resource = resIdx;
                    best_score     = score;
                    found_any     = true;
                }

                bool non_cardinal_cant_resolve = (total_demand - non_cardinal_demand) > res.capacity;

                if (score == 1.0f || (score > 0.0f && non_cardinal_cant_resolve)) {

                    if (setting.use_greed_conflic_resultion_asstimation) {
                        short temp =min_forced;
                        min_forced = std::numeric_limits<short>::max();

                        short excess=total_demand-res.capacity;
                        // No single activity covered excess, try greedy subset
                        std::vector<short> sorted_jobs = current_jobs;
                        std::sort(sorted_jobs.begin(), sorted_jobs.end(), [&](short a, short b){
                            return individual_cost[a] < individual_cost[b];
                        });

                        short combined_demand = 0;
                        for (short j : sorted_jobs) {
                            combined_demand += RCPSPex.activities[j].resource_demands[res.resource_nume];
                            short subset_cost = individual_cost[j]; // max since sorted ascending
                            if (combined_demand >= excess) {
                                min_forced = std::min(min_forced, subset_cost);
                                break;
                            }
                        }

                    }


                    cardinal_conflicts.push_back({current_jobs, min_forced});
                    debug_cardinal_num++;
                }

                // else if (score == 0) {//none cardinal
                //
                // }
                // else {//semi
                //
                // }

                if (setting.use_conflict_prioritization && setting.use_first_conflict) {
                    if (best_score == 1) goto done; // early exit ok
                }
            }
        }
    }

done:
    // Write winning conflict
    t              = best_t;
    resourceType   = best_resource;
    found_conflict = found_any;
    is_size2_conflict = (best_jobs.size() == 2);
    rvs_activities_pool = std::move(best_jobs);
    conflict_solutions = std::move(best_mdas);

 if (setting.use_MDA_sets) {

     if (setting.use_MDA_cache) {
         conflict_key = best_key;

     }
     else {

     }
 }
else {

}




if (found_conflict && setting.use_ancestor_branching) {
        // Impact Mask -> Best Ancestor index
        // uint32_t is perfect for J30 (up to 32 activities)
        std::unordered_map<uint32_t, short> mask_to_ancestor;

        for (int i = 0; i < (int)rvs_activities_pool.size(); ++i) {
            short jobIdx = rvs_activities_pool[i];

            // Iterate through precomputed ancestors for this job
            for (short P : upstream[jobIdx]) {
                uint32_t impact_mask = 0;
                bool reaches_all = true;

                // Calculate the impact mask for ancestor P across the conflict pool
                for (int j = 0; j < (int)rvs_activities_pool.size(); ++j) {
                    short targetJob = rvs_activities_pool[j];

                    // Since upstream[targetJob] is sorted, binary_search is O(log N)
                    bool reaches = (targetJob == P) ||
                                   std::binary_search(upstream[targetJob].begin(),
                                                      upstream[targetJob].end(), P);

                    if (reaches) {
                        impact_mask |= (1u << j);
                    } else {
                        reaches_all = false;
                    }
                }

                // Rule 1: Skip if it pushes everyone (doesn't resolve the conflict)
                if (reaches_all) continue;

                // Rule 3 (Unique Mask) + Rule 2 (Most Downstream/Highest Start Time)
                auto it = mask_to_ancestor.find(impact_mask);
                if (it == mask_to_ancestor.end()) {
                    mask_to_ancestor[impact_mask] = P;
                } else {
                    // Tie-breaker: Keep the lever physically closest to the conflict
                    if (start_times[P] > start_times[it->second]) {
                        it->second = P;
                    }
                }
            }
        }

        // Add the unique strategic levers to the pool
        for (auto const& [mask, ancestorIdx] : mask_to_ancestor) {
            rvs_activities_pool.push_back(ancestorIdx);
        }
    }


    // Fix bug 1: always set h_cost, even when no cardinals
if (setting.heuristic == HeuristicType::NONE) {
    return 0;//h_cost = 0;
}
if (setting.heuristic == HeuristicType::HCBS) {
    short h_cost = 0;

    for (const auto& c : cardinal_conflicts) {
        h_cost = std::max(h_cost, c.second);
    }
    return h_cost;//h_cost = 0;

}
else if (setting.heuristic == HeuristicType::CG) {
    short h_cost=0;
    // Link conflicts via downstream
    // for (int i = 0; i < (int)cardinal_conflicts.size(); i++) {
    //     for (int j = i+1; j < (int)cardinal_conflicts.size(); j++) {
    //         bool linked = false;
    //         for (short job_i : cardinal_conflicts[i].first) {
    //             if (linked) break;
    //             for (short job_j : cardinal_conflicts[j].first) {
    //                 if (std::find(downstream[job_i].begin(),
    //                               downstream[job_i].end(),
    //                               job_j) != downstream[job_i].end() ||
    //                     std::find(downstream[job_j].begin(),
    //                               downstream[job_j].end(),
    //                               job_i) != downstream[job_j].end()) {
    //                     cardinal_conflicts[i].second = std::max(
    //                         cardinal_conflicts[i].second,
    //                         cardinal_conflicts[j].second);
    //                     for (short job : cardinal_conflicts[j].first)
    //                         cardinal_conflicts[i].first.push_back(job);
    //                     cardinal_conflicts.erase(cardinal_conflicts.begin() + j);
    //                     j--;
    //                     linked = true;
    //                     break;
    //                 }
    //             }
    //         }
    //     }
    // }
    //
    // // Upstream enrichment
    // std::set<short> cardinal_jobs;
    // for (auto& [conflict, weight] : cardinal_conflicts)
    //     for (short job : conflict)
    //         cardinal_jobs.insert(job);
    //
    // for (auto& [conflict, weight] : cardinal_conflicts) {
    //     std::set<short> conflict_set(conflict.begin(), conflict.end());
    //     for (short job : conflict)
    //         for (short pred : upstream[job])
    //             if (cardinal_jobs.count(pred))
    //                 conflict_set.insert(pred);
    //     conflict.assign(conflict_set.begin(), conflict_set.end());
    // }
    // Upstream enrichment (Corrected for Admissibility & Common Ancestors)
    for (auto& [conflict, weight] : cardinal_conflicts) {

        // 1. Keep a copy of the original jobs in this specific conflict
        std::vector<short> original_jobs = conflict;

        std::set<short> union_ancestors;
        std::set<short> intersection_ancestors;
        bool first_job = true;

        // 2. Find the Union (all possible ancestors) and Intersection (common ancestors)
        for (short job : original_jobs) {
            // Assuming upstream[job] gives us the ancestors.
            std::set<short> current_ancestors(upstream[job].begin(), upstream[job].end());

            // Build the union of all ancestors
            for (short pred : current_ancestors) {
                union_ancestors.insert(pred);
            }

            // Build the intersection of all ancestors
            if (first_job) {
                intersection_ancestors = current_ancestors;
                first_job = false;
            } else {
                std::set<short> new_intersection;
                for (short pred : current_ancestors) {
                    if (intersection_ancestors.count(pred)) {
                        new_intersection.insert(pred);
                    }
                }
                intersection_ancestors = new_intersection;
            }
        }

        // 3. Add valid ancestors to the conflict set
        std::set<short> final_conflict_set(original_jobs.begin(), original_jobs.end());

        for (short pred : union_ancestors) {
            // ONLY add if it is NOT a common ancestor to ALL original jobs
            if (intersection_ancestors.count(pred) == 0) {
                final_conflict_set.insert(pred);
            }
        }

        // 4. Update the actual conflict vector
        conflict.assign(final_conflict_set.begin(), final_conflict_set.end());
    }
    h_cost = cardinal_conflicts.empty() ? 0 : computeWeightedSetCover(cardinal_conflicts);
}
else if (setting.heuristic == HeuristicType::DG) {
    return 0;//h_cost = 0;
}
    // std::cout << h_cost<< std::endl;
return 0;
}

template<short N>
void RCPSPState_CBS<N>::propagate_with_strong_form_0() {
    std::array<bool, N> visited = {};
    // for (const auto& [f, t] : added_precedences) {
    //     std::cout << "added: " << f << " -> " << t  << std::endl;
    // }
    // std::cout << "sink: " << g_sink_id << std::endl;
   compute_start_recursive(g_sink_id, visited);

}

template<short N>
short RCPSPState_CBS<N>::compute_start_recursive(short act, std::array<bool, N>& visited) {
    if (visited[act]) return start_times[act];
    // if (in_stack[act]) return start_times[act]; // cycle detected — break

    // in_stack[act] = true;.

    short new_start = 0;

    for (short dep : RCPSPex.backword_dependencies[act]) {
        int depIdx = dep - 1;
        compute_start_recursive(depIdx, visited);
        new_start = std::max((int)new_start,
            (int)start_times[depIdx] + RCPSPex.activities[depIdx].duration);
    }

    for (const auto& [f, t] : added_precedences) {
        if (t == act) {
            short fIdx = f; // if f is 1-based
            compute_start_recursive(fIdx, visited);
            new_start = std::max((int)new_start,
                (int)start_times[fIdx] + RCPSPex.activities[fIdx].duration);
        }
    }

    // take max with existing to preserve parent delays
    start_times[act] = std::max(new_start, start_times[act]);
    visited[act] = true;
    return start_times[act];
}


template<short N>
void RCPSPState_CBS<N>::propagate(short activityId) {
    for (short succ : downstream[activityId]) {
        // std::cout << succ <<std::endl;
        short new_start = start_times[succ];
        for (short dep : RCPSPex.backword_dependencies[succ]) {
            int depIdx = dep - 1;
            new_start = std::max((int)new_start,
                start_times[depIdx] + RCPSPex.activities[depIdx].duration);
        }
        // Only update if pushing FORWARD - never allow going backwards
        // if (new_start > start_times[succ]) {
            start_times[succ] = new_start;
        // }
    }
}
//
// void RCPSPState_CBS::propagate_latest(short activityId) {
//     for (short pred : upstream[activityId]) {
//         short new_latest = std::numeric_limits<short>::max();
//
//         // Latest pred can start = min over all its successors' latest starts
//         for (short succ : downstream[pred]) {
//             new_latest = std::min(new_latest,
//                 (short)(latest_start_times[succ]
//                         - RCPSPex.activities[pred].duration));
//         }
//
//         // Only update if tightening — never allow going forwards
//         if (new_latest < latest_start_times[pred]) {
//             latest_start_times[pred] = new_latest;
//         }
//     }
// }


// for (int i = activityId + 1; i < RCPSPex.activities.size(); i++) {
    //     std::string succName = RCPSPex.activities[i].name;
    //
    //     if (RCPSPex.deep_dependencies.count({actName, succName})) {
    //         short new_start = 0;
    //         for (short dep : RCPSPex.backword_dependencies[i]) {
    //             int depIdx = dep - 1;
    //             short finish = start_times[depIdx] + RCPSPex.activities[depIdx].duration;
    //             std::cout << "dep=" << dep
    //                       << " depIdx=" << depIdx
    //                       << " start=" << start_times[depIdx]
    //                       << " duration=" << RCPSPex.activities[depIdx].duration
    //                       << " finish=" << finish << std::endl;
    //             new_start = std::max((int)new_start, (int)finish);
    //         }
    //         std::cout << "Activity " << succName
    //                   << " old_start=" << start_times[i]
    //                   << " new_start=" << new_start << std::endl;
    //         start_times[i] = new_start;
    //     }
    // }

// }
// Definition - only in ONE .cpp file

// ******
void precomputeResourceInfo() {
  resource_info.clear();
  resource_info.resize(RCPSPex.resources.size());

  for (int resIdx = 0; resIdx < RCPSPex.resources.size(); resIdx++) {
    resource_info[resIdx].capacity = RCPSPex.resources[resIdx].second;
    std::string resName = RCPSPex.resources[resIdx].first;
    resource_info[resIdx].resource_nume= resName;//0-3 index

    for (int i = 0; i < RCPSPex.activities.size(); i++) {
      if (RCPSPex.activities[i].resource_demands.count(resName) == 0) continue;
      short demand = RCPSPex.activities[i].resource_demands.at(resName);
      if (demand == 0) continue;
      if (RCPSPex.activities[i].duration == 0) continue;

      resource_info[resIdx].activity_indices.push_back(i);
      resource_info[resIdx].demands.push_back(demand);
        resource_info[resIdx].demand_lookup[i] = demand; // i is the activity index directly


    }
  }
}
// void precomputeDownstream();

void precomputeDownstream() {
  downstream.clear();
  downstream.resize(RCPSPex.activities.size());


  for (int i = 0; i < RCPSPex.activities.size(); i++) {
    downstream[i].clear(); // clear each inner vector too

    // Run a "mock propagate" from activity i
    // collect all activities that would be affected
    std::vector<bool> visited(RCPSPex.activities.size(), false);
    std::queue<short> queue;
    queue.push(i);

    while (!queue.empty()) {
      short curr = queue.front();
      queue.pop();

      // Find direct successors of curr
      for (int j = curr + 1; j < RCPSPex.activities.size(); j++) {
        for (short dep : RCPSPex.backword_dependencies[j]) {
          if (dep - 1 == curr && !visited[j]) {
            visited[j] = true;
            downstream[i].push_back(j);
            queue.push(j);
            break;
          }
        }
      }
    }
    // Already in topological order since j > curr always
    std::sort(downstream[i].begin(), downstream[i].end());

  }
}

void precomputeUpstream() {
  upstream.clear();
  upstream.resize(RCPSPex.activities.size());

  for (int i = 0; i < RCPSPex.activities.size(); i++) {
    upstream[i].clear();

    std::vector<bool> visited(RCPSPex.activities.size(), false);
    std::queue<short> queue;
    queue.push(i);

    while (!queue.empty()) {
      short curr = queue.front();
      queue.pop();

      // Find direct predecessors of curr — reverse of downstream
      for (short dep : RCPSPex.backword_dependencies[curr]) {
        short pred = dep - 1;
        if (!visited[pred]) {
          visited[pred] = true;
          upstream[i].push_back(pred);
          queue.push(pred);
        }
      }
    }

    // Reverse topological order — largest index first
    std::sort(upstream[i].begin(), upstream[i].end(), std::greater<short>());
  }
}


template<short N>
RCPSPState_CBS<N>::RCPSPState_CBS() {
    // ── Forward pass: earliest start times ──────────────────────────────
    std::map<int, int> earlyStartMap;
    for (int i = 1; i <= RCPSPex.activities.size(); i++) {
        int maxFinish = 0;
        for (int dep : RCPSPex.backword_dependencies[i - 1]) {
            int depId = dep - 1;
            maxFinish = std::max(maxFinish,
                earlyStartMap[depId + 1] + RCPSPex.activities[depId].duration);
        }
        earlyStartMap[i] = maxFinish;
    }

    for (int i = 1; i <= RCPSPex.activities.size(); i++)
        start_times[i - 1] = earlyStartMap[i];

    g_sink_id = earlyStartMap.rbegin()->first - 1;
    short makespan = earlyStartMap[g_sink_id + 1]
                   + RCPSPex.activities[g_sink_id].duration;

    // ── Backward pass: latest start times ───────────────────────────────
    // Iterate in reverse — predecessors of j are processed after j
    std::map<int, int> lateStartMap;

    // Sink can start as late as its own early start (it defines makespan)
    lateStartMap[g_sink_id + 1] = earlyStartMap[g_sink_id + 1];

    for (int i = RCPSPex.activities.size(); i >= 1; i--) {
        if (lateStartMap.count(i)) continue; // sink already set

        // Latest this job can finish = min over all successors' latest start
        int minSuccessorStart = makespan; // fallback if no successors
        for (int succ : RCPSPex.dependencies[i - 1]) {
            minSuccessorStart = std::min(minSuccessorStart, lateStartMap[succ]);
        }

        // Latest start = latest finish - own duration
        lateStartMap[i] = minSuccessorStart - RCPSPex.activities[i - 1].duration;
    }

    // for (int i = 1; i <= RCPSPex.activities.size(); i++)
    //     latest_start_times[i - 1] = lateStartMap[i];
    // std::cout << "Activity | Early Start | Late Start | Duration\n";
    // std::cout << "---------------------------------------------\n";
    // for (int i = 1; i <= RCPSPex.activities.size(); i++) {
    //     std::cout << "Act " << std::setw(3) << i
    //               << " | " << std::setw(11) << start_times[i-1]
    //               << " | " << std::setw(10) << latest_start_times[i-1]
    //               << " | " << std::setw(8) << RCPSPex.activities[i-1].duration
    //               << "\n";
    // }
    // std::cout << "Makespan: " << makespan << "\n";
    // std::cout << "------------------------------------------\n";
    compute_h_and_RVS();
}
template<short N>
RCPSPState_CBS<N>::RCPSPState_CBS(const RCPSPState_CBS &prev, short delayedActivity, short conflict_t) {
    // 1. Copy parent
    start_times = prev.start_times;
    // depth=prev.depth+1;
    // start_times = prev.latest_start_times;
    // sink_id = prev.sink_id;

    // 2. Find latest finish of all other activities in conflict
    short new_start = 9999;

    // 1. Pass the specific conflict (prev.RVS[0]) into the helper function
    // 2. Iterate directly over the 'act' values
    std::span<const short> acts = prev.rvs_activities_pool;
    for (short j = 0; j < prev.rvs_activities_pool.size(); j++) {
        short act = acts[j];
        if (act == delayedActivity) continue;
        new_start = std::min(new_start,
            (short)(prev.start_times[act] + RCPSPex.activities[act].duration));
    }

    start_times[delayedActivity] = new_start;
    propagate(delayedActivity);


    // start_times = prev.start_times;
    // // depth=prev.depth+1;
    // // start_times = prev.latest_start_times;
    // // sink_id = prev.sink_id;
    // if (conflict_t>prev.start_times[delayedActivity] + RCPSPex.activities[delayedActivity].duration) {
    //     // --- ANCESTOR LOGIC ---
    //     short target_finish = 9999;
    //     short descendant_start = 9999;
    //
    //     std::span<const short> acts = prev.rvs_activities_pool;
    //
    //     for (short j = 0; j < acts.size(); j++) {
    //         short act = acts[j];
    //
    //         // 1. Check if 'act' is related to the ancestor
    //         // Since your precomputeUpstream sorts descending, we pass std::greater<short>()
    //         bool is_descendant = std::binary_search(upstream[act].begin(), upstream[act].end(),
    //                                                 delayedActivity, std::greater<short>());
    //
    //         if (is_descendant) {
    //             // Track the start time of the descendant we are trying to push.
    //             // If it pushes multiple, we take the earliest one to be safe.
    //             descendant_start = std::min(descendant_start, prev.start_times[act]);
    //         }
    //         else {
    //             // 2. Not related. This job stays put. Find its finish time to get our target.
    //             // (Identical logic to your 'else' block)
    //             if (conflict_t > prev.start_times[act] + RCPSPex.activities[act].duration) continue;
    //             target_finish = std::min(target_finish,
    //                 (short)(prev.start_times[act] + RCPSPex.activities[act].duration));
    //         }
    //     }
    //
    //     // 3. What delay is needed? (Target Finish - Current Descendant Start)
    //     short delay = target_finish - descendant_start;
    //
    //     // 4. Save new start of ancestor: Current Start + Delay
    //     start_times[delayedActivity] = prev.start_times[delayedActivity] + delay;
    //
    //
    //
    //
    // }
    // else{
    // // 2. Find latest finish of all other activities in conflict
    // short new_start = 9999;
    //
    // // 1. Pass the specific conflict (prev.RVS[0]) into the helper function
    // // 2. Iterate directly over the 'act' values
    // std::span<const short> acts = prev.rvs_activities_pool;
    // for (short j = 0; j < prev.rvs_activities_pool.size(); j++) {
    //     short act = acts[j];
    //     if (act == delayedActivity) continue;
    //     if (conflict_t>prev.start_times[act] + RCPSPex.activities[act].duration) continue;
    //     new_start = std::min(new_start,
    //         (short)(prev.start_times[act] + RCPSPex.activities[act].duration));
    // }
    //
    // start_times[delayedActivity] = new_start;
    // }









    // 3. Update delayed activity
    // start_times[delayedActivity] += mindelay;

    // 4. Propagate forward
    // propagate_latest(delayedActivity);


}

template<short N>
RCPSPState_CBS<N>::RCPSPState_CBS(const RCPSPState_CBS& prev, const std::vector<short>& mda_activities, short conflict_t) {
    // 1. Copy parent
    // start_times = prev.start_times;
//     std::cout << "new state"<<std::endl;
//
// std::cout<<mda_activities.size()<<std::endl;
    start_times = prev.start_times;
    added_precedences = prev.added_precedences;

    short new_start = 9999;

    if (!prev.rvs_activities_pool.empty()) {
        // pool available — use it directly
        std::span<const short> acts = prev.rvs_activities_pool;
        for (short act : acts) {
            if (std::find(mda_activities.begin(), mda_activities.end(), act) != mda_activities.end()) continue;
            new_start = std::min(new_start,
                (short)(prev.start_times[act] + RCPSPex.activities[act].duration));
        }
    } else {
        // reconstruct from resource and conflict time
        const ResourceInfo& res = resource_info[prev.resourceType];
        for (int j = 0; j < (int)res.activity_indices.size(); j++) {
            short actIdx = res.activity_indices[j];
            short start  = prev.start_times[actIdx];
            short finish = start + RCPSPex.activities[actIdx].duration;
            if (start <= prev.t && finish > prev.t) {
                if (std::find(mda_activities.begin(), mda_activities.end(), actIdx) != mda_activities.end()) continue;
                new_start = std::min(new_start, finish);
            }
        }
    }

    for (short delayed : mda_activities) {
        start_times[delayed] = new_start;
    }

    // if (added_precedences.empty()) {
        propagate(0);
    // } else {
    //     propagate_with_strong_form_0();
    // }
}
    // 2. Reconstruct current_jobs from resource and conflict time
    // short new_start = 9999;
    // const ResourceInfo& res = resource_info[prev.resourceType];
    // for (int j = 0; j < (int)res.activity_indices.size(); j++) {
    //     short actIdx = res.activity_indices[j];
    //     short start  = prev.start_times[actIdx];
    //     short finish = start + RCPSPex.activities[actIdx].duration;
    //     if (start <= prev.t && finish > prev.t) {
    //         // skip if in MDA set
    //         if (std::find(mda_activities.begin(), mda_activities.end(), actIdx) != mda_activities.end()) continue;
    //         new_start = std::min(new_start, finish);
    //     }
    // }
    // 2. Find latest finish of all other activities in conflict
    // short new_start = 9999;

    // 1. Pass the specific conflict (prev.RVS[0]) into the helper function
    // 2. Iterate directly over the 'act' values
    // std::span<const short> acts = prev.rvs_activities_pool;
    // for (short j = 0; j < prev.rvs_activities_pool.size(); j++) {
    //
    //     short act = acts[j];
    // if (std::find(mda_activities.begin(), mda_activities.end(), act) != mda_activities.end()) continue;
    //     new_start = std::min(new_start,
    //         (short)(start_times[act] + RCPSPex.activities[act].duration));
    // }
    // // 3. Apply to all MDA members
    // // std::cout << "new_start for MDA: " << new_start << std::endl;
    // for (short delayed : mda_activities) {
    //     // std::cout << "delaying activity " << delayed << " from " << start_times[delayed] << " to " << new_start << std::endl;
    //     start_times[delayed] = new_start;
    //     // propagate(delayed); // fast path — no strong constraints
    //
    // }
    // propagate(0); // fast path — no strong constraints

    // if (setting.use_strong_constraints) {
    //     added_precedences = prev.added_precedences;
    //     is_size2_conflict = false;
    //     propagate_with_strong_form_0(); // full pass with strong constraints
    //
    // } else {
    //
    // }
// }

template<short N>
RCPSPState_CBS<N>::RCPSPState_CBS(const RCPSPState_CBS &prev, short from, short to, short conflict_t) {
    // 1. Copy parent
    start_times = prev.start_times;
    is_size2_conflict = false;
    added_precedences = prev.added_precedences;
    added_precedences.push_back({from, to});
    // explicitly enforce new constraint
    start_times[to] = start_times[from] + RCPSPex.activities[from].duration;
    propagate_with_strong_form_0(); // full pass with all strong constraints

    // // propagate downstream effects
    // if (added_precedences.size() == 1) {
    //     propagate(to); // fast path — only one strong constraint, use original propagate
    // } else {
    //     propagate_with_strong_form_0(); // full pass with all strong constraints
    // }
}

template<short N>
bool RCPSPState_CBS<N>::isLeftShiftable() const {
    for (int i = 0; i < RCPSPex.activities.size(); i++) {
        // Compute earliest possible start given predecessors
        short earliest = 0;
        for (short pred : RCPSPex.backword_dependencies[i]) {
            short predIdx = pred - 1;
            earliest = std::max(earliest,
                (short)(start_times[predIdx] +
                        RCPSPex.activities[predIdx].duration));
        }
        // If activity starts later than necessary — left shiftable
        if (start_times[i] > earliest)
            return true;
    }
    return false;
}
template<short N>
bool RCPSPState_CBS<N>::dominates(const RCPSPState_CBS& other) const {
    for (int i = 0; i < RCPSPex.activities.size(); i++)
        if (start_times[i] > other.start_times[i])
            return false;
    return true;
}

void RCPSPState_BAP::computeFirstRVS() const {
    // 1. Clear the pool
    rvs_activities_pool.clear();

    short best_t = 32767;     // Start with the max possible short value
    short best_resIdx = -1;

    for (int resIdx = 0; resIdx < resource_info.size(); resIdx++) {
        const ResourceInfo& res = resource_info[resIdx];

        // 2. Collect event points (std::set automatically sorts them from earliest to latest)
        // std::set<short> events;
        // for (short actIdx : res.activity_indices) {
        //     events.insert(start_times[actIdx]);
        //     events.insert(start_times[actIdx] + RCPSPex.activities[actIdx].duration);
        // }
        thread_local std::vector<short> events;
        events.clear(); // Resets size to 0, but keeps the underlying memory intact

        // 2. Dump all times into the flat array
        for (short actIdx : res.activity_indices) {
            events.push_back(start_times[actIdx]);
            events.push_back(start_times[actIdx] + RCPSPex.activities[actIdx].duration);
        }

        // 3. Sort chronologically (Massively faster than a Red-Black Tree for small N)
        std::sort(events.begin(), events.end());

        // 4. Strip out duplicates
        // (std::unique shifts duplicates to the back, erase chops them off)
        events.erase(std::unique(events.begin(), events.end()), events.end());
        // 3. Check each event point in chronological order
        for (short current_t : events) {

            // MASSIVE SPEEDUP: If this time is already later than or equal to a conflict
            // we found on a previous resource, stop checking this resource entirely!
            if (current_t >= best_t) {
                break;
            }

            short total_demand = 0;
            short conflict_size = 0;

            // Calculate demand without touching the pool yet
            for (int j = 0; j < res.activity_indices.size(); j++) {
                short actIdx = res.activity_indices[j];
                short start = start_times[actIdx];
                short finish = start + RCPSPex.activities[actIdx].duration;

                if (start <= current_t && finish > current_t) {
                    total_demand += res.demands[j];
                    conflict_size++;
                }
            }

            // 4. Did we find a conflict?
            // Because of the `current_t >= best_t` check above, if we find a conflict here,
            // we are 100% guaranteed it is the new global earliest conflict.
            if (total_demand > res.capacity) {

                best_t = current_t;
                best_resIdx = resIdx;

                // Update the state's class variables
                this->t = best_t;
                this->resourceType = best_resIdx;
                this->num_activities = conflict_size;

                // Overwrite the pool with THIS specific conflict's activities
                rvs_activities_pool.clear();
                for (int j = 0; j < res.activity_indices.size(); j++) {
                    short actIdx = res.activity_indices[j];
                    short start = start_times[actIdx];
                    short finish = start + RCPSPex.activities[actIdx].duration;

                    if (start <= current_t && finish > current_t) {
                        rvs_activities_pool.push_back(actIdx);
                    }
                }

                // We found the earliest conflict for THIS resource.
                // Any other conflicts on this resource will be later, so we stop checking it!
                break;
            }
        }
    }

    // 5. Handle the case where the schedule is perfect and there are zero conflicts
    if (best_resIdx == -1) {
        this->t = -1; // Use -1 to signal a terminal/success state in your solver
        this->num_activities = 0;
    }
}

void RCPSPState_BAP::propagate(short activityId) {
    for (short succ : downstream[activityId]) {
        short new_start = 0;
        for (short dep : RCPSPex.backword_dependencies[succ]) {
            int depIdx = dep - 1;
            new_start = std::max((int)new_start,
                start_times[depIdx] + RCPSPex.activities[depIdx].duration);
        }
        // Only update if pushing FORWARD - never allow going backwards
        if (new_start > start_times[succ]) {
            start_times[succ] = new_start;
        }
    }
}

RCPSPState_BAP::RCPSPState_BAP() {
    std::vector<short> unstartedTransitions;
    for (int i = 1; i <= RCPSPex.activities.size(); i++) {
        unstartedTransitions.push_back(i);
    }

    std::map<int, int> earlyStartMap;

    for (int activityId : unstartedTransitions) {
        int maxFinishTime = 0;
        for (int dep : RCPSPex.backword_dependencies[activityId - 1]) {
            int depId = dep - 1;
            maxFinishTime = std::max(maxFinishTime,
                earlyStartMap[depId + 1] + RCPSPex.activities[depId].duration);
        }
        earlyStartMap[activityId] = maxFinishTime;
    }
    // start_times.resize(RCPSPex.activities.size());
    for (int i = 1; i <= RCPSPex.activities.size(); i++) {
        start_times[i-1] = earlyStartMap[i];
    }

    // Sink is last activity in map (highest key)
    g_sink_id = earlyStartMap.rbegin()->first - 1; // -1 for 0-indexed
    // computeRVS hidden for now
    computeFirstRVS();

}

RCPSPState_BAP::RCPSPState_BAP(const RCPSPState_BAP &prev, short delayedActivity, short duration) {
    start_times = prev.start_times;
    // sink_id = prev.sink_id;

    // 2. Find latest finish of all other activities in conflict
    short new_start = 9999;

    // 1. Pass the specific conflict (prev.RVS[0]) into the helper function
    // 2. Iterate directly over the 'act' values
    const short* acts = prev.rvs_activities_pool.data();
    for (short j = 0; j < prev.num_activities; j++) {
        short act = acts[j];
        if (act == delayedActivity) continue;
        new_start = std::min(new_start,
            (short)(prev.start_times[act] + RCPSPex.activities[act].duration));
    }
    start_times[delayedActivity] = new_start;

    propagate(delayedActivity);
    computeFirstRVS();

}

std::vector<std::pair<short, short>> getAvailableTransitionIndices_TT2(
    const std::vector<short> &unstartedTransitions,
    const std::bitset<128> &finishedActivitiys,  // ← Changed to bitset
    const std::array<std::vector<std::pair<short, short>>, 4> &resource_nodes,
    const std::vector<std::pair<short, short>> &activity_nodes,
    const std::vector<std::pair<short, short>> &activeTransitionIndices
) {




    std::vector<std::pair<short, short>> available;

    // Create a set of active task IDs for fast lookup
    std::unordered_set<short> activeTasks;
    for (const auto& [taskID, _] : activeTransitionIndices) {
        activeTasks.insert(taskID);
    }

    for (short transId : unstartedTransitions) {
        // Skip if already active
        if (activeTasks.count(transId) > 0) {
            continue;
        }

        const auto& dependencies = RCPSPex.backword_dependencies[transId - 1];
        const Activity &act = RCPSPex.activities[transId - 1];

        // 1. Precedence constraints
        bool allPredsFinished = true;
        int maxPredFinishTime = 0;

        for (int predId : dependencies) {
            // Check if finished (using bitset)
            if (finishedActivitiys.test(predId)) {  // ← bitset::test()
                // Finished - no waiting needed
                continue;
            }

            // Check if active
            bool isActive = false;
            int activeRemainingTime = 0;
            for (const auto& [activeID, remaining] : activeTransitionIndices) {
                if (activeID == predId) {
                    isActive = true;
                    activeRemainingTime = remaining;
                    break;
                }
            }

            if (isActive) {
                // Predecessor is active - must wait for it to finish
                maxPredFinishTime = std::max(maxPredFinishTime, activeRemainingTime);
            } else {
                // Predecessor not finished and not active - can't start this task
                allPredsFinished = false;
                break;
            }
        }

        if (!allPredsFinished)
            continue;

        // 2. Resource availability
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

            const auto& tokens = sorted_resources[resID];
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
            }
            else {
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

std::vector<std::pair<short, short>> getAvailableTransitionIndices_TT2_backward(
    const std::vector<short> &unstartedTransitions,
    const std::bitset<128> &finishedActivitiys,
    const std::array<std::vector<std::pair<short, short>>, 4> &resource_nodes,
    const std::vector<std::pair<short, short>> &activity_nodes,
    const std::vector<std::pair<short, short>> &activeTransitionIndices) {

    std::vector<std::pair<short, short>> available;

    std::unordered_set<short> activeTasks;
    for (const auto& [taskID, _] : activeTransitionIndices) {
        activeTasks.insert(taskID);
    }

    for (short transId : unstartedTransitions) {
        if (activeTasks.count(transId) > 0) continue;

        const auto& dependencies = RCPSPex.dependencies[transId - 1];
        const Activity &act = RCPSPex.activities[transId - 1];
        const Transition& trans = petri.Transitions[transId - 1];

        bool canUnfinish = true;
        int maxSuccFinishTime = 0;

        // 1. Check successors are unfinished
        for (int succId : dependencies) {
            if (finishedActivitiys.test(succId)) {
                canUnfinish = false;
                break;
            }
        }
        //
        // //     bool isActive = false;
        // //     int activeRemainingTime = 0;
        // //     for (const auto& [activeID, remaining] : activeTransitionIndices) {
        // //         if (activeID == succId) {
        // //             isActive = true;
        // //             activeRemainingTime = remaining;
        // //             break;
        // //         }
        // //     }
        // //
        // //     if (isActive) {
        // //         maxSuccFinishTime = std::max(maxSuccFinishTime, activeRemainingTime);
        // //     }
        // // }

        if (!canUnfinish) continue;

        // 2. Check OUTPUT Activity Tokens MUST exist
        int maxTokenTime = maxSuccFinishTime;

        for (const auto& [placeID, outAmount] : trans.arcs_out_indices) {
            if (placeID >= 4) {
                if (outAmount == 0) continue;  // Skip unused

                short idx = placeID - 4;
                if (activity_nodes[idx].first < outAmount) {
                    canUnfinish = false;
                    break;
                }
                maxTokenTime = std::max(maxTokenTime, static_cast<int>(activity_nodes[idx].second));
            }
        }

        if (!canUnfinish) continue;

        // 3. Check OUTPUT Resource Tokens
        int maxResourceTime = maxTokenTime;

        // Use resource_demands just like forward does
        for (const auto &[res, demand] : act.resource_demands) {
            if (demand == 0) continue;  // Skip if no demand

            int resID = petri.place_name_to_id.at(res);
            const auto& tokens = resource_nodes[resID];

            std::vector<std::pair<short, short>> sorted_tokens = tokens;
            std::sort(sorted_tokens.begin(), sorted_tokens.end(),
                     [](const auto& a, const auto& b) { return a.second < b.second; });

            int totalAvailable = 0;
            int resourceReadyTime = -1;

            for (const auto& [amt, time] : sorted_tokens) {
                totalAvailable += amt;
                if (totalAvailable >= demand) {  // Use demand instead of outAmount
                    resourceReadyTime = static_cast<int>(time);
                    break;
                }
            }

            if (resourceReadyTime == -1) {
                canUnfinish = false;
                break;
            }

            maxResourceTime = std::max(maxResourceTime, resourceReadyTime);
        }

        if (!canUnfinish) continue;

        available.emplace_back(transId, static_cast<short>(maxResourceTime));
    }

    return available;
}

RCPSPState_TT2::RCPSPState_TT2() {
    // Build mapping: resource name -> index (0-3)
    std::unordered_map<int, int> place_to_resource_idx;
    int res_count = 0;
    for (const auto& [resName, cap] : RCPSPex.resources) {
        int resID = petri.place_name_to_id.at(resName);
        place_to_resource_idx[resID] = res_count++;
    }

    // Count activity nodes
    int num_activities = petri.places.size() - res_count;
    activity_nodes.resize(num_activities);

    finishedActivitiys.reset();
    int activity_counter = 0;
    for (int i = 0; i < petri.places.size(); ++i) {
        const auto& place = petri.places[i];

        // Identify Start/End Names
        if (place.arcs_out.empty()) finalstatename = place.name;
        if (place.arcs_in.empty())  initialstatename = place.name;

        // Check if resource node
        auto it = place_to_resource_idx.find(i);

        if (it != place_to_resource_idx.end()) {
            // This is a resource node
            int res_idx = it->second;

            if (place.name == initialstatename) {
                resource_nodes[res_idx].push_back({1, 0});
            } else if (!place.state.empty() && !place.state[0].empty()) {
                int val = place.state[0][0];
                if (val > 0) resource_nodes[res_idx].push_back({val, 0});
            }
        } else {
            // This is an activity node
            if (place.name == initialstatename) {
                activity_nodes[activity_counter] = {1, 0};
            } else if (!place.state.empty() && !place.state[0].empty()) {
                int val = place.state[0][0];
                activity_nodes[activity_counter] = (val > 0) ? std::make_pair<short,short>(val, 0) : std::make_pair<short,short>(0, 0);
            } else {
                activity_nodes[activity_counter] = {0, 0};
            }
            activity_counter++;
        }
    }

    // Add resource capacities
    for (const auto& [resName, cap] : RCPSPex.resources) {
        if (cap > 0) {
            int resID = petri.place_name_to_id.at(resName);
            int res_idx = place_to_resource_idx[resID];

            if (resource_nodes[res_idx].empty()) {
                resource_nodes[res_idx].push_back({cap, 0});
            }
        }
    }
    std::vector<short> tempUnstarted;
    tempUnstarted.reserve(petri.Transitions.size());
    for (int i = 0; i < petri.Transitions.size(); i++) {
        short taskID = i + 1;
        if (finishedActivitiys[taskID] == 0) {
            tempUnstarted.push_back(taskID);
        }
    }


    // h=getForwardHcost_TT2(tempUnstarted,activity_nodes,activeTransitionIndices,finishedActivitiys,nextCritical);
    h = getForwardHcost(tempUnstarted,activeTransitionIndices,nextCritical);  // ← ADD THIS

    predessesor_h=h;
    g = 0;
}

RCPSPState_TT2::RCPSPState_TT2(const RCPSPState_TT2 &prev, short transitionId, short firingTime,bool Direction) {
    // 1. Update Global Cost (G)
    direction=Direction;
if (direction) {
    isDeltaZero = (firingTime == 0);
    g = prev.g + firingTime;
    predessesor_h = prev.h;  // Store parent's h for isDeltaZero optimization
    lastTransitionId=transitionId;
    // 2. Copy State
    finishedActivitiys = prev.finishedActivitiys;
    resource_nodes = prev.resource_nodes;
    activity_nodes = prev.activity_nodes;
    activeTransitionIndices = prev.activeTransitionIndices;  // Copy active list
    nextCritical=prev.nextCritical;

    transitionsCached = false;
    isCriticalInActive = false;

    // if (nextCritical==transitionId) {
    //     isCriticalInActive = true;
    // }
    // auto it = activeTransitionIndices.begin();
    // while (it != activeTransitionIndices.end()) {
    //     if (nextCritical==it->first) {
    //         isCriticalInActive=true;
    //     }
    // }

    AvailableTransitionIndices_TT2.clear(); // or just leave empty
    // 3. TIME SHIFT (Update "Remaining Time")
    if (firingTime > 0) {
        // Update resource tokens
        for (auto& resVec : resource_nodes) {
            for (auto& token : resVec) {
                token.second = std::max(0, token.second - firingTime);
            }
        }

        // Update activity node tokens
        for (auto& token : activity_nodes) {
            if (token.first > 0) {
                token.second = std::max(0, token.second - firingTime);
            }
        }

        // ========== UPDATE ACTIVE LIST ==========
        // Decrement remaining time for all active activities
        auto it = activeTransitionIndices.begin();
        while (it != activeTransitionIndices.end()) {
            it->second -= firingTime;  // Reduce remaining time

            if (it->second <= 0) {
                // Activity finished - mark it as complete
                short completedTaskID = it->first;
                finishedActivitiys[completedTaskID] = 1;

                // Produce output tokens for this completed activity
                const Transition& completedTransition = petri.Transitions[completedTaskID - 1];
                // for (const auto& [placeID, outAmount] : completedTransition.arcs_out_indices) {
                //     if (placeID < 4) {
                //         // Resource output
                //         resource_nodes[placeID].emplace_back(outAmount, 0);  // Available NOW
                //     } else {
                //         // Activity dependency output
                //         short idx = placeID - 4;
                //         activity_nodes[idx] = {outAmount, 0};  // Available NOW
                //     }
                // }

                // Remove from active list
                it = activeTransitionIndices.erase(it);
            } else {
                ++it;
            }
        }
    }

    // 4. Consume Resources (Standard Resources Only)
    const Transition& transition = petri.Transitions[transitionId - 1];
    const Activity& act = RCPSPex.activities[transitionId - 1];
    short duration = act.duration;

    for (const auto& [resName, demand] : act.resource_demands) {
        if (demand > 0) {
            short resID = petri.place_name_to_id.at(resName);
            auto& tokens = resource_nodes[resID];

            std::sort(tokens.begin(), tokens.end(),
                     [](const auto& a, const auto& b) { return a.second < b.second; });

            int remainingDemand = demand;
            auto it = tokens.begin();
            while (remainingDemand > 0 && it != tokens.end()) {
                if (it->first > remainingDemand) {
                    it->first -= remainingDemand;
                    remainingDemand = 0;
                } else {
                    remainingDemand -= it->first;
                    it = tokens.erase(it);
                }
            }
        }
    }

    // 5. Consume Input Dependency Token (Activity Nodes)
    for (const auto& [placeID, inAmount] : transition.arcs_in_indices) {
        if (placeID >= 4) {
            short idx = placeID - 4;
            activity_nodes[idx].first = 0;
            activity_nodes[idx].second = 0;
        }
    }

    // 6. ADD TO ACTIVE LIST (instead of marking as finished immediately)
    // 6. ADD TO ACTIVE LIST & GENERATE FUTURE EVENTS
    if (duration > 0) {
        // A. Track the Task (for "Finished" status logic)
        activeTransitionIndices.emplace_back(transitionId, duration);

        // B. THE FIX: Immediately return outputs as "Future Events"
        // We put resources back NOW, but marked as "Available in 'duration' seconds"
        const Transition& currentTrans = petri.Transitions[transitionId - 1];

        for (const auto& [placeID, outAmount] : currentTrans.arcs_out_indices) {
            if (placeID < 4) {
                // RESOURCE: Return it now with a delay
                resource_nodes[placeID].emplace_back(outAmount, duration);
            }
            else {
                // ACTIVITY TOKEN: Produce it now with a delay
                // (Be careful with index math: placeID 4 is index 0 in activity_nodes?)
                // Assuming your activity_nodes starts from Place 4:
                short idx = placeID - 4;
                // Only add if not already there (or handled by your specific logic)
                // For TT, usually we just set the availability time:
                if (idx < activity_nodes.size()) {
                    if (activity_nodes[idx].first == 0 || activity_nodes[idx].second > duration) {
                        activity_nodes[idx] = {outAmount, duration};
                    }
                }
            }
        }

        // Optional: Sort active list
        std::sort(activeTransitionIndices.begin(), activeTransitionIndices.end());

    } else {
        // Duration is 0 - finish immediately (Existing Logic)
        finishedActivitiys[transitionId] = 1;

        for (const auto& [placeID, outAmount] : petri.Transitions[transitionId-1].arcs_out_indices) {
            if (placeID < 4) {
                resource_nodes[placeID].emplace_back(outAmount, 0);
            } else {
                short idx = placeID - 4;
                if(idx < activity_nodes.size()) activity_nodes[idx] = {outAmount, 0};
            }
        }
    }
    // NOTE: If duration > 0, outputs are produced when activity completes (in TIME SHIFT section above)

    // 8. Canonical Sort & Merge (Only for Resources)
    // 8. Canonical Sort & Merge (CRITICAL FIX)
    for (auto& resVec : resource_nodes) {
        if (resVec.empty()) continue;

        // A. Sort by Time (Ascending)
        std::sort(resVec.begin(), resVec.end(), [](const auto& a, const auto& b) {
             if (a.second != b.second) return a.second < b.second;
             return a.first > b.first; // Optional: put larger chunks first
        });

        // B. Merge split groups with the same time
        // This converts [(5,0), (5,0)] -> [(10,0)]
        auto it = resVec.begin();
        while (it != resVec.end() - 1) {
            auto next = it + 1;
            if (it->second == next->second) {
                // Same time? Merge them!
                it->first += next->first;
                // Remove the second one
                resVec.erase(next);
                // Don't increment 'it', check the new neighbor
            } else {
                ++it;
            }
        }
    }
    if (!activeTransitionIndices.empty()) {
        std::sort(activeTransitionIndices.begin(), activeTransitionIndices.end());
        // std::pair default sort is (First, Second), which means (ID, Time). This is perfect.
    }
//**********cheack here***************
    // 2. SORT ACTIVITY TOKENS
    // Ensures tokens in "waiting places" are always in the same order
    // if (!activity_nodes.empty()) {
    //     std::sort(activity_nodes.begin(), activity_nodes.end());
    // }
//********************************************
    // 3. SORT & MERGE RESOURCES (Crucial for Heuristic Consistency)
    for (auto& resVec : resource_nodes) {
        if (resVec.empty()) continue;

        // Step A: Sort by Time (Availability Time)
        // If times are equal, sort by Amount (to be deterministic)
        std::sort(resVec.begin(), resVec.end(), [](const auto& a, const auto& b) {
            if (a.second != b.second) return a.second < b.second; // Earliest time first
            return a.first < b.first; // Then smallest amount
        });

        // Step B: Merge Adjacent Duplicates (The "Split Resource" Fix)
        // Converts [(5,0), (5,0)] -> [(10,0)]
        auto it = resVec.begin();
        while (it != resVec.end() - 1) {
            auto next = it + 1;
            // If they become available at the exact same time...
            if (it->second == next->second) {
                it->first += next->first; // Merge amounts
                resVec.erase(next);       // Delete the duplicate
                // Do not increment 'it', check the new neighbor
            } else {
                ++it;
            }
        }
    }
    // if (firingTime==7&&transitionId != 26) {
    //     int i;
    //     i++;
    // }
}

else {
    isDeltaZero = (firingTime == 0);
    g = prev.g + firingTime;
    predessesor_h = prev.h;
    lastTransitionId=transitionId;

    // 2. Copy State
    finishedActivitiys = prev.finishedActivitiys;
    resource_nodes = prev.resource_nodes;
    activity_nodes = prev.activity_nodes;
    activeTransitionIndices = prev.activeTransitionIndices;

    transitionsCached = false;
    AvailableTransitionIndices_TT2.clear();

    // 3. TIME SHIFT (Update "Remaining Time")
    finishedActivitiys[transitionId] = 0;

    if (firingTime > 0) {
        // Update resource tokens
        for (auto& resVec : resource_nodes) {
            for (auto& token : resVec) {
                token.second = std::max(0, token.second - firingTime);
            }
        }

        // Update activity node tokens
        for (auto& token : activity_nodes) {
            if (token.first > 0) {
                token.second = std::max(0, token.second - firingTime);
            }
        }

        // Update active list
        auto it = activeTransitionIndices.begin();
        while (it != activeTransitionIndices.end()) {
            it->second -= firingTime;

            if (it->second <= 0) {
                short completedTaskID = it->first;
                it = activeTransitionIndices.erase(it);
            } else {
                ++it;
            }
        }
    }

    // 4. Consume OUTPUT Resources
    const Transition& transition = petri.Transitions[transitionId - 1];
    const Activity& act = RCPSPex.activities[transitionId - 1];
    short duration = act.duration;

    for (const auto& [placeID, outAmount] : transition.arcs_out_indices) {
        if (placeID < 4) {
            auto& tokens = resource_nodes[placeID];

            std::sort(tokens.begin(), tokens.end(),
                     [](const auto& a, const auto& b) { return a.second < b.second; });

            int remainingDemand = outAmount;
            auto it = tokens.begin();
            while (remainingDemand > 0 && it != tokens.end()) {
                if (it->first > remainingDemand) {
                    it->first -= remainingDemand;
                    remainingDemand = 0;
                } else {
                    remainingDemand -= it->first;
                    it = tokens.erase(it);
                }
            }
        }
    }

    // 5. Consume OUTPUT Activity Tokens
    for (const auto& [placeID, outAmount] : transition.arcs_out_indices) {
        if (placeID >= 4) {
            short idx = placeID - 4;
            activity_nodes[idx].first = 0;
            activity_nodes[idx].second = 0;
        }
    }

    // 6. Produce INPUT tokens (what forward consumed)
    // Step 6 - Produce INPUT tokens (what forward consumed)
    if (duration > 0) {
        activeTransitionIndices.emplace_back(transitionId, duration);

        // Produce inputs as future events
        for (const auto& [placeID, inAmount] : transition.arcs_in_indices) {
            if (placeID < 4) {
                resource_nodes[placeID].emplace_back(inAmount, duration);
            }
            else {
                short idx = placeID - 4;

                // ADD THIS DEBUG HERE:
                if (idx == 48 && idx < activity_nodes.size() && activity_nodes[idx].first > 0) {
                    std::cout << "Activity " << transitionId << " overwriting idx=48: old_time="
                              << activity_nodes[idx].second << ", new_duration=" << duration << std::endl;
                }

                if (idx < activity_nodes.size()) {
                    activity_nodes[idx] = {inAmount, duration};
                }
            }
        }
    }
    else {
        // Duration is 0 - finish immediately
        finishedActivitiys[transitionId] = 0;

        // BACKWARD FIX: Produce to arcs_IN, not arcs_OUT!
        for (const auto& [placeID, inAmount] : transition.arcs_in_indices) {
            if (placeID < 4) {
                resource_nodes[placeID].emplace_back(inAmount, 0);
            }
            else {
                short idx = placeID - 4;
                if(idx < activity_nodes.size()) {
                    activity_nodes[idx] = {inAmount, 0};
                }
            }
        }
    }

    // 7. Canonical Sort & Merge
    for (auto& resVec : resource_nodes) {
        if (resVec.empty()) continue;

        std::sort(resVec.begin(), resVec.end(), [](const auto& a, const auto& b) {
             if (a.second != b.second) return a.second < b.second;
             return a.first > b.first;
        });

        auto it = resVec.begin();
        while (it != resVec.end() - 1) {
            auto next = it + 1;
            if (it->second == next->second) {
                it->first += next->first;
                resVec.erase(next);
            } else {
                ++it;
            }
        }
    }

    if (!activeTransitionIndices.empty()) {
        std::sort(activeTransitionIndices.begin(), activeTransitionIndices.end());
    }

    // if (!activity_nodes.empty()) {
    //     std::sort(activity_nodes.begin(), activity_nodes.end());
    // }

    for (auto& resVec : resource_nodes) {
        if (resVec.empty()) continue;

        std::sort(resVec.begin(), resVec.end(), [](const auto& a, const auto& b) {
            if (a.second != b.second) return a.second < b.second;
            return a.first < b.first;
        });

        auto it = resVec.begin();
        while (it != resVec.end() - 1) {
            auto next = it + 1;
            if (it->second == next->second) {
                it->first += next->first;
                resVec.erase(next);
            } else {
                ++it;
            }
        }
    }
}

}

short getForwardHcost_TT2(
    const std::vector<short>& unstartedTransitions,
    const std::vector<std::pair<short, short>>& activity_tokens,
    const std::vector<std::pair<short, short>>& active_activities,
    const std::bitset<128>& finishedActivitiys)  // ← ADD THIS!
{
    std::map<int, int> earlyFinishMap;

    // 1. Initialize ALL finished activities (EFT = 0)
    for (size_t i = 1; i < finishedActivitiys.size(); i++) {
        if (finishedActivitiys.test(i)) {  // Finished
            earlyFinishMap[i] = 0;
        }
    }

    // 2. Initialize active activities (EFT = remaining time)
    for (const auto& [taskID, remainingTime] : active_activities) {
        earlyFinishMap[taskID] = remainingTime;
    }

    int maxH = 0;

    // 3. Include active activities in maxH
    for (const auto& [taskID, remainingTime] : active_activities) {
        maxH = std::max(maxH, (int)remainingTime);
    }

    // 4. Process unstarted activities
    bool changed = true;
    int iterations = 0;
    const int MAX_ITER = 1000;

    while (changed && iterations < MAX_ITER) {
        changed = false;
        iterations++;

        for (short taskID : unstartedTransitions) {
            if (earlyFinishMap.count(taskID) && iterations > 1) {
                continue;
            }

            int maxPredFinish = 0;
            bool allPredsReady = true;

            for (int predID : RCPSPex.backword_dependencies[taskID - 1]) {
                if (earlyFinishMap.count(predID)) {
                    maxPredFinish = std::max(maxPredFinish, earlyFinishMap[predID]);
                } else {
                    allPredsReady = false;
                    break;
                }
            }

            if (!allPredsReady) {
                continue;
            }

            int duration = RCPSPex.activities[taskID - 1].duration;
            int myFinish = maxPredFinish + duration;

            if (!earlyFinishMap.count(taskID) || earlyFinishMap[taskID] != myFinish) {
                earlyFinishMap[taskID] = myFinish;
                changed = true;
            }

            maxH = std::max(maxH, myFinish);
        }
    }

    return maxH;
}


double getForwardHcost_TT(std::vector<short>unstartedTransitions
) {
  std::map<int, int> earlyfinishMap2; // Map to store activity IDs and their early finish times
  double h;
  std::set<int> processedDependencies;
  for (int activityId: unstartedTransitions) {
    int maxFinishTime = 0;
    std::set<int> processedDependencies;

    for (int dep : RCPSPex.backword_dependencies[activityId - 1]) {  // Changed from const auto& to int
      int depId = dep - 1;  // No more std::stoi!

      if (std::find(unstartedTransitions.begin(), unstartedTransitions.end(), depId + 1) != unstartedTransitions.end()) {
        maxFinishTime = std::max(maxFinishTime, earlyfinishMap2[depId+1] + RCPSPex.activities[depId].duration);
      }
      else {
        maxFinishTime = std::max(maxFinishTime, earlyfinishMap2[depId+1]);
      }
    }

    earlyfinishMap2[activityId] = maxFinishTime;
  }

  if (earlyfinishMap2.size()==0) {
    h = 0;
  }
  else {
    h = earlyfinishMap2.rbegin()->second;
  }
  // if (h==39) {
  //     std::cout << "Nodes Expanded: ";
  // }
  return h;  // Added return statement
}
