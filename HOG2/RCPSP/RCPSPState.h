//
// Created by idolu on 06/01/2025.
//
#pragma once
#include <set>
#include "petriclasses.h"
#include "readPetri.cpp"
#ifndef RCPSPSTATE_H
#define RCPSPSTATE_H
using namespace P_RCPSP;
thread_local PetriExample petri;
thread_local RCPSP_example RCPSPex;
std::string finalstatename;
std::string initialstatename;
class RCPSPState {
  public:
  RCPSPState();
     RCPSPState(const RCPSPState& predecessor,const P_RCPSP::Transition& newTransition,bool status,short location,uint64_t &count);

    std::vector<short> marking;
    std::vector<std::pair<short, short>> activeTransitionIndices;  // can possibly be calculated on the fly

    std::array<short, 128> startedActivitiys;   // Was vector
    std::array<short, 128> finishedActivitiys;  // Was vector

    bool status;
  short g=0;
  mutable short h;

  bool operator==(const RCPSPState& other) const;
};

class RCPSPState_TT {
public:
    std::array<std::vector<std::pair<short, short>>, 4> resource_nodes;
    std::vector<std::pair<short, short>> activity_nodes;
    std::array<short, 128> finishedActivitiys;  // Changed from vector
    short g = 0;
mutable short h = 0;
    short predessesor_h = 0;
    RCPSPState_TT();
    RCPSPState_TT(const RCPSPState_TT& prev, short ID, short firingTime);
    bool operator==(const RCPSPState_TT& other) const;
};

#include <vector>
#include <array>
#include <bitset>
#include <utility> // for std::pair


int computeEarlyFinishTime(int activityId);

class RCPSPState_Bi {
public:
    // Core state from TT
    std::array<std::vector<std::pair<short, short>>, 4> resource_nodes;
    std::vector<std::pair<short, short>> activity_nodes;
    std::array<short, 128> finishedActivitiys;
    short g = 0;

    // BAE-specific fields
    bool direction = true;     // true = forward, false = backward
    short f = 0;             // f-value for priority queue
    short g_f = 0;           // g-value in forward direction
    short g_b = 0;           // g-value in backward direction
    short h_f = 0;           // forward heuristic
    short h_b = 0;           // backward heuristic

    // Constructors
    RCPSPState_Bi();
    RCPSPState_Bi(const RCPSPState_Bi& prev, short ID, short firingTime);

    // Convert from TT state
    RCPSPState_Bi(const RCPSPState_TT& tt_state) {
        resource_nodes = tt_state.resource_nodes;
        activity_nodes = tt_state.activity_nodes;
        finishedActivitiys = tt_state.finishedActivitiys;
        g = tt_state.g;
        direction = true;
        f = g_f = g_b = h_f = h_b = 0;
    }

    // Equality operator
    bool operator==(const RCPSPState_Bi& other) const {
        // Debug output
        static int compare_count = 0;
        if (++compare_count <= 5) {
            std::cout << "Comparing states:" << std::endl;
            std::cout << "  This direction=" << direction << ", other direction=" << other.direction << std::endl;

            int this_finished_count = 0, other_finished_count = 0;
            for (int i = 0; i < 128; i++) {
                if (finishedActivitiys[i] != -1) this_finished_count++;
                if (other.finishedActivitiys[i] != -1) other_finished_count++;
            }
            std::cout << "  This finished=" << this_finished_count << ", other finished=" << other_finished_count << std::endl;
        }

        // Check finished activities
        for (int i = 0; i < 128; i++) {
            bool this_finished = (finishedActivitiys[i] != -1);
            bool other_finished = (other.finishedActivitiys[i] != -1);
            if (this_finished != other_finished) return false;
        }

        // Check activity place markings
        if (activity_nodes.size() != other.activity_nodes.size()) return false;
        for (int i = 0; i < activity_nodes.size(); i++) {
            if (activity_nodes[i].first != other.activity_nodes[i].first) {
                return false;
            }
        }

        if (compare_count <= 5) {
            std::cout << "  STATES ARE EQUAL!" << std::endl;
        }

        return true;
    }

