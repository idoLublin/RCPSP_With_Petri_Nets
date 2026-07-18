//
// Created by idolu on 29/12/2024.
//
#ifndef RCPSP_H
#define RCPSP_H
//#include "../algorithms/OldSearchEnvironment.h"
#include "../search/SearchEnvironment.h"
#include "RCPSPState.h"
#include "../utils//GLUtil.h"
#include <functional>
#include "petriclasses.h"
#include "DominanceCBS.h"   // DR5 cutset dominance (guarded by setting.use_dr5)

// h returned for a DR5-dominated node: large enough that A* never expands it,
// finite so nothing downstream trips on inf arithmetic.
inline constexpr double DR5_DOMINATED_H = 1e9;
// #include "Globals.h"
using namespace P_RCPSP;

//creted the RCPSPState in searchgraph
// class RCPSPState{
// searchNode node;
//   };
std::uint64_t count=0;
typedef int action;
namespace P_RCPSP {
class RCPSP : public SearchEnvironment<RCPSPState,int>{
  public:
  RCPSP();
  void GetSuccessors(const RCPSPState &nodeID, std::vector<RCPSPState> &neighbors) const override;
  bool GoalTest(const RCPSPState &node, const RCPSPState &goal) const override;
	double HCost(const RCPSPState &state1, const RCPSPState &state2) const override;
	double GCost(const RCPSPState &state1, const RCPSPState &state2) const override;

  int GetAction(const RCPSPState &nodeID, const RCPSPState &nodeID2) const override;
  int GetNumSuccessors(const RCPSPState &stateID) const;
  void GetActions(const RCPSPState &nodeID, std::vector<int> &actions) const override;
  void ApplyAction(RCPSPState &s, int a) const override;
  uint64_t GetActionHash(int act) const;
  uint64_t GetStateHash(const RCPSPState &node) const;
  bool InvertAction(int &a) const;
  std::vector<RCPSPState> GetSuccessors(const RCPSPState &nodeID) const;
  double GCost(const RCPSPState &node, const int &act) const override;
  };

}

//int test=0;

inline uint64_t RCPSP::GetStateHash(const RCPSPState &node) const {
 // auto startS1 = std::chrono::high_resolution_clock::now();
//test=1;
  std::size_t seed = 0;

  // // 1. Hash Started Activities
  // for (int id = 0; id < node.startedActivitiys.size(); ++id) {
  //   int time = node.startedActivitiys[id];
  //
  //   // Only hash if the activity exists (equivalent to iterating the map)
  //   if (time != -1) {
  //     // Hash the ID (formerly pair.first)
  //     seed ^= std::hash<int>{}(id) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
  //     // Hash the Time (formerly pair.second)
  //     seed ^= std::hash<int>{}(time) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
  //   }
 // }
for (const auto& entry : node.activeTransitionIndices) {
  // Hash the ID
  seed ^= std::hash<short>{}(entry.first) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
  // Hash the Start Time
  seed ^= std::hash<short>{}(entry.second) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}
  // 1. Hash Active Transitions (Vector of Pairs)
  // iterate through the vector directly
  // for (const auto& entry : node.activeTransitionIndices) {
  //   short id = entry.first;   // The Transition/Task ID
  //   short time = entry.second; // The Time/Duration info

  //   // Hash the ID
  //   seed ^= std::hash<int>{}(id) + 0x9e3779b9 + (seed << 6) + (seed >> 2);

  //   // Hash the Time
  //   seed ^= std::hash<int>{}(time) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
  // }
  // 2. Hash Finished Activities
  for (int id = 0; id < node.finishedActivitiys.size(); ++id) {
    int time = node.finishedActivitiys[id];

    // Only hash if the activity exists
    if (time != -1) {
      // Hash the ID (formerly pair.first)
      seed ^= std::hash<int>{}(id) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
      // Hash the Time (formerly pair.second)
     seed ^= std::hash<int>{}(time) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    }
  }
  // auto endS1 = std::chrono::high_resolution_clock::now();
  //
  // hashTIME += endS1-startS1;

  // auto endS1 = std::chrono::high_resolution_clock::now();
  //
  // hashTIME += endS1-startS1;
  return seed;

}

std::vector<int> getAvilableTransitionIndices(const std::vector<short>& marking) {



 // auto startS4 = std::chrono::high_resolution_clock::now();

  std::vector<int> availableIndices;

  // Optimization: Reserve memory to prevent re-allocations
  // We know it can't be bigger than the total number of transitions
  availableIndices.reserve(petri.Transitions.size());

  // Loop through all transitions
  for (int i = 0; i < petri.Transitions.size(); i++) {
    const P_RCPSP::Transition& t = petri.Transitions[i];
    bool available = true;

    // CHANGE 2: Use the Integer indices we built in getPetri
    // t.arcs_in_indices is vector<pair<int, int>>
    for (const auto& arc : t.arcs_in_indices) {

      // CHANGE 3: Direct Access (The Speedup)
      // arc.first  = Place ID (0, 1, 2...)
      // arc.second = Weight (Tokens needed)

      // No .find(), no hashing, just array access!
      if (marking[arc.first] < arc.second) {
        available = false;
        break;
      }
    }

    if (available) {
      availableIndices.push_back(i + 1);
    }
  }

  // auto endS1 = std::chrono::high_resolution_clock::now();
  // avelableTIME += endS1-startS4;

  return availableIndices;
}




inline RCPSP::RCPSP() {
}

inline void RCPSP::GetSuccessors(const RCPSPState &nodeID, std::vector<RCPSPState> &neighbors) const {
  //auto startS1 = std::chrono::high_resolution_clock::now();






  // --- OPTIMIZATION FIX ---
  // 1. Calculate locally (Stack allocation).
  // This replaces the memory-heavy class member.
  std::vector<int> avilableTransitionIndices = getAvilableTransitionIndices(nodeID.marking);
  // ------------------------

  // if (USE_OLD_MODEL) {
  //   int n = availableTransitionIndices.size();
  //
  //   for (int mask = 0; mask < (1 << n); mask++) {
  //     std::vector<int> resourceUsage(petri.Resources.size(), 0);
  //     bool valid = true;
  //     std::vector<int> subset;
  //
  //     // add resource usage from currently active transitions
  //     for (auto& active : nodeID.activeTransitionIndices) {
  //       const Transition& t = petri.Transitions[active.first - 1];
  //       for (int r = 0; r < petri.Resources.size(); r++) {
  //         resourceUsage[r] += t.resourceUsage[r];
  //       }
  //     }
  //
  //     // add each transition in subset
  //     for (int i = 0; i < n; i++) {
  //       if (mask & (1 << i)) {
  //         int transitionIdx = availableTransitionIndices[i];
  //         const Transition& t = petri.Transitions[transitionIdx - 1];
  //         subset.push_back(transitionIdx);
  //         for (int r = 0; r < petri.Resources.size(); r++) {
  //           resourceUsage[r] += t.resourceUsage[r];
  //         }
  //       }
  //     }
  //
  //     // check validity after building subset
  //     for (int r = 0; r < petri.Resources.size(); r++) {
  //       if (resourceUsage[r] > petri.Resources[r].capacity) {
  //         valid = false;
  //         break;
  //       }
  //     }
  //
  //     if (valid) {
  //       count++;
  //       neighbors.emplace_back(RCPSPState(nodeID, subset, count));
  //     }
  //   }
  // }




  // Handle active transitions (Finishing a task)
  if (!nodeID.activeTransitionIndices.empty()) {
    count++;
    int t = 0;

    // (You don't need to calculate 'avilable' here anymore, we did it above)

    // Find the transition with the minimum remaining duration
    for (int i = 0; i < nodeID.activeTransitionIndices.size(); i++) {
      if (nodeID.activeTransitionIndices[i].second < nodeID.activeTransitionIndices[t].second) {
        t = i;
      }
    }

    // Get the actual transition object using the index
    int transitionIdx = nodeID.activeTransitionIndices[t].first;

    // Use 'petri_RCPSP' (Member) or 'petri' (Macro) depending on your setup
    const Transition& transition = petri.Transitions[transitionIdx - 1];

    // Note: Modifying 'transition' here is risky if it's a const ref from the global list.
    // Better to pass the duration to the constructor directly if possible.
    // But assuming this works for your logic:
    Transition tCopy = transition;
    tCopy.duration = nodeID.activeTransitionIndices[t].second;

    neighbors.emplace_back(RCPSPState(nodeID, tCopy, false, t, count));
  }

  // Handle available transitions (Starting a task)
  // Now this loop can see 'avilableTransitionIndices' because it's in the top scope.
  for (int i = 0; i < avilableTransitionIndices.size(); i++) {
    count++;

    int transitionIdx = avilableTransitionIndices[i];
    const Transition& transition = petri.Transitions[transitionIdx - 1];

    // Create successor state
    neighbors.emplace_back(RCPSPState(nodeID, transition, true, i, count));
  }

  // auto endS1 = std::chrono::high_resolution_clock::now();
  // secssesorTIME += endS1 - startS1;
}
inline bool RCPSP::GoalTest(const RCPSPState &node, const RCPSPState &goal) const {
  short finalID = petri.place_name_to_id.at(finalstatename);

  // 2. Check the Vector at that index
  if (node.marking[finalID] == 1) {
    return true;
  }
  return false;
}

double calculateEarlyFinishRecursive(int activityId, std::map<int, int>& earlyfinishMap,
                                    const std::vector<short>& unstartedTransitions,
                                    const std::vector<std::pair<short, short>>& activeTransitions,
                                    const RCPSP_example& RCPSPex) {
  // If we've already computed this activity's early finish time, return it
  if (earlyfinishMap.find(activityId) != earlyfinishMap.end()) {
    return earlyfinishMap[activityId];
  }

  int maxFinishTime = 0;

  // Process all dependencies
  // for (const auto &dep: RCPSPex.backword_dependencies[activityId - 1]) {
  //   int depId = std::stoi(dep) - 1;
  //
  //   // Recursively compute the early finish time of the dependency if not already computed
  //   if (earlyfinishMap.find(depId + 1) == earlyfinishMap.end()) {
  //     calculateEarlyFinishRecursive(depId + 1, earlyfinishMap, unstartedTransitions, activeTransitions, RCPSPex);
  //   }
  //
  //   if (std::find(unstartedTransitions.begin(), unstartedTransitions.end(), depId + 1) != unstartedTransitions.end()) {
  //     int duration = getTransitionDuration2(activeTransitions, std::stoi(dep));
  //     if (duration != -1) {
  //       maxFinishTime = std::max(maxFinishTime, earlyfinishMap[depId + 1] + duration);
  //     } else {
  //       maxFinishTime = std::max(maxFinishTime, earlyfinishMap[depId + 1] + RCPSPex.activities[depId].duration);
  //     }
  //   } else {
  //     maxFinishTime = std::max(maxFinishTime, earlyfinishMap[depId + 1]);
  //   }
  // }

  // Store and return the result
  earlyfinishMap[activityId] = maxFinishTime;
  return maxFinishTime;
}




inline double RCPSP::HCost(const RCPSPState &state1, const RCPSPState &state2) const {
  if (state1.status) {
    return state1.h;//if status is 1 the h is identical to before so we dont need to change a thing
  }
  else {

    // // We use the 'finishedActivitiys' array to determine what is left.
    // std::set<short> unstartedSet;
    //
    // // Iterate all transitions (1..N)
    // for (int i = 0; i < petri.Transitions.size(); i++) {
    //   short taskID = i + 1;
    //   // If value is -1, the task is either Active or Not Started
    //   if (state1.finishedActivitiys[taskID] == -1) {
    //     unstartedSet.insert(taskID);
    //   }
    // }
    //
    // // 2. Prepare Effective Durations
    // // Your helper reads the 2nd value of the pair as "Duration".
    // // The state stores "StartTime".
    // // We must calculate: Remaining = Total - (Now - Start).
    // std::vector<std::pair<short, short>> effectiveActiveDurations;
    // effectiveActiveDurations.reserve(state1.activeTransitionIndices.size());
    //
    // for (const auto& entry : state1.activeTransitionIndices) {
    //   short id = entry.first;
    //   short startTime = entry.second;
    //
    //   // TP LOGIC: Calculate Remaining Duration
    //   int progress = state1.g - startTime;
    //   int originalDuration = RCPSPex.activities[id - 1].duration;
    //   int remaining = originalDuration - progress;
    //
    //   // Safety clamp
    //   if (remaining < 0) remaining = 0;
    //
    //   // Push {ID, RemainingDuration} to the temporary vector
    //   effectiveActiveDurations.push_back({id, (short)remaining});
    // }
    //
    // // 3. Call your Unchanged Helper
    // // The helper now sees "Active Tasks" with their correct Remaining Durations
    // return getForwardHcost(unstartedSet, effectiveActiveDurations);






    std::vector<short> tempUnstarted;

    // Optimization: Reserve max possible size to prevent re-allocations
    // (Using the size logic from your original code)
    tempUnstarted.reserve(petri.Transitions.size());

    // YOUR ORIGINAL LOGIC: Loop i from 1 to size, use ID = i + 1
    for (int i = 0; i < petri.Transitions.size(); i++) {
      short taskID = i + 1; // Preserving your 1-based logic

      // THE FIX: Direct vector access (O(1) speed)
      // Check if value is -1 (meaning "not finished")
      if (state1.finishedActivitiys[taskID] == -1) {
        tempUnstarted.push_back(taskID);
      }
    }

    // NOTE: You must update the definition of getForwardHcost
    // to accept 'const std::vector<int>&' instead of 'std::map...'
    //state1.h = getForwardHcost(tempUnstarted, state1.activeTransitionIndices, state1.finishedActivitiys);

    state1.h =std::max(
    getForwardHcost(tempUnstarted, state1.activeTransitionIndices),
    getforwardResource(tempUnstarted, state1.activeTransitionIndices)
);
    // state1.h = getForwardHcost(tempUnstarted, state1.activeTransitionIndices);
    return state1.h;
  }
  //return state1.h;
}

