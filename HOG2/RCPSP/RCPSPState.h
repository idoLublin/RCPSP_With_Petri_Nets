//
// Created by idolu on 06/01/2025.
//
#pragma once
#include <set>
#include "petriclasses.h"
#include "readPetri.cpp"
#ifndef RCPSPSTATE_H
#define RCPSPSTATE_H
std::string finalstatename;
std::string initialstatename;
class RCPSPState {
  public:
  RCPSPState();
    // Add this inside your class RCPSPState definition
    // ~RCPSPState() {
    //     std::vector<short>().swap(marking);
    //     std::vector<std::pair<short, short>>().swap(activeTransitionIndices);
    //
    //     std::map<int, int>().swap(startedActivitiys);
    //     std::map<int, int>().swap(finishedActivitiys);
    //
    // }
  RCPSPState(const RCPSPState& predecessor,const P_RCPSP::Transition& newTransition,bool status,int location,uint64_t &count);


    std::vector<short> marking;
    std::vector<std::pair<short, short>> activeTransitionIndices;  // Store transition ID and remaining duration


   // bool direction;
   //  bool nodestatus;
  //std::vector<RCPSPState> sons;
  //std::vector<int> unstartedTransitions;


  // unsigned int name=0;
  // unsigned int predecesorname=0;

  // std::map<int, int> startedActivitiys;
  // std::map<int, int> finishedActivitiys;

    std::vector<int> startedActivitiys;
    std::vector<int> finishedActivitiys;

    bool status;
  short g=0;
  mutable short h;

  bool operator==(const RCPSPState& other) const;
};

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
std::vector<std::string> resourceNames = {"R1", "R2", "R3", "R4"};

class RCPSPState_TT {
public:
    // --- Constructors ---
    RCPSPState_TT();
    ~RCPSPState_TT() {
        // 1. Clear complex nested vector
        // This is expensive to delete! It requires looping.
        for (auto& inner_vec : marking) {
            std::vector<std::pair<short, short>>().swap(inner_vec);
        }
        std::vector<std::vector<std::pair<short, short>>>().swap(marking);

        // 2. Clear Map
        std::map<int, int>().swap(finishedActivitiys);
    }
   // RCPSPState_TT(const RCPSPState_TT& predecessor, int newTransitionId, bool applyTransition, int location, uint64_t& count);
    RCPSPState_TT(const RCPSPState_TT& prev, int ID, int firingTime);
    // ~RCPSPState_TT() {
    //     marking.clear();
    //     // unfinishedTransitions.clear();
    //     //startedActivitiys.clear();
    //     finishedActivitiys.clear();
    // }

    // --- Resource state ---
    std::vector<std::vector<std::pair<short, short>>> marking;
    // std::vector<short> unfinishedTransitions;
   // std::map<int, int> startedActivitiys;                          // activityID -> start time
    std::map<int, int> finishedActivitiys;                         // activityID -> finish time
    //std::vector<std::pair<short, short>> avilableTransitionIndices;  // Store transition IDs

    short g = 0;
    //double h = 0;

    // --- Comparison ---
    bool operator==(const RCPSPState_TT& other) const;
};




int computeEarlyFinishTime(int activityId);

#endif // RCPSPSTATE_H