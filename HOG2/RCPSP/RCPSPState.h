//
// Created by idolu on 06/01/2025.
//
#pragma once
#include <set>
#include <bitset>
#include "petriclasses.h"
#include "readPetri.cpp"
#ifndef RCPSPSTATE_H
#define RCPSPSTATE_H

// Maximum supported activities for fixed-size arrays
// If problems exceed this, the code will throw an error at initialization
// Current datasets: j30=32, j60=62, j90=92, j120=122 activities
constexpr int MAX_ACTIVITIES = 128;

std::string finalstatename;
std::string initialstatename;
class RCPSPState {
  public:
  RCPSPState();
     RCPSPState(const RCPSPState& predecessor,const P_RCPSP::Transition& newTransition,bool status,short location,uint64_t &count);
    std::vector<short> marking;
    std::vector<std::pair<short, short>> activeTransitionIndices;  // Store transition ID and remaining duration

    std::array<short, MAX_ACTIVITIES> startedActivitiys;   // -1 = not started
    std::array<short, MAX_ACTIVITIES> finishedActivitiys;  // -1 = not finished

    bool status;
  short g=0;
  mutable short h;

  bool operator==(const RCPSPState& other) const;
};

class RCPSPState_TT {
public:
    std::array<std::vector<std::pair<short, short>>, 4> resource_nodes;
    std::vector<std::pair<short, short>> activity_nodes;
    std::array<short, MAX_ACTIVITIES> finishedActivitiys;  // -1 = not finished
    short g = 0;

    RCPSPState_TT();
    RCPSPState_TT(const RCPSPState_TT& prev, short ID, short firingTime);
    bool operator==(const RCPSPState_TT& other) const;
};

// Forward "TT2" search state (ported from the `ido` branch, forward only).
// Relative-time model: active activities carry a residual duration, time is
// advanced by a firing delta, and finished activities are tracked in a bitset.
// Runs on the consistent Class-1 LBER bound (see computeConsistentLBER_floor).
class RCPSPState_TT2 {
public:
    // Slim state: identity is exactly (finished, active). resource_nodes is a
    // deterministic function of these (renewable invariant) and is reconstructed
    // on demand at expansion (reconstructResourceNodes), never stored per node.
    // Finished tasks (1-based ids).
    std::bitset<128> finishedActivitiys;
    // Active (in-progress) tasks: <taskId, remaining-time>.
    std::vector<std::pair<short, short>> activeTransitionIndices;

    bool isDeltaZero = false;       // firing delta of the step that produced this state was 0
    short g = 0;                    // makespan so far
    mutable short h = 0;
    short predessesor_h = 0;        // parent's h, reused on delta-zero steps
    short lastTransitionId = 0;     // task started by the step that produced this state

    RCPSPState_TT2();
    RCPSPState_TT2(const RCPSPState_TT2& prev, short ID, short firingTime);

    // State identity is (finished, active) — exactly what GetStateHash encodes.
    // (resource_nodes is a deterministic function of these, so it is not compared.)
    bool operator==(const RCPSPState_TT2& other) const {
        if (finishedActivitiys != other.finishedActivitiys) return false;
        if (activeTransitionIndices != other.activeTransitionIndices) return false;
        return true;
    }
};




int computeEarlyFinishTime(int activityId);

// Reconstruct the TT2 resource token lists from the active set (slim TT2 nodes
// do not store resource_nodes). Defined in RCPSPState.cpp.
std::array<std::vector<std::pair<short, short>>, 4> reconstructResourceNodes(
    const std::vector<std::pair<short, short>> &activeTransitionIndices);

class RCPSPState_bi {
public:
    RCPSPState_bi();
    RCPSPState_bi(RCPSPState_bi predecessor,P_RCPSP::Transition newTransition,bool status,int location,uint64_t &count);
    ~RCPSPState_bi() {
        // Clear STL containers explicitly (optional, as they would be destroyed automatically)
        marking.clear();
        unstartedTransitions.clear();
        avilableTransitionIndices.clear();
        activeTransitionIndices.clear();
        startedActivitiys.clear();
        finishedActivitiys.clear();

        // Any additional custom cleanup logic can go here
    }
    std::unordered_map<std::string, int> marking;
    std::set<int> unstartedTransitions;
    std::vector<short> avilableTransitionIndices;  // Store transition IDs
    std::vector<int> avilableDeTransitionIndices;  // Store transition IDs
    std::vector<std::pair<int, int>> activeTransitionIndices;  // Store transition ID and remaining duration

    bool direction;
    bool nodestatus;
    double name=0;
    int predecesorname=0;

    std::set<int> startedActivitiys;
    std::set<int> finishedActivitiys;
    double g_f=0;
    double h_f=0;
    double g_b=0;
    double h_b=0;

    double f=0;


    //int GetG();
    //int checkEnd();
    bool operator==(const RCPSPState_bi& other) const;
};
#endif // RCPSPSTATE_H