inline double RCPSP::GCost(const RCPSPState &state1, const RCPSPState &state2) const {
   return state2.g-state1.g;//+state1.g
}

//NOT IN USE OF A*
inline uint64_t RCPSP::GetActionHash(int act) const {
  // Example hash for an action
  return std::hash<int>()(act);
}
inline void RCPSP::GetActions(const RCPSPState &nodeID, std::vector<int> &actions) const {
}

inline bool RCPSP::InvertAction(int &a) const {
  // Example logic to invert an action
  return true;
}

inline std::vector<RCPSPState> RCPSP::GetSuccessors(const RCPSPState &nodeID) const {
     std::vector<RCPSPState> neighbors;
    return neighbors;
}


inline int RCPSP::GetAction(const RCPSPState &nodeID, const RCPSPState &nodeID2) const {
  return 0;
}

inline int RCPSP::GetNumSuccessors(const RCPSPState &stateID) const {
  return 0;
}
inline void RCPSP::ApplyAction(RCPSPState &s, int a) const {

}

inline double RCPSP::GCost(const RCPSPState &node, const int &act) const {
  return node.g;
}

class RCPSP_BiGreedy : public SearchEnvironment<RCPSPState_BI_TT2, action> {
public:
  std::vector<std::pair<short, short>> getUnfireableTransitionIndices(
    const std::vector<short> startedActivities,
    const std::array<short, 128>& finishedActivitiys,
    const std::array<std::vector<std::pair<short, short>>, 4>& resource_nodes,
    const std::vector<std::pair<short, short>>& activity_nodes
) const {




    int maxPredFinishTime = 0;

    std::vector<std::pair<short, short>> available_de;

    for (short transId : startedActivities) {
      const Activity& act = RCPSPex.activities[transId - 1];

      // Check successors
      bool allbackPredsFinished = true;
      for (int predId : RCPSPex.dependencies[transId - 1]) {
        int finishTime = finishedActivitiys[predId];

        if (finishTime == -1) {
          allbackPredsFinished = false;
          break;
        }
        else {
          maxPredFinishTime = std::max(maxPredFinishTime, finishTime);
        }
      }

      if (!allbackPredsFinished)
        continue;


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
            available_de.emplace_back(transId, maxResourceTime);
        }
    }

    return available_de;
      // For backward: firingTime doesn't need to be negative
      // It represents "time before goal" which is always positive
      // Use the finish time directly (which is relative to goal = 0)
  //    short finishTime = finishedActivitiys[transId];

      // In backward, we use the finish time as the "firing time"
      // The constructor will handle it correctly
//      available_de.emplace_back(transId, finishTime);
    }

  //}

  inline void GetSuccessors(const RCPSPState_BI_TT2 &nodeID, std::vector<RCPSPState_BI_TT2> &neighbors) const override {
    neighbors.clear();

    // std::cout << "GetSuccessors: direction=" << nodeID.direction
    //           << ", g=" << nodeID.g << std::endl;

    if (nodeID.direction) {
      std::vector<short> tempUnstarted;

      // Optimization: Reserve max possible size to prevent re-allocations
      // (Using the size logic from your original code)
      tempUnstarted.reserve(petri.Transitions.size());

      // YOUR ORIGINAL LOGIC: Loop i from 1 to size, use ID = i + 1
      for (int i = 0; i < petri.Transitions.size(); i++) {
        short taskID = i + 1;

        // THE FIX: Direct Vector Access (O(1))
        // Instead of .find() == .end(), we check if the value is -1.
        if (nodeID.finishedActivitiys[taskID] == 0) {
          tempUnstarted.push_back(taskID);
        }
      }

      // ⚠️ IMPORTANT: You must update the definition of 'getAvailableTransitionIndices_TT'
      // to accept 'const std::vector<int>&' for the second argument, instead of 'std::map'.

      std::vector<std::pair<short, short>> avilableTransitionIndices = getAvailableTransitionIndices_TT2(tempUnstarted, nodeID.finishedActivitiys,nodeID.resource_nodes, nodeID.activity_nodes,nodeID.activeTransitionIndices);

      //std::vector<std::pair<short, short>> avilableTransitionIndices = getAvailableTransitionIndices_TT(tempUnstarted, nodeID.finishedActivitiys, nodeID.marking);

      for (const auto& [transId, Timedelta] : avilableTransitionIndices) {


        RCPSPState_BI_TT2 child(nodeID, transId, Timedelta, 1);
        // if (child.g_f - nodeID.g_f != Timedelta) {
        //   std::cerr << "GCOST MISMATCH: Timedelta=" << Timedelta
        //             << " actual g_f diff=" << child.g_f - nodeID.g_f << std::endl;
        // }
        // if (child == child2) {
        //   assert(GetStateHash(child) == GetStateHash(child2)
        //          && "Equal states have different hashes!");
        // }
        neighbors.emplace_back(child);
      }
      // auto endS1 = std::chrono::high_resolution_clock::now();
      // secssesorTIME += endS1 - startS1;

     }
else {

  std::vector<short> tempUnstarted;
  tempUnstarted.reserve(petri.Transitions.size());

  // Loop through all transitions
  for (int i = 0; i < petri.Transitions.size(); i++) {
    short taskID = i + 1;

    // In Backward Search:
    // 0 = Not yet scheduled from the End (We want to move these)
    // 1 = Already scheduled (These are done)
    if (nodeID.finishedActivitiys[taskID] == 1) {
      tempUnstarted.push_back(taskID);
    }
  }

  // PASS THE FIX: Use the BACKWARD availability function
  std::vector<std::pair<short, short>> avilableTransitionIndices =
       getAvailableTransitionIndices_TT2_backward(
           tempUnstarted,
           nodeID.finishedActivitiys, // Pass bitset
           nodeID.resource_nodes,
           nodeID.activity_nodes,
           nodeID.activeTransitionIndices
       );

  for (const auto& [transId, Timedelta] : avilableTransitionIndices) {
    // Generate Neighbor
    // Ensure RCPSPState_TT2 constructor correctly sets 'finishedActivitiys[transId] = 1'
    RCPSPState_BI_TT2 child(nodeID, transId, Timedelta, 0);
    if (child.g_b - nodeID.g_b != Timedelta) {
      std::cerr << "GCOST MISMATCH: Timedelta=" << Timedelta
                << " actual g_b diff=" << child.g_b - nodeID.g_b << std::endl;
    }
    neighbors.emplace_back(child);
  }
}
   // std::cout << "  Generated " << neighbors.size() << " neighbors" << std::endl;
}
   inline bool GoalTest(const RCPSPState_BI_TT2 &node, const RCPSPState_BI_TT2 &goal) const override {
  return (node == goal);
  }

  inline double HCost(const RCPSPState_BI_TT2 &state1, const RCPSPState_BI_TT2 &state2) const override {

    return 0;

    // std::map<int, int> earlyfinishMap; // Map to store activity IDs and their early finish times
  //   //std::map<int, int> visitmap; // Map to store activity IDs and their early finish times
  //   double h;
  //   std::set<int> processedDependencies;
  //   // Iterate over unstarted activitiesint lastElementEarlyFinish = 0;
  //   //int lastElementEarlyFinish = 0;
  //   for (int activityId: state1.unstartedTransitions) {
  //     int maxFinishTime = 0;
  //     std::set<int> processedDependencies;

      // for (const auto &dep: RCPSPex.backword_dependencies[activityId - 1]) {
      //   int depId = std::stoi(dep) - 1;
      //   // if (processedDependencies.count(depId) > 0) continue;
      //   // processedDependencies.insert(depId);
      //   if (std::find(state1.unstartedTransitions.begin(), state1.unstartedTransitions.end(), depId + 1) != state1.unstartedTransitions.end()) {
      //     //maby state1 or state2
      //    // int duration = getTransitionDuration2(state1.activeTransitionIndices, std::stoi(dep));
      //    // if (duration !=-1) {
      //       //maxFinishTime = std::max(maxFinishTime, earlyfinishMap[depId+1] + duration);
      //       //if (RCPSPex.activities[depId].duration !=duration) {
      //       //  std::cout<<name<<":"<<dep<<" "<<activityId<<" "<<RCPSPex.activities[depId].duration-duration<<std::endl;
      //       //}
      //    // }
      //     // else {
      //     //   maxFinishTime = std::max(maxFinishTime, earlyfinishMap[depId+1] + RCPSPex.activities[depId].duration);
      //     //
      //     // }
      //   }
      //   else {
      //     maxFinishTime = std::max(maxFinishTime, earlyfinishMap[depId+1]);
      //   }
      // }

    //   earlyfinishMap[activityId] = maxFinishTime;
    //   //std::cout <<activityId<<":"<< earlyfinishMap[activityId]+RCPSPex.activities[activityId-1].duration << std::endl;
    //   // For last element with duration 0, just use the max finish time of dependencies
    // }
    // if (earlyfinishMap.size()==0) {
    //   h = 0;
    // }
    // else {
    //   h = earlyfinishMap.rbegin()->second;;
    //
    // }
    // return h;
  }

  //
   inline double GCost(const RCPSPState_BI_TT2 &state1, const RCPSPState_BI_TT2 &state2) const override {
    return state2.fireTime;
    if (state2.direction) {
  return std::abs(state2.g_f - state1.g_f);

}
    else {
      return std::abs(state2.g_b - state1.g_b);

    }
  // Track actual transition cost
   }
  inline void GetActions(const RCPSPState_BI_TT2 &state, std::vector<action> &actions) const override {
    // Not used in BidirectionalGreedyBestFirst, but must be implemented
    return;
  }
  virtual action GetAction(const RCPSPState_BI_TT2 &state1, const RCPSPState_BI_TT2 &state2) const override {

    return static_cast<action>(0);
  }
  inline void ApplyAction(RCPSPState_BI_TT2 &state, action action) const override {
    // Not used, but required for abstract class
  }

  inline void UndoAction(RCPSPState_BI_TT2 &state, action action) const override {
    // Not needed for bidirectional search, but required
  }
double GCost(const RCPSPState_BI_TT2 &node, const action &act) const override {
return node.fireTime;
  };
  bool InvertAction(action& a) const override {
    return false; // Replace with appropriate logic
  }
  uint64_t GetActionHash(action act) const override {
    return 0;
  };


  uint64_t GetStateHash(const RCPSPState_BI_TT2 &node) const {
    std::size_t seed = 0;

    // 1. Finished Activities
    for (int i = 1; i <= petri.Transitions.size(); ++i) {
      seed ^= std::hash<int>{}(node.finishedActivitiys[i]) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    }

    // 2. Activity Nodes (Tokens)
    for (const auto& p : node.activity_nodes) {
      seed ^= std::hash<int>{}(p.first) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
      seed ^= std::hash<int>{}(p.second) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    }

    // 3. Resource Nodes
    for (const auto& resourceVec : node.resource_nodes) {
      for (const auto& p : resourceVec) {
        seed ^= std::hash<int>{}(p.first) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        seed ^= std::hash<int>{}(p.second) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
      }
    }

    // --- 4. ACTIVE TRANSITIONS (THE MISSING PIECE) ---
    // Must verify these are SORTED in the state constructor!
    // for (const auto& active : node.activeTransitionIndices) {
    //   // Hash the Task ID
    //   seed ^= std::hash<int>{}(active.first) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    //   // Hash the Remaining Time (Crucial!)
    //   seed ^= std::hash<int>{}(active.second) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    // }
    // seed ^= std::hash<int>{}(node.g) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    // seed ^= std::hash<int>{}(node.h) + 0x9e3779b9 + (seed << 6) + (seed >> 2);

    return seed;
    }

};