    //
    // // 2. Same activity place markings (token counts only)
    // if (activity_nodes.size() != other.activity_nodes.size()) return false;
    // for (int i = 0; i < activity_nodes.size(); i++) {
    //     if (activity_nodes[i].first != other.activity_nodes[i].first) {
    //         return false;
    //     }
    // }

    // 3. NEW: Compare g-values (time/cost)?
    // This could distinguish states at different times
    // BUT: for bidirectional, we might NOT want this
    // because forward at time=50 should match backward at time=50-from-goal

    // 4. NEW: Compare direction?
    // if (direction != other.direction) return false;

//     return true;
// }

    // Create goal state
};


class RCPSPState_TT2 {
public:
    // 1. Resources: Pairs of <Amount/ID, TimeRemaining>
    // TimeRemaining = 0 means available NOW.
    std::array<std::vector<std::pair<short, short>>, 4> resource_nodes;

    // 2. Activity Tokens: Pairs of <PlaceID, TimeRemaining>
    std::vector<std::pair<short, short>> activity_nodes;

    // 3. Finished Tasks: 16 bytes vs 128 bytes
    std::bitset<128> finishedActivitiys;

    // 4. Cached Indices (Derived Data)
    // You want to keep this for now.
    // WARNING: This is redundant data (derived from the nodes above).
    std::vector<std::pair<short, short>> activeTransitionIndices;

    // Add these for caching transitions
    mutable std::vector<std::pair<short, short>> AvailableTransitionIndices_TT2;
    mutable bool transitionsCached = false;
    bool direction=1;
    bool isDeltaZero;
    short g = 0;
    short g_pre = 0;
    mutable short h = 0;
    short predessesor_h = 0;
    short lastTransitionId=0; // <--- SAVE IT HERE
    RCPSPState_TT2();
    RCPSPState_TT2(const RCPSPState_TT2& prev, short ID, short firingTime,bool Direction =1);

    // Equality is CRITICAL for Relative Time
    bool operator==(const RCPSPState_TT2& other) const {
        // 1. Bitset comparison (fast)
        if (finishedActivitiys != other.finishedActivitiys) return false;

        // 2. Vectors must be SORTED for this to work!
        // If State A has [{1, 5}, {2, 3}] and State B has [{2, 3}, {1, 5}],
        // they are the same state, but == will return false if not sorted.
        if (activity_nodes != other.activity_nodes) return false;
        if (resource_nodes != other.resource_nodes) return false;
        if (activeTransitionIndices != other.activeTransitionIndices) {
            std::cerr << "HASH COLLISION: states equal but different activeTransitionIndices!" << std::endl;
            return false;  // treat as different states for now
        }
        return true;
        // CRITICAL: Do NOT compare activeTransitionIndices here!
        // Since it is derived data, if it is calculated/sorted slightly differently
        // it might prevent merging of identical states.
        // Only compare the "Physical" state.

        return true;
    }
};


class RCPSPState_BI_TT2 {
public:
    // 1. Resources: Pairs of <Amount/ID, TimeRemaining>
    // TimeRemaining = 0 means available NOW.
    std::array<std::vector<std::pair<short, short>>, 4> resource_nodes;

    // 2. Activity Tokens: Pairs of <PlaceID, TimeRemaining>
    std::vector<std::pair<short, short>> activity_nodes;

    // 3. Finished Tasks: 16 bytes vs 128 bytes
    std::bitset<128> finishedActivitiys;

