//
// Created by idol on 29/12/2024.
//
// Your First C++ Program
#include "Globals.h"
#include <iostream>
#include "RCPSPState.h"
#include "RCPSPState.cpp"
#include "../../HOG2/generic/TemplateAStar.h"
#include "../../HOG2/generic/BAE.h"
#include "../../HOG2/generic/EPEAStar.h"
#include "AStarCompare.h"

#include <filesystem>
namespace fs = std::filesystem;
#include "RCPSP.h"
//****importent i changed GLUtil.h with recVec == operator abit****//
 //PetriExample petri;
 //RCPSP_example RCPSP1;
#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <atomic>
#include <iostream>
// #include <windows.h>
#include <omp.h> // Include this at the top of Driver.cpp
#include <fstream>
#include <vector>
#include <climits>
#include <fstream>

// #include <sys/resource.h>
//
// long getPeakMemoryKB() {
//     struct rusage usage;
//     getrusage(RUSAGE_SELF, &usage);
//     return usage.ru_maxrss;
// }


// long getPeakMemoryKB() {
//     std::ifstream status("/proc/self/status");
//     std::string line;
//     while (std::getline(status, line)) {
//         if (line.find("VmPeak:") != std::string::npos) {
//             long kb;
//             sscanf(line.c_str(), "VmPeak: %ld kB", &kb);
//             return kb;
//         }
//     }
//     return -1;
// }

void runBenchmark();
void runSolvedProblems();
void sortCSV(const std::string& filename);
// std::atomic<bool> cancel_requested(false);
//
// std::atomic<bool> stop_printing1(false); // Flag to stop the printing thread

// void printNetworkSize1() {
//     while (!stop_printing1) {
//         std::this_thread::sleep_for(std::chrono::seconds(60*5)); // Wait for a second
//     }
// }





int solveRCPSP();
int solveRCPSP_TT();
int solveRCPSP_Bi();
#include <iostream>
#include <fstream>
#include <future>
#include <chrono>
#include <vector>
#include <thread>
#include <thread>
#include <atomic>

// #include <windows.h>
// #include <psapi.h>
// long getPeakMemoryKB() {
//     PROCESS_MEMORY_COUNTERS pmc;
//     GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc));
//     return pmc.PeakWorkingSetSize / 1024;
// }
// // #include <sys/resource.h>
// //
// // long getPeakMemoryKB() {
// //     struct rusage usage;
// //     getrusage(RUSAGE_SELF, &usage);
// //     return usage.ru_maxrss;
// // }
#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
long getPeakMemoryKB() {
    PROCESS_MEMORY_COUNTERS pmc;
    GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc));
    return pmc.PeakWorkingSetSize / 1024;
}
#else
#include <sys/resource.h>
long getPeakMemoryKB() {
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    return usage.ru_maxrss;
}
#endif
struct Bounds {
    int lb = -1;
    int ub = -1;
    bool optimal_known = false;
};

Bounds getBounds(int group, int exam, const std::string& problemType) {
    std::string filename = problemType + "lb.sm";  // j60lb_.sm, j90lb_.sm

    std::string path1 = filename;
    std::string path2 = "HOG2/RCPSP/" + filename;

    std::ifstream file(path1);
    if (!file.is_open()) { file.clear(); file.open(path2); }
    if (!file.is_open()) {
        std::cout << "Could not open " << filename << "\n";
        return {};
    }

    // Skip header until separator
    std::string line;
    while (std::getline(file, line))
        if (line.find("===") != std::string::npos) break;

    int g, e, ub, lb;
    std::string rest;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        if (!(iss >> g >> e >> ub >> lb)) continue;
        if (g == group && e == exam) {
            Bounds b;
            b.ub = ub;
            b.lb = lb;
            b.optimal_known = (line.find('*') != std::string::npos);
            return b;
        }
    }
    return {};
}

double HCost_TT(const RCPSPState_TT &state1, const RCPSPState_TT &state2) {
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

    h =std::max(getForwardHcost_TT(tempUnstarted) - unkTime,
               getForwardHcost_TT(newUnstartedTransitions));
  }
  else {
    // Fallback if no finished activities
    h= getForwardHcost_TT(tempUnstarted);
    // return getForwardHcost_TT(tempUnstarted, state1.finishedActivitiys);
  }
  return h;

}

double HCost_TT2(const RCPSPState_TT2 &state1, const RCPSPState_TT2 &state2) {
    std::vector<short> tempUnstarted;
    tempUnstarted.reserve(petri.Transitions.size());

    for (int i = 0; i < petri.Transitions.size(); i++) {
        short taskID = i + 1;
        // Check for -1 (Not Finished)
        if (state1.finishedActivitiys[taskID] == 0) {
            tempUnstarted.push_back(taskID);
        }
    }

    // 2. Standard CPM Heuristic
    // This calculates the longest path among the unstarted tasks.
    // Since 'g' is the time spent so far, and this H is the time remaining,
    // F = G + H is admissible.
    short h = getForwardHcost_TT(tempUnstarted);

    return h;

}

double HCost_TT2_Backward(const RCPSPState_TT2 &state1, const RCPSPState_TT2 &state2) {
    if (state1.isDeltaZero) {
        state1.h = state1.predessesor_h;
        return state1.h;
    }

    std::vector<short> tempUnstarted;
    tempUnstarted.reserve(petri.Transitions.size());

    for (int i = 0; i < petri.Transitions.size(); i++) {
        short taskID = i + 1;
        // FIX: Use .test() for bitset
        if (!state1.finishedActivitiys.test(taskID)) {
            tempUnstarted.push_back(taskID);
        }
    }

    state1.h = getBackwardHcost(tempUnstarted,
                                    //state1.activity_nodes,
                                    state1.activeTransitionIndices//,
                                  //  state1.finishedActivitiys
                                    );  // ← ADD THIS

    return state1.h;

}