class ForwardRCPSPHeuristic : public Heuristic<RCPSPState_BI_TT2> {
public:
  double HCost(const RCPSPState_BI_TT2 &current, const RCPSPState_BI_TT2 &goal) const override {
    if (current.isDeltaZero) {
      current.h_f = current.predessesor_h_f;
      current.h_b = current.predessesor_h_b;

      if (current.direction) {
        return current.h_f;
      }
      else {
        return current.h_b;
      }
    }

    std::vector<short> tempUnstarted;
    tempUnstarted.reserve(petri.Transitions.size());

    for (int i = 0; i < petri.Transitions.size(); i++) {
      short taskID = i + 1;
      // FIX: Use .test() for bitset
      if (!current.finishedActivitiys.test(taskID)) {
        tempUnstarted.push_back(taskID);
      }
    }

    current.h_f = getForwardHcost(tempUnstarted,
                                    //state1.activity_nodes,
                                    current.activeTransitionIndices//,
                                  //  state1.finishedActivitiys
                                    );  // ← ADD THIS



    // if (current.h_f>current.predessesor_h_f) {
    //   std::cout << "inconsistentF: isDeltaZero=" << current.isDeltaZero
    //             << " firingTime=" << current.fireTime
    //             << " h_f=" << current.h_f
    //             << " pred_h_f=" << current.predessesor_h_f
    //             << " direction=" << current.direction
    //             << std::endl;    }

    return current.h_f;
    // return getForwardHcost_TT(tempUnstarted, state1.finishedActivitiys);
  }
};

class BackwardRCPSPHeuristic : public Heuristic<RCPSPState_BI_TT2> {
public:
  double HCost(const RCPSPState_BI_TT2 &current, const RCPSPState_BI_TT2 &start) const override {
    if (current.isDeltaZero) {
      current.h_f = current.predessesor_h_f;
      current.h_b = current.predessesor_h_b;

      if (current.direction) {
        return current.h_f;
      }
      else {
        return current.h_b;  // returns parent's h_b
      }
    }

    std::vector<short> tempUnstarted;
    tempUnstarted.reserve(petri.Transitions.size());

    for (int i = 0; i < petri.Transitions.size(); i++) {
      short taskID = i + 1;
      // FIX: Use .test() for bitset
      if (current.finishedActivitiys.test(taskID)) {//no ! unlike forward
        tempUnstarted.push_back(taskID);
      }
    }

    current.h_b = getBackwardHcost(tempUnstarted,
                                    //state1.activity_nodes,
                                    current.activeTransitionIndices//,
                                  //  state1.finishedActivitiys
                                    );  // ← ADD THIS
    // if (state1.g+state1.h <44 ) {
    //   std::cout<<"unadmissable";
    // }
    // if (current.h_b>current.predessesor_h_b) {
    //   std::cout<<"inconsistentB";
    // }
    return current.h_b;
  }

private:
  double getBackwardHcost_TT(const std::vector<short>& finishedTasks) const {
    if (finishedTasks.empty()) return 0;

    std::set<int> finishedSet(finishedTasks.begin(), finishedTasks.end());
    std::map<int, int> earlyFinish;

    for (int id : finishedTasks) {
      earlyFinish[id] = 0;
    }

    bool changed = true;
    while (changed) {
      changed = false;

      for (int actId : finishedTasks) {
        int maxPredFinish = 0;
        const auto& predecessors = RCPSPex.backword_dependencies[actId - 1];

        for (short predId : predecessors) {
          if (finishedSet.count(predId) && earlyFinish.count(predId)) {
            maxPredFinish = std::max(maxPredFinish, earlyFinish[predId]);
          }
        }

        int duration = RCPSPex.activities[actId - 1].duration;
        int newFinish = maxPredFinish + duration;

        if (newFinish > earlyFinish[actId]) {
          earlyFinish[actId] = newFinish;
          changed = true;
        }
      }
    }

    int maxFinish = 0;
    for (const auto& [_, time] : earlyFinish) {
      maxFinish = std::max(maxFinish, time);
    }

    return static_cast<double>(maxFinish);
  }
};

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

class RCPSP_TT : public SearchEnvironment<RCPSPState_TT,int>{
public:
  RCPSP_TT();
  void GetSuccessors(const RCPSPState_TT &nodeID, std::vector<RCPSPState_TT> &neighbors) const override;
  bool GoalTest(const RCPSPState_TT &node, const RCPSPState_TT &goal) const override;
  double HCost(const RCPSPState_TT &state1, const RCPSPState_TT &state2) const override;
  double GCost(const RCPSPState_TT &state1, const RCPSPState_TT &state2) const override;
  bool GetNextSuccessor(const RCPSPState_TT &curr, const RCPSPState_TT &goal,RCPSPState_TT &next, double parentH,uint64_t &special, bool &validMove) const;
  int GetNumSuccessors(const RCPSPState_TT &stateID) const;


  int GetAction(const RCPSPState_TT &nodeID, const RCPSPState_TT &nodeID2) const override;
  void GetActions(const RCPSPState_TT &nodeID, std::vector<int> &actions) const override;
  void ApplyAction(RCPSPState_TT &s, int a) const override;
  uint64_t GetActionHash(int act) const;
  uint64_t GetStateHash(const RCPSPState_TT &node) const;
  bool InvertAction(int &a) const;
  std::vector<RCPSPState_TT> GetSuccessors(const RCPSPState_TT &nodeID) const;
  double GCost(const RCPSPState_TT &node, const int &act) const override;
};

inline RCPSP_TT::RCPSP_TT() {
}

inline void RCPSP_TT::GetSuccessors(const RCPSPState_TT &nodeID, std::vector<RCPSPState_TT> &neighbors) const {
  //auto startS1 = std::chrono::high_resolution_clock::now();
  std::vector<short> tempUnstarted;

  // Optimization: Reserve max possible size to prevent re-allocations
  // (Using the size logic from your original code)
  tempUnstarted.reserve(petri.Transitions.size());

  // YOUR ORIGINAL LOGIC: Loop i from 1 to size, use ID = i + 1
  for (int i = 0; i < petri.Transitions.size(); i++) {
    short taskID = i + 1;

    // THE FIX: Direct Vector Access (O(1))
    // Instead of .find() == .end(), we check if the value is -1.
    if (nodeID.finishedActivitiys[taskID] == -1) {
      tempUnstarted.push_back(taskID);
    }
  }

  // ⚠️ IMPORTANT: You must update the definition of 'getAvailableTransitionIndices_TT'
  // to accept 'const std::vector<int>&' for the second argument, instead of 'std::map'.

  std::vector<std::pair<short, short>> avilableTransitionIndices = getAvailableTransitionIndices_TT(tempUnstarted, nodeID.finishedActivitiys,nodeID.resource_nodes, nodeID.activity_nodes);

  //std::vector<std::pair<short, short>> avilableTransitionIndices = getAvailableTransitionIndices_TT(tempUnstarted, nodeID.finishedActivitiys, nodeID.marking);

  for (const auto& [transId, firingTime] : avilableTransitionIndices) {
    neighbors.emplace_back(RCPSPState_TT(nodeID, transId, firingTime));
  }
  // auto endS1 = std::chrono::high_resolution_clock::now();
  // secssesorTIME += endS1 - startS1;

}

inline bool RCPSP_TT::GoalTest(const RCPSPState_TT &node, const RCPSPState_TT &goal) const {
  int actualFinishedCount = 0;
  for (int t : node.finishedActivitiys) {
    if (t != -1) {
      actualFinishedCount++;
    }
  }

  // Compare against the total number of required transitions
  return actualFinishedCount == petri.Transitions.size();  if (node.finishedActivitiys.size() ==petri.Transitions.size())
    return true;
  else
    return false;

}
inline double RCPSP_TT::HCost(const RCPSPState_TT &state1, const RCPSPState_TT &state2) const {
  //return 0;
  // 9. Optimized independent set calculation
  // Optimization: Reserve memory to prevent re-allocations.
  // If you know the number of unstarted tasks (e.g., total - finished_count), use that.
  // Otherwise, just reserve total.

  std::vector<short> tempUnstarted;

  // Optimization: Reserve max possible size to prevent re-allocations
  // (Using the size logic from your original code)
  // Optimization: Pre-allocate memory to avoid reallocations during push_back
  tempUnstarted.reserve(petri.Transitions.size());
  short h;
  for (int i = 0; i < petri.Transitions.size(); i++) {
    short taskID = i + 1;

    // THE FIX: Direct Vector Access (O(1))
    // Instead of map.find(), check if the value at this index is -1.
    // -1 indicates the task has not finished yet.
    if (state1.finishedActivitiys[taskID] == -1) {
      tempUnstarted.push_back(taskID);
    }
  }


int lastActivityId = -1;
  int maxTime = -1;

  // Find last finished activity by ID instead of name
  for (int id = 0; id < state1.finishedActivitiys.size(); ++id) {
    int time = state1.finishedActivitiys[id];

    // THE FIX: Only process tasks that actually finished (time != -1)
    if (time != -1) {
      if (time > maxTime) {
        maxTime = time;
        lastActivityId = id;
      }
    }
  }

  if (lastActivityId != -1) {
    const std::string& lastActivityName = RCPSPex.activities[lastActivityId - 1].name;

    // Pre-reserve vectors
    std::vector<int> independentSet;
    independentSet.reserve(tempUnstarted.size());

    // Filter independent transitions
    for (int actIdx : tempUnstarted) {
      const std::string& actName = RCPSPex.activities[actIdx - 1].name;
      if (RCPSPex.deep_dependencies.find({lastActivityName, actName}) == RCPSPex.deep_dependencies.end()) {
        independentSet.push_back(actIdx);
      }
    }

    // Create lookup set for efficient filtering
    std::unordered_set<int> independentLookup(independentSet.begin(), independentSet.end());
    std::vector<short> newUnstartedTransitions;
    newUnstartedTransitions.reserve(tempUnstarted.size());

    for (int id : tempUnstarted) {
      if (independentLookup.find(id) == independentLookup.end()) {
        newUnstartedTransitions.push_back(id);
      }
    }

    // 10. Calculate heuristic efficiently
    int latestStart = 0;

    // FIX: Iterate through vector indices
    for (int id = 0; id < state1.finishedActivitiys.size(); ++id) {
      int finishTime = state1.finishedActivitiys[id];

      // Check if valid finish time exists
      if (finishTime != -1) {
        // 1. Get the duration of this activity
        // Note: 'id' is 1-based, so subtract 1 to access the static activities vector
        int duration = RCPSPex.activities[id - 1].duration;

        // 2. Calculate Start Time
        int startTime = finishTime - duration;

        // 3. Update Max
        if (startTime > latestStart) {
          latestStart = startTime;
        }
      }
    }

    int unkTime = state1.g - latestStart;

    // FIX: Copy the vector (std::vector copy is deep by default)
    std::array<short, 128> finishedActivitiysnew = state1.finishedActivitiys;

    for (int actIdx : independentSet) {
      // FIX: Direct index access
      finishedActivitiysnew[actIdx] = 0;
    }

    // return std::max(getForwardHcost_TT(tempUnstarted, state1.finishedActivitiys) - unkTime,
    //            getForwardHcost_TT(newUnstartedTransitions, finishedActivitiysnew));
//return getForwardHcost_TT(tempUnstarted) - unkTime;
   short h1 =getForwardHcost_TT(tempUnstarted) - unkTime;
    short h2=getForwardHcost_TT(newUnstartedTransitions);
    if (h1>=h2) {
      h=h1;
    }
    else {
      h=h2;
    }
    state1.h=h;
    if (state1.h>77) {
      std::cout<<"not addmissable"<< "\n";
      std::cout << "latestStart=" << latestStart << ", state1.g=" << state1.g << ", unkTime=" << unkTime << std::endl;
      std::cout << "All finished activities and their start times:" << std::endl;
      for (int id = 1; id < state1.finishedActivitiys.size(); ++id) {
        int finishTime = state1.finishedActivitiys[id];
        if (finishTime != -1) {
          int duration = RCPSPex.activities[id - 1].duration;
          int startTime = finishTime - duration;
          std::cout << "  Activity " << id << ": start=" << startTime << ", finish=" << finishTime << ", duration=" << duration << std::endl;
        }
      }
    }
  }
  else {
    // Fallback if no finished activities
    h= getForwardHcost_TT(tempUnstarted);
    // return getForwardHcost_TT(tempUnstarted, state1.finishedActivitiys);
    state1.h=h;

  }
// if (h+state1.g<state2.g) {
// std::cout << "not admissable";
// }
// if (h+state1.g>70) {
//   std::cout<<state1.g<<"\n";
// }


  return h;

}
inline double RCPSP_TT::GCost(const RCPSPState_TT &state1, const RCPSPState_TT &state2) const {
  return state2.g-state1.g;//+state1.g
}

