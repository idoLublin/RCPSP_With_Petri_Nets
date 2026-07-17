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

void runBenchmark(const std::string& problemType);
void runSingleConfig(const std::string& problemType, int configNum);
void runBenchmarkTT2(const std::string& problemType);
void applyConfigNum(int n);
void runConfigResume(const std::string& filename, const std::string& problemType, int startGroup, int startExam);
void runSingleConfigResume(const std::string& problemType, int configNum, int startGroup, int startExam);
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
    reset_mda_cache<N>();
    get_cbs_dominance_table<N>().clear();   // DR5 table is per-instance
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
    last.start_times[g_sink_id]=0;
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
        bool solved = (!path.empty() || first.rvs_activities_pool.empty());
        bool correct = (makespan == opt);

        file << (correct ? "True" : "False") << ","
             << problemType << ","
             << "CBS" << ","
             << opt << ",-1,";

        if (solved && !correct) {
            allcorrect = false;
            // exit(0); // only exit if we finished but got wrong answer

            // ── DEBUG: wrong answer — print schedule and scan for violations ──
            const RCPSPState_CBS<N>& dbgState = path.empty() ? first : path.back();
            std::cout << "\n=== WRONG ANSWER DEBUG (" << group << "," << exam
                      << ") makespan=" << makespan << " optimal=" << opt << " ===\n";

            // Root makespan (path[0] is the root state)
            short root_mk = path.empty() ? 0 : path[0].start_times[g_sink_id];
            std::cout << "Root makespan=" << root_mk << "\n";

            // Save ORIGINAL fc_stored for all path states BEFORE any recheck modifies them
            std::vector<bool> original_fc;
            original_fc.reserve(path.size());
            for (const auto& ps : path) original_fc.push_back(ps.found_conflict);

            // KEY DIAGNOSTIC 1: does compute_h_and_RVS detect the conflict NOW?
            bool fc_before = dbgState.found_conflict;  // = original_fc.back()
            short h_recheck = dbgState.compute_h_and_RVS();
            bool fc_after = dbgState.found_conflict;
            std::cout << "found_conflict in stored state (before recheck): " << fc_before << "\n";
            std::cout << "compute_h_and_RVS() recheck: h=" << h_recheck
                      << "  found_conflict_after=" << fc_after << "\n";

            // KEY DIAGNOSTIC 2: look up goal state in the OC list, check stored g
            uint64_t goalID;
            dataLocation goalLoc = astar.openClosedList.Lookup(as1.GetStateHash(dbgState), goalID);
            if (goalLoc != kNotFound) {
                const auto& goalItem = astar.openClosedList.Lookat(goalID);
                double stored_g     = goalItem.g;
                double expected_g   = (double)(dbgState.start_times[g_sink_id] - root_mk);
                double data_mk      = goalItem.data.start_times[g_sink_id];
                std::cout << "[OC-GOAL] stored_g=" << stored_g
                          << " expected_g=" << expected_g
                          << " data.makespan=" << data_mk
                          << " data.start_times[g_sink_id]=" << goalItem.data.start_times[g_sink_id]
                          << " stored_fc=" << goalItem.data.found_conflict
                          << " loc=" << goalLoc << "\n";
                if (std::abs(stored_g - expected_g) > 0.5)
                    std::cout << "  [!!] g INCONSISTENCY: stored_g=" << stored_g
                              << " but data.makespan - root = " << expected_g << "\n";
            } else {
                std::cout << "[OC-GOAL] goal state NOT FOUND in OC list (unexpected)\n";
            }

            // KEY DIAGNOSTIC 3: scan OC list for ALL g-data inconsistencies
            int inconsistent_count = 0;
            int fc_false_count = 0;
            for (int ii = 0; ii < (int)astar.openClosedList.size(); ii++) {
                const auto& item = astar.openClosedList.Lookat(ii);
                double item_expected_g = (double)(item.data.start_times[g_sink_id] - root_mk);
                if (std::abs(item.g - item_expected_g) > 0.5) {
                    inconsistent_count++;
                    if (inconsistent_count <= 5) { // print first 5
                        std::cout << "  [g-INCONSIST] idx=" << ii
                                  << " stored_g=" << item.g
                                  << " expected_g=" << item_expected_g
                                  << " data.mk=" << item.data.start_times[g_sink_id]
                                  << " fc=" << item.data.found_conflict << "\n";
                    }
                }
                if (!item.data.found_conflict) fc_false_count++;
            }
            std::cout << "OC list g-inconsistencies: " << inconsistent_count
                      << "  fc=false items: " << fc_false_count << "\n";

            // PATH REPLAY: walk the entire CBS path, use ORIGINAL fc values (not modified by recheck)
            std::cout << "CBS path replay (" << path.size() << " states):\n";
            for (int pi = 0; pi < (int)path.size(); pi++) {
                bool fc_stored = original_fc[pi];  // use saved original
                short h_pi = path[pi].compute_h_and_RVS();
                bool fc_now = path[pi].found_conflict;
                short mk = path[pi].start_times[g_sink_id];
                if (fc_stored != fc_now || (!fc_stored && pi < (int)path.size()-1)) {
                    std::cout << "  [!!] path[" << pi << "] makespan=" << mk
                              << " fc_stored=" << fc_stored << " fc_now=" << fc_now
                              << " h=" << h_pi << "\n";
                } else {
                    std::cout << "      path[" << pi << "] makespan=" << mk
                              << " fc_stored=" << fc_stored << " fc_now=" << fc_now << "\n";
                }
            }

            // Print full schedule
            std::cout << "Schedule:\n";
            for (int i = 0; i < (int)RCPSPex.activities.size(); i++) {
                std::cout << "  act[" << i << "]: start=" << dbgState.start_times[i]
                          << "  dur=" << RCPSPex.activities[i].duration
                          << "  finish=" << (dbgState.start_times[i] + RCPSPex.activities[i].duration)
                          << "\n";
            }

            // Independent resource-conflict check
            bool any_res_conflict = false;
            std::cout << "Resource conflicts:\n";
            for (int resIdx = 0; resIdx < (int)resource_info.size(); resIdx++) {
                const ResourceInfo& res = resource_info[resIdx];
                std::vector<short> events;
                for (short actIdx : res.activity_indices)
                    events.push_back(dbgState.start_times[actIdx]);
                std::sort(events.begin(), events.end());
                events.erase(std::unique(events.begin(), events.end()), events.end());
                for (short t : events) {
                    short total_demand = 0;
                    std::vector<short> conflicting;
                    for (int j2 = 0; j2 < (int)res.activity_indices.size(); j2++) {
                        short actIdx = res.activity_indices[j2];
                        short s = dbgState.start_times[actIdx];
                        short f = s + RCPSPex.activities[actIdx].duration;
                        if (s <= t && f > t) {
                            total_demand += res.demands[j2];
                            conflicting.push_back(actIdx);
                        }
                    }
                    if (total_demand > res.capacity) {
                        any_res_conflict = true;
                        std::cout << "  RES CONFLICT res=" << resIdx
                                  << " t=" << t
                                  << " demand=" << total_demand
                                  << "/" << res.capacity << " acts={";
                        for (short a : conflicting) std::cout << a << " ";
                        std::cout << "}\n";
                    }
                }
            }
            if (!any_res_conflict) std::cout << "  none\n";

            // Independent precedence check
            bool any_prec_viol = false;
            std::cout << "Precedence violations:\n";
            for (int i = 0; i < (int)RCPSPex.activities.size(); i++) {
                for (short dep : RCPSPex.backword_dependencies[i]) {
                    int predIdx = dep - 1;
                    short pred_finish = dbgState.start_times[predIdx]
                                      + RCPSPex.activities[predIdx].duration;
                    if (dbgState.start_times[i] < pred_finish) {
                        any_prec_viol = true;
                        std::cout << "  PREC VIOL act[" << i << "] starts=" << dbgState.start_times[i]
                                  << " but pred[" << predIdx << "] finishes=" << pred_finish << "\n";
                    }
                }
            }
            if (!any_prec_viol) std::cout << "  none\n";

            std::cout << "=== END DEBUG ===\n\n";
        }
    } else {
        Bounds b = getBounds(group, exam, problemType);
        bool solved = (!path.empty() || first.rvs_activities_pool.empty());
        bool correct = b.optimal_known ? (makespan == b.lb) : (makespan >= b.lb && makespan <= b.ub);

        file << (b.optimal_known ? (correct ? "True" : "False") : "Unknown") << ","
             << problemType << ","
             << "CBS" << ","
             << b.lb << "," << b.ub << ",";

        if (solved && !correct) {
            if (b.optimal_known) {
                allcorrect = false;

                // exit(0); // only exit if optimal known and we finished with wrong answer
            }
        }
    }
    file << p.NC << "," << p.RF << "," << p.RS << ","
         << ((!path.empty() || first.rvs_activities_pool.empty()) ? "True" : "False") << ","
         << astar.GetNodesExpanded() << ","
         << astar.GetNodesTouched() << ","
         << path.size() << ","
         << peakMemKB << ","
         << setting.use_first_conflict << ","
         << setting.use_conflict_prioritization << ","
         << (int)setting.heuristic << ","
         << setting.use_MDA_sets << ","
         << setting.use_MDA_cache << ","
         << setting.use_strong_constraints << ","
         << setting.use_MDA_BAB << ","
         << debug_cardinal_num / max(1, astar.GetNodesTouched()) << "\n";
    if (!allcorrect) {
        std::cout <<"Error: incorrect results" <<std::endl;
        // exit(0); // disabled: log wrong answers and continue benchmark
    }

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
template<int N>
int solveRCPSP_BAP_impl(int group, int exam, const std::string& filename, const std::string& problemType) {
    debug_cardinal_num = 0;
    reset_mda_cache<N>();
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
    last.t = 0;
    last.resourceType = -1;

    TemplateAStar<RCPSPState_CBS<N>, int, RCPSP_CBS<N>> astar;
    std::vector<RCPSPState_CBS<N>> path;

    std::chrono::duration<double> elapsed;
    auto start = std::chrono::high_resolution_clock::now();
    astar.GetPath(&as1, first, last, path);
    auto end = std::chrono::high_resolution_clock::now();
    elapsed = end - start;
    long peakMemKB = getPeakMemoryKB();

    int makespan = 0;
    bool solved = (!path.empty() || first.rvs_activities_pool.empty());

    if (solved) {
        RCPSPState_CBS<N>& finalState = path.empty() ? first : path.back();
        makespan = finalState.start_times[RCPSPex.activities.size() - 1] +
                   RCPSPex.activities[RCPSPex.activities.size() - 1].duration;
        std::cout << "\nFinal makespan: " << makespan << std::endl;
    } else {
        std::cout << "Path not found or timeout occurred.\n";
    }

    std::cout << "Nodes Expanded: " << astar.GetNodesExpanded() << std::endl;
    std::cout << "Nodes Touched: " << astar.GetNodesTouched() << std::endl;

    std::ofstream file(filename, std::ios::app);
    InstanceParams p = getParams(group);

    file << group << "," << exam << "," << elapsed.count() << "," << makespan << ",";

    if (problemType == "j30") {
        int opt = getOptimalMakespan(group, exam, problemType);
        bool correct = (makespan == opt);
        file << (correct ? "True" : "False") << "," << problemType << ",BAP," << opt << ",-1,";
        if (solved && !correct) allcorrect = false;
    } else {
        Bounds b = getBounds(group, exam, problemType);
        bool correct = b.optimal_known ? (makespan == b.lb) : (makespan >= b.lb && makespan <= b.ub);
        file << (b.optimal_known ? (correct ? "True" : "False") : "Unknown") << ","
             << problemType << ",BAP," << b.lb << "," << b.ub << ",";
        if (solved && !correct && b.optimal_known) allcorrect = false;
    }

    file << p.NC << "," << p.RF << "," << p.RS << ","
         << (solved ? "True" : "False") << ","
         << astar.GetNodesExpanded() << ","
         << astar.GetNodesTouched() << ","
         << path.size() << ","
         << peakMemKB << ","
         << setting.use_first_conflict << ","
         << setting.use_conflict_prioritization << ","
         << (int)setting.heuristic << ","
         << setting.use_MDA_sets << ","
         << setting.use_MDA_cache << ","
         << setting.use_strong_constraints << ","
         << setting.use_MDA_BAB << ","
         << debug_cardinal_num / max(1, astar.GetNodesTouched()) << "\n";

    return 0;
}