    // 4. Cached Indices (Derived Data)
    // You want to keep this for now.
    // WARNING: This is redundant data (derived from the nodes above).
    std::vector<std::pair<short, short>> activeTransitionIndices;
    short fireTime=0;
    // Add these for caching transitions
    mutable std::vector<std::pair<short, short>> AvailableTransitionIndices_TT2;
    mutable bool transitionsCached = false;
    bool direction=1;
    bool isDeltaZero;
    short g_f = 0;
    short g_b = 0;
    short g_pre = 0;
    mutable short h_f = 0;
    mutable short h_b = 0;
    // mutable short f_f=0;
    // mutable short f_b=0;
    short predessesor_h_f = 0;
    short predessesor_h_b = 0;
    short lastTransitionId=0; // <--- SAVE IT HERE
    RCPSPState_BI_TT2();
    RCPSPState_BI_TT2(const RCPSPState_BI_TT2& prev, short transitionId, short firingTime,bool Direction =1);

    // Equality is CRITICAL for Relative Time
    bool operator==(const RCPSPState_BI_TT2& other) const {

        if (finishedActivitiys != other.finishedActivitiys) return false;
        if (activity_nodes != other.activity_nodes) return false;
        if (resource_nodes != other.resource_nodes) return false;

        // Temporarily add this:
        if (activeTransitionIndices != other.activeTransitionIndices) {
            std::cerr << "HASH COLLISION: states equal but different activeTransitionIndices!" << std::endl;
            return false;  // treat as different states for now
        }
        return true;
    }
    // bool operator==(const RCPSPState_BI_TT2& other) const {
    //     // 1. Bitset comparison (fast)
    //     if (finishedActivitiys != other.finishedActivitiys) return false;
    //
    //     // 2. Vectors must be SORTED for this to work!
    //     // If State A has [{1, 5}, {2, 3}] and State B has [{2, 3}, {1, 5}],
    //     // they are the same state, but == will return false if not sorted.
    //     if (activity_nodes != other.activity_nodes) return false;
    //     if (resource_nodes != other.resource_nodes) return false;
    //
    //     // CRITICAL: Do NOT compare activeTransitionIndices here!
    //     // Since it is derived data, if it is calculated/sorted slightly differently
    //     // it might prevent merging of identical states.
    //     // Only compare the "Physical" state.
    //
    //     return true;
    // }
};

// IN YOUR HEADER FILE (.h)
std::vector<std::pair<short, short>> getAvailableTransitionIndices_TT2(
    const std::vector<short> &unstartedTransitions,
    const std::bitset<128> &finishedActivitiys,  // ← Changed to bitset
    const std::array<std::vector<std::pair<short, short>>, 4> &resource_nodes,
    const std::vector<std::pair<short, short>> &activity_nodes,
    const std::vector<std::pair<short, short>> &activeTransitionIndices
);

std::vector<std::pair<short, short>> getAvailableTransitionIndices_TT2_backward(
    const std::vector<short> &unstartedTransitions,
    const std::bitset<128> &finishedActivitiys,  // ← Changed to bitset
    const std::array<std::vector<std::pair<short, short>>, 4> &resource_nodes,
    const std::vector<std::pair<short, short>> &activity_nodes,
    const std::vector<std::pair<short, short>> &activeTransitionIndices
);


std::vector<std::pair<short, short>> getAvailableTransitionIndices_TT(
    const std::vector<short> &unstartedTransitions,
    const std::vector<short> &finishedActivitiys,
    const std::array<std::vector<std::pair<short, short>>, 4> &resource_nodes,
    const std::vector<std::pair<short, short>> &activity_nodes
);
double getForwardHcost_TT(std::vector<short>unstartedTransitions);
double getForwardHcost(std::vector<short>unstartedTransitions,
                      std::vector<std::pair<short, short>>activeTransitionIndices
                      );
double getBackwardHcost(std::vector<short>unstartedTransitions,
                      std::vector<std::pair<short, short>>activeTransitionIndices
                      );
short getForwardHcost_TT2(
    const std::vector<short>& unstartedTransitions,
    const std::vector<std::pair<short, short>>& activity_tokens,
    const std::vector<std::pair<short, short>>& active_activities,
    const std::bitset<128>& finishedActivitiys) ;
double getForwardHcost_TT(const std::array<short, 128>& unstartedTransitions);




#endif // RCPSPSTATE_H