inline uint64_t RCPSP_TT::GetStateHash(const RCPSPState_TT &node) const {
  std::size_t seed = 0;
  // 1. Finished Activities (Array 128)
  for (int id = 0; id < 128; ++id) {
    // מצפינים את ה-ID ואת הזמן היחסי (Time - G)
    // בלי IF: גם אם זה מינוס אחד, זה נכנס לחישוב
    seed ^= std::hash<int>{}(id) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    seed ^= std::hash<int>{}(node.finishedActivitiys[id]) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
  }

  // 2. Activity Nodes (Vector of Pairs)
  for (const auto& p : node.activity_nodes) {
    // p.first = ID, p.second = Time
    seed ^= std::hash<int>{}(p.first) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    seed ^= std::hash<int>{}(p.second) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
  }

  // 3. Resource Nodes (Array 4 of Vectors of Pairs)
  for (int r = 0; r < 4; ++r) {
    // מצפינים את אינדקס המשאב כדי להפריד בין הוקטורים
    seed ^= std::hash<int>{}(r) + 0x9e3779b9 + (seed << 6) + (seed >> 2);

    for (const auto& p : node.resource_nodes[r]) {
      // p.first = Amount/ID, p.second = Time
      seed ^= std::hash<int>{}(p.first) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
      seed ^= std::hash<int>{}(p.second) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    }
  }

  return seed;

}

inline bool RCPSP_TT::GetNextSuccessor(const RCPSPState_TT &curr, const RCPSPState_TT &goal,
                      RCPSPState_TT &next, double parentH,
                      uint64_t &special, bool &validMove) const
{

  // Cache for storing transition lists per node
  static std::unordered_map<uint64_t, std::vector<std::pair<short, short>>> transitionCache;

  // Compute a hash for the current state
  uint64_t nodeHash = 0;
  for (size_t i = 0; i < curr.finishedActivitiys.size(); i++) {
    nodeHash = nodeHash * 31 + (curr.finishedActivitiys[i] + 1000); // +1000 to handle -1 values
  }
  // Add resource state to hash for uniqueness
  for (size_t i = 0; i < curr.resource_nodes.size(); i++) {
    for (const auto& [amt, time] : curr.resource_nodes[i]) {
      nodeHash = nodeHash * 31 + amt;
      nodeHash = nodeHash * 31 + time;
    }
  }

  std::vector<std::pair<short, short>> avilableTransitionIndices;

  if (special == 0) {
    // First call for this node - generate the transition list
    std::vector<short> tempUnstarted;
    tempUnstarted.reserve(petri.Transitions.size());

    for (int i = 0; i < petri.Transitions.size(); i++) {
      short taskID = i + 1;
      if (curr.finishedActivitiys[taskID] == -1) {
        tempUnstarted.push_back(taskID);
      }
    }

    avilableTransitionIndices = getAvailableTransitionIndices_TT(tempUnstarted, curr.finishedActivitiys,
                                                                  curr.resource_nodes, curr.activity_nodes);

    // Optional: Sort for determinism (use this if sorting by time works for you)
    if (avilableTransitionIndices.size() > 1) {
      std::sort(avilableTransitionIndices.begin(), avilableTransitionIndices.end(),
        [](const std::pair<short, short>& a, const std::pair<short, short>& b) {
           // if (a.second != b.second) return a.second < b.second;  // By firing time
            return a.first < b.first;  // Then by transition ID
        });
    }

    // Cache the transition list
    transitionCache[nodeHash] = avilableTransitionIndices;
  } else {
    // Subsequent calls - retrieve from cache
    auto it = transitionCache.find(nodeHash);
    if (it != transitionCache.end()) {
      avilableTransitionIndices = it->second;
    } else {
      // This shouldn't happen, but handle it gracefully
      std::cerr << "ERROR: Cache miss for node hash " << nodeHash << " with special=" << special << std::endl;
      validMove = false;
      return false;
    }
  }

  unsigned int index = (unsigned int)special;

  // Check if we have run out of moves
  if (index >= avilableTransitionIndices.size()) {
    // Clean up cache when node is fully expanded
    transitionCache.erase(nodeHash);
    validMove = false;
    return false;
  }

  // Retrieve the specific pair for this step
  std::pair<short, short> selectedMove = avilableTransitionIndices[index];

  // Generate the full state ONLY for this move
  next = RCPSPState_TT(curr, selectedMove.first, selectedMove.second);

  // Mark as valid and increment index
  validMove = true;
  special++;

  // Return true if there are more moves in the list
  return (index + 1 < avilableTransitionIndices.size());
}

inline uint64_t RCPSP_TT::GetActionHash(int act) const {
  // Example hash for an action
  return std::hash<int>()(act);
}
inline void RCPSP_TT::GetActions(const RCPSPState_TT &nodeID, std::vector<int> &actions) const {
  // for (int i = 0; i < nodeID.sons.size(); ++i) {
  //   actions.push_back(i); // Add the index of each available transition as an action.
  // }
}

inline bool RCPSP_TT::InvertAction(int &a) const {
  // Example logic to invert an action
  return true;
}

inline std::vector<RCPSPState_TT> RCPSP_TT::GetSuccessors(const RCPSPState_TT &nodeID) const {

  std::vector<RCPSPState_TT> neighbors;
  return neighbors;
}


inline int RCPSP_TT::GetAction(const RCPSPState_TT &nodeID, const RCPSPState_TT &nodeID2) const {
  return 0;
}

//inline uint64_t RCPSP::GetStateHash(const RCPSPState &s) const {
// return s.name;
//}
inline int RCPSP_TT::GetNumSuccessors(const RCPSPState_TT &stateID) const {
  //auto startS1 = std::chrono::high_resolution_clock::now();
  std::vector<short> tempUnstarted;

  // Optimization: Reserve max possible size to prevent re-allocations
  // (Using the size logic from your original code)
  tempUnstarted.reserve(petri.Transitions.size());

  // YOUR ORIGINAL LOGIC: Loop i from 1 to size, use ID = i + 1
  for (int i = 0; i < petri.Transitions.size(); i++) {
    short taskID = i + 1;

    // THE FIX: Direct Vector Access (O(1))
    // Instead of .find() == .end(), we check if the value is -1.
    if (stateID.finishedActivitiys[taskID] == -1) {
      tempUnstarted.push_back(taskID);
    }
  }

  // ⚠️ IMPORTANT: You must update the definition of 'getAvailableTransitionIndices_TT'
  // to accept 'const std::vector<int>&' for the second argument, instead of 'std::map'.

  std::vector<std::pair<short, short>> avilableTransitionIndices = getAvailableTransitionIndices_TT(tempUnstarted, stateID.finishedActivitiys,stateID.resource_nodes, stateID.activity_nodes);

  return avilableTransitionIndices.size();
}


inline void RCPSP_TT::ApplyAction(RCPSPState_TT &s, int a) const {

}
inline double RCPSP_TT::GCost(const RCPSPState_TT &node, const int &act) const {
  return node.g;
}




class RCPSP_TT2 : public SearchEnvironment<RCPSPState_TT2,int>{
public:
  RCPSP_TT2();
  void GetSuccessors(const RCPSPState_TT2 &nodeID, std::vector<RCPSPState_TT2> &neighbors) const override;
  bool GoalTest(const RCPSPState_TT2 &node, const RCPSPState_TT2 &goal) const override;
  double HCost(const RCPSPState_TT2 &state1, const RCPSPState_TT2 &state2) const override;
  double GCost(const RCPSPState_TT2 &state1, const RCPSPState_TT2 &state2) const override;
  bool GetNextSuccessor(const RCPSPState_TT2 &curr, const RCPSPState_TT2 &goal,RCPSPState_TT2 &next, double parentH,uint64_t &special, bool &validMove) const;


  int GetNumSuccessors(const RCPSPState_TT2 &stateID) const;


  int GetAction(const RCPSPState_TT2 &nodeID, const RCPSPState_TT2 &nodeID2) const override;
  void GetActions(const RCPSPState_TT2 &nodeID, std::vector<int> &actions) const override;
  void ApplyAction(RCPSPState_TT2 &s, int a) const override;
  uint64_t GetActionHash(int act) const;
  uint64_t GetStateHash(const RCPSPState_TT2 &node) const;
  bool InvertAction(int &a) const;
  std::vector<RCPSPState_TT2> GetSuccessors(const RCPSPState_TT2 &nodeID) const;
  double GCost(const RCPSPState_TT2 &node, const int &act) const override;
};

inline RCPSP_TT2::RCPSP_TT2() {
}


inline void RCPSP_TT2::GetSuccessors(const RCPSPState_TT2 &nodeID, std::vector<RCPSPState_TT2> &neighbors) const {
  //auto startS1 = std::chrono::high_resolution_clock::now();
  std::vector<short> tempUnstarted;

  // Optimization: Reserve max possible size to prevent re-allocations
  // (Using the size logic from your original code)
  tempUnstarted.reserve(petri.Transitions.size());

  // YOUR ORIGINAL LOGIC: Loop i from 1 to size, use ID = i + 1
  for (int i = 0; i < petri.Transitions.size(); i++) {
    short taskID = i + 1;

    // THE FIX: Direct Vector Access (O(1))
    // Instead of .find() == .end(), we check if the value is -1.
    if (nodeID.finishedActivitiys[taskID] == 0) {
      tempUnstarted.push_back(taskID);
    }
  }

  // ⚠️ IMPORTANT: You must update the definition of 'getAvailableTransitionIndices_TT'
  // to accept 'const std::vector<int>&' for the second argument, instead of 'std::map'.

  std::vector<std::pair<short, short>> avilableTransitionIndices = getAvailableTransitionIndices_TT2(tempUnstarted, nodeID.finishedActivitiys,nodeID.resource_nodes, nodeID.activity_nodes,nodeID.activeTransitionIndices);

  //std::vector<std::pair<short, short>> avilableTransitionIndices = getAvailableTransitionIndices_TT(tempUnstarted, nodeID.finishedActivitiys, nodeID.marking);

  for (const auto& [transId, Timedelta] : avilableTransitionIndices) {
    neighbors.emplace_back(RCPSPState_TT2(nodeID, transId, Timedelta,1));
  }
  // auto endS1 = std::chrono::high_resolution_clock::now();
  // secssesorTIME += endS1 - startS1;

}

inline bool RCPSP_TT2::GoalTest(const RCPSPState_TT2 &node, const RCPSPState_TT2 &goal) const {
  int count = 0;

  // רצים רק על הטווח הרלוונטי (למשל 1 עד 30)
  for (int i = 1; i <= petri.Transitions.size(); i++) {
    // אם הערך הוא 1 (סיים), מוסיפים לספירה
    if (node.finishedActivitiys[i] == 1) {
      count++;
    }
  }

  // אם ספרנו בדיוק כמספר המשימות בבעיה -> סיימנו
  return count == petri.Transitions.size();

}