int solveRCPSP_BAP(int group, int exam, const std::string& filename, const std::string& problemType = "j30") {
    std::cout << "started solving BAP: " << group << ":" << exam << std::endl;
    setProblemSize(problemType);
    if (problemType == "j30")       return solveRCPSP_BAP_impl<32>(group, exam, filename, problemType);
    else if (problemType == "j60")  return solveRCPSP_BAP_impl<62>(group, exam, filename, problemType);
    else if (problemType == "j90")  return solveRCPSP_BAP_impl<92>(group, exam, filename, problemType);
    else if (problemType == "j120") return solveRCPSP_BAP_impl<122>(group, exam, filename, problemType);
    return 0;
}

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

void applyConfig(bool prio, bool first, HeuristicType h, bool mda) {
    setting.use_conflict_prioritization = prio;
    setting.use_first_conflict          = first;
    setting.heuristic                   = h;
    setting.use_MDA_sets                = mda;
    setting.use_MDA_cache               = mda;
    setting.use_MDA_BAB                 = mda;
    setting.use_strong_constraints      = false;
}

void runCrashDiagnostic() {
    // Tests exactly the 4 instances that caused crashes in previous runs,
    // across all 8 configs, with no exit on wrong answer.
    // Crash points found from last-written rows in output_46/49/56/57:
    //   Prio only  (output_46): crashed on (29,3)
    //   Prio+MDA   (output_49): crashed on (25,7)
    //   MDA only   (output_56): crashed on (41,9)
    //   H+MDA      (output_57): crashed on (46,7)
    std::string folder = "new_results";
    std::string baseName = "output_";
    std::string extension = ".csv";
    std::string filename = getNextFilename(folder, baseName, extension);
    std::ofstream file(filename);
    if (!file.is_open()) { std::cerr << "Error opening file!" << std::endl; return; }
    file << "group,exam,time,makespan,correct,setType,model,optimalOrLB,UB,NC,RF,RS,"
         << "finished,expandNumber,generatedNumber,depth,maxMem,"
         << "useFirst,useConflictPrioritization,useHeuristic,useMDASets,useMDACache,useStrongConstraints,useMDABAB,cardinalityRatio"
         << std::endl;
    file.close();

    const std::vector<std::pair<int,int>> suspects = {{29,3},{25,7},{41,9},{46,7}};

    auto runSuspects = [&]() {
        allcorrect = true;
        for (auto [g, e] : suspects)
            solveRCPSP_CBS(g, e, filename, "j30");
        std::cout << (allcorrect ? "All correct" : "Some INCORRECT") << std::endl;
    };

    std::cout << "\n=== DIAG: Baseline ===" << std::endl;
    applyConfig(false, true, HeuristicType::NONE, false);
    runSuspects();

    std::cout << "\n=== DIAG: Prio only ===" << std::endl;
    applyConfig(true, false, HeuristicType::NONE, false);
    runSuspects();

    std::cout << "\n=== DIAG: H only ===" << std::endl;
    applyConfig(false, false, HeuristicType::HCBS, false);
    runSuspects();

    std::cout << "\n=== DIAG: MDA only ===" << std::endl;
    applyConfig(false, false, HeuristicType::NONE, true);
    runSuspects();

    std::cout << "\n=== DIAG: Prio + H ===" << std::endl;
    applyConfig(true, false, HeuristicType::HCBS, false);
    runSuspects();

    std::cout << "\n=== DIAG: Prio + MDA ===" << std::endl;
    applyConfig(true, false, HeuristicType::NONE, true);
    runSuspects();

    std::cout << "\n=== DIAG: H + MDA ===" << std::endl;
    applyConfig(false, false, HeuristicType::HCBS, true);
    runSuspects();

    std::cout << "\n=== DIAG: All features ===" << std::endl;
    applyConfig(true, false, HeuristicType::HCBS, true);
    runSuspects();

    std::cout << "\nDiagnostic done -> " << filename << std::endl;
}

