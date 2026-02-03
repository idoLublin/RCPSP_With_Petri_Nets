//
// Created by idol on 29/12/2024.
//
// Your First C++ Program

#include <iostream>
#include "RCPSPState.h"
#include "RCPSPState.cpp"
#include "../../HOG2/generic/TemplateAStar.h"
#include "../../HOG2/generic/BAE.h"
#include "../../HOG2/generic/EPEAStar.h"
#include <filesystem> // <--- 1. Make sure this is here
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
void runBenchmark();
void runSolvedProblems();
void sortCSV(const std::string& filename);
std::atomic<bool> cancel_requested(false);

std::atomic<bool> stop_printing1(false); // Flag to stop the printing thread

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
    getPetri(petri, group, exam,problemType);
    getRCPSP(RCPSPex, group, exam,problemType);

    RCPSPState_TT2 first;
    RCPSPState_TT2 last = first;


    last.g = HCost_TT2(last, first);

    RCPSP_TT2 as1;

    TemplateAStar<RCPSPState_TT2, int, RCPSP_TT2> astar;
    //EPEAStar<RCPSPState_TT, int, RCPSP_TT> astar;
    std::vector<RCPSPState_TT2> path;

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
        << "TT"<< ","
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
    RCPSPState_TT first_tt;
    RCPSPState_Bi first(first_tt);
    first.direction = true;

    // Create goal state properly
    RCPSPState_Bi last;
    last.direction = false;

    // Resize to match first
    last.activity_nodes.resize(first.activity_nodes.size());

    // Clear all activity nodes
    for (auto& pair : last.activity_nodes) {
        pair = {0, 0};
    }

    // Set final sink place
    if (finalstatename.empty()) {
        std::cerr << "ERROR: finalstatename not initialized!" << std::endl;
        return -1;
    }

    auto it = petri.place_name_to_id.find(finalstatename);
    if (it == petri.place_name_to_id.end()) {
        std::cerr << "ERROR: finalstatename not found!" << std::endl;
        return -1;
    }

    short finalID = it->second;
    if (finalID >= 4 && (finalID - 4) < last.activity_nodes.size()) {
        last.activity_nodes[finalID - 4] = {1, 0};
    }

    // Copy resources from first
    last.resource_nodes = first.resource_nodes;

    // CRITICAL: Mark all activities as finished AFTER everything else
    for (int i = 1; i <= petri.Transitions.size(); i++) {
        last.finishedActivitiys[i] = -1;  // Mark as finished
    }

    last.g = 0;
    last.f = last.g_f = last.g_b = last.h_f = last.h_b = 0;
    auto it2 = petri.place_name_to_id.find(finalstatename);

    // Always check if it was found to avoid a crash
    if (it2 != petri.place_name_to_id.end()) {
        // Use it2->second to get the actual ID integer
        last.finishedActivitiys[it2->second] = 0;
    }
    else {
        // Optional: Handle error if name not found
        std::cerr << "Error: Place " << finalstatename << " not found!" << std::endl;
    }
    std::cout << "Goal state setup - checking finished activities:" << std::endl;
    int goal_finished = 0;
    // for (int i = 0; i < 128; i++) {
    //     if (last.finishedActivitiys[i] != -1) goal_finished++;
    // }
    std::cout << "  Goal has " << goal_finished << " finished activities" << std::endl;
    // Set resources an
    std::vector<RCPSPState_Bi> path;
    ForwardRCPSPHeuristic H_F;
    BackwardRCPSPHeuristic H_B;
    RCPSP_BiGreedy bs1;

    BAE<RCPSPState_Bi, int, RCPSP_BiGreedy> Bi_RCPSP;

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

                std::cout << "Meeting point at index " << i << std::endl;
                std::cout << "  Forward: g=" << path[i].g << ", g_f=" << path[i].g_f << std::endl;
                std::cout << "  Backward: g=" << path[i+1].g << ", g_b=" << path[i+1].g_b << std::endl;
                std::cout << "  Makespan = g_f + g_b = " << path[i].g_f << " + " << path[i+1].g_b << " = " << makespan << std::endl;
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

// --- DELETE the entire parallel/sequential loop from your main() ---
// --- Replace main() with this simplified structure ---