inline double RCPSP_TT2::HCost(const RCPSPState_TT2 &state1, const RCPSPState_TT2 &state2) const {
  if (state1.isDeltaZero){//||!state1.isCriticalInActive) {
    state1.h = state1.predessesor_h;
    return state1.h;
  }

  std::vector<short> tempUnfinished;
  tempUnfinished.reserve(petri.Transitions.size());

  for (int i = 0; i < petri.Transitions.size(); i++) {
    short taskID = i + 1;
    // FIX: Use .test() for bitset
    if (!state1.finishedActivitiys.test(taskID)) {
      tempUnfinished.push_back(taskID);
    }
  }

  state1.h = std::max(getForwardHcost(tempUnfinished,state1.activeTransitionIndices,state1.nextCritical),getforwardResource(tempUnfinished,state1.activeTransitionIndices));  // ← ADD THIS
  return state1.h;
}
inline double RCPSP_TT2::GCost(const RCPSPState_TT2 &state1, const RCPSPState_TT2 &state2) const {
  return state2.g-state1.g;//+state1.g
}
// Old Boost hash_combine — used 32-bit constant 0x9e3779b9, giving only ~32-bit
// effective collision resistance (birthday paradox causes collisions for 1M+ states).
// inline uint64_t RCPSP_TT2::GetStateHash(const RCPSPState_TT2 &node) const {
//   std::size_t seed = 0;
//
//   // 1. Finished Activities
//   for (int i = 1; i <= petri.Transitions.size(); ++i) {
//     seed ^= std::hash<int>{}(node.finishedActivitiys[i]) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
//   }
//
//   // 2. Activity Nodes (Tokens)
//   // for (const auto& p : node.activity_nodes) {
//   //   seed ^= std::hash<int>{}(p.first) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
//   //   seed ^= std::hash<int>{}(p.second) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
//   // }
//   //
//   // // 3. Resource Nodes
//   // for (const auto& resourceVec : node.resource_nodes) {
//   //   for (const auto& p : resourceVec) {
//   //     seed ^= std::hash<int>{}(p.first) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
//   //     seed ^= std::hash<int>{}(p.second) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
//   //   }
//   // }
//
//   // --- 4. ACTIVE TRANSITIONS (THE MISSING PIECE) ---
//   // Must verify these are SORTED in the state constructor!
//   for (const auto& active : node.activeTransitionIndices) {
//     // Hash the Task ID
//     seed ^= std::hash<int>{}(active.first) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
//     // Hash the Remaining Time (Crucial!)
//     seed ^= std::hash<int>{}(active.second) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
//   }
//   // seed ^= std::hash<int>{}(node.g) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
//   // seed ^= std::hash<int>{}(node.h) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
//
//   return seed;
// }

// FNV-1a 64-bit hash — true 64-bit avalanche, no birthday collision for 1M+ states.
inline uint64_t RCPSP_TT2::GetStateHash(const RCPSPState_TT2 &node) const {
  constexpr uint64_t FNV_OFFSET = 14695981039346656037ULL;
  constexpr uint64_t FNV_PRIME  = 1099511628211ULL;
  uint64_t h = FNV_OFFSET;

  auto mix_int = [&](int v) {
    const uint8_t* p = reinterpret_cast<const uint8_t*>(&v);
    for (size_t i = 0; i < sizeof(int); ++i) {
      h ^= static_cast<uint64_t>(p[i]);
      h *= FNV_PRIME;
    }
  };

  // 1. Finished Activities (indexed 1..N)
  for (int i = 1; i <= (int)petri.Transitions.size(); ++i) {
    mix_int(node.finishedActivitiys[i]);
  }

  // 4. Active Transitions: (task_id, remaining_time) pairs — must be sorted
  for (const auto& active : node.activeTransitionIndices) {
    mix_int(active.first);
    mix_int(active.second);
  }

  return h;
}

inline bool RCPSP_TT2::GetNextSuccessor(const RCPSPState_TT2 &curr, const RCPSPState_TT2 &goal,
                      RCPSPState_TT2 &next, double parentH,
                      uint64_t &special, bool &validMove) const
{
  if (special == 0 && !curr.transitionsCached) {
    // First call for this node - generate and cache
    std::vector<short> tempUnstarted;
    tempUnstarted.reserve(petri.Transitions.size());

    for (int i = 0; i < petri.Transitions.size(); i++) {
      short taskID = i + 1;
      if (curr.finishedActivitiys[taskID] == 0) {
        tempUnstarted.push_back(taskID);
      }
    }

    curr.AvailableTransitionIndices_TT2 = getAvailableTransitionIndices_TT2(tempUnstarted,
                                                                curr.finishedActivitiys,
                                                                curr.resource_nodes,
                                                                curr.activity_nodes,
                                                                curr.activeTransitionIndices);

    // Sort for determinism
    if (curr.AvailableTransitionIndices_TT2.size() > 1) {
      std::sort(curr.AvailableTransitionIndices_TT2.begin(), curr.AvailableTransitionIndices_TT2.end(),
        [](const std::pair<short, short>& a, const std::pair<short, short>& b) {
            return a.first < b.first;
        });
    }

    curr.transitionsCached = true;
  }

  unsigned int index = (unsigned int)special;

  // Check if we have run out of moves
  if (index >= curr.AvailableTransitionIndices_TT2.size()) {
    validMove = false;
    return false;
  }

  // Retrieve the specific pair for this step
  std::pair<short, short> selectedMove = curr.AvailableTransitionIndices_TT2[index];

  // Generate the next state for this move
  next = RCPSPState_TT2(curr, selectedMove.first, selectedMove.second);

  validMove = true;
  special++;

  return (index + 1 < curr.AvailableTransitionIndices_TT2.size());
}

inline uint64_t RCPSP_TT2::GetActionHash(int act) const {
  // Example hash for an action
  return std::hash<int>()(act);
}
inline void RCPSP_TT2::GetActions(const RCPSPState_TT2 &nodeID, std::vector<int> &actions) const {
  // for (int i = 0; i < nodeID.sons.size(); ++i) {
  //   actions.push_back(i); // Add the index of each available transition as an action.
  // }
}

inline bool RCPSP_TT2::InvertAction(int &a) const {
  // Example logic to invert an action
  return true;
}

inline std::vector<RCPSPState_TT2> RCPSP_TT2::GetSuccessors(const RCPSPState_TT2 &nodeID) const {

  std::vector<RCPSPState_TT2> neighbors;
  return neighbors;
}


inline int RCPSP_TT2::GetAction(const RCPSPState_TT2 &nodeID, const RCPSPState_TT2 &nodeID2) const {
  return 0;
}

//inline uint64_t RCPSP::GetStateHash(const RCPSPState &s) const {
// return s.name;
//}
inline int RCPSP_TT2::GetNumSuccessors(const RCPSPState_TT2 &stateID) const {
  //auto startS1 = std::chrono::high_resolution_clock::now();
  std::vector<short> tempUnstarted;

  // Optimization: Reserve max possible size to prevent re-allocations
  // (Using the size logic from your original code)
  tempUnstarted.reserve(petri.Transitions.size());

  // YOUR ORIGINAL LOGIC: Loop i from 1 to size, use ID = i + 1
  for (int i = 0; i < petri.Transitions.size(); i++) {
    short taskID = i + 1;

    // THE FIX: Direct Vector Access (O(1))
    // Instead of .find() == .end(), we check if the value is -1.
    if (stateID.finishedActivitiys[taskID] == 0) {
      tempUnstarted.push_back(taskID);
    }
  }

  // ⚠️ IMPORTANT: You must update the definition of 'getAvailableTransitionIndices_TT'
  // to accept 'const std::vector<int>&' for the second argument, instead of 'std::map'.

  std::vector<std::pair<short, short>> avilableTransitionIndices = getAvailableTransitionIndices_TT2(tempUnstarted, stateID.finishedActivitiys,stateID.resource_nodes, stateID.activity_nodes,stateID.activeTransitionIndices);

  return avilableTransitionIndices.size();
}


inline void RCPSP_TT2::ApplyAction(RCPSPState_TT2 &s, int a) const {

}
inline double RCPSP_TT2::GCost(const RCPSPState_TT2 &node, const int &act) const {
  return node.g;
}

class RCPSP_TT2_Backward : public SearchEnvironment<RCPSPState_TT2,int>{
public:
  RCPSP_TT2_Backward();
  void GetSuccessors(const RCPSPState_TT2 &nodeID, std::vector<RCPSPState_TT2> &neighbors) const override;
  bool GoalTest(const RCPSPState_TT2 &node, const RCPSPState_TT2 &goal) const override;
  double HCost(const RCPSPState_TT2 &state1, const RCPSPState_TT2 &state2) const override;
  double GCost(const RCPSPState_TT2 &state1, const RCPSPState_TT2 &state2) const override;
  bool GetNextSuccessor(const RCPSPState_TT2 &curr, const RCPSPState_TT2 &goal,RCPSPState_TT2 &next, double parentH,uint64_t &special, bool &validMove) const;


  int GetNumSuccessors(const RCPSPState_TT2 &stateID) const;


  int GetAction(const RCPSPState_TT2 &nodeID, const RCPSPState_TT2 &nodeID2) const override;
  void GetActions(const RCPSPState_TT2 &nodeID, std::vector<int> &actions) const override;
  void ApplyAction(RCPSPState_TT2 &s, int a) const override;
  uint64_t GetActionHash(int act) const;
  uint64_t GetStateHash(const RCPSPState_TT2 &node) const;
  bool InvertAction(int &a) const;
  std::vector<RCPSPState_TT2> GetSuccessors(const RCPSPState_TT2 &nodeID) const;
  double GCost(const RCPSPState_TT2 &node, const int &act) const override;
};

inline RCPSP_TT2_Backward::RCPSP_TT2_Backward() {
}


inline void RCPSP_TT2_Backward::GetSuccessors(const RCPSPState_TT2 &nodeID, std::vector<RCPSPState_TT2> &neighbors) const {

  std::vector<short> tempUnstarted;
  tempUnstarted.reserve(petri.Transitions.size());

  // Loop through all transitions
  for (int i = 0; i < petri.Transitions.size(); i++) {
    short taskID = i + 1;

    // In Backward Search:
    // 0 = Not yet scheduled from the End (We want to move these)
    // 1 = Already scheduled (These are done)
    if (nodeID.finishedActivitiys[taskID] == 1) {
      tempUnstarted.push_back(taskID);
    }
  }

  // PASS THE FIX: Use the BACKWARD availability function
  std::vector<std::pair<short, short>> avilableTransitionIndices =
       getAvailableTransitionIndices_TT2_backward(
           tempUnstarted,
           nodeID.finishedActivitiys, // Pass bitset
           nodeID.resource_nodes,
           nodeID.activity_nodes,
           nodeID.activeTransitionIndices
       );

  for (const auto& [transId, Timedelta] : avilableTransitionIndices) {
    // Generate Neighbor
    // Ensure RCPSPState_TT2 constructor correctly sets 'finishedActivitiys[transId] = 1'
    neighbors.emplace_back(RCPSPState_TT2(nodeID, transId, Timedelta,0));
  }
}

inline bool RCPSP_TT2_Backward::GoalTest(const RCPSPState_TT2 &node, const RCPSPState_TT2 &goal) const {
  int count = 0;

  // רצים רק על הטווח הרלוונטי (למשל 1 עד 30)
  for (int i = 1; i <= petri.Transitions.size(); i++) {
    // אם הערך הוא 1 (סיים), מוסיפים לספירה
    if (node.finishedActivitiys[i] == 1) {
      count++;
    }
  }

  // אם ספרנו בדיוק כמספר המשימות בבעיה -> סיימנו
  return count == 0;

}




inline double RCPSP_TT2_Backward::HCost(const RCPSPState_TT2 &state1, const RCPSPState_TT2 &state2) const {
  if (state1.isDeltaZero) {
    state1.h = state1.predessesor_h;
    return state1.h;
  }

  std::vector<short> tempUnstarted;
  tempUnstarted.reserve(petri.Transitions.size());

  for (int i = 0; i < petri.Transitions.size(); i++) {
    short taskID = i + 1;
    // FIX: Use .test() for bitset
    if (state1.finishedActivitiys.test(taskID)) {//no ! unlike forward
      tempUnstarted.push_back(taskID);
    }
  }

  state1.h = getBackwardHcost(tempUnstarted,
                                  //state1.activity_nodes,
                                  state1.activeTransitionIndices//,
                                //  state1.finishedActivitiys
                                  );  // ← ADD THIS
// if (state1.g+state1.h <44 ) {
//   std::cout<<"unadmissable";
// }
  return state1.h;
}

inline double RCPSP_TT2_Backward::GCost(const RCPSPState_TT2 &state1, const RCPSPState_TT2 &state2) const {
  return state2.g-state1.g;//+state1.g
}

inline uint64_t RCPSP_TT2_Backward::GetStateHash(const RCPSPState_TT2 &node) const {
  std::size_t seed = 0;

  // 1. Finished Activities
  for (int i = 1; i <= petri.Transitions.size(); ++i) {
    seed ^= std::hash<int>{}(node.finishedActivitiys[i]) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
  }

 // 2. Activity Nodes (Tokens)
  for (const auto& p : node.activity_nodes) {
    seed ^= std::hash<int>{}(p.first) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    seed ^= std::hash<int>{}(p.second) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
  }

  // 3. Resource Nodes
  for (const auto& resourceVec : node.resource_nodes) {
    for (const auto& p : resourceVec) {
      seed ^= std::hash<int>{}(p.first) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
      seed ^= std::hash<int>{}(p.second) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    }
  }

  // --- 4. ACTIVE TRANSITIONS (THE MISSING PIECE) ---
  // Must verify these are SORTED in the state constructor!
  // for (const auto& active : node.activeTransitionIndices) {
  //   // Hash the Task ID
  //   seed ^= std::hash<int>{}(active.first) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
  //   // Hash the Remaining Time (Crucial!)
  //   seed ^= std::hash<int>{}(active.second) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
  // }
  // seed ^= std::hash<int>{}(node.g) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
  // seed ^= std::hash<int>{}(node.h) + 0x9e3779b9 + (seed << 6) + (seed >> 2);

  return seed;
}