void runWrongAnswerDebug() {
    // Only the 4 instance-config combos that gave wrong answers in output_59.csv.
    // All other combos timed out — skip them to save time.
    std::string folder = "new_results";
    std::string baseName = "output_";
    std::string extension = ".csv";
    std::string filename = getNextFilename(folder, baseName, extension);
    std::ofstream file(filename);
    if (!file.is_open()) { std::cerr << "Error opening file!" << std::endl; return; }
    file << "group,exam,time,makespan,correct,setType,model,optimalOrLB,UB,NC,RF,RS,"
         << "finished,expandNumber,generatedNumber,depth,maxMem,"
         << "useFirst,useConflictPrioritization,useHeuristic,useMDASets,useMDACache,useStrongConstraints,useMDABAB,cardinalityRatio"
         << std::endl;
    file.close();

    // (29,3) Prio only  — was wrong: makespan=53, optimal=78
    std::cout << "\n=== Prio only | (29,3) ===\n";
    applyConfig(true, false, HeuristicType::NONE, false);
    solveRCPSP_CBS(29, 3, filename, "j30");

    // (41,9) MDA only   — was wrong: makespan=86, optimal=92
    std::cout << "\n=== MDA only | (41,9) ===\n";
    applyConfig(false, false, HeuristicType::NONE, true);
    solveRCPSP_CBS(41, 9, filename, "j30");

    // (25,7) Prio+MDA   — was wrong: makespan=85, optimal=95
    std::cout << "\n=== Prio+MDA | (25,7) ===\n";
    applyConfig(true, false, HeuristicType::NONE, true);
    solveRCPSP_CBS(25, 7, filename, "j30");

    // (46,7) H+MDA      — was wrong: makespan=53, optimal=59
    std::cout << "\n=== H+MDA | (46,7) ===\n";
    applyConfig(false, false, HeuristicType::HCBS, true);
    solveRCPSP_CBS(46, 7, filename, "j30");

    std::cout << "\nDebug run done -> " << filename << "\n";
}