int solveRCPSP(int group, int exam, const std::string& filename,const std::string& problemType="j30") {
    std::cout << "started solving: " << group<<":"<<exam << std::endl;

    // generateTIME= std::chrono::duration<double>(0);
    // avelableTIME= std::chrono::duration<double>(0);
    // HTIME= std::chrono::duration<double>(0);
    // hashTIME= std::chrono::duration<double>(0);
    // comperTime= std::chrono::duration<double>(0);
    // secssesorTIME= std::chrono::duration<double>(0);
    // clock_t setupTIME = clock();

    getPetri(petri, group, exam,problemType);
    getRCPSP(RCPSPex, group, exam,problemType);

    RCPSPex.computeAndStoreDeepDependencies();

    RCPSPState first;
    RCPSPState last = first;
    last.h = 0;

    for (int i = 0; i < last.marking.size(); ++i) {
        if (last.marking[i] == 1) {
            last.marking[i] = 0;
        }
    }

    // 2. Set the Goal State
    // Use the map we built to translate "FinalStateName" -> Integer ID
    // Then set that specific index to 1.
    int finalID = petri.place_name_to_id.at(finalstatename);
    last.marking[finalID] = 1;

    RCPSP as1;
    TemplateAStar<RCPSPState, int, RCPSP> astar;
    std::vector<RCPSPState> path;


    bool finished = false;
    bool timeout_occurred = false;
    std::chrono::duration<double> elapsed;


    clock_t setupend = clock();





    auto start = std::chrono::high_resolution_clock::now();
    astar.GetPath(&as1, first, last, path);
    auto end = std::chrono::high_resolution_clock::now();
    elapsed = end - start;

    int makespan = 0;

    if (!path.empty()) {
        std::cout << "Path found!" << std::endl;
        for (const auto& state : path) {
            std::cout << "g: " << state.g << std::endl;

            std::cout << "active: ";
            for (const auto& [transIdx, duration] : state.activeTransitionIndices)
                std::cout << " " << transIdx;
            std::cout << std::endl;

            // std::cout << "available: ";
            // for (int transIdx : state.avilableTransitionIndices)
            //     std::cout << " " << transIdx;
            // std::cout << std::endl << std::endl;

            makespan = state.g;
        }
    } else {
        std::cout << "Path not found or timeout occurred.\n";
    }

    std::cout << "Nodes Expanded: " << astar.GetNodesExpanded() << std::endl;
    std::cout << "Nodes Touched: " << astar.GetNodesTouched() << std::endl;

    std::ofstream file(filename, std::ios::app);
    file << group << "," << exam << "," << elapsed.count() << ","
             << (!path.empty() ? "True" : "False") << ","
         << makespan << ","
         << astar.GetNodesExpanded() << ","
         << astar.GetNodesTouched() << ","
         << path.size() << ","
         << "TP"<< ","
         << problemType<< ","
         << (useCS ? "True" : "False")<< ","
       //  << "\n";
        //  << 100 * generateTIME.count() / elapsed.count() << ","
        //  << generateTIME.count() / astar.GetNodesTouched() << ","
        //  << 100 * avelableTIME.count() / elapsed.count() << ","
        //  << avelableTIME.count() / astar.GetNodesTouched() << ","
        //  << 100 * hashTIME.count() / elapsed.count() << ","
        //  << hashTIME.count() / astar.GetNodesTouched() << ","
        //  << 100 * HTIME.count() / elapsed.count() << ","
        //  << HTIME.count() / count<< ","
        // << 100 * comperTime.count() / elapsed.count() << ","
        //  << comperTime.count() / astar.GetNodesTouched() << ","
        //  << 100 * secssesorTIME.count() / elapsed.count() << ","
        //  << secssesorTIME.count() / count<< ","
         << "\n";





    return 0;
}
    int solveRCPSP_TT(int group, int exam, const std::string& filename,const std::string& problemType="j30") {
    std::cout << "started solving EPEA*: " << group<<":"<<exam << std::endl;
    count=0;
    getPetri(petri, group, exam,problemType);
    getRCPSP(RCPSPex, group, exam,problemType);

    RCPSPState_TT first;
    RCPSPState_TT last = first;


    last.g = HCost_TT(last, first);

    RCPSP_TT as1;

    TemplateAStar<RCPSPState_TT, int, RCPSP_TT> astar;
    //EPEAStar<RCPSPState_TT, int, RCPSP_TT> astar;
    std::vector<RCPSPState_TT> path;

    // astar.SetReopenNodes(true);  // ← ADD THIS!

    std::chrono::duration<double> elapsed;

    auto start = std::chrono::high_resolution_clock::now();
   astar.GetPath(&as1, first, last, path);
    // 1. Setup the search
    // astar.InitializeSearch(&as1, first, last, path);
    //
    // // 2. Setup the timer
    // auto startTime = std::chrono::steady_clock::now();
    // auto timeLimit = std::chrono::minutes(5);
    //
    // // 3. Run the loop manually
    // bool found = false;
    // while (!astar.DoSingleSearchStep(path))
    // {
    //     // Check time every step (or every 1000 steps for speed)
    //     auto currentTime = std::chrono::steady_clock::now();
    //     if (currentTime - startTime > timeLimit) {
    //         printf("TIMEOUT: EPEA* search exceeded 5 minutes.\n");
    //         break;
    //     }
    // }
    //
    // // 4. Check if we actually found a path
    // if (path.size() > 0) {
    //     printf("Solution found! Length: %llu\n", path.size());
    // } else {
    //     printf("Failed to find solution (Timeout or No Path).\n");
    // }
    //
    //



    auto end = std::chrono::high_resolution_clock::now();
    elapsed = end - start;

    int makespan = 0;

    if (!path.empty()) {
        std::cout << "Path found!" << std::endl;

        //for (const auto& state : path) {
        RCPSPState_TT state=path.back();
            //std::cout << "g: " << state.g;

            // for (const auto& [actId, startTime] : state.startedActivitiys) {
            //     std::cout << actId << ":" << startTime << " ";
            // }

            std::cout << std::endl;
            makespan = state.g;
        //}

        std::cout << "\nFinal makespan: " << makespan << std::endl;
    }
     else {
        std::cout << "Path not found or timeout occurred.\n";
    }

    std::cout << "Nodes Expanded: " << astar.GetNodesExpanded() << std::endl;
    std::cout << "Nodes Touched: " << astar.GetUniqueNodesExpanded() << std::endl;
    std::cout << "Nodes Touched: " << astar.GetNodesTouched() << std::endl;

    std::ofstream file(filename, std::ios::app);
    file << group << "," << exam << "," << elapsed.count() << ","
         << (!path.empty() ? "True" : "False") << ","
         << makespan << ","
         << astar.GetNodesExpanded() << ","
         << astar.GetNodesTouched() << ","
         << path.size() << ","
        << "TT"<< ","
        << problemType<< ","
         << (useCS ? "True" : "False")<< ","
//     << 100 * generateTIME.count() / elapsed.count() << ","
// << generateTIME.count() / astar.GetNodesTouched() << ","
// << 100 * avelableTIME.count() / elapsed.count() << ","
// << avelableTIME.count() / astar.GetNodesTouched() << ","
// << 100 * hashTIME.count() / elapsed.count() << ","
// << hashTIME.count() / astar.GetNodesTouched() << ","
// << 100 * HTIME.count() / elapsed.count() << ","
// << HTIME.count() / count
// << 100 * comperTime.count() / elapsed.count() << ","
// << comperTime.count() / astar.GetNodesTouched() << ","
// << 100 * secssesorTIME.count() / elapsed.count() << ","
// << secssesorTIME.count() / count
         << "\n";

    return 0;
}

 int solveRCPSP_TT2(int group, int exam, const std::string& filename,const std::string& problemType="j30") {
    std::cout << "started solving TT2: " << group<<":"<<exam << std::endl;
    count=0;
    LB=0;
    getPetri(petri, group, exam,problemType);
    getRCPSP(RCPSPex, group, exam,problemType);

    RCPSPState_TT2 first;
    RCPSPState_TT2 last = first;


    last.g = HCost_TT2(last, first);

    RCPSP_TT2 as1;

    TemplateAStar<RCPSPState_TT2, int, RCPSP_TT2> astar;
    //EPEAStar<RCPSPState_TT2, int, RCPSP_TT2> astar;
    std::vector<RCPSPState_TT2> path;

    // astar.SetReopenNodes(true);  // ← ADD THIS!

    std::chrono::duration<double> elapsed;

    auto start = std::chrono::high_resolution_clock::now();
   astar.GetPath(&as1, first, last, path);
    // // 1. Setup the search
    // astar.InitializeSearch(&as1, first, last, path);
    //
    // // 2. Setup the timer
    // auto startTime = std::chrono::steady_clock::now();
    // auto timeLimit = std::chrono::minutes(5);
    //
    // // 3. Run the loop manually
    // bool found = false;
    // while (!astar.DoSingleSearchStep(path))
    // {
    //     // Check time every step (or every 1000 steps for speed)
    //     auto currentTime = std::chrono::steady_clock::now();
    //     if (currentTime - startTime > timeLimit) {
    //         printf("TIMEOUT: EPEA* search exceeded 5 minutes.\n");
    //         break;
    //     }
    // }
    //
    // // 4. Check if we actually found a path
    // if (path.size() > 0) {
    //     printf("Solution found! Length: %llu\n", path.size());
    // } else {
    //     printf("Failed to find solution (Timeout or No Path).\n");
    // }
    //




    auto end = std::chrono::high_resolution_clock::now();
    elapsed = end - start;
    long peakMemKB = getPeakMemoryKB(); // ADD THIS

    int makespan = 0;

    if (!path.empty()) {
        std::cout << "Path found!" << std::endl;
        std::cout << "{'scheduling': {";

        bool first = true;
        int realJobCount = 0;

        for (const auto& state : path) {
            // Skip the root node (Action -1) or Dummy Source (0) if you don't want it counted
            // Adjust 'state.lastTransitionId > 0' if Task 0 is a real job in your system.
            if (state.lastTransitionId <= 0) continue;

            if (!first) std::cout << ", ";
            std::cout << "'" << state.lastTransitionId << "': " << state.g<<","<<state.h;

            first = false;
            realJobCount++;
        }

        std::cout << "}, ";

        // 2. Print Statistics
        makespan = path.back().g; // Final state G is the makespan

        std::cout << "'total_jobs_scheduled': " << realJobCount << ", ";
        std::cout << "'makespan': " << makespan << ", ";
        std::cout << "'solved': True, ";
        std::cout << "}" << std::endl;
        //for (const auto& state : path) {
        RCPSPState_TT2 state=path.back();
            //std::cout << "g: " << state.g;

            // for (const auto& [actId, startTime] : state.startedActivitiys) {
            //     std::cout << actId << ":" << startTime << " ";
            // }

            std::cout << std::endl;
            makespan = state.g;
        //}

        std::cout << "\nFinal makespan: " << makespan << std::endl;
    }
     else {
        std::cout << "Path not found or timeout occurred.\n";
    }

   std::cout << "Nodes Expanded: " << astar.GetNodesExpanded() << std::endl;
   // std::cout << "Nodes Touched: " << astar.GetUniqueNodesExpanded() << std::endl;
    std::cout << "Nodes Touched: " << astar.GetNodesTouched() << std::endl;

    std::ofstream file(filename, std::ios::app);
    file << group << "," << exam << "," << elapsed.count() << ","
         << (!path.empty() ? "True" : "False") << ","
         << makespan << ","
         << astar.GetNodesExpanded() << ","
         << astar.GetNodesTouched() << ","
         << path.size() << ","
        << "TT2"<< ","
        << problemType<< ","
        << peakMemKB << ","  // ADD THIS
        << LB << ","  // ADD THIS

         // << (useCS ? "True" : "False")<< ","
         << "\n";

    return 0;
}

 int solveRCPSP_TT2_Backward(int group, int exam, const std::string& filename,const std::string& problemType="j30") {
    std::cout << "started solving TT2 Backward: " << group<<":"<<exam << std::endl;
    count=0;
    getPetri(petri, group, exam,problemType);
    getRCPSP(RCPSPex, group, exam,problemType);

// 1. Initialize Goal (Project Start)
    // The constructor already creates the "Project Start" state (Source token, 0 finished).
    // --- BACKWARD SEARCH SETUP ---

    // 1. Goal Node = Project Start (Everything "Reverse Scheduled" / Done)
    RCPSPState_TT2 backward_goal;
    backward_goal.finishedActivitiys.reset(); // Goal is All 1s
    backward_goal.g = 0;
    backward_goal.h = 0;

    // 2. Start Node = Project End (Nothing "Reverse Scheduled" yet)
    RCPSPState_TT2 backward_start;
    // Clear all bits first (Ensure unused bits 33-127 are 0)
    backward_start.finishedActivitiys.reset();

    // Set ONLY the bits for actual tasks (1..N) to 1
    for (int i = 1; i <= petri.Transitions.size(); ++i) {
        backward_start.finishedActivitiys.set(i);
    }

    // 3. Move Token to Sink (Project End)
    // The default constructor puts the token at Source. We must move it to Sink.
    int act_idx = 0;
    int source_idx = -1;
    int sink_idx = -1;

    std::unordered_map<int, int> place_to_res_check;
    int r_c = 0;
    for (const auto& [resName, cap] : RCPSPex.resources) {
        place_to_res_check[petri.place_name_to_id.at(resName)] = r_c++;
    }

    for (int i = 0; i < petri.places.size(); ++i) {
        if (place_to_res_check.count(i)) continue; // Skip resources

        if (petri.places[i].arcs_in.empty()) source_idx = act_idx; // Source
        if (petri.places[i].arcs_out.empty()) sink_idx = act_idx;   // Sink
        act_idx++;
    }

    // Perform the Swap
    if (source_idx != -1) backward_start.activity_nodes[source_idx] = {0, 0}; // Remove from Source
    if (sink_idx != -1)   backward_start.activity_nodes[sink_idx]   = {1, 0}; // Add to Sink

    // 4. Set Heuristic
    // 4. Set Heuristic properly using the Environment
    // This calculates the Critical Path from "End" to "Start"
    backward_start.h = HCost_TT2_Backward(backward_start, backward_goal);
    backward_start.predessesor_h = backward_start.h;
    // 4. Run A* (Backward)
    // Start at "Project End", go to "Project Start"
   // astar.GetPath(&as1, startNode, goalNode, path);
    RCPSP_TT2_Backward as1;

    TemplateAStar<RCPSPState_TT2, int, RCPSP_TT2_Backward> astar;
    //EPEAStar<RCPSPState_TT2, int, RCPSP_TT2> astar;
    std::vector<RCPSPState_TT2> path;

    // astar.SetReopenNodes(true);  // ← ADD THIS!

    std::chrono::duration<double> elapsed;

    auto start = std::chrono::high_resolution_clock::now();
    astar.GetPath(&as1, backward_start, backward_goal, path);
    // 1. Setup the search
    // astar.InitializeSearch(&as1, first, last, path);
    //
    // // 2. Setup the timer
    // auto startTime = std::chrono::steady_clock::now();
    // auto timeLimit = std::chrono::minutes(5);
    //
    // // 3. Run the loop manually
    // bool found = false;
    // while (!astar.DoSingleSearchStep(path))
    // {
    //     // Check time every step (or every 1000 steps for speed)
    //     auto currentTime = std::chrono::steady_clock::now();
    //     if (currentTime - startTime > timeLimit) {
    //         printf("TIMEOUT: EPEA* search exceeded 5 minutes.\n");
    //         break;
    //     }
    // }
    //
    // // 4. Check if we actually found a path
    // if (path.size() > 0) {
    //     printf("Solution found! Length: %llu\n", path.size());
    // } else {
    //     printf("Failed to find solution (Timeout or No Path).\n");
    // }





    auto end = std::chrono::high_resolution_clock::now();
    elapsed = end - start;

    int makespan = 0;

    if (!path.empty()) {
        std::cout << "Path found!" << std::endl;
        std::cout << "{'scheduling': {";

        bool first = true;
        int realJobCount = 0;

        for (const auto& state : path) {
            // Skip the root node (Action -1) or Dummy Source (0) if you don't want it counted
            // Adjust 'state.lastTransitionId > 0' if Task 0 is a real job in your system.
            if (state.lastTransitionId <= 0) continue;

            if (!first) std::cout << ", ";
            std::cout << "'" << state.lastTransitionId << "': " << state.g<<","<<state.h;

            first = false;
            realJobCount++;
        }

        std::cout << "}, ";

        // 2. Print Statistics
        makespan = path.back().g+path.back().h; // Final state G is the makespan

        std::cout << "'total_jobs_scheduled': " << realJobCount << ", ";
        std::cout << "'makespan': " << makespan << ", ";
        std::cout << "'solved': True, ";
        std::cout << "}" << std::endl;
        //for (const auto& state : path) {
        RCPSPState_TT2 state=path.back();
            //std::cout << "g: " << state.g;

            // for (const auto& [actId, startTime] : state.startedActivitiys) {
            //     std::cout << actId << ":" << startTime << " ";
            // }

            std::cout << std::endl;
            makespan = state.g;
        //}

        std::cout << "\nFinal makespan: " << makespan << std::endl;
    }
     else {
        std::cout << "Path not found or timeout occurred.\n";
    }

   std::cout << "Nodes Expanded: " << astar.GetNodesExpanded() << std::endl;
   // std::cout << "Nodes Touched: " << astar.GetUniqueNodesExpanded() << std::endl;
    std::cout << "Nodes Touched: " << astar.GetNodesTouched() << std::endl;

    std::ofstream file(filename, std::ios::app);
    file << group << "," << exam << "," << elapsed.count() << ","
         << (!path.empty() ? "True" : "False") << ","
         << makespan << ","
         << astar.GetNodesExpanded() << ","
         << astar.GetNodesTouched() << ","
         << path.size() << ","
        << "TT2_backward"<< ","
        << problemType<< ","
         << (useCS ? "True" : "False")<< ","
         << "\n";

    return 0;
}


