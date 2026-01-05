//
// Created by idolu on 06/01/2025.

#include <iostream>
#include <vector>
#include <set>
#include <unordered_set>
#include "petriclasses.h"
#include "RCPSPState.h"
#include "RCPSPState.h"
#include <thread>
#include <chrono>
#include <atomic>
#include <algorithm>
#include <climits>
#include <climits>
using namespace P_RCPSP;


// std::chrono::duration<double> generateTIME;
// std::chrono::duration<double> avelableTIME;
// std::chrono::duration<double> HTIME;
// std::chrono::duration<double>hashTIME;
//
// std::chrono::duration<double> comperTime;
// std::chrono::duration<double>secssesorTIME;




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
 thread_local PetriExample petri;
 thread_local RCPSP_example RCPSPex;
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
  if (useCS) {
    // h=computeSequenceLowerBoundWithMax2(
    //     unstartedTransitions,
    //     activeTransitionIndices,
    //     earlyfinishMap2,
    //     earlyfinishMap3,
    //     h,
    //     finishedActivitiys); // BL_Cs heuristic
    // auto endS1 = std::chrono::high_resolution_clock::now();
    // avelableTIME += endS1 - startS3;
    return h;
  }
  else {
    // auto endS1 = std::chrono::high_resolution_clock::now();
    // avelableTIME += endS1 - startS3;
    return h;

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
RCPSPState_bi::RCPSPState_bi(): nodestatus(false) {
 // auto startS1 = std::chrono::high_resolution_clock::now();
  //startedActivitiys.insert(0);
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
    unstartedTransitions.insert(i+1);
    //unstartedTransitions.insert(i);
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
  //generateTIME += endS1 - startS1;

  // Change: Get indices of available transitions instead of full Transition objects
  //avilableTransitionIndices = getAvilableTransitionIndices(marking);

 // avilableDeTransitionIndices = getAvilableDetransitionIndices(marking);
  g_b = 0;
  g_f = 0;
  name = 0;
  //h_f=getForwardHcost(unstartedTransitions,activeTransitionIndices);
  h_b=0;
  f=h_f;
}


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

RCPSPState_bi::RCPSPState_bi(RCPSPState_bi predecesor, Transition active, bool status, int location, uint64_t &count) {
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
    //g_f = predecesor.g_f;

    if (status) {
      //h_f = predecesor.h_f;

      // Apply arcs_in from the transition
      for (const auto& arc : petri.Transitions[active.name-1].arcs_in_indices) {

        // arc.first  is now the integer Place ID (e.g., 5)
        // arc.second is the token count (e.g., 1)

        // This is a direct array access. 1 CPU cycle.
       // marking[arc.first] -= arc.second;
      }

      // Store index and duration instead of full Transition
      activeTransitionIndices.push_back({active.name, active.duration});
      startedActivitiys.insert(active.name);
      if (active.duration==0) {
        status=0;
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
        if (activeTransitionIndices[i].second <0) {
          activeTransitionIndices[i].second =0;

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
      //generateTIME += endS1-startS4;

      //h_f=getForwardHcost(unstartedTransitions,activeTransitionIndices);

    }
    f=g_f+h_f;
    h_b=getBackwardHcost2(startedActivitiys,finishedActivitiys,activeTransitionIndices);
    f=2*g_f+h_f-h_b;
    //f=2*g_f+h_f;

    //avilableTransitionIndices = getAvilableTransitionIndices(marking);
  }
  else {


    // Similar transformation for the backward direction
    if (status) {
      //h_b = predecesor.h_b;

      // for (const auto& arc : petri.Transitions[active.name-1].arcs_out) {
      //   marking[arc.first] -= arc.second;
      // }

      activeTransitionIndices.push_back({active.name, 0});
      auto it = std::find(finishedActivitiys.begin(), finishedActivitiys.end(), active.name);
      if (it != finishedActivitiys.end()) {
        finishedActivitiys.erase(it);
      }
      if (active.duration==0) {
        status=0;
      }
     // auto endS1 = std::chrono::high_resolution_clock::now();
      //generateTIME += endS1-startS4;
    }
    if (!status) {
     g_b += (petri.Transitions[active.name-1].duration-active.duration);

      auto it = std::find(startedActivitiys.begin(), startedActivitiys.end(), active.name);
      if (it != startedActivitiys.end()) {
        startedActivitiys.erase(it);

      }
      unstartedTransitions.insert(active.name);

      for (int i = activeTransitionIndices.size() - 1; i >= 0; --i) {
        activeTransitionIndices[i].second += (petri.Transitions[active.name-1].duration-active.duration);
        if (activeTransitionIndices[i].second>petri.Transitions[activeTransitionIndices[i].first-1].duration) {
          activeTransitionIndices[i].second=petri.Transitions[activeTransitionIndices[i].first-1].duration;
        }
        if (activeTransitionIndices[i].first == active.name) {
          // for (const auto& arc : petri.Transitions[active.name-1].arcs_in) {
          //   marking[arc.first] += arc.second;
          // }
          activeTransitionIndices.erase(activeTransitionIndices.begin() + i);
        }
      }
      //auto endS1 = std::chrono::high_resolution_clock::now();
     // generateTIME += endS1-startS4;
      h_b=getBackwardHcost2(startedActivitiys,finishedActivitiys,activeTransitionIndices);

    }
    //h_f=getForwardHcost(unstartedTransitions,activeTransitionIndices);

    avilableDeTransitionIndices = getAvilableDetransitionIndices(marking);
    f=2*g_b+h_b-h_f;
    //f=g_b;
    //f=g_b+h_b;
  }




  // You'll need to modify these functions to return indices instead of Transitions
//   if (direction) {
// }
// else {
//   }
int asdasd;
  asdasd++;
}

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
bool RCPSPState_bi::operator==(const RCPSPState_bi &other) const {
  // if (this->expanded != other.expanded) {
  //   return false;
  // }
  if (this->activeTransitionIndices!= other.activeTransitionIndices) {
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


std::vector<std::pair<short, short>> consumeResourceList(
    const std::vector<std::pair<short, short>>& resource,
    int amount,
    int currentTime
) {
  if (amount < 1)
    return resource;

  // Check if already sorted to avoid unnecessary sorting
  std::vector<std::pair<short, short>> resourceCopy = resource;

  // Only sort if not already sorted (you could maintain sorted invariant)
  std::sort(resourceCopy.begin(), resourceCopy.end(), [](const auto& a, const auto& b) {
      return a.second > b.second; // DESCENDING by time
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

  // Use erase-remove idiom efficiently
  resourceCopy.erase(
      std::remove_if(resourceCopy.begin(), resourceCopy.end(),
                    [](const auto& p) { return p.first <= 0; }),
      resourceCopy.end()
  );

  return resourceCopy;
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

    return h;  // Added return statement
}
RCPSPState_TT::RCPSPState_TT(const RCPSPState_TT &prev, short transitionId, short firingTime) {
    finishedActivitiys = prev.finishedActivitiys;
    resource_nodes = prev.resource_nodes;
    activity_nodes = prev.activity_nodes;

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
//       // DEBUG: Check if Task 2 (usually the first real task) has dependencies
//       if (transId == 2 && dependencies.empty()) {
//         std::cout << "❌ CRITICAL ERROR: Task 2 has NO dependencies! (JSON Loading Failed)" << std::endl;
//         exit(1); // Stop immediately so you see this
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
//         for (const std::string &predStr : RCPSPex.backword_dependencies[transId - 1]) {
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
    const std::vector<std::pair<short, short>> &activity_nodes
);


std::vector<std::pair<short, short>> getAvailableTransitionIndices_TT(
    const std::vector<short> &unstartedTransitions,
    const std::array<short, 128> &finishedActivitiys,
    const std::array<std::vector<std::pair<short, short>>, 4> &resource_nodes,
    const std::vector<std::pair<short, short>> &activity_nodes
) {
    std::vector<std::pair<short, short>> available;

    for (short transId : unstartedTransitions) {
        const auto& dependencies = RCPSPex.backword_dependencies[transId - 1];

        // DEBUG: Check if Task 2 has dependencies
        if (transId == 2 && dependencies.empty()) {
            std::cout << "❌ CRITICAL ERROR: Task 2 has NO dependencies! (JSON Loading Failed)" << std::endl;
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
  return true;
}