// ── Non-minimal delay correctness test ───────────────────────────────────────
// Usage:  Driver_bench nmd_test
void runNonMinimalDelayTest() {
    // Problems with real resource conflicts — not trivially easy.
    // Chosen because they have 40k-400k TT2 node expansions.
    const std::vector<std::pair<int,int>> cases = {
        {1,7}, {4,3}, {4,9}, {10,2}, {2,1}
    };
    const std::vector<std::pair<int,std::string>> configs = {
        {1, "Baseline"},   // exercises single-delay path (Location C + D)
        {4, "MDA-only"},   // exercises MDA path (Location A + B)
    };

    std::string filename = getNextFilename("new_results", "output_nmd_test_", ".csv");
    { std::ofstream hdr(filename);
      hdr << "group,exam,time,makespan,correct,setType,model,optimalOrLB,UB,NC,RF,RS,"
          << "finished,expandNumber,generatedNumber,depth,maxMem,"
          << "useFirst,useConflictPrioritization,useHeuristic,useMDASets,useMDACache,"
          << "useStrongConstraints,useMDABAB,cardinalityRatio\n"; }

    setting.use_non_minimal_delay = true;

    for (auto& [cfgNum, cfgName] : configs) {
        applyConfigNum(cfgNum);
        allcorrect = true;
        std::cout << "\n=== NMD | cfg" << cfgNum << " (" << cfgName << ") ===\n";
        for (auto& [g, e] : cases)
            solveRCPSP_CBS(g, e, filename, "j30");
        std::cout << (allcorrect ? "  ALL CORRECT\n" : "  *** ERRORS ***\n");
    }

    setting.use_non_minimal_delay = false;
    std::cout << "\nResults: " << filename << "\n";
}