// inline bool RCPSP_TT2_Backward::GetNextSuccessor(const RCPSPState_TT2 &curr, const RCPSPState_TT2 &goal,
//                       RCPSPState_TT2 &next, double parentH,
//                       uint64_t &special, bool &validMove) const
// {
//   if (special == 0 && !curr.transitionsCached) {
//     // First call for this node - generate and cache
//     std::vector<short> tempUnstarted;
//     tempUnstarted.reserve(petri.Transitions.size());
//
//     for (int i = 0; i < petri.Transitions.size(); i++) {
//       short taskID = i + 1;
//       if (curr.finishedActivitiys[taskID] == 0) {
//         tempUnstarted.push_back(taskID);
//       }
//     }
//
//     curr.AvailableTransitionIndices_TT2 = getAvailableTransitionIndices_TT2(tempUnstarted,
//                                                                 curr.finishedActivitiys,
//                                                                 curr.resource_nodes,
//                                                                 curr.activity_nodes,
//                                                                 curr.activeTransitionIndices);
//
//     // Sort for determinism
//     if (curr.AvailableTransitionIndices_TT2.size() > 1) {
//       std::sort(curr.AvailableTransitionIndices_TT2.begin(), curr.AvailableTransitionIndices_TT2.end(),
//         [](const std::pair<short, short>& a, const std::pair<short, short>& b) {
//             return a.first < b.first;
//         });
//     }
//
//     curr.transitionsCached = true;
//   }
//
//   unsigned int index = (unsigned int)special;
//
//   // Check if we have run out of moves
//   if (index >= curr.AvailableTransitionIndices_TT2.size()) {
//     validMove = false;
//     return false;
//   }
//
//   // Retrieve the specific pair for this step
//   std::pair<short, short> selectedMove = curr.AvailableTransitionIndices_TT2[index];
//
//   // Generate the next state for this move
//   next = RCPSPState_TT2(curr, selectedMove.first, selectedMove.second);
//
//   validMove = true;
//   special++;
//
//   return (index + 1 < curr.AvailableTransitionIndices_TT2.size());
// }

inline uint64_t RCPSP_TT2_Backward::GetActionHash(int act) const {
  // Example hash for an action
  return std::hash<int>()(act);
}
inline void RCPSP_TT2_Backward::GetActions(const RCPSPState_TT2 &nodeID, std::vector<int> &actions) const {
  // for (int i = 0; i < nodeID.sons.size(); ++i) {
  //   actions.push_back(i); // Add the index of each available transition as an action.
  // }
}

inline bool RCPSP_TT2_Backward::InvertAction(int &a) const {
  // Example logic to invert an action
  return true;
}

inline std::vector<RCPSPState_TT2> RCPSP_TT2_Backward::GetSuccessors(const RCPSPState_TT2 &nodeID) const {

  std::vector<RCPSPState_TT2> neighbors;
  return neighbors;
}


inline int RCPSP_TT2_Backward::GetAction(const RCPSPState_TT2 &nodeID, const RCPSPState_TT2 &nodeID2) const {
  return 0;
}

//inline uint64_t RCPSP::GetStateHash(const RCPSPState &s) const {
// return s.name;
//}
inline int RCPSP_TT2_Backward::GetNumSuccessors(const RCPSPState_TT2 &stateID) const {
  //auto startS1 = std::chrono::high_resolution_clock::now();
  std::vector<short> tempUnstarted;

  // Optimization: Reserve max possible size to prevent re-allocations
  // (Using the size logic from your original code)
  tempUnstarted.reserve(petri.Transitions.size());

  // YOUR ORIGINAL LOGIC: Loop i from 1 to size, use ID = i + 1
  for (int i = 0; i < petri.Transitions.size(); i++) {
    short taskID = i + 1;

    // THE FIX: Direct Vector Access (O(1))
    // Instead of .find() == .end(), we check if the value is -1.
    if (stateID.finishedActivitiys[taskID] == 0) {
      tempUnstarted.push_back(taskID);
    }
  }

  // ⚠️ IMPORTANT: You must update the definition of 'getAvailableTransitionIndices_TT'
  // to accept 'const std::vector<int>&' for the second argument, instead of 'std::map'.

  std::vector<std::pair<short, short>> avilableTransitionIndices = getAvailableTransitionIndices_TT2(tempUnstarted, stateID.finishedActivitiys,stateID.resource_nodes, stateID.activity_nodes,stateID.activeTransitionIndices);

  return avilableTransitionIndices.size();
}


inline void RCPSP_TT2_Backward::ApplyAction(RCPSPState_TT2 &s, int a) const {

}
inline double RCPSP_TT2_Backward::GCost(const RCPSPState_TT2 &node, const int &act) const {
  return node.g;
}

template<short N>
class RCPSP_CBS : public SearchEnvironment<RCPSPState_CBS<N>,int>{
public:
  RCPSP_CBS();
  void GetSuccessors(const RCPSPState_CBS<N> &nodeID, std::vector<RCPSPState_CBS<N>> &neighbors) const override;
  bool GoalTest(const RCPSPState_CBS<N> &node, const RCPSPState_CBS<N> &goal) const override;
  double HCost(const RCPSPState_CBS<N> &state1, const RCPSPState_CBS<N> &state2) const override;
  double GCost(const RCPSPState_CBS<N> &state1, const RCPSPState_CBS<N> &state2) const override;

  int GetAction(const RCPSPState_CBS<N> &nodeID, const RCPSPState_CBS<N> &nodeID2) const override;
  int GetNumSuccessors(const RCPSPState_CBS<N> &stateID) const;
  void GetActions(const RCPSPState_CBS<N> &nodeID, std::vector<int> &actions) const override;
  void ApplyAction(RCPSPState_CBS<N> &s, int a) const override;
  uint64_t GetActionHash(int act) const;
  uint64_t GetStateHash(const RCPSPState_CBS<N> &node) const;
  bool InvertAction(int &a) const;
  std::vector<RCPSPState_CBS<N>> GetSuccessors(const RCPSPState_CBS<N> &nodeID) const;
  double GCost(const RCPSPState_CBS<N> &node, const int &act) const override;

};


template<short N>
inline RCPSP_CBS<N>::RCPSP_CBS() {
}

// inline void RCPSP_CBS::GetSuccessors(const RCPSPState_CBS &nodeID, std::vector<RCPSPState_CBS> &neighbors) const {
//
//   if (nodeID.RVS.empty()) return;
//
//   const Conflict& first = nodeID.RVS[0];
//   for (short j = 0; j < first.activities.size(); j++) {
//     neighbors.emplace_back(nodeID, first.activities[j], first.t);
//   }
// }
// inline void RCPSP_CBS::GetSuccessors(const RCPSPState_CBS &nodeID, std::vector<RCPSPState_CBS> &neighbors) const {
//
//   if (nodeID.rvs_activities_pool.empty()) return;
//
//   // const Conflict& first = nodeID.RVS[0];
//
//   // Use the helper to stream activities directly out of the global pool
//   // const Conflict& first = nodeID.rvs_activities_pool;
//   std::span<const short> acts = nodeID.rvs_activities_pool;
//   for (short j = 0; j < nodeID.rvs_activities_pool.size(); j++) {
//     RCPSPState_CBS child(nodeID, acts[j], nodeID.t);
//     if (child.isLeftShiftable())
//       continue;  // prune this child
//     neighbors.emplace_back(child);
//   }
// //   std::sort(neighbors.begin(), neighbors.end(),
// // [](const auto& a, const auto& b) {
// //     return a.makespan() < b.makespan();
// // });
// }
// put this BEFORE GetSuccessors, at file scope or as a static helper
template<short N>
bool satisfies_optimal(const RCPSPState_CBS<N>& s) {
  static const std::array<short, 32> optimal_37_1 = {
    0,  // 0 (act1)
    0,  // 1 (act2)
    0,  // 2 (act3)
    8,  // 3 (act4)
    27, // 4 (act5)
    2,  // 5 (act6)
    17, // 6 (act7)
    25, // 7 (act8)
    19, // 8 (act9)
    7,  // 9 (act10)
    17, // 10 (act11)
    37, // 11 (act12)
    36, // 12 (act13)
    27, // 13 (act14)
    19, // 14 (act15)
    59, // 15 (act16)
    34, // 16 (act17)
    13, // 17 (act18)
    44, // 18 (act19)
    36, // 19 (act20)
    36, // 20 (act21)
    37, // 21 (act22)
    51, // 22 (act23)
    44, // 23 (act24)
    39, // 24 (act25)
    53, // 25 (act26)
    53, // 26 (act27)
    62, // 27 (act28)
    71, // 28 (act29)
    66, // 29 (act30)
    62, // 30 (act31)
    79  // 31 (act32)
};
  for (int i = 0; i < (int)RCPSPex.activities.size(); i++) {
    if (s.start_times[i] > optimal_37_1[i]) return false;
  }
  // std::cout<<"s";
  return true;
}