//not working
int solveRCPSP_Bi(int group, int exam, const std::string& filename, const std::string& problemType="j30") {
    std::cout << "started solving: " << group << ":" << exam << std::endl;

    getPetri(petri, group, exam);
    getRCPSP(RCPSPex, group, exam);

    // Create start state - this initializes finalstatename
    RCPSPState_BI_TT2 first_tt;
    RCPSPState_BI_TT2 first(first_tt);
    first.direction = true;

    // Start with clean state
    RCPSPState_BI_TT2 last;
    last.direction = false;

    // 1. All activities finished
    for (int i = 1; i <= petri.Transitions.size(); i++) {
        last.finishedActivitiys[i] = 1;
    }

    // 2. Resources fully restored (same as initial state)
    last.resource_nodes = first.resource_nodes;

    // 3. Activity nodes - all zero except final sink place
    last.activity_nodes.resize(first.activity_nodes.size());
    for (auto& p : last.activity_nodes) p = {0, 0};
    short finalID = petri.place_name_to_id.at(finalstatename);
    last.activity_nodes[finalID - 4] = {1, 0};

    // 4. Zero cost/heuristic fields
    last.g_f = last.g_b = last.h_f = last.h_b = 0;
    last.activeTransitionIndices.clear();

    // Verify
    int goal_finished = 0;
    for (int i = 1; i <= petri.Transitions.size(); i++) {
        if (last.finishedActivitiys[i]) goal_finished++;
    }
    std::cout << "Goal has " << goal_finished << " finished activities" << std::endl;
    // Should print 32 for J30




    // // Create goal state properly
    // RCPSPState_BI_TT2 last;
    // last.direction = false;
    //
    // // Resize to match first
    // last.activity_nodes.resize(first.activity_nodes.size());
    //
    // // Clear all activity nodes
    // for (auto& pair : last.activity_nodes) {
    //     pair = {0, 0};
    // }
    //
    // // Set final sink place
    // if (finalstatename.empty()) {
    //     std::cerr << "ERROR: finalstatename not initialized!" << std::endl;
    //     return -1;
    // }
    //
    // auto it = petri.place_name_to_id.find(finalstatename);
    // if (it == petri.place_name_to_id.end()) {
    //     std::cerr << "ERROR: finalstatename not found!" << std::endl;
    //     return -1;
    // }
    //
    // short finalID = it->second;
    // if (finalID >= 4 && (finalID - 4) < last.activity_nodes.size()) {
    //     last.activity_nodes[finalID - 4] = {1, 0};
    // }
    //
    // // Copy resources from first
    // last.resource_nodes = first.resource_nodes;
    //
    // // CRITICAL: Mark all activities as finished AFTER everything else
    // for (int i = 1; i <= petri.Transitions.size(); i++) {
    //     last.finishedActivitiys[i] = -1;  // Mark as finished
    // }
    //
    // last.g_b = 0;
    // last.g_f = last.g_b = last.h_f = last.h_b = 0;
    // auto it2 = petri.place_name_to_id.find(finalstatename);
    //
    // // Always check if it was found to avoid a crash
    // if (it2 != petri.place_name_to_id.end()) {
    //     // Use it2->second to get the actual ID integer
    //     last.finishedActivitiys[it2->second] = 0;
    // }
    // else {
    //     // Optional: Handle error if name not found
    //     std::cerr << "Error: Place " << finalstatename << " not found!" << std::endl;
    // }
    // std::cout << "Goal state setup - checking finished activities:" << std::endl;
    // int goal_finished = 0;
    // for (int i = 0; i < 128; i++) {
    //     if (last.finishedActivitiys[i] != -1) goal_finished++;
    // }
    std::cout << "  Goal has " << goal_finished << " finished activities" << std::endl;
    // Set resources an
    std::vector<RCPSPState_BI_TT2> path;
    ForwardRCPSPHeuristic H_F;
    BackwardRCPSPHeuristic H_B;
    RCPSP_BiGreedy bs1;

    BAE<RCPSPState_BI_TT2, int, RCPSP_BiGreedy> Bi_RCPSP;

    bool finished = false;
    std::chrono::duration<double> elapsed;

    auto start = std::chrono::high_resolution_clock::now();
    Bi_RCPSP.GetPath(&bs1, first, last, &H_F, &H_B, path);
    auto end = std::chrono::high_resolution_clock::now();
    elapsed = end - start;
    double max_f = 0;
    double max_b = 0;

    if (!path.empty()) {
        finished = true;
        std::cout << "Path found!" << std::endl;

        // Find the meeting point where forward and backward met
        double makespan = 0;

        for (int i = 0; i < path.size() - 1; i++) {
            if (path[i].direction != path[i+1].direction) {
                // Meeting point: g_f + g_b
                makespan = path[i].g_f + path[i+1].g_b;
                makespan= Bi_RCPSP.GetSolutionCost();

                std::cout << "Meeting point at index " << i << std::endl;
                std::cout << "  Forward: g=" << path[i].g_f << ", g_f=" << path[i].g_f << std::endl;
                std::cout << "  Backward: g=" << path[i+1].g_b << ", g_b=" << path[i+1].g_b << std::endl;
                std::cout << "  Makespan = " <<makespan << std::endl;
                break;
            }
        }

        std::cout << "Final Makespan: " << makespan << std::endl;
        max_f = makespan;
    }

    std::cout << "Nodes Expanded: " << Bi_RCPSP.GetNodesExpanded() << std::endl;
    std::cout << "Nodes Touched: " << Bi_RCPSP.GetNodesTouched() << std::endl;
    return 0;  // ADD THIS
}