// Sweep one exam across a range of parameter groups, into a single CSV. Walks the
// whole NC x RF x RS grid quickly instead of grinding exam-by-exam through group 1,
// which is what correctness validation actually needs.
void runSweep(const std::string& ptype, int cfg, int startG, int endG, int exam) {
    applyConfigNum(cfg);
    std::string f = getNextFilename("new_results",
        "output_sweep_" + ptype + "_cfg" + std::to_string(cfg) + "_e" + std::to_string(exam) + "_", ".csv");
    { std::ofstream h(f);
      h << "group,exam,time,makespan,correct,setType,model,optimalOrLB,UB,NC,RF,RS,"
        << "finished,expandNumber,generatedNumber,depth,maxMem,useFirst,useConflictPrioritization,"
        << "useHeuristic,useMDASets,useMDACache,useStrongConstraints,useMDABAB,cardinalityRatio\n"; }
    for (int g = startG; g <= endG; g++) {
        std::cout << "\n=== sweep cfg" << cfg << " group " << g << " exam " << exam << " ===\n";
        solveRCPSP_CBS(g, exam, f, ptype);
    }
    std::cout << "\nsweep done -> " << f << std::endl;
}

// ── DR5/B&P dominance diagnostics ────────────────────────────────────────────
// solve_from_state: exact optimal makespan of the subtree rooted at an arbitrary
// state, with dominance OFF. Lets us test the dominance CLAIM directly:
// "S dominates S'" asserts optimum(S) <= optimum(S'). Any dumped pair violating
// that is a concrete counterexample to the rule as implemented.
template<short N>
int solve_from_state(const std::vector<int>& starts) {
    const bool saved = setting.use_dr5;
    setting.use_dr5 = false;
    reset_mda_cache<N>();

    RCPSP_CBS<N> env;
    RCPSPState_CBS<N> s;
    for (int i = 0; i < (int)starts.size() && i < N; i++) s.start_times[i] = (short)starts[i];
    // Repair precedence consistency. A no-op for real dumped states (already
    // consistent, and propagate only pushes forward), but it makes hand-built
    // test vectors legal instead of silently garbage.
    s.propagate(0);
    s.rvs_activities_pool.clear();
    s.added_precedences.clear();

    RCPSPState_CBS<N> goal = s;
    goal.start_times[g_sink_id] = 0;
    goal.resourceType = -1;
    goal.rvs_activities_pool.clear();

    TemplateAStar<RCPSPState_CBS<N>, int, RCPSP_CBS<N>> astar;
    std::vector<RCPSPState_CBS<N>> path;
    astar.GetPath(&env, s, goal, path);

    const int sink = (int)RCPSPex.activities.size() - 1;
    const RCPSPState_CBS<N>& fin = path.empty() ? s : path.back();
    setting.use_dr5 = saved;
    return fin.start_times[sink] + RCPSPex.activities[sink].duration;
}

// Driver_x verifydom <type> <group> <exam> <cfg> <pairsfile>
void runVerifyDom(const std::string& ptype, int group, int exam, int cfg,
                  const std::string& pairsFile) {
    setProblemSize(ptype);
    getRCPSP(RCPSPex, group, exam, ptype);
    resource_info.clear(); downstream.clear(); upstream.clear();
    precomputeDownstream(); precomputeUpstream(); precomputeResourceInfo();
    applyConfigNum(cfg);

    // SELF-TEST the instrument before trusting it. Solving from the root must
    // reproduce the known optimum; solving from a deliberately delayed schedule
    // must get worse. If either fails, solve_from_state is ignoring the start
    // times it is given and any "0 violations" verdict is meaningless.
    {
        // The genuine root is the default-constructed state's earliest-start
        // schedule, NOT an all-zeros vector (which violates precedence).
        RCPSPState_CBS<32> rootState;
        std::vector<int> root(RCPSPex.activities.size());
        for (int i = 0; i < (int)root.size(); i++) root[i] = rootState.start_times[i];

        int optRoot = solve_from_state<32>(root);
        int known   = getOptimalMakespan(group, exam, ptype);

        std::vector<int> shifted = root;
        shifted[1] = root[1] + 15;             // pin activity 1 late; must not help
        int optShifted = solve_from_state<32>(shifted);

        std::cout << "[self-test] optimum(root)=" << optRoot << "  known optimum=" << known
                  << "  optimum(act1 pinned +15)=" << optShifted << std::endl;
        if (optRoot != known)
            std::cout << "[self-test] FAIL: cannot reproduce the known optimum from the root — "
                         "verifier is unsound, ignore its verdict.\n";
        else if (optShifted < optRoot)
            std::cout << "[self-test] FAIL: pinning IMPROVED the makespan — start_times ignored.\n";
        else
            std::cout << "[self-test] PASS: verifier reproduces the optimum and respects start_times.\n";
    }

    std::ifstream in(pairsFile);
    if (!in.is_open()) { std::cerr << "cannot open " << pairsFile << "\n"; return; }

    auto parse = [](const std::string& csv) {
        std::vector<int> v; std::stringstream ss(csv); std::string tok;
        while (std::getline(ss, tok, ',')) if (!tok.empty()) v.push_back(std::atoi(tok.c_str()));
        return v;
    };

    std::string line;
    int checked = 0, violations = 0;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        std::stringstream ss(line);
        std::string rvstS, domS, prunedS;
        std::getline(ss, rvstS, ';'); std::getline(ss, domS, ';'); std::getline(ss, prunedS, ';');
        std::vector<int> dom = parse(domS), pruned = parse(prunedS);
        if (dom.empty() || pruned.empty()) continue;

        int optDom    = solve_from_state<32>(dom);
        int optPruned = solve_from_state<32>(pruned);
        ++checked;
        if (checked <= 8)
            std::cout << "pair " << checked << ": optimum(dominating)=" << optDom
                      << "  optimum(pruned)=" << optPruned << "\n";
        if (optDom > optPruned) {
            ++violations;
            std::cout << "VIOLATION #" << violations << "  rvst=" << rvstS
                      << "  optimum(dominating)=" << optDom
                      << " > optimum(pruned)=" << optPruned << "\n";
            std::cout << "  dominating: " << domS << "\n  pruned    : " << prunedS << "\n";
            if (violations >= 3) break;
        }
    }
    std::cout << "\nverifydom: checked=" << checked << " violations=" << violations << std::endl;
}