static const std::array<short, 32> optimal_37_1 = {
  0,  // 0 (act1)
  0,  // 1 (act2)
  0,  // 2 (act3)
  8,  // 3 (act4)
  27, // 4 (act5)
  2,  // 5 (act6)
  17, // 6 (act7)
  25, // 7 (act8)
  19, // 8 (act9)
  7,  // 9 (act10)
  17, // 10 (act11)
  37, // 11 (act12)
  36, // 12 (act13)
  27, // 13 (act14)
  19, // 14 (act15)
  59, // 15 (act16)
  34, // 16 (act17)
  13, // 17 (act18)
  44, // 18 (act19)
  36, // 19 (act20)
  36, // 20 (act21)
  37, // 21 (act22)
  51, // 22 (act23)
  44, // 23 (act24)
  39, // 24 (act25)
  53, // 25 (act26)
  53, // 26 (act27)
  62, // 27 (act28)
  71, // 28 (act29)
  66, // 29 (act30)
  62, // 30 (act31)
  79  // 31 (act32)
};
template<short N>
inline void RCPSP_CBS<N>::GetSuccessors(const RCPSPState_CBS<N> &nodeID,
                                      std::vector<RCPSPState_CBS<N>> &neighbors) const {
  // Bidirectional dominance: a state whose table entry was retired by a later
  // dominating state expands to nothing — its subtree is covered elsewhere.
  if (g_use_bidir && setting.use_dr5 &&
      get_cbs_dominance_table<N>().is_killed(nodeID))
    return;

  // Self-heal: states enter OPEN with their heavy conflict vectors stripped
  // (see HCost). Recompute them on demand at expansion; deterministic, so the
  // regenerated fields are identical to what was stripped.
  if (nodeID.found_conflict && nodeID.rvs_activities_pool.empty())
    nodeID.compute_h_and_RVS();

  // Child generation for one parent, shared by the normal path and the inline
  // (B&P Descendants) recursion below. Requires parent's full conflict fields.
  auto gen_children = [](const RCPSPState_CBS<N>& par, std::vector<RCPSPState_CBS<N>>& out) {
    if (setting.use_MDA_sets) {
      if (setting.use_MDA_cache) {
        if (setting.use_strong_constraints && par.is_size2_conflict) {
          short A = par.rvs_activities_pool[0];
          short B = par.rvs_activities_pool[1];
          out.emplace_back(par, A, B, par.t);
          out.emplace_back(par, B, A, par.t);
        }
        else {
          const auto& sets = get_mda_cache<N>().at(par.conflict_key);
          for (const auto& set : sets)
            out.emplace_back(par, set, par.t);
        }
      }
      else {
        for (const MDA& mda : par.conflict_solutions)
          out.emplace_back(par, mda.activities, par.t);
      }
    }
    else {
      if (par.rvs_activities_pool.empty()) return;
      std::span<const short> acts = par.rvs_activities_pool;
      for (short j = 0; j < (short)par.rvs_activities_pool.size(); j++)
        out.emplace_back(par, acts[j], par.t);
    }
  };

  gen_children(nodeID, neighbors);

  // RCPSP_INLINE: B&P's Descendants recursion. An intermediate child (same
  // RVST and scheduled set as the expanded node) is not enqueued to OPEN —
  // it is expanded in place, and only REAL descendants (or goals) are emitted.
  // Local dedup by exact start_times; depth-bounded with a sound fallback
  // (emit as a normal child) if the intermediate layer explodes.
  if (g_use_inline && (setting.use_MDA_sets ? true : !nodeID.rvs_activities_pool.empty())) {
    const CBSCutsetKey<N> ipkey = cbs_cutset_key<N>(nodeID, RCPSPex.activities);
    std::vector<RCPSPState_CBS<N>> work = std::move(neighbors);
    neighbors.clear();
    std::unordered_set<std::array<short, N>, CBSStateArrHash<N>> seen;
    size_t inlined = 0;
    const size_t INLINE_LIMIT = 4096;
    while (!work.empty()) {
      RCPSPState_CBS<N> s = std::move(work.back());
      work.pop_back();
      if (!seen.insert(s.start_times).second) continue;      // local duplicate
      if (g_use_ub && s.start_times[g_sink_id] >= g_incumbent) { ++g_ub_pruned; continue; }
      short ru = -1;
      const bool hc = s.compute_first_conflict(ru);
      if (!hc) { neighbors.push_back(std::move(s)); continue; }   // goal child
      const CBSCutsetKey<N> ck = cbs_cutset_key<N>(s, RCPSPex.activities);
      const bool real = (ck.rvst != ipkey.rvst) && !(ck.bits == ipkey.bits);
      if (real || ++inlined > INLINE_LIMIT) { neighbors.push_back(std::move(s)); continue; }
      // intermediate: expand in place (needs the branched-on conflict => full scan)
      s.compute_h_and_RVS();
      gen_children(s, work);
    }
  }
  // Bell & Park (1990) state dominance against the global table, checked ONCE per
  // generated child — their Descendants procedure, Line 8:
  //   IF New-RVST-and-Sched-Set(...) And Not(State-Dominated(...)) THEN keep
  // A dominated child is dropped outright, so it is never added to OPEN and never
  // expanded. Off unless setting.use_dr5.
  if (setting.use_dr5 || g_use_ub || g_use_leftshift) {
    // Parent's (A_s, RVST). nodeID.compute_h_and_RVS() has already run (HCost).
    const CBSCutsetKey<N> pkey = setting.use_dr5
        ? cbs_cutset_key<N>(nodeID, RCPSPex.activities) : CBSCutsetKey<N>{};
    size_t w = 0;   // compaction write index (single pass, no O(k^2) erase)
    for (size_t i = 0; i < neighbors.size(); i++) {
      bool drop = false;

      // UB pruning (B&P Descendants Line 4): propagate only pushes start times
      // forward, so every solution in this child's subtree has makespan >= the
      // child's own — and we already hold a feasible schedule at g_incumbent.
      if (g_use_ub && neighbors[i].start_times[g_sink_id] >= g_incumbent) {
        drop = true; ++g_ub_pruned;
      }

      if (!drop) {
        // LIGHT scan only: the dominance key needs just t_first + the earliest
        // conflict pool + start_times. The full compute_h_and_RVS (scoring, MDA,
        // heuristic) runs later — via HCost, and only for children A* actually
        // keeps as new nodes. Duplicates that A* will discard never pay for it.
        short res_unused = -1;
        const bool has_conflict = neighbors[i].compute_first_conflict(res_unused);
        if (!has_conflict) {
          // conflict-free child = feasible schedule: tighten the incumbent,
          // and keep the child (it is a goal for A*) — never prune it.
          if (g_use_ub)
            g_incumbent = std::min(g_incumbent, neighbors[i].start_times[g_sink_id]);
        } else if (g_use_leftshift && neighbors[i].left_shift_prunable()) {
          // Persistent precedence-idle gap with a feasible shift: no descendant
          // is an active schedule; a sibling branch holds the active optimum.
          drop = true;
        } else if (setting.use_dr5) {
          const CBSCutsetKey<N> ckey = cbs_cutset_key<N>(neighbors[i], RCPSPex.activities);
          // Descendants Line 8: only a child whose RVST AND scheduled set both differ
          // from the parent's is a real state to remember and compare. Anything else is
          // an intermediate state — skip it entirely, or the parent (always <= its own
          // child) would dominate it and wipe out the subtree.
          const bool is_real_descendant = (ckey.rvst != pkey.rvst) && !(ckey.bits == pkey.bits);
          if (is_real_descendant)
            drop = get_cbs_dominance_table<N>().check_and_insert(neighbors[i], RCPSPex.activities);
        }
      }
      if (!drop) {
        if (w != i) neighbors[w] = std::move(neighbors[i]);
        w++;
      }
    }
    neighbors.resize(w);
  }

if (setting.use_dominance){
    // Dominance pruning
    auto dominates = [](const RCPSPState_CBS<N>& a, const RCPSPState_CBS<N>& b) {
        // a dominates b if all start times of a <= b
        if (setting.use_first_conflict) {
            // Bell & Park: only compare unscheduled activities
            // scheduled = finish before RVST
            if (a.t != b.t) return false;
            for (int i = 0; i < (int)RCPSPex.activities.size(); i++) {
                short finish = a.start_times[i] + RCPSPex.activities[i].duration;
                if (finish <= a.t) continue; // scheduled — skip
                if (a.start_times[i] > b.start_times[i]) return false;
            }
        } else {
            // General: compare all start times
            for (int i = 0; i < (int)RCPSPex.activities.size(); i++)
                if (a.start_times[i] > b.start_times[i]) return false;
        }
        return true;
    };

    // Remove dominated neighbors
    for (int i = 0; i < (int)neighbors.size(); i++) {
        bool dominated = false;
        for (int j = 0; j < (int)neighbors.size(); j++) {
            if (i == j) continue;
            if (dominates(neighbors[j], neighbors[i])) {
                dominated = true;
                break;
            }
        }
        if (dominated) {
            neighbors.erase(neighbors.begin() + i);
            i--;
        }
    }
  }


  // if (satisfies_optimal(nodeID)) {
  //   bool any_child_satisfies = false;
  //   int satisfying_child = -1;
  //   for (int c = 0; c < (int)neighbors.size(); c++) {
  //     if (satisfies_optimal(neighbors[c])) {
  //       any_child_satisfies = true;
  //       satisfying_child = c;
  //       break;
  //     }
  //   }
  //   if (!any_child_satisfies) {
  //     std::cout << "=== LEMMA 2 VIOLATED ===" << std::endl;
  //     std::cout << "parent makespan=" << nodeID.makespan() << std::endl;
  //     std::cout << "conflict t=" << nodeID.t << " r=" << nodeID.resourceType << std::endl;
  //     std::cout << "pool: ";
  //     for (short a : nodeID.rvs_activities_pool)
  //       std::cout << a << "(s=" << nodeID.start_times[a]
  //                 << ",f=" << nodeID.start_times[a]+RCPSPex.activities[a].duration << ") ";
  //     std::cout << std::endl;
  //     std::cout << "children differences from optimal:" << std::endl;
  //     for (int c = 0; c < (int)neighbors.size(); c++) {
  //       std::cout << "child " << c << " makespan=" << neighbors[c].makespan() << ": ";
  //       for (int i = 0; i < (int)RCPSPex.activities.size(); i++) {
  //         if (neighbors[c].start_times[i] > optimal_37_1[i])
  //           std::cout << "act" << i << "(" << neighbors[c].start_times[i]
  //                     << ">" << optimal_37_1[i] << ") ";
  //       }
  //       std::cout << std::endl;
  //     }
  //   }
    // else {
    //   std::cout << "Lemma 2 OK: parent_makespan=" << nodeID.makespan()
    //             << " satisfying_child_makespan=" << neighbors[satisfying_child].makespan()
    //             << std::endl;
    // }
  // }

  // Lean mode: strip the (now-consumed) heavy vectors from the expanded
  // node's CLOSED copy as well; the self-heal at the top regenerates them if
  // this node is ever re-expanded.
  if (g_use_lean) {
    nodeID.rvs_activities_pool.clear();
    nodeID.first_conflict_pool.clear();
    nodeID.conflict_solutions.clear();
  }
}
template<short N>
inline bool RCPSP_CBS<N>::GoalTest(const RCPSPState_CBS<N> &node, const RCPSPState_CBS<N> &goal) const {
  return !node.found_conflict;
}
template<short N>
inline double RCPSP_CBS<N>::HCost(const RCPSPState_CBS<N> &state1, const RCPSPState_CBS<N> &state2) const {
  // std::cout << "H: ";
  // return .0;
  // compute_h_and_RVS is expensive and state-deterministic; cache its result so
  // repeat HCost calls on the same state object (or copies of it) are free.
  if (state1.h_cached) return state1.h_cache;
  const double h = state1.compute_h_and_RVS();//return h_cost
  state1.h_cache  = h;
  state1.h_cached = true;
  // RCPSP_LEAN=1: strip the heavy per-conflict vectors before this state is
  // copied into OPEN (TemplateAStar calls HCost right before AddOpenNode).
  // GetSuccessors self-heals by recomputing at expansion; fields are state-
  // deterministic, so the search is unchanged. Off by default: costs ~15-30%
  // time for a modest memory cut (-4% cfg1, ~-23%/node on MDA-heavy configs).
  if (g_use_lean) {
    state1.rvs_activities_pool.clear();
    state1.first_conflict_pool.clear();
    state1.conflict_solutions.clear();
  }

  // NOTE: the Bell & Park dominance check used to live here. It was moved to
  // GetSuccessors (see below) because TemplateAStar calls HCost an unpredictable
  // number of times per node, and the check both reads AND writes the table — so
  // table contents depended on A*'s evaluation order rather than on which states
  // were actually generated. B&P check dominance once per generated child, in
  // their Descendants procedure; GetSuccessors is the faithful place for it.
  return h;

  // return state1.h_cost;
}
template<short N>
  inline double RCPSP_CBS<N>::GCost(const RCPSPState_CBS<N> &state1, const RCPSPState_CBS<N> &state2) const {
  // double g = state2.start_times[g_sink_id] - state1.start_times[g_sink_id];
  // if (g < 0) {
  //   std::cout << "NEGATIVE GCOST: " << g
  //             << " parent_makespan=" << state1.start_times[g_sink_id]
  //             << " child_makespan=" << state2.start_times[g_sink_id] << std::endl;
  // }
  // return g;
    return state2.start_times[g_sink_id] - state1.start_times[g_sink_id];
  }
// template<short N>
// inline uint64_t RCPSP_CBS<N>::GetStateHash(const RCPSPState_CBS<N> &node) const {
//   std::size_t seed = 0;
//   for (const auto& st : node.start_times) {
//     seed ^= std::hash<short>{}(st) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
//   }
//   // for (const auto& [f, t] : node.added_precedences) {
//   //   seed ^= std::hash<short>{}(f) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
//   //   seed ^= std::hash<short>{}(t) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
//   // }
//   return seed;
// }
template<short N>
inline uint64_t RCPSP_CBS<N>::GetStateHash(const RCPSPState_CBS<N> &node) const {
  // FNV-1a 64-bit over the raw bytes of start_times.
  //
  // The previous Boost hash_combine used std::hash<short>(x)=x (identity), so
  // every state began with the same seed (start_times[0] is always 0 for the
  // source activity).  For CBS states that differ in only a few values, the
  // resulting hashes had far too many collisions.
  //
  // FNV-1a processes every byte with a multiply-xor, giving strong avalanche
  // across all 64 bits even for small/structured integer arrays.
  constexpr uint64_t FNV_OFFSET = 14695981039346656037ULL;
  constexpr uint64_t FNV_PRIME  =  1099511628211ULL;

  uint64_t h = FNV_OFFSET;
  const uint8_t* p = reinterpret_cast<const uint8_t*>(node.start_times.data());
  for (size_t i = 0; i < N * sizeof(short); ++i) {
    h ^= static_cast<uint64_t>(p[i]);
    h *= FNV_PRIME;
  }
  return h;
}

template<short N>
inline int RCPSP_CBS<N>::GetAction(const RCPSPState_CBS<N> &nodeID, const RCPSPState_CBS<N> &nodeID2) const {
  return SearchEnvironment<RCPSPState_CBS<N>, int>::GetAction(nodeID, nodeID2);
}
template<short N>
inline int RCPSP_CBS<N>::GetNumSuccessors(const RCPSPState_CBS<N> &stateID) const {
  return SearchEnvironment<RCPSPState_CBS<N>, int>::GetNumSuccessors(stateID);
}
template<short N>
inline void RCPSP_CBS<N>::GetActions(const RCPSPState_CBS<N> &nodeID, std::vector<int> &actions) const {
  return;
}
template<short N>
inline void RCPSP_CBS<N>::ApplyAction(RCPSPState_CBS<N> &s, int a) const {
return;
}
template<short N>
inline uint64_t RCPSP_CBS<N>::GetActionHash(int act) const {
  return 0;
}


