//
// Created by idolu on 06/01/2025.
//
#pragma once
#include <set>
#include <bitset>
#include <iostream>
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


// ============================================================================
// RCPSPState_TT2 — Timed Transition Petri Net with Resources (TTPNR)
// ============================================================================
// Per Lublin, Atzmon & Cohen, SoCS 2026: "Petri Net Induced Heuristic Search
// for Resource Constrained Scheduling". State uses relative-delay tokens —
// each control-flow / resource token carries its residual delay theta.
// g(s) = sum of Delta-t firing jumps = true elapsed makespan.
class RCPSPState_TT2 {
public:
    // Resource tokens grouped by resource index: pairs of <amount/ID, residual delay>.
    // delay = 0 means available NOW.
    std::array<std::vector<std::pair<short, short>>, 4> resource_nodes;

    // Activity control-flow tokens: pairs of <placeID, residual delay>.
    std::vector<std::pair<short, short>> activity_nodes;

    // Bit i set => activity i finished (1-based).
    std::bitset<128> finishedActivitiys;

    // Cached derived data: <activityID, remaining duration> for currently-running tasks.
    std::vector<std::pair<short, short>> activeTransitionIndices;

    mutable std::vector<std::pair<short, short>> AvailableTransitionIndices_TT2;
    mutable bool transitionsCached = false;
    bool direction = 1;
    bool isDeltaZero = false;
    bool isCriticalInActive = false;
    short g = 0;
    short g_pre = 0;
    mutable short h = 0;
    short predessesor_h = 0;
    short lastTransitionId = 0;
    mutable short nextCritical = 0;

    RCPSPState_TT2();
    RCPSPState_TT2(const RCPSPState_TT2& prev, short transitionId, short firingTime, bool Direction = 1);

    bool operator==(const RCPSPState_TT2& other) const {
        if (finishedActivitiys != other.finishedActivitiys) return false;
        if (activity_nodes != other.activity_nodes) return false;
        if (resource_nodes != other.resource_nodes) return false;
        if (activeTransitionIndices != other.activeTransitionIndices) return false;
        return true;
    }
};




int computeEarlyFinishTime(int activityId);

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