int getOptimalMakespan(int group, int exam, const std::string& problemType) {
    std::string filename = problemType + "opt.sm";

    std::string path1 = filename;                    // Console
    std::string path2 = "HOG2/RCPSP/" + filename;   // Green button

    std::ifstream file(path1);
    if (!file.is_open()) {
        file.clear();
        file.open(path2);
    }

    if (!file.is_open()) {
        std::cout << "Could not open " << filename << "\n";
        std::cout << "Tried: " << path1 << "\n";
        std::cout << "Tried: " << path2 << "\n";
        return -1;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.find("---") != std::string::npos) break;
    }

    int g, e, makespan;
    double cpu;
    while (file >> g >> e >> makespan >> cpu) {
        if (g == group && e == exam)
            return makespan;
    }

    return -1;
}

struct InstanceParams {
    float NC, RF, RS;
};

InstanceParams getParams(int group) {
    // group is 1-based, 1-48
    static const float NC_vals[] = {1.5f, 1.8f, 2.1f};         // 3 values
    static const float RF_vals[] = {0.25f, 0.50f, 0.75f, 1.0f}; // 4 values
    static const float RS_vals[] = {0.2f, 0.5f, 0.7f, 1.0f};    // 4 values

    int g = group - 1;  // 0-based index: 0 to 47

    // RS changes every 1: Cycle of 4
    int rs_idx = g % 4;

    // RF changes every 4: Cycle of 4 (4 * 4 = 16)
    int rf_idx = (g / 4) % 4;

    // NC changes every 16: 3 total values (16 * 3 = 48)
    int nc_idx = g / 16;

    return {NC_vals[nc_idx], RF_vals[rf_idx], RS_vals[rs_idx]};
}
int extractBounds(const std::string& filename, const std::string& problemType) {
    std::ofstream file(filename, std::ios::app);

    // Write header
    file << "group,exam,problem_type,lb,ub,optimal\n";

    int numGroups = 48;
    int numExams = 10;

    for (int group = 1; group <= numGroups; group++) {
        for (int exam = 1; exam <= numExams; exam++) {
            if (problemType == "j30") {
                int opt = getOptimalMakespan(group, exam, problemType);
                file << group << ","
                     << exam << ","
                     << problemType << ","
                     << opt << ","
                     << opt << ","
                     << "True\n";
            } else {
                Bounds b = getBounds(group, exam, problemType);
                file << group << ","
                     << exam << ","
                     << problemType << ","
                     << b.lb << ","
                     << b.ub << ","
                     << (b.optimal_known ? "True" : "False") << "\n";
            }
        }
    }

    file.close();
    return 0;
}
void setProblemSize(const std::string& problemType) {
    // extracts the number from "j30", "j60", "j120" etc.
    CONFLICT_SIZE = std::stoul(problemType.substr(1))+2;
}
// int solveRCPSP_CBS(int group, int exam, const std::string& filename, const std::string& problemType="j30") {
//     std::cout << "started solving CBS: " << group << ":" << exam << std::endl;
//     setProblemSize(problemType);  // CONFLICT_SIZE = 30
//     // int optMakespan = getOptimalMakespan(group, exam, problemType);
//     // std::ifstream test("j30opt.sm");
// debug_cardinal_num=0;
//
//     // getPetri(petri, group, exam, problemType);
//     getRCPSP(RCPSPex, group, exam, problemType);
//     RCPSP_CBS<32> as1;
//     resource_info.clear();
//     downstream.clear();
//     upstream.clear();
//     precomputeDownstream(); // call once after loading
//     precomputeUpstream(); // call once after loading
//     precomputeResourceInfo();
//     RCPSPState_CBS<CONFLICT_SIZE> first;
//     // first.computeRVS();
//     RCPSPState_CBS<CONFLICT_SIZE> last = first;
//     // last.num_activities=0;
//     last.resourceType=-1;
//     last.rvs_activities_pool.clear();
//
//
//
//     TemplateAStar<RCPSPState_CBS<CONFLICT_SIZE>, int, RCPSP_CBS<CONFLICT_SIZE>> astar;
//     std::vector<RCPSPState_CBS<CONFLICT_SIZE>> path;
//
//     std::chrono::duration<double> elapsed;
//     auto start = std::chrono::high_resolution_clock::now();
//
//     astar.GetPath(&as1, first, last, path);
//
//     auto end = std::chrono::high_resolution_clock::now();
//     elapsed = end - start;
//     long peakMemKB = getPeakMemoryKB();
//
//     int makespan = 0;
//
//
//     if (!path.empty() || first.rvs_activities_pool.empty()) {
//
//         // Get final state - either from path or initial state if already feasible
//         RCPSPState_CBS& finalState = path.empty() ? first : path.back();
//
//         makespan = finalState.start_times[RCPSPex.activities.size()-1] +
//                    RCPSPex.activities[RCPSPex.activities.size()-1].duration;
//
//         std::cout << "{'scheduling': {";
//         bool firstActivity = true;
//         for (int i = 0; i < RCPSPex.activities.size(); i++) {
//             if (!firstActivity) std::cout << ", ";
//             std::cout << "'" << i+1 << "': " << finalState.start_times[i];
//             firstActivity = false;
//         }
//         std::cout << "}, ";
//         std::cout << "'makespan': " << makespan << ", ";
//         std::cout << "'solved': True, ";
//         std::cout << "}" << std::endl;
//         std::cout << "\nFinal makespan: " << makespan << std::endl;
//     }
//     else {
//         std::cout << "Path not found or timeout occurred.\n";
//     }
//
//     std::cout << "Nodes Expanded: " << astar.GetNodesExpanded() << std::endl;
//     std::cout << "Nodes Touched: " << astar.GetNodesTouched() << std::endl;
//
//
//
//
//
//     std::ofstream file(filename, std::ios::app);
//
//     // Verify optimality
//     InstanceParams p = getParams(group);
//
//     file << group << ","
//          << exam << ","
//          << elapsed.count() << ","
//          << makespan << ",";
//
//     if (problemType == "j30") {
//         int opt = getOptimalMakespan(group, exam, problemType);
//         file << (makespan == opt ? "True" : "False") << ","
//              << problemType << ","
//              << "CBS" << ","
//              << opt << ",-1,";
//         if (makespan != opt) {
//             allcorrect = false;
//         }
//     } else {
//         Bounds b = getBounds(group, exam, problemType);
//         file << (b.optimal_known ? (makespan == b.lb ? "True" : "False") : "Unknown") << ","
//              << problemType << ","
//              << "CBS" << ","
//              << b.lb << "," << b.ub << ",";
//     }
//
//     file << p.NC << "," << p.RF << "," << p.RS << ","
//          << ((!path.empty() || first.rvs_activities_pool.empty()) ? "True" : "False") << ","
//          << astar.GetNodesExpanded() << ","
//          << astar.GetNodesTouched() << ","
//          << path.size() << ","
//          << peakMemKB << ","
//          << setting.use_conflict_prioritization << ","
//          << (int)setting.heuristic << ","
//         << setting.use_first_conflict << ","
//         << setting.use_dominance << ","
//         << setting.use_greed_conflic_resultion_asstimation << ","
//
//          << debug_cardinal_num/max(1,astar.GetNodesTouched()) << "\n";
//
//     return 0;
// }

