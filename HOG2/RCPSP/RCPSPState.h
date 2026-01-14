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

    RCPSPState_TT();
    RCPSPState_TT(const RCPSPState_TT& prev, short ID, short firingTime);
    bool operator==(const RCPSPState_TT& other) const;
};




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
#endif // RCPSPSTATE_H