// int main(int argc, char *argv[]) {
//     // Expects one argument: The starting Group ID for this batch (e.g., 1, 3, 5, etc.)
//     if (argc != 3) {
//         std::cerr << "Usage: ./Driver <START_GROUP_ID>" << std::endl;
//         return 1;
//     }
//
//     int startGroup = std::stoi(argv[1]);
//     std::string outputFolder = argv[2]; // Get folder from script
//     // This single execution will process TWO groups sequentially: G, G+1.
//     // G = startGroup (e.g., 1, 3, 5...)
//     // G+1 = next group (e.g., 2, 4, 6...)
//
//     // FIX: Use the Group ID directly in the filename
//     // This guarantees Job 14 writes to "..._14.csv" and Job 15 writes to "..._15.csv"
//     std::string filename = "results/batch_group_" + std::to_string(startGroup) + ".csv";
//
//     std::ofstream file(filename);
//     if (!file.is_open()) {
//         std::cerr << "❌ Error opening file: " << filename << std::endl;
//         return 1;
//     }
//     // Process Group 'G'
//     for (int j = 1; j < 11; j++) {
//         solveRCPSP(startGroup, j, filename, "j30");
//
//         // Same for TT
//         solveRCPSP_TT(startGroup, j, filename, "j30");    }
//
//     // Process Group 'G+1'
//
//     return 0;
// }

 int main() {
     runBenchmark();


    return 0;
}