template<int N>
int solveRCPSP_CBS_impl(int group, int exam, const std::string& filename, const std::string& problemType) {
    debug_cardinal_num = 0;

    getRCPSP(RCPSPex, group, exam, problemType);
    resource_info.clear();
    downstream.clear();
    upstream.clear();
    precomputeDownstream();
    precomputeUpstream();
    precomputeResourceInfo();

    RCPSP_CBS<N> as1;

    RCPSPState_CBS<N> first;
    RCPSPState_CBS<N> last = first;
    last.resourceType = -1;
    last.rvs_activities_pool.clear();

    TemplateAStar<RCPSPState_CBS<N>, int, RCPSP_CBS<N>> astar;
    std::vector<RCPSPState_CBS<N>> path;

    std::chrono::duration<double> elapsed;
    auto start = std::chrono::high_resolution_clock::now();
    astar.GetPath(&as1, first, last, path);
    auto end = std::chrono::high_resolution_clock::now();
    elapsed = end - start;
    long peakMemKB = getPeakMemoryKB();

    int makespan = 0;

    if (!path.empty() || first.rvs_activities_pool.empty()) {
        RCPSPState_CBS<N>& finalState = path.empty() ? first : path.back();

        makespan = finalState.start_times[RCPSPex.activities.size() - 1] +
                   RCPSPex.activities[RCPSPex.activities.size() - 1].duration;

        std::cout << "{'scheduling': {";
        bool firstActivity = true;
        for (int i = 0; i < (int)RCPSPex.activities.size(); i++) {
            if (!firstActivity) std::cout << ", ";
            std::cout << "'" << i + 1 << "': " << finalState.start_times[i];
            firstActivity = false;
        }
        std::cout << "}, ";
        std::cout << "'makespan': " << makespan << ", ";
        std::cout << "'solved': True, ";
        std::cout << "}" << std::endl;
        std::cout << "\nFinal makespan: " << makespan << std::endl;
    } else {
        std::cout << "Path not found or timeout occurred.\n";
    }

    std::cout << "Nodes Expanded: " << astar.GetNodesExpanded() << std::endl;
    std::cout << "Nodes Touched: " << astar.GetNodesTouched() << std::endl;

    std::ofstream file(filename, std::ios::app);
    InstanceParams p = getParams(group);

    file << group << ","
         << exam << ","
         << elapsed.count() << ","
         << makespan << ",";

    if (problemType == "j30") {
        int opt = getOptimalMakespan(group, exam, problemType);
        file << (makespan == opt ? "True" : "False") << ","
             << problemType << ","
             << "CBS" << ","
             << opt << ",-1,";
        if (makespan != opt)
            allcorrect = false;
    } else {
        Bounds b = getBounds(group, exam, problemType);
        file << (b.optimal_known ? (makespan == b.lb ? "True" : "False") : "Unknown") << ","
             << problemType << ","
             << "CBS" << ","
             << b.lb << "," << b.ub << ",";
        if (makespan < b.lb||makespan >b.ub)
            allcorrect = false;
    }

    file << p.NC << "," << p.RF << "," << p.RS << ","
         << ((!path.empty() || first.rvs_activities_pool.empty()) ? "True" : "False") << ","
         << astar.GetNodesExpanded() << ","
         << astar.GetNodesTouched() << ","
         << path.size() << ","
         << peakMemKB << ","
         << setting.use_conflict_prioritization << ","
         << (int)setting.heuristic << ","
         << setting.use_first_conflict << ","
         << setting.use_dominance << ","
         << setting.use_greed_conflic_resultion_asstimation << ","
         << debug_cardinal_num / max(1, astar.GetNodesTouched()) << "\n";

    return 0;
}

