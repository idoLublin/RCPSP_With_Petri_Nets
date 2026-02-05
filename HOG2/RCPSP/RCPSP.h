//
// Created by idolu on 29/12/2024.
//
#ifndef RCPSP_H
#define RCPSP_H
//#include "../algorithms/OldSearchEnvironment.h"
#include "../search/SearchEnvironment.h"
#include "RCPSPState.h"
#include "HeuristicTypes.h"
#include "../utils//GLUtil.h"
#include <functional>

using namespace P_RCPSP;

// Forward declarations for DP-based heuristic functions
double getForwardHcostDP(const std::vector<short>& unstartedTransitions,
                         const std::vector<std::pair<short, short>>& activeTransitionIndices);
double getForwardHcostDP_TT(const std::vector<short>& unstartedTransitions);
void initializeHeuristicDP();
extern thread_local bool heuristicDPInitialized;

// Forward declarations for non-DP heuristic functions
double getForwardHcost(std::vector<short> unstartedTransitions,
                       std::vector<std::pair<short, short>> activeTransitionIndices);
double getForwardHcost_TT(std::vector<short> unstartedTransitions);

// Forward declarations for LBCS heuristic
double computeLBCS(const std::vector<short>& unstartedTransitions,
                   const std::vector<std::pair<short, short>>& activeTransitionIndices);
double computeLBCS_TT(const std::vector<short>& unstartedTransitions);

// Global DP toggle (defined in Driver.cpp)
extern bool useDPHeuristic;

// Global heuristic type (defined in Driver.cpp)
extern P_RCPSP::HeuristicType activeHeuristic;

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

  // 1. Hash Started Activities
  for (int id = 0; id < node.startedActivitiys.size(); ++id) {
    int time = node.startedActivitiys[id];

    // Only hash if the activity exists (equivalent to iterating the map)
    if (time != -1) {
      // Hash the ID (formerly pair.first)
      seed ^= std::hash<int>{}(id) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
      // Hash the Time (formerly pair.second)
      seed ^= std::hash<int>{}(time) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    }
  }

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

    // Heuristic calculation based on selected heuristic type
    if (activeHeuristic == P_RCPSP::HeuristicType::LBCS) {
      state1.h = computeLBCS(tempUnstarted, state1.activeTransitionIndices);
    } else if (useDPHeuristic) {
      state1.h = getForwardHcostDP(tempUnstarted, state1.activeTransitionIndices);
    } else {
      state1.h = getForwardHcost(tempUnstarted, state1.activeTransitionIndices);
    }
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

class RCPSP_BiGreedy : public SearchEnvironment<RCPSPState_bi, action> {
public:

inline void GetSuccessors(const RCPSPState_bi &nodeID, std::vector<RCPSPState_bi> &neighbors) const override {
neighbors.clear();
  if (GetExpandForward) {
    if (!nodeID.activeTransitionIndices.empty()) {
      count++;
      int t = 0;
      // Find the transition with the minimum remaining duration
      for (int i = 0; i < nodeID.activeTransitionIndices.size(); i++) {
        if (nodeID.activeTransitionIndices[i].second < nodeID.activeTransitionIndices[t].second) {
          t = i;
        }
      }

      // Get the actual transition object using the index
      int transitionIdx = nodeID.activeTransitionIndices[t].first;
      Transition transition = petri.Transitions[transitionIdx - 1];
      transition.duration= nodeID.activeTransitionIndices[t].second;

      // Create successor state
      neighbors.emplace_back(RCPSPState_bi(nodeID, transition, false, t, count));
    }

    // Handle available transitions
    for (int i = 0; i < nodeID.avilableTransitionIndices.size(); i++) {
      count++;
      // Get the actual transition object using the index
      int transitionIdx = nodeID.avilableTransitionIndices[i];
      const Transition& transition = petri.Transitions[transitionIdx - 1];

      // Create successor state
      neighbors.emplace_back(RCPSPState_bi(nodeID, transition, true, i, count));
    }
  }
else {
      if (nodeID.activeTransitionIndices.size() > 0) {
        count++;
        int t = 0;
        for (int i = 0; i < nodeID.activeTransitionIndices.size(); i++) {
           if (petri.Transitions[nodeID.activeTransitionIndices[i].first-1].duration -nodeID.activeTransitionIndices[i].second< petri.Transitions[nodeID.activeTransitionIndices[t].first-1].duration -nodeID.activeTransitionIndices[t].second) {
            t = i;
          }
        }
        int transitionIdx = nodeID.activeTransitionIndices[t].first;
        Transition transition = petri.Transitions[transitionIdx - 1];
        transition.duration= nodeID.activeTransitionIndices[t].second;

        // Create successor state
        neighbors.emplace_back(RCPSPState_bi(nodeID, transition, false, t, count));      }

      for (int i = 0; i < nodeID.avilableDeTransitionIndices.size(); i++) {
        count++;
        int transitionIdx = nodeID.avilableDeTransitionIndices[i];
        const Transition& transition = petri.Transitions[transitionIdx - 1];

        // Create successor state
        neighbors.emplace_back(RCPSPState_bi(nodeID, transition, true, i, count));      }


    }
  }
   inline bool GoalTest(const RCPSPState_bi &node, const RCPSPState_bi &goal) const override {
return false;
    if (goal.marking.at(finalstatename) == 1) {
     if (node.name == 0) {
       return false;
     }
     return node.marking.at(finalstatename) == 1;
   }
  else {
    return node.marking.at("_pre_1") == 1;
  }
  }