int main(int argc, char* argv[]) {
    // Usage:
    //   Driver_bench <size> <cfg>    — CBS: single config, for parallel runs
    //   Driver_bench <size>          — CBS: all 8 configs for one size, sequentially
    //   Driver_bench                 — CBS: all sizes all configs, fully sequential
    //   Driver_bench tt2_<size>      — TT2: run all problems for one size
    //   Driver_bench nmd_test        — correctness test for use_non_minimal_delay flag
    //
    // <size> : j30 | j60 | j90 | j120
    // <cfg>  : 1..8  (1=Baseline, 2=Prio, 3=H, 4=MDA, 5=Prio+H, 6=Prio+MDA, 7=H+MDA, 8=All)
    //
    // env RCPSP_DR5=1 : enable DR5 cutset dominance (DominanceCBS.h) on top of the
    //                   chosen config. Off by default, and applyConfig() never touches
    //                   it, so the 8 configs above are unchanged unless this is set.
    //                   Env rather than a flag because argv is positional here.
    // Rule selection is independent of the on/off flag, so it also applies to the
    // diagnostic modes below (which enable dominance directly).
    if (const char* r = std::getenv("RCPSP_DOM_RULE")) {
        const std::string rs(r);
        g_dom_rule = (rs == "dr5")  ? DOM_DR5
                   : (rs == "dr5s") ? DOM_DR5S
                   : (rs == "both") ? DOM_BOTH
                                    : DOM_BP;
    }
    if (const char* e = std::getenv("RCPSP_DR5")) {
        setting.use_dr5 = (std::atoi(e) != 0);
        std::cout << "State dominance: " << (setting.use_dr5 ? "ON" : "OFF") << "  rule="
                  << (g_dom_rule == DOM_DR5 ? "DR5" : g_dom_rule == DOM_DR5S ? "DR5S"
                      : g_dom_rule == DOM_BOTH ? "B&P+DR5S" : "Bell&Park")
                  << std::endl;
    }
    if (argc >= 2) {
        std::string arg1 = argv[1];
        if (arg1 == "sweep") {
            // sweep <type> <cfg> <startGroup> <endGroup> <exam>
            if (argc < 7) { std::cerr << "Usage: sweep <type> <cfg> <startG> <endG> <exam>\n"; return 1; }
            runSweep(argv[2], std::atoi(argv[3]), std::atoi(argv[4]), std::atoi(argv[5]), std::atoi(argv[6]));
        } else if (arg1 == "dumpdom") {
            // dumpdom <type> <group> <exam> <cfg> <outfile> — run one instance with
            // dominance ON, appending every prune pair to <outfile>.
            if (argc < 7) { std::cerr << "Usage: dumpdom <type> <group> <exam> <cfg> <outfile>\n"; return 1; }
            applyConfigNum(std::atoi(argv[5]));
            setting.use_dr5 = true;
            g_dom_dump_path = argv[6];
            std::remove(g_dom_dump_path.c_str());
            std::string f = getNextFilename("new_results", "output_dumpdom_", ".csv");
            { std::ofstream h(f); h << "group,exam,time,makespan,correct,setType,model,optimalOrLB,UB,NC,RF,RS,"
                 << "finished,expandNumber,generatedNumber,depth,maxMem,useFirst,useConflictPrioritization,"
                 << "useHeuristic,useMDASets,useMDACache,useStrongConstraints,useMDABAB,cardinalityRatio\n"; }
            solveRCPSP_CBS(std::atoi(argv[3]), std::atoi(argv[4]), f, argv[2]);
            std::cout << "prune pairs -> " << g_dom_dump_path << std::endl;
        } else if (arg1 == "verifydom") {
            // verifydom <type> <group> <exam> <cfg> <pairsfile>
            if (argc < 7) { std::cerr << "Usage: verifydom <type> <group> <exam> <cfg> <pairsfile>\n"; return 1; }
            runVerifyDom(argv[2], std::atoi(argv[3]), std::atoi(argv[4]), std::atoi(argv[5]), argv[6]);
        } else if (arg1 == "nmd_test") {
            runNonMinimalDelayTest();
        } else if (arg1 == "resume") {
            // Usage: Driver_bench resume <problemType> <cfg> <startGroup> <startExam>
            // Example: Driver_bench resume j90 6 14 4
            if (argc < 6) {
                std::cerr << "Usage: Driver_bench resume <type> <cfg> <startGroup> <startExam>\n";
                return 1;
            }
            std::string ptype = argv[2];
            int cfg        = std::atoi(argv[3]);
            int startGroup = std::atoi(argv[4]);
            int startExam  = std::atoi(argv[5]);
            runSingleConfigResume(ptype, cfg, startGroup, startExam);
        } else if (arg1.rfind("tt2_", 0) == 0) {
            // TT2 mode: argument is "tt2_j30", "tt2_j60", "tt2_j90"
            std::string problemType = arg1.substr(4);
            runBenchmarkTT2(problemType);
        } else if (argc >= 3) {
            int cfg = std::atoi(argv[2]);
            runSingleConfig(argv[1], cfg);
        } else {
            runBenchmark(argv[1]);
        }
    } else {
        runNonMinimalDelayTest();
    }
    return 0;
}