int solveRCPSP_CBS(int group, int exam, const std::string& filename, const std::string& problemType = "j30") {
    std::cout << "started solving CBS: " << group << ":" << exam << std::endl;
    setProblemSize(problemType);

    if (problemType == "j30")
        return solveRCPSP_CBS_impl<32>(group, exam, filename, problemType);
    else if (problemType == "j60")
        return solveRCPSP_CBS_impl<62>(group, exam, filename, problemType);
    else if (problemType == "j90")
        return solveRCPSP_CBS_impl<92>(group, exam, filename, problemType);
    else if (problemType == "j120")
        return solveRCPSP_CBS_impl<122>(group, exam, filename, problemType);
}
// int solveRCPSP_BAP(int group, int exam, const std::string& filename, const std::string& problemType="j30") {
//     std::cout << "started solving BAP: " << group << ":" << exam << std::endl;
//     setProblemSize(problemType);
//     getRCPSP(RCPSPex, group, exam, problemType);
//     RCPSP_CBS as1;
//     resource_info.clear();
//     downstream.clear();
//     precomputeDownstream();
//     precomputeResourceInfo();
//     RCPSPState_CBS first;
//     RCPSPState_CBS last = first;
//     last.t=0;
//     last.resourceType=-1;
//
//
//     TemplateAStar<RCPSPState_CBS, int, RCPSP_CBS> astar;
//     std::vector<RCPSPState_CBS> path;
//
//     std::chrono::duration<double> elapsed;
//     auto start = std::chrono::high_resolution_clock::now();
//
//     astar.GetPath(&as1, first, last, path);
//
//     auto end = std::chrono::high_resolution_clock::now();
//     elapsed = end - start;
//     long peakMemKB = getPeakMemoryKB();
//
//     int makespan = 0;
//
//
//     if (!path.empty() || first.rvs_activities_pool.empty()) {
//
//         // Get final state - either from path or initial state if already feasible
//         RCPSPState_CBS& finalState = path.empty() ? first : path.back();
//
//         makespan = finalState.start_times[RCPSPex.activities.size()-1] +
//                    RCPSPex.activities[RCPSPex.activities.size()-1].duration;
//
//         std::cout << "{'scheduling': {";
//         bool firstActivity = true;
//         for (int i = 0; i < RCPSPex.activities.size(); i++) {
//             if (!firstActivity) std::cout << ", ";
//             std::cout << "'" << i+1 << "': " << finalState.start_times[i];
//             firstActivity = false;
//         }
//         std::cout << "}, ";
//         std::cout << "'makespan': " << makespan << ", ";
//         std::cout << "'solved': True, ";
//         std::cout << "}" << std::endl;
//         std::cout << "\nFinal makespan: " << makespan << std::endl;
//     }
//     else {
//         std::cout << "Path not found or timeout occurred.\n";
//     }
//
//     std::cout << "Nodes Expanded: " << astar.GetNodesExpanded() << std::endl;
//     std::cout << "Nodes Touched: " << astar.GetNodesTouched() << std::endl;
//     std::ofstream file(filename, std::ios::app);
//     file << group << "," << exam << "," << elapsed.count() << ","
//          << ((!path.empty()|| first.rvs_activities_pool.empty()) ? "True" : "False") << ","
//          << makespan << ","
//          << astar.GetNodesExpanded() << ","
//          << astar.GetNodesTouched() << ","
//          << path.size() << ","
//          << "BAP" << ","
//          << problemType << ","
//          << peakMemKB << ",";
//     // Verify optimality
//     int optMakespan = getOptimalMakespan(group, exam, "j30opt.sm");
//     if (optMakespan != -1) {
//         bool correct = (makespan == optMakespan);
//         std::cout << "Optimal makespan: " << optMakespan << std::endl;
//         std::cout << "Our makespan: " << makespan << std::endl;
//         std::cout << "Correct: " << (correct ? "YES" : "NO") << std::endl;
//
//         // Also write to file
//         file << (correct ? "True" : "False") << ","
//              << optMakespan << ",";
//         if (!correct) {
//             allcorrect = false;
//         }
//     }
//     file << "\n";
//     return 0;
// }