template<short N>
inline bool RCPSP_CBS<N>::InvertAction(int &a) const {
  return true;

}
template<short N>
inline std::vector<RCPSPState_CBS<N>> RCPSP_CBS<N>::GetSuccessors(const RCPSPState_CBS<N> &nodeID) const {
  std::vector<RCPSPState_CBS<N>> neighbors;
  return neighbors;

}
template<short N>
inline double RCPSP_CBS<N>::GCost(const RCPSPState_CBS<N> &node, const int &act) const {
  return 0;;
}

class RCPSP_BAP : public SearchEnvironment<RCPSPState_BAP,int>{
public:
  RCPSP_BAP();
  void GetSuccessors(const RCPSPState_BAP &nodeID, std::vector<RCPSPState_BAP> &neighbors) const override;
  bool GoalTest(const RCPSPState_BAP &node, const RCPSPState_BAP &goal) const override;
  double HCost(const RCPSPState_BAP &state1, const RCPSPState_BAP &state2) const override;
  double GCost(const RCPSPState_BAP &state1, const RCPSPState_BAP &state2) const override;

  int GetAction(const RCPSPState_BAP &nodeID, const RCPSPState_BAP &nodeID2) const override;
  int GetNumSuccessors(const RCPSPState_BAP &stateID) const;
  void GetActions(const RCPSPState_BAP &nodeID, std::vector<int> &actions) const override;
  void ApplyAction(RCPSPState_BAP &s, int a) const override;
  uint64_t GetActionHash(int act) const;
  uint64_t GetStateHash(const RCPSPState_BAP &node) const;
  bool InvertAction(int &a) const;
  std::vector<RCPSPState_BAP> GetSuccessors(const RCPSPState_BAP &nodeID) const;
  double GCost(const RCPSPState_BAP &node, const int &act) const override;

};

inline RCPSP_BAP::RCPSP_BAP() {
}

inline void RCPSP_BAP::GetSuccessors(const RCPSPState_BAP &nodeID, std::vector<RCPSPState_BAP> &neighbors) const {

  if (nodeID.rvs_activities_pool.empty()) return;

  const short* acts = nodeID.rvs_activities_pool.data();
  for (short j = 0; j < nodeID.num_activities; j++) {
    neighbors.emplace_back(nodeID, acts[j], nodeID.t);
  }
}
inline bool RCPSP_BAP::GoalTest(const RCPSPState_BAP &node, const RCPSPState_BAP &goal) const {
  if (node.rvs_activities_pool.size()==0) {
    return true;
  }
  else {
    return false;
  }
}

inline double RCPSP_BAP::HCost(const RCPSPState_BAP &state1, const RCPSPState_BAP &state2) const {
  return 0;
}
inline double RCPSP_BAP::GCost(const RCPSPState_BAP &state1, const RCPSPState_BAP &state2) const {
  return state2.start_times[g_sink_id] - state1.start_times[g_sink_id];
}
inline uint64_t RCPSP_BAP::GetStateHash(const RCPSPState_BAP &node) const {
  std::size_t seed = 0;

  for (const auto& st : node.start_times) {
    seed ^= std::hash<short>{}(st) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
  }

  return seed;
}



inline int RCPSP_BAP::GetAction(const RCPSPState_BAP &nodeID, const RCPSPState_BAP &nodeID2) const {
  return SearchEnvironment<RCPSPState_BAP, int>::GetAction(nodeID, nodeID2);
}

inline int RCPSP_BAP::GetNumSuccessors(const RCPSPState_BAP &stateID) const {
  return SearchEnvironment<RCPSPState_BAP, int>::GetNumSuccessors(stateID);
}

inline void RCPSP_BAP::GetActions(const RCPSPState_BAP &nodeID, std::vector<int> &actions) const {
  return;
}

inline void RCPSP_BAP::ApplyAction(RCPSPState_BAP &s, int a) const {
return;
}

inline uint64_t RCPSP_BAP::GetActionHash(int act) const {
  return 0;
}



inline bool RCPSP_BAP::InvertAction(int &a) const {
  return true;

}

inline std::vector<RCPSPState_BAP> RCPSP_BAP::GetSuccessors(const RCPSPState_BAP &nodeID) const {
  std::vector<RCPSPState_BAP> neighbors;
  return neighbors;

}

inline double RCPSP_BAP::GCost(const RCPSPState_BAP &node, const int &act) const {
  return 0;;
}


template<short N>
class oldRCPSP : public SearchEnvironment<oldRCPSPState<N>,int>{
public:
  oldRCPSP();
  void GetSuccessors(const oldRCPSPState<N> &nodeID, std::vector<oldRCPSPState<N>> &neighbors) const override;
  bool GoalTest(const oldRCPSPState<N> &node, const oldRCPSPState<N> &goal) const override;
  double HCost(const oldRCPSPState<N> &state1, const oldRCPSPState<N> &state2) const override;
  double GCost(const oldRCPSPState<N> &state1, const oldRCPSPState<N> &state2) const override;

  int GetAction(const oldRCPSPState<N> &nodeID, const oldRCPSPState<N> &nodeID2) const override;
  int GetNumSuccessors(const oldRCPSPState<N> &stateID) const;
  void GetActions(const oldRCPSPState<N> &nodeID, std::vector<int> &actions) const override;
  void ApplyAction(oldRCPSPState<N> &s, int a) const override;
  uint64_t GetActionHash(int act) const;
  uint64_t GetStateHash(const oldRCPSPState<N> &node) const;
  bool InvertAction(int &a) const;
  std::vector<oldRCPSPState<N>> GetSuccessors(const oldRCPSPState<N> &nodeID) const;
  double GCost(const oldRCPSPState<N> &node, const int &act) const override;
};

template<short N>
oldRCPSP<N>::oldRCPSP() {
}

template<short N>
inline void oldRCPSP<N>::GetSuccessors(const oldRCPSPState<N> &nodeID,
std::vector<oldRCPSPState<N>> &neighbors) const {
  std::vector<short> available = getAvailableActivities(nodeID);
  int n = available.size();

  // Calculate current resource usage from active transitions
  std::map<std::string, short> baseUsage;
  for (auto& [actIdx, remaining] : nodeID.activeTransitionIndices) {
    for (auto& [resource, demand] : RCPSPex.activities[actIdx].resource_demands) {
      baseUsage[resource] += demand;
    }
  }

  // Iterate over all subsets including empty set
  for (int mask = 0; mask < (1 << n); mask++) {
    std::map<std::string, short> subsetUsage = baseUsage;
    std::vector<short> subset;
    bool valid = true;

    for (int i = 0; i < n; i++) {
      if (mask & (1 << i)) {
        short actIdx = available[i];
        // Add this activity's resource demand
        for (auto& [resource, demand] :
             RCPSPex.activities[actIdx].resource_demands) {
          subsetUsage[resource] += demand;
             }
        // Check validity
        for (auto& [resource, capacity] : RCPSPex.resources) {
          if (subsetUsage[resource] > capacity) {
            valid = false;
            break;
          }
        }
        if (!valid) break;
        subset.push_back(actIdx);
      }
    }

    if (valid) {
      // Create successor state with this subset
      neighbors.emplace_back(oldRCPSPState<N>(nodeID, subset, count));
    }

  }
}
template<short N>
inline bool oldRCPSP<N>::GoalTest(const oldRCPSPState<N> &node, const oldRCPSPState<N> &goal) const {
  // Goal is reached when the dummy finish activity has been completed
  short finalID = RCPSPex.activity_len - 1;
  return node.finishedActivitiys[finalID] != -1;
}
template<short N>
inline double oldRCPSP<N>::HCost(const oldRCPSPState<N> &state1, const oldRCPSPState<N> &state2) const {
  std::vector<short> tempUnstarted;

  // Optimization: Reserve max possible size to prevent re-allocations
  // (Using the size logic from your original code)
  tempUnstarted.reserve(RCPSPex.activities.size());

  // YOUR ORIGINAL LOGIC: Loop i from 1 to size, use ID = i + 1
  // for (int i = 0; i <state1.finishedActivitiys.size(); i++) {
  //   short taskID = i+1; // Preserving your 1-based logic
  //
  //   // THE FIX: Direct vector access (O(1) speed)
  //   // Check if value is -1 (meaning "not finished")
  //   if (state1.finishedActivitiys[taskID] == -1) {
  //     tempUnstarted.push_back(taskID);
  //   }
  // }
  for (int i = 0; i < RCPSPex.activities.size(); i++) {
    if (state1.finishedActivitiys[i] == -1) {
      tempUnstarted.push_back(i+1); // 0-based
    }
  }
  // NOTE: You must update the definition of getForwardHcost
  // to accept 'const std::vector<int>&' instead of 'std::map...'
  // short h = getForwardHcost(tempUnstarted, state1.activeTransitionIndices, state1.finishedActivitiys);
  // // short h = getForwardHcost(tempUnstarted, state1.activeTransitionIndices);
  // return h;
  //   std::vector<short> tempUnfinished;


//   tempUnfinished.reserve(RCPSPex.activities.size());
//   tempUnfinished
return getForwardHcost0Based(tempUnstarted, state1.activeTransitionIndices);
// return getForwardHcost(tempUnstarted, state1.activeTransitionIndices);
   return std::max(
       getForwardHcost(tempUnstarted, state1.activeTransitionIndices),
       getforwardResource(tempUnstarted, state1.activeTransitionIndices)
   );
}

template<short N>
  inline double oldRCPSP<N>::GCost(const oldRCPSPState<N> &state1, const oldRCPSPState<N> &state2) const {
    return state2.g - state1.g;
  }
template<short N>
inline uint64_t oldRCPSP<N>::GetStateHash(const oldRCPSPState<N> &node) const {
  std::size_t seed = 0;
  for (const auto& entry : node.activeTransitionIndices) {
    // Hash the ID
    seed ^= std::hash<short>{}(entry.first) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    // Hash the Start Time
    seed ^= std::hash<short>{}(entry.second) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
  }

  for (int id = 0; id < node.startedActivitiys.size(); ++id) {
    int time = node.startedActivitiys[id];

    // Only hash if the activity exists
    // if (time != -1) {
      // Hash the ID (formerly pair.first)
      seed ^= std::hash<int>{}(id) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
      // Hash the Time (formerly pair.second)
      seed ^= std::hash<int>{}(time) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    // }
  }
  for (int id = 0; id < node.finishedActivitiys.size(); ++id) {
    int time = node.finishedActivitiys[id];

    // Only hash if the activity exists
    // if (time != -1) {
      // Hash the ID (formerly pair.first)
      seed ^= std::hash<int>{}(id) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
      // Hash the Time (formerly pair.second)
      seed ^= std::hash<int>{}(time) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    // }
  }
  return seed;
}


template<short N>
inline int oldRCPSP<N>::GetAction(const oldRCPSPState<N> &nodeID, const oldRCPSPState<N> &nodeID2) const {
  return SearchEnvironment<oldRCPSPState<N>, int>::GetAction(nodeID, nodeID2);
}
template<short N>
inline int oldRCPSP<N>::GetNumSuccessors(const oldRCPSPState<N> &stateID) const {
  return SearchEnvironment<oldRCPSPState<N>, int>::GetNumSuccessors(stateID);
}
template<short N>
inline void oldRCPSP<N>::GetActions(const oldRCPSPState<N> &nodeID, std::vector<int> &actions) const {
  return;
}
template<short N>
inline void oldRCPSP<N>::ApplyAction(oldRCPSPState<N> &s, int a) const {
return;
}
template<short N>
inline uint64_t oldRCPSP<N>::GetActionHash(int act) const {
  return 0;
}


template<short N>
inline bool oldRCPSP<N>::InvertAction(int &a) const {
  return true;

}
template<short N>
inline std::vector<oldRCPSPState<N>> oldRCPSP<N>::GetSuccessors(const oldRCPSPState<N> &nodeID) const {
  std::vector<oldRCPSPState<N>> neighbors;
  return neighbors;

}
template<short N>
inline double oldRCPSP<N>::GCost(const oldRCPSPState<N> &node, const int &act) const {
  return 0;;
}



#endif //RCPSP_H
//
// Created by idolu on 06/01/2025.
//