void runConfig(const std::string& filename, const std::string& problemType) {
    allcorrect = true;
    for (int i = 1; i <= 48; i++) {
        for (int j = 1; j <= 10; j++) {
            solveRCPSP_CBS(i, j, filename, problemType);
        }
    }
    if (allcorrect)
        std::cout << "All correct" << std::endl;
    else
        std::cout << "Error: incorrect results" << std::endl;
}

void runConfigBAP(const std::string& filename, const std::string& problemType) {
    allcorrect = true;
    for (int i = 1; i <= 48; i++) {
        for (int j = 1; j <= 10; j++) {
            solveRCPSP_BAP(i, j, filename, problemType);
        }
    }
    if (allcorrect)
        std::cout << "All correct" << std::endl;
    else
        std::cout << "Error: incorrect results" << std::endl;
}


static const char* CFG_NAMES[] = {
    "", "baseline", "prio", "h", "mda", "prio_h", "prio_mda", "h_mda", "all"
};

void applyConfigNum(int n) {
    switch (n) {
        case 1: applyConfig(false, true,  HeuristicType::NONE, false); break; // Baseline
        case 2: applyConfig(true,  false, HeuristicType::NONE, false); break; // Prio only
        case 3: applyConfig(false, false, HeuristicType::HCBS, false); break; // H only
        case 4: applyConfig(false, false, HeuristicType::NONE, true);  break; // MDA only
        case 5: applyConfig(true,  false, HeuristicType::HCBS, false); break; // Prio + H
        case 6: applyConfig(true,  false, HeuristicType::NONE, true);  break; // Prio + MDA
        case 7: applyConfig(false, false, HeuristicType::HCBS, true);  break; // H + MDA
        case 8: applyConfig(true,  false, HeuristicType::HCBS, true);  break; // All features
        default:
            std::cerr << "Unknown config " << n << " (use 1-8)\n";
            std::exit(1);
    }
}

void runSingleConfig(const std::string& problemType, int configNum) {
    if (configNum < 1 || configNum > 8) {
        std::cerr << "Config must be 1-8\n"; std::exit(1);
    }
    std::string folder = "new_results";
    std::string baseName = "output_" + problemType + "_cfg" + std::to_string(configNum) + "_";
    std::string filename = getNextFilename(folder, baseName, ".csv");
    std::ofstream file(filename);
    if (!file.is_open()) { std::cerr << "Cannot open " << filename << "\n"; return; }
    file << "group,exam,time,makespan,correct,setType,model,optimalOrLB,UB,NC,RF,RS,"
         << "finished,expandNumber,generatedNumber,depth,maxMem,"
         << "useFirst,useConflictPrioritization,useHeuristic,useMDASets,useMDACache,useStrongConstraints,useMDABAB,cardinalityRatio"
         << std::endl;
    file.close();

    std::cout << "=== " << CFG_NAMES[configNum] << " | " << problemType << " ===" << std::endl;
    applyConfigNum(configNum);
    runConfig(filename, problemType);
    std::cout << "Done -> " << filename << std::endl;
}

// ── Resume a single config from a specific (group, exam) ─────────────────────
// Writes to a NEW file (no header — concatenate with the original file's header row
// using:  cat original.csv <(tail -n +2 resume.csv) > combined.csv)
void runConfigResume(const std::string& filename, const std::string& problemType,
                     int startGroup, int startExam) {
    // Safety: ensure NMD flag is off — it must not bleed into benchmark runs
    setting.use_non_minimal_delay = false;

    allcorrect = true;
    bool started = false;
    for (int i = 1; i <= 48; i++) {
        for (int j = 1; j <= 10; j++) {
            if (!started) {
                if (i < startGroup || (i == startGroup && j < startExam)) continue;
                started = true;
            }
            solveRCPSP_CBS(i, j, filename, problemType);
        }
    }
    std::cout << (allcorrect ? "All correct" : "Error: incorrect results") << std::endl;
}