void runBenchmark() {
    std::string folder = "new_results";
    std::string baseName = "output!!!!!!_";
    std::string extension = ".csv";

    std::string filename = getNextFilename(folder, baseName, extension);

    // Create and write to file
    std::ofstream file(filename);
    // Open file stream

    // Check if file is open
    if (!file.is_open()) {
        std::cerr << "Error opening file!" << std::endl;
        return;
    }

    // Write header
    //file << "group,exam,time,finished,makespan,expand number,generated number,generatedTime%,generatedTime(ave),avilableTime%,avilableTime(ave),hashTime%,hashTime(ave),HcostTime%,HcostTime(ave)" << std::endl;
    file << "group,exam,time,finished,makespan,expand number,generated number,depth,PetriType,SetType,Use CS,generatedTime%,generatedTime(ave),avilableTime%,avilableTime(ave),hashTime%,hashTime(ave),HcostTime%,HcostTime(ave),hashTime(ave),comperTime%,comperTime(ave),succsesroTime%,sucssesorTime(ave)" << std::endl;
    //file << "group,exam,initialHcost" << std::endl;
 //omp_set_num_threads(10);
     //omp_set_num_threads(2); // 1. Set the core count.
     //#pragma omp parallel for collapse(2) schedule(dynamic)
   //  solveRCPSP_Bi(16, 1, filename, "j30");
    // solveRCPSP_TT(16, 9, filename, "j30");
solveRCPSP_TT2(16,8,filename,"j30");
solveRCPSP_TT2(16,9,filename,"j30");
solveRCPSP_TT2(16,4,filename,"j30");
    solveRCPSP_TT(16,8,filename,"j30");
    solveRCPSP_TT(16,9,filename,"j30");
    solveRCPSP_TT(16,4,filename,"j30");
    // for(int i = 1; i < 49; i++) {
    //     for(int j = 1; j < 11; j++) {
    //
    //         // 1. CLEAN THE SLATE (Crucial for thread_local variables)
    //         petri.reset();
    //         RCPSPex.reset();
    //
    //         // 2. SOLVE
    //         solveRCPSP(i, j, filename, "j30");
    //         //solveRCPSP_TT(i, j, filename, "j30");
    //     }
    // }


    // for(int i = 16; i < 17; i++) {
    //     for(int j = 10; j >0; j--) {
    //
    //         // 1. CLEAN THE SLATE (Crucial for thread_local variables)
    //         petri.reset();
    //         RCPSPex.reset();
    //
    //         // 2. SOLVE
    //         //        solveRCPSP(i, j, filename, "j30");
    //         solveRCPSP_TT(i, j, filename, "j30");
    //     }
    // }


    // for(int i = 16; i < 17; i++) {
    //     for(int j = 1; j < 11; j++) {
    //
    //         // 1. CLEAN THE SLATE (Crucial for thread_local variables)
    //         petri.reset();
    //         RCPSPex.reset();
    //
    //         // 2. SOLVE
    //         //        solveRCPSP(i, j, filename, "j30");
    //         solveRCPSP_TT(i, j, filename, "j30");
    //     }
    // }
    // solveRCPSP_TT(9, 1, filename, "j30");

    // solveRCPSP(9, 1, filename, "j30");


   // solveRCPSP_TT(9, 1, filename, "j30");

   // sortCSV(filename);


    // solveRCPSP(-1,-1,filename,"j30");
    //
    // for(int i=16;i<17;i++) {
    //      for(int j=5;j<11;j++) {
    //      solveRCPSP(i,j,filename,"j30");
    //      solveRCPSP_TT(i,j,filename,"j30");
    //   //   getinitialHcost(i,j,filename);
    //      }
    //  }

     //solveRCPSP(34, 9, filename);
  //   solveRCPSP(34, 10, filename);



    // solveRCPSP(34, 9, filename);
    // solveRCPSP_TT(34, 9, filename);
    // useCS=false;
    // solveRCPSP(34, 9, filename);
    // solveRCPSP_TT(34, 9, filename);
    // useCS=true;
    // solveRCPSP(34, 9, filename);
    // solveRCPSP_TT(34, 9, filename);

    useCS=false;
    //solveRCPSP(33,9,filename,"j60");
    //solveRCPSP(33,10,filename,"j60");
     //for(int i=34;i<49;i++) {
       // for(int j=1;j<11;j++) {
      //  solveRCPSP(i,j,filename,"j60");
       // }
   // }
   // solveRCPSP(48,5,filename,"j90");
    //solveRCPSP(48,6,filename,"j90");
    //solveRCPSP(48,7,filename,"j90");
    //solveRCPSP(48,8,filename,"j90");
    //solveRCPSP(48,9,filename,"j90");
    //solveRCPSP(48,10,filename,"j90");

    //for(int i=26;i<49;i++) {
       // for(int j=1;j<11;j++) {
          //  solveRCPSP(i,j,filename,"j90");
       // }
    //}


    // J120 לא רץ

    // for(int i=1;i<49;i++) {
    //     for(int j=1;j<11;j++) {
    //         solveRCPSP(i,j,filename,"j120");
    //     }
    // }

   // for(int i=1;i<49;i++) {
   //     for(int j=1;j<11;j++) {
   //         solveRCPSP_TT(i,j,filename,"j120");
   //     }
    //}


    useCS=true;

//    for(int i=1;i<49;i++) {
  //      for(int j=1;j<11;j++) {
    //        solveRCPSP(i,j,filename,"j90");
      //  }
   // }

   // for(int i=1;i<49;i++) {
       // for(int j=1;j<11;j++) {
           // solveRCPSP(i,j,filename,"j120");
      //  }
   // }

   // for(int i=1;i<49;i++) {
 //       for(int j=1;j<11;j++) {
   //         solveRCPSP_TT(i,j,filename,"j30");
 //       }
   // }

    // for(int i=17;i<49;i++) {
    //     for(int j=1;j<11;j++) {
    //         solveRCPSP_TT(i,j,filename,"j60");
    //     }
    // }

    // for(int i=1;i<49;i++) {
    //     for(int j=1;j<11;j++) {
    //         solveRCPSP_TT(i,j,filename,"j90");
    //     }
    // }

    //for(int i=1;i<49;i++) {
       //for(int j=1;j<11;j++) {
           // solveRCPSP_TT(i,j,filename,"j120");
      //  }
   // }


   //  for(int i=1;i<49;i++) {
   //     for(int j=1;j<11;j++) {
   //     solveRCPSP(i,j,filename);
   //     //getinitialHcost(i,j,filename);
   //     }
   // }


    // for(int i=1;i<49;i++) {
    //      for(int j=1;j<11;j++) {
    //      solveRCPSP_TT(i,j,filename);
    //   //   getinitialHcost(i,j,filename);
    //      }
    //  }

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



