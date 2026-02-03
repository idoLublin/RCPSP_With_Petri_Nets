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
    state1.h = getForwardHcost(tempUnstarted, state1.activeTransitionIndices);
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

class RCPSP_BiGreedy : public SearchEnvironment<RCPSPState_Bi, action> {
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

  inline void GetSuccessors(const RCPSPState_Bi &nodeID, std::vector<RCPSPState_Bi> &neighbors) const override {
    neighbors.clear();

    std::cout << "GetSuccessors: direction=" << nodeID.direction
              << ", g=" << nodeID.g << std::endl;

    if (nodeID.direction) {
        // FORWARD
        std::vector<short> tempUnstarted;
        tempUnstarted.reserve(petri.Transitions.size());

        for (int i = 0; i < petri.Transitions.size(); i++) {
            short taskID = i + 1;
            if (nodeID.finishedActivitiys[taskID] == -1) {
                tempUnstarted.push_back(taskID);
            }
        }

        std::cout << "  Forward: Unstarted=" << tempUnstarted.size() << std::endl;

    //     auto availableTransitionIndices =
    //         getAvailableTransitionIndices_TT(tempUnstarted, nodeID.finishedActivitiys,
    //                                          nodeID.resource_nodes, nodeID.activity_nodes);
    //
    //     std::cout << "  Forward: Available=" << availableTransitionIndices.size() << std::endl;
    //
    //     for (const auto& [transId, firingTime] : availableTransitionIndices) {
    //         neighbors.emplace_back(RCPSPState_Bi(nodeID, transId, firingTime));
    //     }
    //
    // } else {
    //     // BACKWARD
    //     std::vector<short> tempFinished;
    //     tempFinished.reserve(petri.Transitions.size());
    //
    //     for (int i = 0; i < petri.Transitions.size(); i++) {
    //         short taskID = i + 1;
    //         if (nodeID.finishedActivitiys[taskID] == -1) {
    //             tempFinished.push_back(taskID);
    //         }
    //     }
        //
        // std::cout << "  Backward: Finished=" << tempFinished.size() << std::endl;
        //
        // auto unfireableTransitions =
        //     getUnfireableTransitionIndices(tempFinished, nodeID.finishedActivitiys,
        //                                   nodeID.resource_nodes, nodeID.activity_nodes);
    //
    //     std::cout << "  Backward: Unfireable=" << unfireableTransitions.size() << std::endl;
    //
    //     for (const auto& [transId, firingTime] : unfireableTransitions) {
    //         neighbors.emplace_back(RCPSPState_Bi(nodeID, transId, firingTime));
    //     }
     }

   // std::cout << "  Generated " << neighbors.size() << " neighbors" << std::endl;
}
   inline bool GoalTest(const RCPSPState_Bi &node, const RCPSPState_Bi &goal) const override {
  return (node == goal);
  }