void runSingleConfigResume(const std::string& problemType, int configNum,
                           int startGroup, int startExam) {
    if (configNum < 1 || configNum > 8) {
        std::cerr << "Config must be 1-8\n"; std::exit(1);
    }
    std::string folder = "new_results";
    std::string baseName = "output_" + problemType + "_cfg" + std::to_string(configNum) + "_resume_";
    std::string filename = getNextFilename(folder, baseName, ".csv");

    // Write header to the new file so it is self-contained
    { std::ofstream hdr(filename);
      if (!hdr.is_open()) { std::cerr << "Cannot open " << filename << "\n"; return; }
      hdr << "group,exam,time,makespan,correct,setType,model,optimalOrLB,UB,NC,RF,RS,"
          << "finished,expandNumber,generatedNumber,depth,maxMem,"
          << "useFirst,useConflictPrioritization,useHeuristic,useMDASets,useMDACache,"
          << "useStrongConstraints,useMDABAB,cardinalityRatio\n"; }

    std::cout << "=== " << CFG_NAMES[configNum] << " | " << problemType
              << " | resume from (" << startGroup << "," << startExam << ") ===" << std::endl;

    setting.use_non_minimal_delay = false;   // explicit guard
    applyConfigNum(configNum);
    runConfigResume(filename, problemType, startGroup, startExam);
    std::cout << "Done -> " << filename << std::endl;
}

void runBenchmark(const std::string& problemType) {
    std::string folder = "new_results";
    // Include problem type in filename so parallel processes don't race.
    std::string baseName = "output_" + problemType + "_";
    std::string extension = ".csv";
    std::string filename = getNextFilename(folder, baseName, extension);
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error opening file!" << std::endl;
        return;
    }
    file << "group,exam,time,makespan,correct,setType,model,optimalOrLB,UB,NC,RF,RS,"
         << "finished,expandNumber,generatedNumber,depth,maxMem,"
         << "useFirst,useConflictPrioritization,useHeuristic,useMDASets,useMDACache,useStrongConstraints,useMDABAB,cardinalityRatio"
         << std::endl;
    file.close();

    // --- Config 1: Baseline — no features ---
    std::cout << "\n=== Baseline | " << problemType << " ===" << std::endl;
    applyConfig(false, true, HeuristicType::NONE, false);
    runConfig(filename, problemType);

    // --- Config 2: Prio only ---
    std::cout << "\n=== Prio only | " << problemType << " ===" << std::endl;
    applyConfig(true, false, HeuristicType::NONE, false);
    runConfig(filename, problemType);

    // --- Config 3: H only (no prio, no MDA) ---
    std::cout << "\n=== H only | " << problemType << " ===" << std::endl;
    applyConfig(false, false, HeuristicType::HCBS, false);
    runConfig(filename, problemType);

    // --- Config 4: MDA only (no prio, no H) ---
    std::cout << "\n=== MDA only | " << problemType << " ===" << std::endl;
    applyConfig(false, false, HeuristicType::NONE, true);
    runConfig(filename, problemType);

    // --- Config 5: Prio + H, no MDA ---
    std::cout << "\n=== Prio + H (no MDA) | " << problemType << " ===" << std::endl;
    applyConfig(true, false, HeuristicType::HCBS, false);
    runConfig(filename, problemType);

    // --- Config 6: Prio + MDA, no H ---
    std::cout << "\n=== Prio + MDA (no H) | " << problemType << " ===" << std::endl;
    applyConfig(true, false, HeuristicType::NONE, true);
    runConfig(filename, problemType);

    // --- Config 7: H + MDA, no prio ---
    std::cout << "\n=== H + MDA (no prio) | " << problemType << " ===" << std::endl;
    applyConfig(false, false, HeuristicType::HCBS, true);
    runConfig(filename, problemType);

    // --- Config 8: All features (Prio + H + MDA) ---
    std::cout << "\n=== All features | " << problemType << " ===" << std::endl;
    applyConfig(true, false, HeuristicType::HCBS, true);
    runConfig(filename, problemType);

    std::cout << "\nBenchmark done (" << problemType << ") -> " << filename << std::endl;
}

void runConfigTT2(const std::string& filename, const std::string& problemType) {
    for (int i = 1; i <= 48; i++) {
        for (int j = 1; j <= 10; j++) {
            solveRCPSP_TT2(i, j, filename, problemType);
        }
    }
}

void runBenchmarkTT2(const std::string& problemType) {
    std::string folder = "new_results";
    std::string baseName = "output_tt2_" + problemType + "_";
    std::string filename = getNextFilename(folder, baseName, ".csv");
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error opening file: " << filename << std::endl;
        return;
    }
    // Header matches what solveRCPSP_TT2 writes per row
    file << "group,exam,time,solved,makespan,expandNumber,generatedNumber,depth,model,problemType,maxMem,LB"
         << std::endl;
    file.close();

    std::cout << "\n=== TT2 | " << problemType << " ===" << std::endl;
    runConfigTT2(filename, problemType);
    std::cout << "\nTT2 benchmark done (" << problemType << ") -> " << filename << std::endl;
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




extern "C" void renderScene() {}