template<int N>
int solveRCPSP_old_impl(int group, int exam, const std::string& filename,const std::string& problemType="j30") {
    // std::cout << "started solving: " << group<<":"<<exam << std::endl;

    getPetri(petri, group, exam,problemType);
    getRCPSP(RCPSPex, group, exam,problemType);
    RCPSPex.activity_len=RCPSPex.activities.size();
    // RCPSPex.computeAndStoreDeepDependencies();

    oldRCPSPState<N> first;
    oldRCPSPState<N> last = first;
    last.finishedActivitiys.fill(1);

    // int finalID = petri.place_name_to_id.at(finalstatename);

    oldRCPSP<N> as1;
    TemplateAStar<oldRCPSPState<N>, int, oldRCPSP<N>> astar;
    std::vector<oldRCPSPState<N>> path;


    bool finished = false;
    bool timeout_occurred = false;
    std::chrono::duration<double> elapsed;


    clock_t setupend = clock();





    auto start = std::chrono::high_resolution_clock::now();
    astar.GetPath(&as1, first, last, path);
    auto end = std::chrono::high_resolution_clock::now();
    elapsed = end - start;

    int makespan = 0;

    if (!path.empty()) {
        std::cout << "Path found!" << std::endl;
        for (const auto& state : path) {
            std::cout << "g: " << state.g << std::endl;

            std::cout << "active: ";
            for (const auto& [transIdx, duration] : state.activeTransitionIndices)
                std::cout << " " << transIdx;
            std::cout << std::endl;

            // std::cout << "available: ";
            // for (int transIdx : state.avilableTransitionIndices)
            //     std::cout << " " << transIdx;
            // std::cout << std::endl << std::endl;

            makespan = state.g;
        }
    } else {
        std::cout << "Path not found or timeout occurred.\n";
    }

    std::cout << "Nodes Expanded: " << astar.GetNodesExpanded() << std::endl;
    std::cout << "Nodes Touched: " << astar.GetNodesTouched() << std::endl;

    std::ofstream file(filename, std::ios::app);
    file << group << "," << exam << "," << elapsed.count() << ","
             << (!path.empty() ? "True" : "False") << ","
         << makespan << ","
         << astar.GetNodesExpanded() << ","
         << astar.GetNodesTouched() << ","
         << path.size() << ","
         << "TP"<< ","
         << problemType<< ","
         << (useCS ? "True" : "False")<< ","
       //  << "\n";
        //  << 100 * generateTIME.count() / elapsed.count() << ","
        //  << generateTIME.count() / astar.GetNodesTouched() << ","
        //  << 100 * avelableTIME.count() / elapsed.count() << ","
        //  << avelableTIME.count() / astar.GetNodesTouched() << ","
        //  << 100 * hashTIME.count() / elapsed.count() << ","
        //  << hashTIME.count() / astar.GetNodesTouched() << ","
        //  << 100 * HTIME.count() / elapsed.count() << ","
        //  << HTIME.count() / count<< ","
        // << 100 * comperTime.count() / elapsed.count() << ","
        //  << comperTime.count() / astar.GetNodesTouched() << ","
        //  << 100 * secssesorTIME.count() / elapsed.count() << ","
        //  << secssesorTIME.count() / count<< ","
         << "\n";





    return 0;
}
int solveoldRCPSP(int group, int exam, const std::string& filename, const std::string& problemType = "j30") {
    std::cout << "started solving old: " << group << ":" << exam << std::endl;
    setProblemSize(problemType);

    if (problemType == "j30")
        return solveRCPSP_old_impl<32>(group, exam, filename, problemType);
    else if (problemType == "j60")
        return solveRCPSP_old_impl<62>(group, exam, filename, problemType);
    else
        return solveRCPSP_old_impl<92>(group, exam, filename, problemType);
}

std::string getNextFilename(const std::string& folder, const std::string& baseName, const std::string& extension) {
    // Ensure folder exists
    if (!fs::exists(folder)) {
        fs::create_directories(folder);
    }

    int count = 1;
    std::string newFilename;

    do {
        newFilename = folder + "/" + baseName + std::to_string(count) + extension;
        count++;
    } while (fs::exists(newFilename)); // Ensure unique filename

    return newFilename;
}


 int main() {
     runBenchmark();
    return 0;
}


void runBenchmark() {
    std::string folder = "new_results";
    std::string baseName = "output!!!!!!_";
    std::string extension = ".csv";
    std::string filename = getNextFilename(folder, baseName, extension);
    std::ofstream file(filename);
    // Open file stream
    if (!file.is_open()) {
        std::cerr << "Error opening file!" << std::endl;
        return;
    }

    setting.use_conflict_prioritization = true;  // cardinal > semi > non cardinal
    // setting.heuristic = HeuristicType::NONE;   // no heuristic
    // setting.heuristic = HeuristicType::CG;   // cardinal hitting set
    setting.heuristic = HeuristicType::HCBS;   // cardinal hitting set
    // setting.heuristic = HeuristicType::DG;     // dependency graph

    setting.use_first_conflict = false;  // cardinal > semi > non cardinal
    setting.use_ancestor_branching          = false;
    setting.use_dominance          = false;
    setting.use_pair_decomposition         = false;
    setting.use_greed_conflic_resultion_asstimation=false;
    setting.use_MDA_sets=true;
    setting.use_MDA_cache=true;
    setting.use_strong_constraints=false;
    setting.use_MDA_BAB=true;


 // file << "group,exam,time,finished,makespan,expand number,generated number,depth,PetriType,SetType,max mem,calculated LB,generatedTime%,generatedTime(ave),avilableTime%,avilableTime(ave),hashTime%,hashTime(ave),HcostTime%,HcostTime(ave),hashTime(ave),comperTime%,comperTime(ave),succsesroTime%,sucssesorTime(ave)" << std::endl;
    file << "group,exam,time,makespan,correct,setType,model,optimalOrLB,UB,NC,RF,RS,"
         << "finished,expandNumber,generatedNumber,depth,maxMem,"
         << "useConflictPrioritization,useHeuristic,usefirstconflict,usedominance,usebetterReslution,cardianlity ratio"
         << std::endl;


    if (!setting.use_conflict_prioritization&& !(setting.heuristic == HeuristicType::NONE)) {
        std::cout <<"Error: invalid setting"<< std::endl;
        exit(0);
    }
    // for(int i = 1; i < 49; i++) {
    //     for(int j = 1; j < 11; j++) {
    //         petri.reset();
    //         RCPSPex.reset();
    //
    //         // 2. SOLVE
    //         // solveRCPSP_TT2_Backward(i, j, filename, "j30");
    //         // solveRCPSP_CBS(i, j, filename, "j60");
    //         solveRCPSP_CBS(i, j, filename, "j30");
    //          // solveRCPSP_TT2(i, j, filename, "j30");
    //         //solveRCPSP_TT(i, j, filename, "j30");
    //          // solveRCPSP_Bi(i, j, filename, "j30");
    //     }
    // }

    // for (int j = 1; j < 11; j++) {
    //     // solveRCPSP(16, j, filename, "j30");
    //     // solveRCPSP_TT2(16, j, filename, "j30");
    //     // solveoldRCPSP(16, j, filename, "j30");
    //     solveRCPSP_CBS(24, j, filename, "j30");
    //     solveRCPSP_CBS(24, j, filename, "j60");
    //     solveRCPSP_CBS(24, j, filename, "j90");
    //     // solveRCPSP_CBS(1, j, filename, "j90");
    // }
    // for (int j = 1; j < 11; j++) {
    //     // solveRCPSP(startGroup, j, filename, setType);
    //     // solveRCPSP_CBS(17, j, filename, "j30");
    //     solveRCPSP_CBS(17, j, filename, "j60");
    //     // solveRCPSP_CBS(17, j, filename, "j90");
    // }
    // for (int j = 1; j < 11; j++) {
    //     // solveRCPSP(startGroup, j, filename, setType);
    //     // solveRCPSP_CBS(33, j, filename, "j30");
    //     solveRCPSP_CBS(33, j, filename, "j60");
    //     // solveRCPSP_CBS(33, j, filename, "j90");
    // }
    // extractBounds(filename,"j90");


    // solveRCPSP_TT2(16, 9, filename, "j30");
    solveRCPSP_CBS(5, 5, filename, "j60");

    std::cout <<debug_cardinal_num <<std::endl;

if (allcorrect) {
    std::cout <<"All correct" <<std::endl;
}
else {
    std::cout <<"Error: incorrect results" <<std::endl;
}
}