  inline double HCost(const RCPSPState_Bi &state1, const RCPSPState_Bi &state2) const override {
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
   inline double GCost(const RCPSPState_Bi &state1, const RCPSPState_Bi &state2) const override {
return std::abs(state2.g - state1.g);
  // Track actual transition cost
   }
  inline void GetActions(const RCPSPState_Bi &state, std::vector<action> &actions) const override {
    // Not used in BidirectionalGreedyBestFirst, but must be implemented
    return;
  }
  virtual action GetAction(const RCPSPState_Bi &state1, const RCPSPState_Bi &state2) const override {

    return static_cast<action>(0);
  }
  inline void ApplyAction(RCPSPState_Bi &state, action action) const override {
    // Not used, but required for abstract class
  }

  inline void UndoAction(RCPSPState_Bi &state, action action) const override {
    // Not needed for bidirectional search, but required
  }
double GCost(const RCPSPState_Bi &node, const action &act) const override {
return node.g;
  };
  bool InvertAction(action& a) const override {
    return false; // Replace with appropriate logic
  }
  uint64_t GetActionHash(action act) const override {
    return 0;
  };


  uint64_t GetStateHash(const RCPSPState_Bi &node) const {
    size_t h = 0;

    // Hash which activities are finished
    for (int i = 0; i < 128; i++) {
      if (node.finishedActivitiys[i] != -1) {
        h ^= std::hash<int>{}(i) + 0x9e3779b9 + (h << 6) + (h >> 2);
      }
    }


    // Hash activity node token counts
    // for (int i = 0; i < node.activity_nodes.size(); i++) {
    //   if (node.activity_nodes[i].first > 0) {
    //     h ^= std::hash<int>{}(i * 1000 + node.activity_nodes[i].first);
    //   }
    // }

    return h;
    // //auto startS1 = std::chrono::high_resolution_clock::now();
    // std::size_t seed = 0;
    // // Hash the marking (Petri net state)
    //
    // // Hash the finishedActivitiys vector
    // for (const auto& activity : node.finishedActivitiys) {
    //   seed ^= std::hash<int>{}(activity) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    // }
    //
    // // auto endS1 = std::chrono::high_resolution_clock::now();
    // // hashTIME += endS1 - startS1;
    // return seed;

    // Hash the finished activities
    // for (const auto& pair : node.finishedActivitiys) {
    //   seed ^= std::hash<int>{}(pair.first) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    //   seed ^= std::hash<int>{}(pair.second) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    // }

    // Distinguish forward vs. backward search by modifying the seed
    //seed ^= (GetExpandForward ? 0xAAAAAAAAAAAAAAAA : 0x5555555555555555);


  }

};

class ForwardRCPSPHeuristic : public Heuristic<RCPSPState_Bi> {
public:
  double HCost(const RCPSPState_Bi &current, const RCPSPState_Bi &goal) const override {
std::vector<short> tempUnstarted;

  // Optimization: Reserve max possible size to prevent re-allocations
  // (Using the size logic from your original code)
  // Optimization: Pre-allocate memory to avoid reallocations during push_back
  tempUnstarted.reserve(petri.Transitions.size());

  for (int i = 0; i < petri.Transitions.size(); i++) {
    short taskID = i + 1;

    // THE FIX: Direct Vector Access (O(1))
    // Instead of map.find(), check if the value at this index is -1.
    // -1 indicates the task has not finished yet.
    if (current.finishedActivitiys[taskID] == -1) {
      tempUnstarted.push_back(taskID);
    }
  }


int lastActivityId = -1;
  int maxTime = -1;

  // Find last finished activity by ID instead of name
  for (int id = 0; id < current.finishedActivitiys.size(); ++id) {
    int time = current.finishedActivitiys[id];

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

      if (RCPSPex.deep_dependencies.find({lastActivityName, actName}) == RCPSPex.deep_dependencies.end())
        {
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
    for (int id = 0; id < current.finishedActivitiys.size(); ++id) {
      int finishTime = current.finishedActivitiys[id];

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

    int unkTime = current.g - latestStart;

    // FIX: Copy the vector (std::vector copy is deep by default)
    std::array<short, 128> finishedActivitiysnew = current.finishedActivitiys;

    for (int actIdx : independentSet) {
      // FIX: Direct index access
      finishedActivitiysnew[actIdx] = 0;
    }

    // return std::max(getForwardHcost_TT(tempUnstarted, state1.finishedActivitiys) - unkTime,
    //            getForwardHcost_TT(newUnstartedTransitions, finishedActivitiysnew));
    return std::max(getForwardHcost_TT(tempUnstarted) - unkTime,
           getForwardHcost_TT(newUnstartedTransitions));
  } else {
    // Fallback if no finished activities
    return getForwardHcost_TT(tempUnstarted);
    // return getForwardHcost_TT(tempUnstarted, state1.finishedActivitiys);
  }  }
};

class BackwardRCPSPHeuristic : public Heuristic<RCPSPState_Bi> {
public:
  double HCost(const RCPSPState_Bi &current, const RCPSPState_Bi &start) const override {
    std::vector<short> finishedTasks;
    finishedTasks.reserve(petri.Transitions.size());

    // Collect activities that ARE finished in current state
    for (int i = 1; i <= petri.Transitions.size(); i++) {
      if (current.finishedActivitiys[i] != -1) {
        finishedTasks.push_back(i);
      }
    }

    if (finishedTasks.empty()) return 0;

    return getBackwardHcost_TT(finishedTasks);
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
// inline bool RCPSP_TT::GetNextSuccessor(const RCPSPState_TT &curr, const RCPSPState_TT &goal,
//                       RCPSPState_TT &next, double parentH,
//                       uint64_t &special, bool &validMove) const
// {
//   std::vector<short> tempUnstarted;
//   tempUnstarted.reserve(petri.Transitions.size());
//
//   for (int i = 0; i < petri.Transitions.size(); i++) {
//     short taskID = i + 1;
//     if (curr.finishedActivitiys[taskID] == -1) {
//       tempUnstarted.push_back(taskID);
//     }
//   }
//
//   std::vector<std::pair<short, short>> avilableTransitionIndices =
//     getAvailableTransitionIndices_TT(tempUnstarted, curr.finishedActivitiys,
//                                       curr.resource_nodes, curr.activity_nodes);
// if (curr.g+parentH>63) {
//   int asd;
//   asd++;
// }
//   // std::sort(avilableTransitionIndices.begin(), avilableTransitionIndices.end(),
//   //     [](const std::pair<short, short>& a, const std::pair<short, short>& b) {
//   //         return a.second < b.second; // Try smallest firing times first?
//   //     });
//   unsigned int index = (unsigned int)special;
//
//  //  if (special == 0) {
//  // //   std::cout << "NEW NODE: " << avilableTransitionIndices.size() << " successors available" << std::endl;
//  //  }
//
//   if (index >= avilableTransitionIndices.size()) {
//   //  std::cout << "  CLOSING after generating " << special << " successors" << std::endl;
//     validMove = false;
//     return false;
//   }
//
//   //std::cout << "  Generating successor " << (special+1) << "/" << avilableTransitionIndices.size() << std::endl;
//
//   static int nodeExpansionCount = 0;
//   static std::map<int, int> nodeSuccessors; // Track how many successors each node should have
//
//   if (special == 0) {
//     nodeExpansionCount++;
//     nodeSuccessors[nodeExpansionCount] = avilableTransitionIndices.size();
//     std::cout << "Node " << nodeExpansionCount << ": Starting expansion, "
//               << avilableTransitionIndices.size() << " successors expected" << std::endl;
//   }
//
//   //unsigned int index = (unsigned int)special;
//
//   if (index >= avilableTransitionIndices.size()) {
//     // Node is fully expanded
//     std::cout << "Node " << nodeExpansionCount << ": CLOSED after generating "
//               << special << "/" << nodeSuccessors[nodeExpansionCount]
//               << " successors" << std::endl;
//
//     if (special != nodeSuccessors[nodeExpansionCount]) {
//       std::cout << "❌ BUG! Expected " << nodeSuccessors[nodeExpansionCount]
//                 << " but only generated " << special << std::endl;
//     }
//
//     validMove = false;
//     return false;
//   }
//
//   std::cout << "  Generating successor " << (special+1) << "/"
//             << avilableTransitionIndices.size()
//             << ": (" << avilableTransitionIndices[index].first
//             << "," << avilableTransitionIndices[index].second << ")" << std::endl;
//
//   std::pair<short, short> selectedMove = avilableTransitionIndices[index];
//   next = RCPSPState_TT(curr, selectedMove.first, selectedMove.second);
//
//   validMove = true;
//   special++;
//
//   return (index + 1 < avilableTransitionIndices.size());
// }
//

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
    neighbors.emplace_back(RCPSPState_TT2(nodeID, transId, Timedelta));
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
  if (state1.isDeltaZero) {
    state1.h = state1.predessesor_h;  // FIX: = not ==
    return state1.h;
  }

  std::vector<short> tempUnstarted;
  tempUnstarted.reserve(petri.Transitions.size());

  for (int i = 0; i < petri.Transitions.size(); i++) {
    short taskID = i + 1;
    if (state1.finishedActivitiys[taskID] == 0) {
      tempUnstarted.push_back(taskID);
    }
  }

  state1.h = getForwardHcost_TT2(tempUnstarted,state1.activity_nodes,state1.activeTransitionIndices);  // ← Pass active list

  return state1.h;
}
inline double RCPSP_TT2::GCost(const RCPSPState_TT2 &state1, const RCPSPState_TT2 &state2) const {
  return state2.g-state1.g;//+state1.g
}
inline uint64_t RCPSP_TT2::GetStateHash(const RCPSPState_TT2 &node) const {
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
  for (const auto& active : node.activeTransitionIndices) {
    // Hash the Task ID
    seed ^= std::hash<int>{}(active.first) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    // Hash the Remaining Time (Crucial!)
    seed ^= std::hash<int>{}(active.second) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
  }

  return seed;
}

inline bool RCPSP_TT2::GetNextSuccessor(const RCPSPState_TT2 &curr, const RCPSPState_TT2 &goal,
                      RCPSPState_TT2 &next, double parentH,
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
      if (curr.finishedActivitiys[taskID] == 0) {
        tempUnstarted.push_back(taskID);
      }
    }

    avilableTransitionIndices = getAvailableTransitionIndices_TT2(tempUnstarted, curr.finishedActivitiys,
                                                                  curr.resource_nodes, curr.activity_nodes,curr.activeTransitionIndices);

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
  next = RCPSPState_TT2(curr, selectedMove.first, selectedMove.second);

  // Mark as valid and increment index
  validMove = true;
  special++;

  // Return true if there are more moves in the list
  return (index + 1 < avilableTransitionIndices.size());
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




#endif //RCPSP_H
//
// Created by idolu on 06/01/2025.
//