  inline double HCost(const RCPSPState_bi &state1, const RCPSPState_bi &state2) const override {
  std::map<int, int> earlyfinishMap; // Map to store activity IDs and their early finish times
    //std::map<int, int> visitmap; // Map to store activity IDs and their early finish times
    double h;
    std::set<int> processedDependencies;
    // Iterate over unstarted activitiesint lastElementEarlyFinish = 0;
    //int lastElementEarlyFinish = 0;
    for (int activityId: state1.unstartedTransitions) {
      int maxFinishTime = 0;
      std::set<int> processedDependencies;

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

      earlyfinishMap[activityId] = maxFinishTime;
      //std::cout <<activityId<<":"<< earlyfinishMap[activityId]+RCPSPex.activities[activityId-1].duration << std::endl;
      // For last element with duration 0, just use the max finish time of dependencies
    }
    if (earlyfinishMap.size()==0) {
      h = 0;
    }
    else {
      h = earlyfinishMap.rbegin()->second;;

    }
    return h;
  }

  //
   inline double GCost(const RCPSPState_bi &state1, const RCPSPState_bi &state2) const override {
  return 0;
  if (state2.direction) {
  return state2.g_f - state1.g_f;
}
else {
    return state2.g_b - state1.g_b;
  }
  // Track actual transition cost
   }
  inline void GetActions(const RCPSPState_bi &state, std::vector<action> &actions) const override {
    // Not used in BidirectionalGreedyBestFirst, but must be implemented
    return;
  }
  virtual action GetAction(const RCPSPState_bi &state1, const RCPSPState_bi &state2) const override {

    return static_cast<action>(0);
  }
  inline void ApplyAction(RCPSPState_bi &state, action action) const override {
    // Not used, but required for abstract class
  }

  inline void UndoAction(RCPSPState_bi &state, action action) const override {
    // Not needed for bidirectional search, but required
  }
double GCost(const RCPSPState_bi &node, const action &act) const override {
if (node.direction) {
    return node.g_f;
}
else {
  return node.g_b;
}
  };
  bool InvertAction(action& a) const override {
    return false; // Replace with appropriate logic
  }
  uint64_t GetActionHash(action act) const override {
    return 0;
  };


  uint64_t GetStateHash(const RCPSPState_bi &node) const {

    //auto startS1 = std::chrono::high_resolution_clock::now();
    std::size_t seed = 0;
    // Hash the marking (Petri net state)
    for (const auto& pair : node.activeTransitionIndices) {
      seed ^= std::hash<int>{}(pair.first) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
      seed ^= std::hash<int>{}(pair.second) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    }

    // Hash the startedActivitiys vector
    for (const auto& activity : node.startedActivitiys) {
      seed ^= std::hash<int>{}(activity) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    }

    // Hash the finishedActivitiys vector
    for (const auto& activity : node.finishedActivitiys) {
      seed ^= std::hash<int>{}(activity) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    }

    // auto endS1 = std::chrono::high_resolution_clock::now();
    // hashTIME += endS1 - startS1;
    return seed;

    // Hash the finished activities
    // for (const auto& pair : node.finishedActivitiys) {
    //   seed ^= std::hash<int>{}(pair.first) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    //   seed ^= std::hash<int>{}(pair.second) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    // }

    // Distinguish forward vs. backward search by modifying the seed
    //seed ^= (GetExpandForward ? 0xAAAAAAAAAAAAAAAA : 0x5555555555555555);


  }

};