struct ResultRow {
    std::string fullLine;
    // Sorting Keys
    std::string petriType; // TP/TT
    std::string setType;   // j30
    int setSize;           // 30 (for sorting)
    int group;
    int exam;
};

void sortCSV(const std::string& filename) {
    std::cout << "🔄 Sorting results..." << std::endl;

    std::ifstream inFile(filename);
    if (!inFile.is_open()) return;

    std::vector<ResultRow> rows;
    std::string line, header;

    // 1. Read Header
    if (getline(inFile, header)) {
        // Ensure we keep the header!
    }

    // 2. Read and Parse Rows
    while (getline(inFile, line)) {
        if (line.empty()) continue;

        std::stringstream ss(line);
        std::string segment;
        ResultRow row;
        row.fullLine = line;
        int colIndex = 0;

        // Parse columns by comma
        while (getline(ss, segment, ',')) {
            // Column 0: Group
            if (colIndex == 0) try { row.group = std::stoi(segment); } catch (...) { row.group = 0; }
            // Column 1: Exam
            else if (colIndex == 1) try { row.exam = std::stoi(segment); } catch (...) { row.exam = 0; }
            // Column 8: PetriType (TP/TT)
            else if (colIndex == 8) row.petriType = segment;
            // Column 9: SetType (j30)
            else if (colIndex == 9) {
                row.setType = segment;
                // Extract integer for sorting (j30 -> 30)
                std::string nums = segment;
                nums.erase(std::remove_if(nums.begin(), nums.end(), [](char c){ return !isdigit(c); }), nums.end());
                try { row.setSize = std::stoi(nums); } catch (...) { row.setSize = 0; }
            }
            colIndex++;
        }
        rows.push_back(row);
    }
    inFile.close();

    // 3. Sort (Priority: PetriType -> SetSize -> Group -> Exam)
    std::sort(rows.begin(), rows.end(), [](const ResultRow& a, const ResultRow& b) {
        if (a.petriType != b.petriType) return a.petriType < b.petriType; // TP vs TT
        if (a.setSize != b.setSize) return a.setSize < b.setSize;         // j30 vs j60
        if (a.group != b.group) return a.group < b.group;                 // 1 vs 2
        return a.exam < b.exam;                                           // 1 vs 2
    });

    // 4. Write Back
    std::ofstream outFile(filename);
    outFile << header << "\n"; // Write original header
    for (const auto& row : rows) {
        outFile << row.fullLine << "\n";
    }

    std::cout << "✅ Sorted " << rows.size() << " rows." << std::endl;
}




void getinitialHcost(int i, int i1, const std::string & string);
void getinitialHcost(int group, int exam, const std::string &filename) {
    std::cout << "started solving: " << group<<":"<<exam << std::endl;
    count=0;
    double initalHcost=0;
    getPetri(petri, group, exam);
    getRCPSP(RCPSPex, group, exam);

    RCPSPState first;
    //initalHcost=getForwardHcost(first.unfinishedTransitions,first.activeTransitionIndices);

    std::cout << "initalHcost\n";



    std::ofstream file(filename, std::ios::app);
    file << group << "," << exam << "," << initalHcost<<std::endl;

}

void solver_group(int startGroup,const std::string& filename) {
    for (int j = 1; j < 11; j++) {
        //solveRCPSP(startGroup, j, filename, "j30");
        solveRCPSP_TT(startGroup, j, filename, "j30");

    }
}


// int main(int argc, char *argv[]) {
//     // Expects one argument: The starting Group ID for this batch (e.g., 1, 3, 5, etc.)
//     int startGroup = std::stoi(argv[1]);
//     std::string outputFolder = argv[2]; // <--- Get the absolute path
//
//     // Construct filename: "/home/.../results_job_123/results_group_5.
//
//
//
//
//     std::string filename = outputFolder + "/results_group_" + std::to_string(startGroup) + ".csv";
//
//     std::ofstream file(filename);
//     solver_group(startGroup,filename);
//     solver_group(startGroup+1,filename);
//     solver_group(startGroup+2,filename);
//     solver_group(startGroup+3,filename);
//     solver_group(startGroup+4,filename);
//     solver_group(startGroup+5,filename);
//     solver_group(startGroup+6,filename);
//     solver_group(startGroup+7,filename);
// }




void solver_group(int startGroup, const std::string& filename, const std::string& setType) {
    for (int j = 1; j < 11; j++) {
         solveRCPSP(startGroup, j, filename, setType);
        //solveRCPSP_TT(startGroup, j, filename, setType);
    }
}
void solver_group_TT(int startGroup, const std::string& filename, const std::string& setType) {
    for (int j = 1; j < 11; j++) {
        // solveRCPSP(startGroup, j, filename, setType);
        solveRCPSP_TT(startGroup, j, filename, setType);
    }
}
// int main(int argc, char *argv[]) {
//     // Expects one argument: The starting Group ID for this batch (e.g., 1, 3, 5, etc.)
//     int startGroup = std::stoi(argv[1]);
//     std::string outputFolder = argv[2]; // <--- Get the absolute path
//
//     // Construct filename: "/home/.../results_job_123/results_group_5.
//
//
//
//
//     std::string filename = outputFolder + "/results_group_" + std::to_string(startGroup) + ".csv";
//
//     std::ofstream file(filename);
//     // solver_group_TT(startGroup,filename,"j30");
//     // solver_group_TT(startGroup+1,filename,"j30");
//     // solver_group_TT(startGroup+2,filename,"j30");
//
//
//
//     // solver_group(startGroup,filename,"j30");
//     // solver_group(startGroup+1,filename,"j30");
//     // solver_group(startGroup+2,filename,"j30");
//
//     solver_group(startGroup,filename,"j60");
//     solver_group(startGroup+1,filename,"j60");
//     solver_group(startGroup+2,filename,"j60");
//
//
//     solver_group(startGroup,filename,"j90");
//     solver_group(startGroup+1,filename,"j90");
//     solver_group(startGroup+2,filename,"j90");
//
//     // solver_group_TT(startGroup,filename,"j30");
//     // solver_group_TT(startGroup+1,filename,"j30");
//     // solver_group_TT(startGroup+2,filename,"j30");
//
//     solver_group_TT(startGroup,filename,"j60");
//     solver_group_TT(startGroup+1,filename,"j60");
//     solver_group_TT(startGroup+2,filename,"j60");
//
//
//     solver_group_TT(startGroup,filename,"j90");
//     solver_group_TT(startGroup+1,filename,"j90");
//     solver_group_TT(startGroup+2,filename,"j90");
//     // solver_group(startGroup+3,filename);
//     // solver_group(startGroup+4,filename);
//     // solver_group(startGroup+5,filename);
//     // solver_group(startGroup+6,filename);
//     // solver_group(startGroup+7,filename);
// }
//