class ForwardRCPSPHeuristic : public Heuristic<RCPSPState_bi> {
public:
  double HCost(const RCPSPState_bi &current, const RCPSPState_bi &goal) const override {
    return current.h_f;
  }
};

class BackwardRCPSPHeuristic : public Heuristic<RCPSPState_bi> {
public:
  double HCost(const RCPSPState_bi &goal, const RCPSPState_bi &current) const override {
    return goal.h_b;

  }
};


class RCPSP_TT : public SearchEnvironment<RCPSPState_TT,int>{
public:
  RCPSP_TT();
  void GetSuccessors(const RCPSPState_TT &nodeID, std::vector<RCPSPState_TT> &neighbors) const override;
  bool GoalTest(const RCPSPState_TT &node, const RCPSPState_TT &goal) const override;
  double HCost(const RCPSPState_TT &state1, const RCPSPState_TT &state2) const override;
  double GCost(const RCPSPState_TT &state1, const RCPSPState_TT &state2) const override;

  int GetAction(const RCPSPState_TT &nodeID, const RCPSPState_TT &nodeID2) const override;
  int GetNumSuccessors(const RCPSPState_TT &stateID) const;
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
  // 9. Optimized independent set calculation
  // Optimization: Reserve memory to prevent re-allocations.
  // If you know the number of unstarted tasks (e.g., total - finished_count), use that.
  // Otherwise, just reserve total.
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

    // Heuristic calculation based on selected heuristic type
    if (activeHeuristic == P_RCPSP::HeuristicType::LBCS) {
      return std::max(computeLBCS_TT(tempUnstarted) - unkTime,
             computeLBCS_TT(newUnstartedTransitions));
    } else if (useDPHeuristic) {
      return std::max(getForwardHcostDP_TT(tempUnstarted) - unkTime,
             getForwardHcostDP_TT(newUnstartedTransitions));
    } else {
      return std::max(getForwardHcost_TT(tempUnstarted) - unkTime,
             getForwardHcost_TT(newUnstartedTransitions));
    }
  } else {
    // Fallback if no finished activities
    if (activeHeuristic == P_RCPSP::HeuristicType::LBCS) {
      return computeLBCS_TT(tempUnstarted);
    } else if (useDPHeuristic) {
      return getForwardHcostDP_TT(tempUnstarted);
    } else {
      return getForwardHcost_TT(tempUnstarted);
    }
  }
}
inline double RCPSP_TT::GCost(const RCPSPState_TT &state1, const RCPSPState_TT &state2) const {
  return state2.g-state1.g;//+state1.g
}

inline uint64_t RCPSP_TT::GetStateHash(const RCPSPState_TT &node) const {
  std::size_t seed = 0;

  // 1. Hash all finished activities (including -1 entries for unfinished)
  for (int id = 0; id < 128; ++id) {
    seed ^= std::hash<int>{}(id) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    seed ^= std::hash<int>{}(node.finishedActivitiys[id]) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
  }

  // 2. Hash activity nodes
  for (const auto& p : node.activity_nodes) {
    seed ^= std::hash<int>{}(p.first) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    seed ^= std::hash<int>{}(p.second) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
  }

  // 3. Hash resource nodes
  for (int r = 0; r < 4; ++r) {
    seed ^= std::hash<int>{}(r) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    for (const auto& p : node.resource_nodes[r]) {
      seed ^= std::hash<int>{}(p.first) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
      seed ^= std::hash<int>{}(p.second) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    }
  }

  return seed;
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
  // int i=0;
  // if (stateID.activeTransitions.size() > 0){i=1;}
  // return stateID.avilableTransition.size()+i;
  return 0;
}



inline void RCPSP_TT::ApplyAction(RCPSPState_TT &s, int a) const {

}
inline double RCPSP_TT::GCost(const RCPSPState_TT &node, const int &act) const {
  return node.g;
}

#endif //RCPSP_H
//
// Created by idolu on 06/01/2025.
//
