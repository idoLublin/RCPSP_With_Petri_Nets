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
#include <ctime>            // timestamped, self-describing run filenames
namespace fs = std::filesystem;
#include "RCPSP.h"
#include "LazyAStarCBS.h"   // RCPSP_LAZY=1: deferred-heuristic A* (compute conflicts/h at pop, not insertion)
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
#include <sstream>
#include <tuple>
#include <map>
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
inline int serialSGS_makespan();   // feasible-schedule UB seed (defined below)
int getDatasheetUB(int group, int exam, const std::string& problemType); // datasheet UB (defined below)
void runBenchmarkTT2(const std::string& problemType);
void runBenchmarkTT2BAE(const std::string& problemType);
void runCbsInitF(const std::string& problemType);   // root-state f (no search) for heuristic eval
void runTt2InitF(const std::string& problemType);   // TT2 root-state f (no search)
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

// ── Root trivial-optimality shortcut for TT2 / TTPNR ─────────────────────────
// If the CPM/EST schedule (every activity started at its earliest
// precedence-feasible time, resources ignored) is ALSO resource-feasible, then
// it attains the CPM lower bound while satisfying every constraint, so it is
// provably optimal — we can return it immediately and skip A* entirely.
//
// Fills estStart (1-based; index = activity id) and outMakespan, and returns
// true iff the EST schedule is resource-feasible. One-directional: a false
// result just means "run the normal search", so there is no correctness risk.
static bool tryTrivialRootSchedule_TT2(std::vector<int>& estStart, int& outMakespan) {
    const int N = static_cast<int>(RCPSPex.activities.size());
    if (N == 0) return false;

    // 1. Earliest start times (longest path over precedence, resources ignored).
    //    Fixpoint iteration so we do NOT rely on activity ids being topologically
    //    ordered — an under-estimated EST could otherwise fire the shortcut on an
    //    infeasible schedule.
    estStart.assign(N + 1, 0);
    bool changed = true;
    while (changed) {
        changed = false;
        for (int i = 1; i <= N; ++i) {
            int est = 0;
            for (short pred : RCPSPex.backword_dependencies[i - 1]) {
                int predFinish = estStart[pred] + RCPSPex.activities[pred - 1].duration;
                if (predFinish > est) est = predFinish;
            }
            if (est != estStart[i]) { estStart[i] = est; changed = true; }
        }
    }

    // 2. CPM makespan = latest earliest-finish over all activities (== the root
    //    heuristic getForwardHcost_TT, which is the admissible CPM lower bound).
    int H = 0;
    for (int i = 1; i <= N; ++i)
        H = std::max(H, estStart[i] + static_cast<int>(RCPSPex.activities[i - 1].duration));
    outMakespan = H;

    // 3. Whole-profile resource-feasibility check: for each resource, accumulate
    //    demand over EVERY time unit and verify capacity is never exceeded. This
    //    is a full timeline sweep (not per-activity / pairwise), so it catches a
    //    3+-way overlap that a local check would miss.
    for (const auto& [resName, capacity] : RCPSPex.resources) {
        std::vector<int> usage(H + 1, 0);
        for (int i = 1; i <= N; ++i) {
            const auto& act = RCPSPex.activities[i - 1];
            const int dur = act.duration;
            if (dur <= 0) continue;
            auto it = act.resource_demands.find(resName);
            if (it == act.resource_demands.end() || it->second <= 0) continue;
            const int d = it->second;
            const int s = estStart[i];
            for (int t = s; t < s + dur; ++t) usage[t] += d;
        }
        for (int t = 0; t < H; ++t)
            if (usage[t] > capacity) return false;
    }
    return true;
}

// ── Reversed-instance backward ────────────────────────────────────────────────
// RCPSP is symmetric under precedence reversal: the reversed instance has the SAME
// optimal makespan. Reversing the loaded petri/RCPSP in place turns the existing fast
// forward solver into an efficient, correct BACKWARD (reverse-net) search — inheriting
// all its DR/heuristic machinery. A reverse-net state translates to a forward cut by
// θ_fwd = τ − θ_bwd on the shared (active) activities (the meet key, used later for BAE*).
static bool g_tt2_reverse = false;
inline void reverseLoadedInstance() {
    // 1. swap successor/predecessor lists (getAvailable reads backword_dependencies)
    std::swap(RCPSPex.dependencies, RCPSPex.backword_dependencies);
    // 2. flip each transition's ACTIVITY arcs (placeID>=4): old outputs->inputs, old
    //    inputs->outputs. Resources stay symmetric: keep resource-return arcs (arcs_out,
    //    placeID<4); drop old resource-consume arcs in arcs_in (consumption uses
    //    resource_demands, which the ctor reads directly and which is unchanged).
    for (auto& t : petri.Transitions) {
        std::vector<std::pair<short,short>> nin, nout;
        for (auto& pr : t.arcs_out_indices) { if (pr.first >= 4) nin.push_back(pr); else nout.push_back(pr); }
        for (auto& pr : t.arcs_in_indices)  { if (pr.first >= 4) nout.push_back(pr); }
        t.arcs_in_indices  = std::move(nin);
        t.arcs_out_indices = std::move(nout);
    }
    // 3. flip place string-arc maps so source/sink swap (the ctor finds source via empty
    //    arcs_in, sink via empty arcs_out). Then clear the OLD source's initial token so
    //    it does not seed a spurious token at the new sink; the new source (old sink) is
    //    tokened by name-match in the ctor.
    std::string oldSource;
    for (auto& p : petri.places) if (p.arcs_in.empty()) { oldSource = p.name; break; }
    for (auto& p : petri.places) std::swap(p.arcs_in, p.arcs_out);
    for (auto& p : petri.places) if (p.name == oldSource) p.state = {{0}};
}

 int solveRCPSP_TT2(int group, int exam, const std::string& filename,const std::string& problemType="j30") {
    std::cout << "started solving TT2: " << group<<":"<<exam << std::endl;
    count=0;
    LB=0;
    getPetri(petri, group, exam,problemType);
    getRCPSP(RCPSPex, group, exam,problemType);
    if (g_tt2_reverse) reverseLoadedInstance();   // reversed-instance backward (same optimum)
    // Instance RS for RS-adaptive gating (RCPSP_TT2_RSADAPT); group-derived cycle.
    // Must precede the root HCost_TT2 below so the gate/single-res see correct state.
    { static const double RSL[4] = {0.2, 0.5, 0.7, 1.0}; g_instance_rs = RSL[((group - 1) % 4 + 4) % 4]; }
    g_instance_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(astar_timeout_seconds); g_instance_deadline_set = true; // sub-solves share this 300s budget
    // The single-resource LB reads resource_info/upstream, which the TT2 path does not
    // otherwise populate — build them once here when the bound is enabled.
    if (g_tt2_singleres || g_tt2_ub || g_tt2_hierrs) { precomputeResourceInfo(); precomputeUpstream(); precomputeDownstream(); }
    if (g_tt2_hierrs) precomputeRSInflation();   // Kmin/Kmax profile + resolve target RS (needs g_instance_rs + resource_info)

    RCPSPState_TT2 first;
    RCPSPState_TT2 last = first;


    last.g = HCost_TT2(last, first);

    get_tt2_dominance_table().clear();   // DR5 table is per-instance (RCPSP_TT2_DR5=1)
    g_tt2_sym_pruned = 0;                // symmetry-breaking counter is per-instance
    g_tt2_dr4_pruned = 0;                // DR4 delayed-start prune counter is per-instance
    g_tt2_immsel_fired = 0;              // immediate-selection counter is per-instance
    g_tt2_freefire_fired = 0;            // free-fire counter is per-instance
    g_tt2_theta_better = 0;              // Θ-tree "bound beat existing" counter is per-instance
    g_tt2_singleres_better = g_tt2_singleres_calls = 0;   // single-resource LB telemetry, per-instance
    g_tt2_hierrs_better = g_tt2_hierrs_calls = g_cbs_hierrs_cache_hits = 0;   // hier-RS LB telemetry, per-instance
    g_hierrs_time_sec = 0.0; g_cbs_hierrs_cache.clear();   // hier-RS timer + result cache, per-instance
    g_heur_time_sec = g_singleres_time_sec = g_mincut_time_sec = 0.0; g_heur_calls = 0; g_root_h = -1.0; g_shadow_set.clear(); g_shadow_probes = g_shadow_hits = 0; g_succ_probes = g_succ_hits = g_hcache_probes = g_hcache_hits = 0; if (g_subsolve_succ_cache || g_h_cache) get_tt2_cache().clear();  // always-on instrumentation, per-instance
    // TT2 UB (RCPSP_TT2_UB=1): seed the incumbent from a REAL feasible primal (serial
    // SGS, not the datasheet-optima cheat). Off => incumbent stays MAX => no pruning.
    g_ub_pruned = 0; g_incumbent = std::numeric_limits<short>::max();
    if (g_tt2_ub) {
        int sgs = serialSGS_makespan();
        if (sgs > 0 && sgs < (int)std::numeric_limits<short>::max()) g_incumbent = (short)sgs;
        // DIAGNOSTIC ONLY (RCPSP_TT2_UB_SEED=<int>): override the incumbent to measure the
        // memory CEILING a tighter primal could reach. NOT for production (may inject a UB
        // below opt, which would be unsound) — use only to gauge UB's potential.
        if (const char* e = std::getenv("RCPSP_TT2_UB_SEED")) { int v = std::atoi(e); if (v > 0) g_incumbent = (short)v; }
        std::cout << "TT2 UB seed: SGS=" << sgs << " incumbent=" << g_incumbent << std::endl;
    }

    // ── Root trivial-optimality shortcut ────────────────────────────────────
    // If the CPM/EST schedule is already resource-feasible it is provably
    // optimal (attains the CPM lower bound) — return it without running A*.
    {
        auto tstart = std::chrono::high_resolution_clock::now();
        std::vector<int> trivEst;
        int trivMakespan = 0;
        if (tryTrivialRootSchedule_TT2(trivEst, trivMakespan)) {
            auto tend = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> telapsed = tend - tstart;
            long peakMemKB = getPeakMemoryKB();

            std::cout << "TT2 trivial-at-root: EST schedule is resource-feasible => "
                      << "optimal makespan=" << trivMakespan
                      << " (0 expansions, A* skipped)" << std::endl;
            std::cout << "{'scheduling': {";
            bool firstJob = true;
            int realJobCount = 0;
            for (int i = 1; i < (int)trivEst.size(); ++i) {
                int dur = RCPSPex.activities[i - 1].duration;
                if (!firstJob) std::cout << ", ";
                std::cout << "'" << i << "': " << (trivEst[i] + dur);  // finish time
                firstJob = false;
                realJobCount++;
            }
            std::cout << "}, 'total_jobs_scheduled': " << realJobCount
                      << ", 'makespan': " << trivMakespan
                      << ", 'solved': True, 'trivialAtRoot': True}" << std::endl;

            std::ofstream file(filename, std::ios::app);
            file << group << "," << exam << "," << telapsed.count() << ","
                 << "True" << ","
                 << trivMakespan << ","
                 << 0 << ","                       // expandNumber (A* never ran)
                 << 0 << ","                       // generatedNumber
                 << 0 << ","                       // depth
                 << "TT2" << ","
                 << problemType << ","
                 << peakMemKB << ","
                 << LB << ","
                 << g_tt2_dr5 << ","
                 << g_tt2_batch << ","
                 << g_tt2_batch_cap << ","
                 << get_tt2_dominance_table().pruned << ","
                 << get_tt2_dominance_table().thinned << ","
                 << get_tt2_dominance_table().checks << ","
                 << get_tt2_dominance_table().inserts << ","
                 << get_tt2_dominance_table().maxBucket << ","
                 << g_tt2_sym << ","
                 << g_tt2_sym_pruned << ","
                 << g_tt2_gendesc << ","
                 << g_tt2_sym_tiebreak << ","
                 << astar_timeout_seconds << ","
                 << 1 << ","                       // trivialAtRoot
                 << (g_tt2_theta ? 1 : 0) << ","   // useThetaBound
                 << g_tt2_theta_better << ","      // thetaBoundBetter (0 here: A* never ran)
                 << g_tt2_dr4 << "," << g_tt2_dr4_pruned << ","   // useTT2DR4, dr4Pruned
                 << g_tt2_immsel << "," << g_tt2_immsel_fired << ","      // useTT2ImmSel, immSelFired
                 << (g_tt2_rsadapt ? 1 : 0) << "," << g_tt2_rs_threshold << "," << g_instance_rs << ","   // useTT2RSAdapt,tt2RsThreshold,instanceRS
                 << (g_tt2_singleres ? 1 : 0) << "," << g_tt2_singleres_better << "," << g_tt2_singleres_calls << ","   // useTT2SingleRes,tt2SingleResBetter,tt2SingleResCalls,heuristicLowRS,heuristicHighRS,rootH,heurTimeSec,heurCalls,singleResTimeSec
                 << tt2_heuristic_name(true) << "," << tt2_heuristic_name(!g_tt2_rsadapt) << ","   // heuristicLowRS,heuristicHighRS
                 << g_root_h << "," << g_heur_time_sec << "," << g_heur_calls << "," << g_singleres_time_sec << "," << subsolver_name() << "," << g_shadow_probes << "," << g_shadow_hits   // ...,subsolver,shadowProbes,shadowHits
                 << "," << (g_tt2_ub ? 1 : 0) << "," << g_ub_pruned   // useTT2UB,ubPruned
                 << "," << (g_tt2_hierrs ? 1 : 0) << "," << g_tt2_hierrs_better << "," << g_tt2_hierrs_calls << "," << g_hierrs_time_sec << "," << g_cbs_hierrs_cache_hits   // useTT2HierRS,tt2HierRsBetter,tt2HierRsCalls,tt2HierRsTimeSec,tt2HierRsCacheHits
                 << "\n";
            return 0;
        }
    }

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

        // ── DIAGNOSTIC (RCPSP_DUMP_PENULT=1): dump the pre-sink (penultimate)
        //    state marking for external verification of the penultimate rule.
        //    Env-guarded; no effect on normal runs. Safe to delete. ──
        if (std::getenv("RCPSP_DUMP_PENULT") && path.size() >= 2) {
            const RCPSPState_TT2& pen = path[path.size() - 2];
            const int NT = (int)petri.Transitions.size();
            std::cout << "PENULT g=" << pen.g << " finished=";
            for (int i = 1; i <= NT; ++i) if (pen.finishedActivitiys.test(i)) std::cout << i << ",";
            std::cout << " active=";
            for (const auto& pr : pen.activeTransitionIndices) std::cout << pr.first << ":" << pr.second << ",";
            std::cout << " actTok=";
            for (int i = 0; i < (int)pen.activity_nodes.size(); ++i)
                if (pen.activity_nodes[i].first > 0)
                    std::cout << i << "=" << pen.activity_nodes[i].first << "^" << pen.activity_nodes[i].second << ",";
            std::cout << " resTok=";
            for (int r = 0; r < (int)pen.resource_nodes.size(); ++r) {
                if (pen.resource_nodes[r].empty()) continue;
                std::cout << "r" << r << "[";
                for (const auto& tk : pen.resource_nodes[r]) std::cout << tk.first << "^" << tk.second << " ";
                std::cout << "]";
            }
            std::cout << " GOALg=" << path.back().g << std::endl;
        }

        // ── DIAGNOSTIC (RCPSP_DUMP_PATH=1): dump every path state's activity-token
        //    marking for per-activity backward-transition verification. Env-guarded. ──
        if (std::getenv("RCPSP_DUMP_PATH")) {
            for (size_t i = 0; i < path.size(); ++i) {
                const RCPSPState_TT2& s = path[i];
                std::cout << "PATHSTATE i=" << i << " lt=" << s.lastTransitionId
                          << " g=" << s.g << " act=";
                for (size_t k = 0; k < s.activity_nodes.size(); ++k)
                    if (s.activity_nodes[k].first > 0)
                        std::cout << k << ":" << s.activity_nodes[k].first
                                  << "^" << s.activity_nodes[k].second << ",";
                std::cout << "\n";
            }
        }
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

    if (g_tt2_dr5) {
        auto& dt = get_tt2_dominance_table();
        std::cout << "TT2 DR5: pruned=" << dt.pruned << " checks=" << dt.checks
                  << " inserts=" << dt.inserts << " thinned=" << dt.thinned
                  << " comparisons=" << dt.comparisons
                  << " buckets=" << dt.bucketCount() << " maxBucket=" << dt.maxBucket << std::endl;
    }
    if (g_tt2_sym)
        std::cout << "TT2 SYM: successors skipped by canonical order = " << g_tt2_sym_pruned << std::endl;
    if (g_tt2_theta)
        std::cout << "TT2 THETA: bound beat existing at " << g_tt2_theta_better << " states" << std::endl;
    if (g_tt2_dr4)
        std::cout << "TT2 DR4: successors pruned = " << g_tt2_dr4_pruned << std::endl;
    if (g_tt2_immsel)
        std::cout << "TT2 IMMSEL: forced-single-branch nodes = " << g_tt2_immsel_fired << std::endl;

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
        // ── active TT2 feature flags + DR5 dominance stats (self-documenting run) ──
        << g_tt2_dr5 << ","
        << g_tt2_batch << ","
        << g_tt2_batch_cap << ","
        << get_tt2_dominance_table().pruned << ","
        << get_tt2_dominance_table().thinned << ","
        << get_tt2_dominance_table().checks << ","
        << get_tt2_dominance_table().inserts << ","
        << get_tt2_dominance_table().maxBucket << ","
        << g_tt2_sym << ","
        << g_tt2_sym_pruned << ","
        // ── remaining configurable TT2 knobs, appended for full run traceability ──
        << g_tt2_gendesc << ","
        << g_tt2_sym_tiebreak << ","
        << astar_timeout_seconds << ","
        << 0 << ","   // trivialAtRoot (shortcut did not fire on this instance)
        << (g_tt2_theta ? 1 : 0) << ","   // useThetaBound
        << g_tt2_theta_better << ","      // thetaBoundBetter
        << g_tt2_dr4 << "," << g_tt2_dr4_pruned << ","   // useTT2DR4, dr4Pruned
        << g_tt2_immsel << "," << g_tt2_immsel_fired << ","      // useTT2ImmSel, immSelFired
        << (g_tt2_rsadapt ? 1 : 0) << "," << g_tt2_rs_threshold << "," << g_instance_rs << ","   // useTT2RSAdapt,tt2RsThreshold,instanceRS
        << (g_tt2_singleres ? 1 : 0) << "," << g_tt2_singleres_better << "," << g_tt2_singleres_calls << ","   // useTT2SingleRes,tt2SingleResBetter,tt2SingleResCalls,heuristicLowRS,heuristicHighRS,rootH,heurTimeSec,heurCalls,singleResTimeSec
                 << tt2_heuristic_name(true) << "," << tt2_heuristic_name(!g_tt2_rsadapt) << ","   // heuristicLowRS,heuristicHighRS
                 << g_root_h << "," << g_heur_time_sec << "," << g_heur_calls << "," << g_singleres_time_sec << "," << subsolver_name() << "," << g_shadow_probes << "," << g_shadow_hits   // ...,subsolver,shadowProbes,shadowHits
                 << "," << (g_tt2_ub ? 1 : 0) << "," << g_ub_pruned   // useTT2UB,ubPruned
                 << "," << (g_tt2_hierrs ? 1 : 0) << "," << g_tt2_hierrs_better << "," << g_tt2_hierrs_calls << "," << g_hierrs_time_sec << "," << g_cbs_hierrs_cache_hits   // useTT2HierRS,tt2HierRsBetter,tt2HierRsCalls,tt2HierRsTimeSec,tt2HierRsCacheHits
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

// ── Inflated-resource warm start (RCPSP_WARMSTART=1), CBS only ────────────────
// ONE-TIME, before the real search: solve the SAME instance with every resource
// capacity inflated by g_warmstart_k (ceil) on a small wall budget
// (g_warmstart_budget_s). The inflated instance is a relaxation — strictly easier
// — so this usually finishes fast; on timeout we just leave g_warmstart_ok=false
// and the real search runs with its normal ordering. The resulting schedule is
// INFEASIBLE for the real caps, so it is used ONLY to key CBS branch ordering,
// never as a bound. Requires getRCPSP + precompute* already done. All per-instance
// caches the inner solve touches (dominance table, MDA cache) are wiped afterwards
// so the real search starts pristine; g_incumbent/capacities are saved+restored.
template<int N>
void computeWarmstartOrder() {
    g_warmstart_ok = false;
    g_warmstart_infl_mk = -1;
    g_warmstart_sec = 0.0;
    g_warmstart_start.assign(RCPSPex.activities.size(), 0);
    if (resource_info.empty() || RCPSPex.activities.empty()) return;

    // Inflate capacities (save originals). Two modes:
    //  - RS target (g_warmstart_rs > 0): per-resource, set capacity to the value
    //    giving that Resource Strength — Kmin = max activity demand, Kmax = peak of
    //    the earliest-start (CPM, resource-blind) demand profile. Instance-adaptive.
    //  - else: flat multiplier ceil(k * capacity).
    // Both are clamped to never drop below the real capacity (must stay a relaxation).
    std::vector<short> savedCap(resource_info.size());
    for (size_t r = 0; r < resource_info.size(); ++r) savedCap[r] = resource_info[r].capacity;

    if (g_warmstart_rs > 0.0) {
        const int n = (int)RCPSPex.activities.size();
        std::vector<int> es(n, 0);                       // CPM earliest starts (precedence only)
        for (int i = 0; i < n; ++i)
            for (int dep : RCPSPex.backword_dependencies[i]) {
                int d = dep - 1;
                es[i] = std::max(es[i], es[d] + (int)RCPSPex.activities[d].duration);
            }
        int horizon = 0;
        for (int i = 0; i < n; ++i) horizon = std::max(horizon, es[i] + (int)RCPSPex.activities[i].duration);

        for (size_t r = 0; r < resource_info.size(); ++r) {
            const auto& ri = resource_info[r];
            short Kmin = 0;
            for (short d : ri.demands) Kmin = std::max(Kmin, d);      // max single-activity demand
            std::vector<int> prof(horizon + 1, 0);                     // earliest-start demand profile
            for (size_t j = 0; j < ri.activity_indices.size(); ++j) {
                short a = ri.activity_indices[j], d = ri.demands[j];
                if (d == 0) continue;
                for (int t = es[a]; t < es[a] + (int)RCPSPex.activities[a].duration; ++t) prof[t] += d;
            }
            int Kmax = Kmin;
            for (int t = 0; t < horizon; ++t) Kmax = std::max(Kmax, prof[t]);
            long cap = (long)std::ceil((double)Kmin + g_warmstart_rs * (double)(Kmax - Kmin));
            cap = std::max<long>(cap, savedCap[r]);                    // never below real (stay a relaxation)
            resource_info[r].capacity = (short)std::min<long>(cap, (long)std::numeric_limits<short>::max());
        }
    } else {
        for (size_t r = 0; r < resource_info.size(); ++r) {
            long inflated = (long)std::ceil(g_warmstart_k * (double)savedCap[r]);
            resource_info[r].capacity =
                (short)std::min<long>(inflated, (long)std::numeric_limits<short>::max());
        }
    }

    // Save/clamp the search knobs the inner solve reads, then run it. The inflated
    // solve gets its OWN, SEPARATE budget (not stolen from the real search): 0 =>
    // the full search timeout, so if the strictly-easier problem can't be solved,
    // the real one has no hope either. Its wall time is recorded (warmstartSec).
    const bool      saved_ws  = g_use_warmstart;      g_use_warmstart = false; // no recursion
    const long long saved_to  = astar_timeout_seconds;
    if (g_warmstart_budget_s > 0) astar_timeout_seconds = g_warmstart_budget_s;  // else keep full timeout
    const short     saved_inc = g_incumbent;          g_incumbent = std::numeric_limits<short>::max();
    get_cbs_dominance_table<N>().clear();
    reset_mda_cache<N>();

    RCPSP_CBS<N> env;
    RCPSPState_CBS<N> first;
    RCPSPState_CBS<N> last = first;
    last.start_times[g_sink_id] = 0;
    last.resourceType = -1;
    last.rvs_activities_pool.clear();
    TemplateAStar<RCPSPState_CBS<N>, int, RCPSP_CBS<N>> astar;
    std::vector<RCPSPState_CBS<N>> path;
    auto ws_t0 = std::chrono::high_resolution_clock::now();
    astar.GetPath(&env, first, last, path);
    g_warmstart_sec = std::chrono::duration<double>(
        std::chrono::high_resolution_clock::now() - ws_t0).count();

    // Pick the schedule: the found goal, or (when GetPath returns empty because
    // start==goal) the root itself if it is already conflict-free under inflated
    // caps. Only a genuine budget exhaustion with no goal leaves ordering disabled.
    const RCPSPState_CBS<N>* fs = nullptr;
    if (!path.empty()) {
        fs = &path.back();
    } else {
        short rc = -1;
        if (!first.compute_first_conflict(rc)) fs = &first;   // root already feasible
    }
    if (fs) {
        for (size_t i = 0; i < RCPSPex.activities.size(); ++i)
            g_warmstart_start[i] = fs->start_times[i];
        g_warmstart_ok = true;
        g_warmstart_infl_mk = fs->start_times[g_sink_id];
        std::cout << "WARMSTART: inflated ("
                  << (g_warmstart_rs > 0.0 ? ("RS=" + std::to_string(g_warmstart_rs))
                                           : ("k=" + std::to_string(g_warmstart_k)))
                  << ") schedule ready, makespan=" << fs->start_times[g_sink_id]
                  << "  (budget " << g_warmstart_budget_s << "s)\n";
    } else {
        std::cout << "WARMSTART: inflated solve found no schedule in "
                  << g_warmstart_budget_s << "s -> ordering disabled this instance\n";
    }

    // Restore real caps/knobs and wipe the caches the inner solve dirtied.
    for (size_t r = 0; r < resource_info.size(); ++r) resource_info[r].capacity = savedCap[r];
    astar_timeout_seconds = saved_to;
    g_use_warmstart       = saved_ws;
    g_incumbent           = saved_inc;
    get_cbs_dominance_table<N>().clear();
    reset_mda_cache<N>();
}

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
    // Instance Resource Strength for RS-adaptive gating (RCPSP_CBS_RSADAPT). RS cycles
    // {0.2,0.5,0.7,1.0} every group in PSPLIB j30/j60/j90 (verified against j90 solve
    // rates 2026-08-23). Group-derived so it needs no datasheet metadata.
    { static const double RSL[4] = {0.2, 0.5, 0.7, 1.0}; g_instance_rs = RSL[((group - 1) % 4 + 4) % 4]; }
    g_instance_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(astar_timeout_seconds); g_instance_deadline_set = true; // sub-solves share this 300s budget
    // Hierarchical RS-relaxation LB: precompute the Kmin/Kmax inflation profile and
    // resolve this instance's target RS (needs g_instance_rs + resource_info, both set).
    if (g_cbs_hierrs) precomputeRSInflation();

    // ── RCPSP_WARMSTART: one-time inflated-resource solve to seed branch order ──
    // Runs before the UB reset/seed below so it can freely reset the incumbent and
    // per-instance caches; it restores everything it touches.
    if (g_use_warmstart) computeWarmstartOrder<N>();

    // ── Upper-bound pruning: per-instance reset + SGS seed ──
    g_incumbent = std::numeric_limits<short>::max();
    g_ub_pruned = 0;
    g_leftshift_pruned = 0;
    g_lazy_hcost_evals = 0;
    g_lazy_reinserts = 0;
    g_cbs_theta_better = 0;   // Θ-tree "floor beat HCBS" counter is per-instance
    g_cbs_subset_better = g_cbs_subset_solves = g_cbs_subset_expands_total = 0;   // subset LB telemetry, per-instance
    g_cbs_subset_capped = g_cbs_subset_maxexpands = g_cbs_subset_cache_hits = 0;
    g_cbs_subset_cache.clear();   // subset LB result cache is per-instance
    g_cbs_mincut_better = g_cbs_mincut_calls = 0;   // min-cut LB telemetry, per-instance
    g_cbs_singleres_better = g_cbs_singleres_calls = 0;   // single-resource LB telemetry, per-instance
    g_cbs_hierrs_better = g_cbs_hierrs_calls = g_cbs_hierrs_cache_hits = 0;   // hier-RS LB telemetry, per-instance
    g_hierrs_time_sec = 0.0; g_cbs_hierrs_cache.clear();   // hier-RS timer + result cache, per-instance
    g_heur_time_sec = g_singleres_time_sec = g_mincut_time_sec = 0.0; g_heur_calls = 0; g_root_h = -1.0; g_shadow_set.clear(); g_shadow_probes = g_shadow_hits = 0; g_succ_probes = g_succ_hits = g_hcache_probes = g_hcache_hits = 0; if (g_subsolve_succ_cache || g_h_cache) get_tt2_cache().clear();  // always-on instrumentation, per-instance
    g_imp2_fires = 0; g_imp2_max_depth = 0;   // Improvement 2 (RCPSP_MDA_RECURSE) telemetry, per-instance
    g_orderswap_cand = 0;                     // order-swap measurement (RCPSP_ORDERSWAP), per-instance
    g_nmd_overshoot_events = 0; g_nmd_lost_siblings = 0;   // NMD overshoot diagnostic, per-instance
    LB = 0;   // proven-LB accumulator (TemplateAStar: LB = max(LB, popped f))
    if (g_use_ub) {
        int ub = getDatasheetUB(group, exam, problemType);
        if (ub > 0) g_incumbent = (short)ub;
        std::cout << "UB seed (datasheet): " << ub << std::endl;
    }

    RCPSP_CBS<N> as1;

    RCPSPState_CBS<N> first;
    RCPSPState_CBS<N> last = first;
    last.start_times[g_sink_id]=0;
    last.resourceType = -1;
    last.rvs_activities_pool.clear();

    // Init-f (root f) = root earliest-start makespan + root heuristic; computed on a
    // COPY so the real search's start state stays pristine. Recorded in the CSV
    // (rootF), together with the proven LB after the search (provenLB = rootMk + LB).
    const int root_mk_csv = (int)first.start_times[g_sink_id];
    int root_f_csv;
    { RCPSPState_CBS<N> probe = first; root_f_csv = root_mk_csv + (int)probe.compute_h_and_RVS(); }

    TemplateAStar<RCPSPState_CBS<N>, int, RCPSP_CBS<N>> astar;
    std::vector<RCPSPState_CBS<N>> path;

    std::chrono::duration<double> elapsed;
    auto start = std::chrono::high_resolution_clock::now();
    // Search engine: eager TemplateAStar (default) or the deferred-heuristic
    // LazyAStarCBS (RCPSP_LAZY=1). Both expose GetPath / GetNodesExpanded /
    // GetNodesTouched and share the AStarOpenClosed data type, so the rest of the
    // function (and the wrong-answer debug's openClosedList access) is unchanged.
    long nExpanded = 0, nTouched = 0;
    if (g_use_lazy) {
        LazyAStarCBS<RCPSPState_CBS<N>, int, RCPSP_CBS<N>> lazyAstar;
        lazyAstar.GetPath(&as1, first, last, path);
        nExpanded = (long)lazyAstar.GetNodesExpanded();
        nTouched  = (long)lazyAstar.GetNodesTouched();
    } else {
        astar.GetPath(&as1, first, last, path);
        nExpanded = (long)astar.GetNodesExpanded();
        nTouched  = (long)astar.GetNodesTouched();
    }
    auto end = std::chrono::high_resolution_clock::now();
    elapsed = end - start;
    long peakMemKB = getPeakMemoryKB();

    int makespan = 0;

    // ── UB: no goal popped, but if OPEN exhausted (not timeout) under an
    // incumbent, every subtree with makespan >= incumbent was soundly pruned
    // and the incumbent schedule itself is therefore optimal. On timeout,
    // report the proven LB / incumbent-UB gap.
    bool ub_exhaust_optimal = false;
    if (g_use_ub && path.empty() && !first.rvs_activities_pool.empty()) {
        const bool timed_out = elapsed.count() >= 0.98 * (double)astar_timeout_seconds;
        if (!timed_out && g_incumbent < std::numeric_limits<short>::max()) {
            makespan = g_incumbent;
            ub_exhaust_optimal = true;
            std::cout << "UB-exhaust: OPEN emptied under incumbent " << (int)g_incumbent
                      << " -> incumbent is optimal (ub_pruned=" << g_ub_pruned << ")\n";
        } else if (timed_out) {
            short root_mk = first.start_times[g_sink_id];
            std::cout << "TIMEOUT gap: LB=" << (root_mk + LB) << " UB="
                      << (g_incumbent == std::numeric_limits<short>::max() ? -1 : (int)g_incumbent)
                      << " (ub_pruned=" << g_ub_pruned << ")\n";
        }
    }

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

    std::cout << "Nodes Expanded: " << nExpanded << std::endl;
    std::cout << "Nodes Touched: " << nTouched << std::endl;

    // ── Per-rule prune breakdown (debug) ──
    {
        auto& dt = get_cbs_dominance_table<N>();
        size_t nb = 0, maxb = 0; double avgb = 0;
        dt.bp_bucket_stats(nb, maxb, avgb);
        std::cout << "Prunes per rule: UB=" << g_ub_pruned
                  << " LEFTSHIFT=" << g_leftshift_pruned
                  << " DOMINANCE=" << dt.pruned
                  << "  (dom checks=" << dt.checks << " stored=" << dt.stored
                  << " bp=" << dt.bp_count << " dr5=" << dt.dr5_count
                  << " killed=" << dt.killed_count
                  << " | bp_buckets=" << nb << " maxBucket=" << maxb
                  << " avgBucket=" << avgb << ")" << std::endl;
        if (g_use_lazy)
            std::cout << "Lazy A*: hcost_evals(scans@pop)=" << g_lazy_hcost_evals
                      << " reinserts=" << g_lazy_reinserts
                      << "  vs expanded=" << nExpanded << " touched=" << nTouched << std::endl;
    }

    std::ofstream file(filename, std::ios::app);
    InstanceParams p = getParams(group);

    file << group << ","
         << exam << ","
         << elapsed.count() << ","
         << makespan << ",";

    if (problemType == "j30") {
        int opt = getOptimalMakespan(group, exam, problemType);
        bool solved = (!path.empty() || first.rvs_activities_pool.empty() || ub_exhaust_optimal);
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
        bool solved = (!path.empty() || first.rvs_activities_pool.empty() || ub_exhaust_optimal);
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
         << ((!path.empty() || first.rvs_activities_pool.empty() || ub_exhaust_optimal) ? "True" : "False") << ","
         << nExpanded << ","
         << nTouched << ","
         << path.size() << ","
         << peakMemKB << ","
         << setting.use_first_conflict << ","
         << setting.use_conflict_prioritization << ","
         << (int)setting.heuristic << ","
         << setting.use_MDA_sets << ","
         << setting.use_MDA_cache << ","
         << setting.use_strong_constraints << ","
         << setting.use_MDA_BAB << ","
         << debug_cardinal_num / max(1L, nTouched) << ","
         // ── active experiment rules + per-rule prune counts ──
         << setting.use_dr5 << ","
         << (g_dom_rule==DOM_BP?"bp":g_dom_rule==DOM_DR5?"dr5":g_dom_rule==DOM_DR5S?"dr5s":"both") << ","
         << g_use_ub << ","
         << g_use_hybrid << ","
         << g_hybrid_threshold << ","
         << g_use_leftshift << ","
         << g_use_bidir << ","
         << g_ub_pruned << ","
         << g_leftshift_pruned << ","
         << get_cbs_dominance_table<N>().pruned << ","
         << get_cbs_dominance_table<N>().checks << ","
         << get_cbs_dominance_table<N>().stored << ","
         << g_use_lazy << ","
         << g_dom_skyline << ","
         << g_lazy_hcost_evals << ","
         << g_lazy_reinserts << ","
         // ── remaining configurable knobs, appended so every run is fully self-documenting ──
         << setting.use_non_minimal_delay << ","
         << setting.use_ancestor_branching << ","
         << setting.use_dominance << ","
         << setting.use_pair_decomposition << ","
         << setting.use_greed_conflic_resultion_asstimation << ","
         << g_use_lean << ","
         << g_use_inline << ","
         << g_dom_store_cap << ","
         << astar_timeout_seconds << ","
         << g_use_warmstart << ","
         << g_warmstart_k << ","
         << g_warmstart_budget_s << ","
         << g_warmstart_dir << ","
         << g_use_setdelay << ","
         << g_warmstart_rs << ","
         << (g_warmstart_ok ? 1 : 0) << ","
         << g_warmstart_infl_mk << ","
         << root_f_csv << ","
         << (root_mk_csv + (int)LB) << ","
         << g_warmstart_sec << ","
         << g_use_dr4 << ","
         << (g_cbs_theta ? 1 : 0) << ","   // useThetaBound
         << g_cbs_theta_better << ","      // thetaBoundBetter
         << (g_cbs_subset ? 1 : 0) << ","  // useSubsetLB
         << g_cbs_subset_better << ","     // subsetBetter
         << g_cbs_subset_solves << ","     // subsetSolves
         << g_cbs_subset_expands_total << ","  // subsetExpandsTotal
         << g_cbs_subset_capped << ","     // subsetCapped
         << g_cbs_subset_maxexpands << ","  // subsetMaxExpands
         << g_cbs_subset_cache_hits << ","      // subsetCacheHits
         << setting.use_mda_recursive_delay << ","  // useMdaRecursive (Improvement 2)
         << g_imp2_fires << ","                 // imp2Fires (internal-MDA splits this instance)
         << g_imp2_max_depth << ","             // imp2MaxDepth (deepest internal split this instance)
         << setting.use_nmd_precedence << ","   // useNmdPrecedence (Improvement 1)
         << g_orderswap_cand << ","             // orderSwapCand (rule-7 candidates DR5 kept; measurement)
         << (g_cbs_mincut ? 1 : 0) << ","       // useCbsMinCut (RCPSP_CBS_MINCUT)
         << g_cbs_mincut_better << ","          // minCutBetter (times the min-cut floor beat h)
         << g_cbs_mincut_calls << ","           // minCutCalls (times the gated bound was computed)
         << (g_cbs_rsadapt ? 1 : 0) << ","      // useRSAdapt (RCPSP_CBS_RSADAPT)
         << g_cbs_rs_threshold << ","           // rsThreshold
         << g_instance_rs << ","                // instanceRS (this instance's Resource Strength)
         << (g_cbs_singleres ? 1 : 0) << ","    // useSingleRes (RCPSP_CBS_SINGLERES)
         << g_cbs_singleres_better << ","       // singleResBetter
         << g_cbs_singleres_calls << ","        // singleResCalls
         << cbs_heuristic_name(true) << ","                 // heuristicLowRS (RS<=thresh regime)
         << cbs_heuristic_name(!g_cbs_rsadapt) << ","       // heuristicHighRS (RS>thresh; == low if RSADAPT off)
         << g_root_h << ","                                 // rootH (heuristic at start node)
         << g_heur_time_sec << ","                          // heurTimeSec (total time in HCost)
         << g_heur_calls << ","                             // heurCalls
         << g_singleres_time_sec << ","                     // singleResTimeSec
         << g_mincut_time_sec << ","                        // minCutTimeSec
         << (g_cbs_hierrs ? 1 : 0) << ","                   // useHierRS
         << g_cbs_hierrs_better << ","                      // hierRsBetter
         << g_cbs_hierrs_calls << ","                       // hierRsCalls
         << g_hierrs_time_sec << ","                        // hierRsTimeSec
         << g_cbs_hierrs_cache_hits << "," << subsolver_name() << "," << g_shadow_probes << "," << g_shadow_hits << "\n";   // hierRsCacheHits,subsolver,shadowProbes,shadowHits
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

// ── Self-describing run filenames ─────────────────────────────────────────────
// Compact tag of the ACTIVE feature flags, so a filename says what it ran instead
// of an opaque _1/_2/_3 counter. Adapts to whatever is on (CBS or TT2). Empty when
// nothing beyond the base config is enabled (e.g. plain baseline).
static std::string activeFeatureTag() {
    std::string t;
    auto add = [&](const std::string& s){ if(!t.empty()) t += "-"; t += s; };
    // CBS dominance / experiment flags
    if (setting.use_dr5) add(g_dom_rule==DOM_BP?"bp":g_dom_rule==DOM_DR5?"dr5":g_dom_rule==DOM_DR5S?"dr5s":"both");
    if (g_use_hybrid)    add("hyb");
    if (g_use_ub)        add("ub");
    if (g_use_lazy)      add("lazy");
    if (g_dom_skyline)   add("sky");
    if (g_use_leftshift) add("ls");
    if (g_use_bidir)     add("bidir");
    if (g_use_lean)      add("lean");
    if (g_use_inline)    add("inl");
    // TT2 flags
    if (g_tt2_dr5)       add("tt2dr5");
    if (g_tt2_batch)     add("batch");
    if (g_tt2_sym)       add("sym");
    if (g_tt2_gendesc)   add("gendesc");
    if (g_tt2_theta)     add("theta");
    return t;
}

// Local wall-clock stamp YYYYMMDD-HHMMSS — makes every run unique AND time-ordered.
static std::string nowStamp() {
    std::time_t tt = std::time(nullptr);
    std::tm tmv = *std::localtime(&tt);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y%m%d-%H%M%S", &tmv);
    return std::string(buf);
}

std::string getNextFilename(const std::string& folder, const std::string& baseName, const std::string& extension) {
    // Ensure folder exists
    if (!fs::exists(folder)) {
        fs::create_directories(folder);
    }

    // baseName (e.g. "output_j30_cfg8_") + active-feature tag + timestamp, so the
    // name is self-documenting and sorts chronologically. The old numeric counter
    // is kept only to break same-second collisions.
    const std::string tag = activeFeatureTag();
    std::string stem = baseName + (tag.empty() ? "" : tag + "_") + nowStamp();

    std::string newFilename = folder + "/" + stem + extension;
    int count = 2;
    while (fs::exists(newFilename)) { // same-second collision -> disambiguate
        newFilename = folder + "/" + stem + "_" + std::to_string(count) + extension;
        count++;
    }
    return newFilename;
}

// RCPSP_OUT overrides the auto-generated output path with one explicit file, so
// concurrent arms can each write their own CSV with zero filename races. When set,
// we still ensure the parent folder exists (getNextFilename's folder create is skipped).
std::string outName(const std::string& folder, const std::string& baseName, const std::string& extension) {
    if (const char* o = std::getenv("RCPSP_OUT")) if (*o) {
        std::string p(o); try { fs::path pp(p); if (pp.has_parent_path()) fs::create_directories(pp.parent_path()); } catch (...) {}
        return p;
    }
    return getNextFilename(folder, baseName, extension);
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
    std::string filename = outName(folder, baseName, extension);
    std::ofstream file(filename);
    if (!file.is_open()) { std::cerr << "Error opening file!" << std::endl; return; }
    file << "group,exam,time,makespan,correct,setType,model,optimalOrLB,UB,NC,RF,RS,"
         << "finished,expandNumber,generatedNumber,depth,maxMem,"
         << "useFirst,useConflictPrioritization,useHeuristic,useMDASets,useMDACache,useStrongConstraints,useMDABAB,cardinalityRatio,useDR5,domRule,useUB,useHybrid,hybridT,useLeftshift,useBidir,ubPruned,leftshiftPruned,domPruned,domChecks,domStored,useLazy,useSkyline,lazyEvals,lazyReinserts,useNonMinimalDelay,useAncestorBranching,useDominanceSib,usePairDecomp,useHGreed,useLean,useInline,domCap,timeoutS,useWarmStart,warmStartK,warmStartBudgetS,warmStartDir,useSetDelay,warmStartRS,warmstartEngaged,warmstartInflMk,rootF,provenLB,warmstartSec,useDR4,useThetaBound,thetaBoundBetter,useSubsetLB,subsetBetter,subsetSolves,subsetExpandsTotal,subsetCapped,subsetMaxExpands,subsetCacheHits,useMdaRecursive,imp2Fires,imp2MaxDepth,useNmdPrecedence,orderSwapCand,useCbsMinCut,minCutBetter,minCutCalls,useRSAdapt,rsThreshold,instanceRS,useSingleRes,singleResBetter,singleResCalls,heuristicLowRS,heuristicHighRS,rootH,heurTimeSec,heurCalls,singleResTimeSec,minCutTimeSec,useHierRS,hierRsBetter,hierRsCalls,hierRsTimeSec,hierRsCacheHits,subsolver,shadowProbes,shadowHits"
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
    std::string filename = outName(folder, baseName, extension);
    std::ofstream file(filename);
    if (!file.is_open()) { std::cerr << "Error opening file!" << std::endl; return; }
    file << "group,exam,time,makespan,correct,setType,model,optimalOrLB,UB,NC,RF,RS,"
         << "finished,expandNumber,generatedNumber,depth,maxMem,"
         << "useFirst,useConflictPrioritization,useHeuristic,useMDASets,useMDACache,useStrongConstraints,useMDABAB,cardinalityRatio,useDR5,domRule,useUB,useHybrid,hybridT,useLeftshift,useBidir,ubPruned,leftshiftPruned,domPruned,domChecks,domStored,useLazy,useSkyline,lazyEvals,lazyReinserts,useNonMinimalDelay,useAncestorBranching,useDominanceSib,usePairDecomp,useHGreed,useLean,useInline,domCap,timeoutS,useWarmStart,warmStartK,warmStartBudgetS,warmStartDir,useSetDelay,warmStartRS,warmstartEngaged,warmstartInflMk,rootF,provenLB,warmstartSec,useDR4,useThetaBound,thetaBoundBetter,useSubsetLB,subsetBetter,subsetSolves,subsetExpandsTotal,subsetCapped,subsetMaxExpands,subsetCacheHits,useMdaRecursive,imp2Fires,imp2MaxDepth,useNmdPrecedence,orderSwapCand,useCbsMinCut,minCutBetter,minCutCalls,useRSAdapt,rsThreshold,instanceRS,useSingleRes,singleResBetter,singleResCalls,heuristicLowRS,heuristicHighRS,rootH,heurTimeSec,heurCalls,singleResTimeSec,minCutTimeSec,useHierRS,hierRsBetter,hierRsCalls,hierRsTimeSec,hierRsCacheHits,subsolver,shadowProbes,shadowHits"
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
          << "useStrongConstraints,useMDABAB,cardinalityRatio,useDR5,domRule,useUB,useHybrid,hybridT,useLeftshift,useBidir,ubPruned,leftshiftPruned,domPruned,domChecks,domStored,useLazy,useSkyline,lazyEvals,lazyReinserts,useNonMinimalDelay,useAncestorBranching,useDominanceSib,usePairDecomp,useHGreed,useLean,useInline,domCap,timeoutS,useWarmStart,warmStartK,warmStartBudgetS,warmStartDir,useSetDelay,warmStartRS,warmstartEngaged,warmstartInflMk,rootF,provenLB,warmstartSec,useDR4,useThetaBound,thetaBoundBetter,useSubsetLB,subsetBetter,subsetSolves,subsetExpandsTotal,subsetCapped,subsetMaxExpands,subsetCacheHits,useMdaRecursive,imp2Fires,imp2MaxDepth,useNmdPrecedence,orderSwapCand,useCbsMinCut,minCutBetter,minCutCalls,useRSAdapt,rsThreshold,instanceRS,useSingleRes,singleResBetter,singleResCalls,heuristicLowRS,heuristicHighRS,rootH,heurTimeSec,heurCalls,singleResTimeSec,minCutTimeSec,useHierRS,hierRsBetter,hierRsCalls,hierRsTimeSec,hierRsCacheHits,subsolver,shadowProbes,shadowHits\n"; }

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

// Serial SGS with latest-start priority: builds a FEASIBLE schedule (precedence
// + resources respected) and returns its makespan — a valid upper bound for the
// incumbent. Activities are placed in precedence-feasible order, each at its
// earliest resource-feasible start. O(n^2 * horizon) worst case, run once per
// instance at the root.
inline int serialSGS_makespan() {
    const int n = (int)RCPSPex.activities.size();
    if (n == 0) return 0;

    // CPM latest starts (priority rule): reuse a forward pass for horizon.
    std::vector<int> es(n, 0);
    for (int i = 0; i < n; i++)                       // PSPLIB order: preds have smaller index
        for (int dep : RCPSPex.backword_dependencies[i]) {
            int d = dep - 1;
            es[i] = std::max(es[i], es[d] + RCPSPex.activities[d].duration);
        }
    int horizon = 0;
    for (int i = 0; i < n; i++) horizon += std::max(1, (int)RCPSPex.activities[i].duration);

    std::vector<int> ls(n, horizon);
    ls[n-1] = es[n-1];                                // schedule sink by CPM as anchor for priorities
    for (int i = n - 2; i >= 0; i--) {
        int m = horizon;
        for (int succ : RCPSPex.dependencies[i]) m = std::min(m, ls[succ - 1]);
        ls[i] = m - RCPSPex.activities[i].duration;
    }

    // resource usage over time
    const int R = (int)resource_info.size();
    std::vector<std::vector<short>> used(R, std::vector<short>(horizon + 1, 0));

    std::vector<int> start(n, -1);
    std::vector<char> done(n, 0);
    for (int placed = 0; placed < n; placed++) {
        // pick unscheduled activity with all preds scheduled, min latest start
        int pick = -1;
        for (int i = 0; i < n; i++) {
            if (done[i]) continue;
            bool ready = true;
            for (int dep : RCPSPex.backword_dependencies[i])
                if (!done[dep - 1]) { ready = false; break; }
            if (ready && (pick == -1 || ls[i] < ls[pick])) pick = i;
        }
        if (pick == -1) return -1;                    // shouldn't happen (DAG)

        int t0 = 0;
        for (int dep : RCPSPex.backword_dependencies[pick]) {
            int d = dep - 1;
            t0 = std::max(t0, start[d] + RCPSPex.activities[d].duration);
        }
        const int dur = RCPSPex.activities[pick].duration;
        // demands of `pick` per resource
        std::vector<short> dem(R, 0);
        for (int r = 0; r < R; r++) {
            auto it = resource_info[r].demand_lookup.find((short)pick);
            if (it != resource_info[r].demand_lookup.end()) dem[r] = it->second;
        }
        int t = t0;
        while (true) {
            bool ok = true;
            for (int r = 0; r < R && ok; r++) {
                if (dem[r] == 0) continue;
                for (int u = t; u < t + dur; u++)
                    if (used[r][u] + dem[r] > resource_info[r].capacity) { ok = false; break; }
            }
            if (ok) break;
            t++;
            if (t + dur > horizon) return -1;         // defensive; horizon is sufficient
        }
        start[pick] = t;
        done[pick] = 1;
        for (int r = 0; r < R; r++)
            if (dem[r] > 0)
                for (int u = t; u < t + dur; u++) used[r][u] += dem[r];
    }
    return start[n-1] + RCPSPex.activities[n-1].duration;
}

// ── Datasheet UB (env RCPSP_UB=1) ────────────────────────────────────────────
// Seed the UB-pruning incumbent from a datasheet, cheapest source first:
//   1. known optimum (j30 optima file / j60-j90 bounds with '*')  -> exact UB
//   2. published UB from the bounds file (j60/j90 unknown-optimum) -> feasible UB
//   3. a cached value from ub_cache.csv (previously computed)
//   4. else compute a feasible serial-SGS schedule and cache it
// NOTE (see memory project_ub_hardcoded_swap): this is a deliberate "cheat" for
// performance experiments. For the paper it MUST become an on-the-fly UB
// (serialSGS_makespan / greedy dive / TT2 result), or it's telling the solver
// the answer. requires getRCPSP + precompute* already done (RCPSPex is loaded).
static std::map<std::tuple<std::string,int,int>, int> g_ub_cache;
static bool g_ub_cache_loaded = false;
static void loadUBCache() {
    if (g_ub_cache_loaded) return;
    g_ub_cache_loaded = true;
    std::ifstream f("ub_cache.csv");
    if (!f.is_open()) { f.clear(); f.open("HOG2/RCPSP/ub_cache.csv"); }
    std::string line;
    while (std::getline(f, line)) {
        std::stringstream ss(line); std::string tok; std::vector<std::string> p;
        while (std::getline(ss, tok, ',')) p.push_back(tok);
        if (p.size() == 4) g_ub_cache[{p[0], std::atoi(p[1].c_str()), std::atoi(p[2].c_str())}] = std::atoi(p[3].c_str());
    }
}
static void saveUBCache(const std::string& sz, int g, int e, int ub) {
    g_ub_cache[{sz,g,e}] = ub;
    std::ofstream f("ub_cache.csv", std::ios::app);
    f << sz << "," << g << "," << e << "," << ub << "\n";
}
int getDatasheetUB(int group, int exam, const std::string& problemType) {
    if (problemType == "j30") {
        int opt = getOptimalMakespan(group, exam, problemType);
        if (opt > 0) return opt;
    } else {
        Bounds b = getBounds(group, exam, problemType);
        if (b.optimal_known && b.lb > 0) return b.lb;   // exact optimum
        if (b.ub > 0) return b.ub;                       // published feasible UB
    }
    loadUBCache();
    auto it = g_ub_cache.find({problemType, group, exam});
    if (it != g_ub_cache.end()) return it->second;
    int sgs = serialSGS_makespan();                      // our best feasible attempt
    if (sgs > 0) saveUBCache(problemType, group, exam, sgs);
    return sgs;
}

// Sweep one exam across a range of parameter groups, into a single CSV. Walks the
// whole NC x RF x RS grid quickly instead of grinding exam-by-exam through group 1,
// which is what correctness validation actually needs.
// ── S* reachability DFS: walk ONLY optimal-consistent children, find the dead-end ─
// A node N can still reach S* iff N.start_times[i] <= g_star[i] for all i. DFS the
// consistent frontier from the root; the first consistent node whose children ALL
// overshoot S* is exactly the conflict where the non-minimal delay loses the optimum.
template<short N>
void traceStarLoss_impl(int group, int exam, const std::string& ptype) {
    reset_mda_cache<N>();
    getRCPSP(RCPSPex, group, exam, ptype);
    resource_info.clear(); downstream.clear(); upstream.clear();
    precomputeDownstream(); precomputeUpstream(); precomputeResourceInfo();

    RCPSP_CBS<N> env;
    RCPSPState_CBS<N> root;
    auto consistent = [&](const RCPSPState_CBS<N>& s) {
        for (int i = 0; i < (int)g_star.size() && i < N; i++)
            if (s.start_times[i] > g_star[i]) return false;
        return true;
    };
    if (!consistent(root)) { std::cout << "ROOT already inconsistent with S* (bad S*?)\n"; return; }

    std::unordered_set<std::array<short, N>, CBSStateArrHash<N>> visited;
    bool found = false;
    long g_nodes = 0, g_leaves = 0; short g_bestLeaf = 32767;
    std::function<void(RCPSPState_CBS<N>&, int)> dfs = [&](RCPSPState_CBS<N>& node, int depth) {
        if (found) return;
        if (!visited.insert(node.start_times).second) return;
        ++g_nodes;
        node.compute_h_and_RVS();
        if (!node.found_conflict) {                        // consistent leaf: a feasible schedule <= S*
            ++g_leaves; g_bestLeaf = std::min(g_bestLeaf, node.start_times[g_sink_id]);
            return;
        }
        std::vector<RCPSPState_CBS<N>> children;
        env.GetSuccessors(node, children);
        std::vector<int> cons; int progressing = 0;
        for (int k = 0; k < (int)children.size(); k++) {
            if (children[k].start_times == node.start_times) continue;   // self-loop (no progress) — discarded as duplicate in real search
            ++progressing;
            if (consistent(children[k])) cons.push_back(k);
        }
        if (cons.empty() && progressing > 0) {
            found = true;
            std::cout << "\n===== OPTIMUM LOST at depth " << depth
                      << " | conflict t=" << node.t << " resource=" << node.resourceType << " =====\n";
            std::cout << "conflict participants (act: start..finish | S*):\n";
            for (short a : node.rvs_activities_pool)
                std::cout << "   act" << (a+1) << ": " << node.start_times[a] << ".."
                          << (node.start_times[a]+RCPSPex.activities[a].duration)
                          << " | S*=" << g_star[a]
                          << " dem=" << resource_info[node.resourceType].demand_lookup.at(a) << "\n";
            std::cout << "capacity(res " << node.resourceType << ")="
                      << resource_info[node.resourceType].capacity << "\n";
            std::cout << children.size() << " children — each overshoots S* at:\n";
            for (int k = 0; k < (int)children.size(); k++) {
                std::cout << "   child" << k << ":";
                for (int i = 0; i < (int)g_star.size() && i < N; i++)
                    if (children[k].start_times[i] > g_star[i])
                        std::cout << " act" << (i+1) << "(" << children[k].start_times[i] << ">" << g_star[i] << ")";
                std::cout << "\n";
            }
            return;
        }
        for (int k : cons) dfs(children[k], depth+1);
    };
    setting.use_non_minimal_delay = true;   // trace the non-minimal generator
    dfs(root, 0);
    std::cout << "consistent nodes visited=" << g_nodes
              << "  consistent conflict-free leaves=" << g_leaves
              << "  best leaf makespan=" << (g_leaves? g_bestLeaf : -1) << "\n";
    if (!found) std::cout << "No dead-end found: S* stayed reachable along the consistent frontier.\n";
}

void traceStarLoss(const std::string& ptype, int group, int exam) {
    if      (ptype == "j30") traceStarLoss_impl<32>(group, exam, ptype);
    else if (ptype == "j60") traceStarLoss_impl<62>(group, exam, ptype);
    else if (ptype == "j90") traceStarLoss_impl<92>(group, exam, ptype);
    else std::cout << "unsupported type\n";
}

void runSweep(const std::string& ptype, int cfg, int startG, int endG, int exam) {
    applyConfigNum(cfg);
    std::string f = getNextFilename("new_results",
        "output_sweep_" + ptype + "_cfg" + std::to_string(cfg) + "_e" + std::to_string(exam) + "_", ".csv");
    { std::ofstream h(f);
      h << "group,exam,time,makespan,correct,setType,model,optimalOrLB,UB,NC,RF,RS,"
        << "finished,expandNumber,generatedNumber,depth,maxMem,useFirst,useConflictPrioritization,"
        << "useHeuristic,useMDASets,useMDACache,useStrongConstraints,useMDABAB,cardinalityRatio,useDR5,domRule,useUB,useHybrid,hybridT,useLeftshift,useBidir,ubPruned,leftshiftPruned,domPruned,domChecks,domStored,useLazy,useSkyline,lazyEvals,lazyReinserts,useNonMinimalDelay,useAncestorBranching,useDominanceSib,usePairDecomp,useHGreed,useLean,useInline,domCap,timeoutS,useWarmStart,warmStartK,warmStartBudgetS,warmStartDir,useSetDelay,warmStartRS,warmstartEngaged,warmstartInflMk,rootF,provenLB,warmstartSec,useDR4,useThetaBound,thetaBoundBetter,useSubsetLB,subsetBetter,subsetSolves,subsetExpandsTotal,subsetCapped,subsetMaxExpands,subsetCacheHits,useMdaRecursive,imp2Fires,imp2MaxDepth,useNmdPrecedence,orderSwapCand,useCbsMinCut,minCutBetter,minCutCalls,useRSAdapt,rsThreshold,instanceRS,useSingleRes,singleResBetter,singleResCalls,heuristicLowRS,heuristicHighRS,rootH,heurTimeSec,heurCalls,singleResTimeSec,minCutTimeSec,useHierRS,hierRsBetter,hierRsCalls,hierRsTimeSec,hierRsCacheHits,subsolver,shadowProbes,shadowHits\n"; }
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

// ============================================================================
//  BACKWARD-MEET TT2 SEARCH  (mode: tt2bwd)
//  Backward A* over FORWARD-semantics markings. From the goal marking it un-fires
//  transitions (validated generate-and-verify inverse-fire) to reach the root at
//  the optimal makespan. Each backward state is a genuine forward marking (every
//  edge verified by forward-firing it back), so it meets the forward search.
//  No dominance yet (branchy => timeouts expected on larger instances).
// ============================================================================
namespace BWMEET {
static int NT = 0, g_sinkOutIdx = -1, g_maxDur = 0;
static uint64_t g_buildpre_calls = 0, g_succ_added = 0;   // generation instrumentation
static std::vector<int> g_ef;   // CPM earliest finish per activity (preprocess for delta)
inline void computeEf() {
    g_ef.assign(NT+1, 0); bool ch=true; int guard=0;
    while (ch && guard++ < NT+2) { ch=false;
        for (int i=1;i<=NT;i++){ int es=0; for (short p:RCPSPex.backword_dependencies[i-1]) es=std::max(es,g_ef[p]);
            int v=es+RCPSPex.activities[i-1].duration; if (v>g_ef[i]){g_ef[i]=v;ch=true;} } }
}
static std::vector<std::pair<int,int>> idx2ft;          // act-node idx -> (fromTrans,toTrans), -1=none
static std::vector<std::vector<int>>  inIdxOf, outIdxOf; // per transition (1-based)
static std::vector<std::pair<std::string,short>> g_resList;

inline void precompute() {
    NT = (int)petri.Transitions.size();
    std::set<int> resPlaceIds; g_resList.clear();
    for (auto& pr : RCPSPex.resources) {
        if (petri.place_name_to_id.count(pr.first)) resPlaceIds.insert(petri.place_name_to_id.at(pr.first));
        g_resList.push_back(pr);
    }
    auto parseTid = [](const std::string& s) -> int {
        if (s.empty()) return -1;
        for (char c : s) if (!std::isdigit((unsigned char)c)) return -1;
        try { return std::stoi(s); } catch (...) { return -1; }
    };
    idx2ft.clear(); std::map<int,int> placeId2ai; int ai = 0;
    for (int pid = 0; pid < (int)petri.places.size(); ++pid) {
        if (resPlaceIds.count(pid)) continue;
        const auto& pl = petri.places[pid];
        int fromT = pl.arcs_in.empty()  ? -1 : parseTid(pl.arcs_in.begin()->first);
        int toT   = pl.arcs_out.empty() ? -1 : parseTid(pl.arcs_out.begin()->first);
        idx2ft.push_back({fromT, toT}); placeId2ai[pid] = ai++;
    }
    inIdxOf.assign(NT+1, {}); outIdxOf.assign(NT+1, {});
    for (int t = 1; t <= NT; ++t) {
        const auto& tr = petri.Transitions[t-1];
        for (auto& pr : tr.arcs_in_indices)  if (placeId2ai.count(pr.first)) inIdxOf[t].push_back(placeId2ai[pr.first]);
        for (auto& pr : tr.arcs_out_indices) if (placeId2ai.count(pr.first)) outIdxOf[t].push_back(placeId2ai[pr.first]);
    }
    g_sinkOutIdx = -1;
    for (int idx = 0; idx < (int)idx2ft.size(); ++idx)
        if (idx2ft[idx].second == -1 && idx2ft[idx].first != -1) g_sinkOutIdx = idx;
    g_maxDur = 0; for (auto& a : RCPSPex.activities) g_maxDur = std::max(g_maxDur, (int)a.duration);
    computeEf();   // CPM earliest-finish per activity, used by the deterministic un-fire δ
}
// resources = {(demand_x, rem_x): x active} + free at 0 ; grouped by time asc
inline void deriveResources(const std::vector<std::pair<short,short>>& active,
                            std::array<std::vector<std::pair<short,short>>,4>& out) {
    for (auto& v : out) v.clear();
    for (int ri = 0; ri < (int)g_resList.size(); ++ri) {
        const std::string& rn = g_resList[ri].first; short cap = g_resList[ri].second;
        std::map<short,int> byTime; int used = 0;
        for (auto& pr : active) {
            auto& dem = RCPSPex.activities[pr.first-1].resource_demands;
            auto it = dem.find(rn);
            if (it != dem.end() && it->second > 0) { byTime[pr.second] += it->second; used += it->second; }
        }
        if (cap - used > 0) byTime[0] += (cap - used);
        for (auto& kv : byTime) if (kv.second > 0) out[ri].push_back({(short)kv.second, kv.first});
    }
}
inline bool contains(const std::vector<int>& v, int x){ for(int e:v) if(e==x) return true; return false; }
inline uint64_t hashState(const RCPSPState_TT2& n) {
    uint64_t seed=0; auto mix=[&](uint64_t v){ seed ^= v + 0x9e3779b97f4a7c15ULL + (seed<<6) + (seed>>2); };
    for(int i=1;i<=NT;i++) if(n.finishedActivitiys.test(i)) mix((uint64_t)i*2654435761ULL);
    for(size_t k=0;k<n.activity_nodes.size();++k) mix(k*131ULL + (uint64_t)n.activity_nodes[k].first*7ULL + (uint64_t)n.activity_nodes[k].second*1000003ULL);
    for(auto& rv:n.resource_nodes) for(auto& p:rv) mix((uint64_t)p.first*100003ULL + (uint64_t)p.second);
    for(auto& p:n.activeTransitionIndices) mix((uint64_t)p.first*99991ULL + (uint64_t)p.second*17ULL);
    return seed;
}
// un-fire a with wait D, reactivating reactRem (id->rem). false if structurally invalid.
inline bool buildPre(const RCPSPState_TT2& cur, int a, int D,
                     const std::map<int,int>& reactRem, RCPSPState_TT2& pre) {
    ++g_buildpre_calls;
    pre = cur; pre.transitionsCached=false; pre.AvailableTransitionIndices_TT2.clear(); pre.hKnown=false;
    short dur = RCPSPex.activities[a-1].duration;
    if (dur > 0) {
        auto& av = pre.activeTransitionIndices;
        auto it = std::find(av.begin(), av.end(), std::make_pair((short)a,(short)dur));
        if (it == av.end()) return false; av.erase(it);
    } else { if (!pre.finishedActivitiys.test(a)) return false; pre.finishedActivitiys.reset(a); }
    for (int idx : outIdxOf[a]) pre.activity_nodes[idx] = {0,0};
    // Un-shift survivors by +D, but CAP each at its own duration: going backward past an
    // activity's start would give remaining>dur (impossible). The cap parks it at full
    // duration = "just started here", so it becomes un-fireable on the next step instead
    // of poisoning the state (user rule: un-fire it if you can, else cap at max duration).
    for (int idx = 0; idx < (int)pre.activity_nodes.size(); ++idx) {
        if (contains(inIdxOf[a], idx) || contains(outIdxOf[a], idx)) continue;
        if (pre.activity_nodes[idx].first > 0 && pre.activity_nodes[idx].second > 0) {
            int x = idx2ft[idx].first;                                  // producing activity
            int cap = (x >= 1) ? (int)RCPSPex.activities[x-1].duration : (int)pre.activity_nodes[idx].second + D;
            pre.activity_nodes[idx].second = (short)std::min((int)pre.activity_nodes[idx].second + D, cap);
        }
    }
    for (auto& p : pre.activeTransitionIndices)
        p.second = (short)std::min((int)p.second + D, (int)RCPSPex.activities[p.first-1].duration);
    for (auto& kv : reactRem) {
        int id = kv.first, rem = kv.second;
        if (!pre.finishedActivitiys.test(id)) return false;
        pre.finishedActivitiys.reset(id);
        pre.activeTransitionIndices.push_back({(short)id,(short)rem});
        for (int idx : outIdxOf[id]) if (pre.activity_nodes[idx].first > 0) pre.activity_nodes[idx].second = (short)rem;
    }
    for (int idx : inIdxOf[a]) {
        int x = idx2ft[idx].first; short tau = 0;
        auto it = reactRem.find(x); if (x != -1 && it != reactRem.end()) tau = (short)it->second;
        pre.activity_nodes[idx] = {1, tau};
    }
    std::sort(pre.activeTransitionIndices.begin(), pre.activeTransitionIndices.end());
    deriveResources(pre.activeTransitionIndices, pre.resource_nodes);
    return true;
}
// Validity of the un-fire candidate. Default (light): a fires from pre at exactly the
// intended wait D (cheap: one availability scan). Full (RCPSP_BWD_FULLVERIFY=1): also
// forward-fire and require the result to equal cur (the prototyping guarantee; slow,
// recomputes the heuristic). The prediction rule is validated across j30, so light is
// the production path.
static bool g_bwd_fullverify = false;
inline bool verify(const RCPSPState_TT2& pre, int a, int D, const RCPSPState_TT2& cur) {
    std::vector<short> unstarted;
    for (int i = 1; i <= NT; ++i) if (!pre.finishedActivitiys.test(i)) unstarted.push_back((short)i);
    auto avail = getAvailableTransitionIndices_TT2(unstarted, pre.finishedActivitiys,
                     pre.resource_nodes, pre.activity_nodes, pre.activeTransitionIndices);
    short e = -1; for (auto& pr : avail) if (pr.first == a) { e = pr.second; break; }
    if (e != D) return false;
    if (!g_bwd_fullverify) return true;
    RCPSPState_TT2 child(pre, (short)a, e, 1);
    return child == cur;
}
inline void enumReact(const RCPSPState_TT2& cur,int a,int D,const std::vector<int>& cand,int pos,
        std::map<int,int>& chosen, std::vector<RCPSPState_TT2>& out, std::set<uint64_t>& seen){
    if (pos == (int)cand.size()) {
        RCPSPState_TT2 pre;
        if (buildPre(cur,a,D,chosen,pre) && verify(pre,a,D,cur)) {
            uint64_t h=hashState(pre); if(!seen.count(h)){ seen.insert(h); pre.g=(short)(cur.g+D); out.push_back(pre); ++g_succ_added; }
        }
        return;
    }
    int x = cand[pos];
    enumReact(cur,a,D,cand,pos+1,chosen,out,seen);                       // skip x
    for (int rem = 1; rem <= D; ++rem) { chosen[x]=rem; enumReact(cur,a,D,cand,pos+1,chosen,out,seen); chosen.erase(x); }
}
// DETERMINISTIC un-fire (forward-frame, byte-identical target): ONE predecessor per
// un-fireable transition, NO enumeration. Δ is not guessed analytically — it is made to
// EQUAL the forward rule by construction: build a tentative predecessor, then ask the
// forward model's own getAvailableTransitionIndices_TT2 what delay it would fire `a` at,
// and correct Δ to that value (a bounded SCALAR fixpoint over one state, not a search).
//   reactivation δ_x = max(0, D − (ef(L) − ef(x)))  (L = latest-finishing finished pred)
//   Δ  = getAvailable(pre).delay(a)                  (= max(precedence δ, resource-ready))
inline std::vector<RCPSPState_TT2> backwardSuccessors(const RCPSPState_TT2& cur) {
    std::vector<RCPSPState_TT2> out;
    std::vector<short> unstartedBuf;
    for (int a = 1; a <= NT; ++a) {
        short dur = RCPSPex.activities[a-1].duration;
        // gate: a is the just-started activity (active at full duration), or a dur-0 finished node
        if (dur > 0) { bool last=false; for (auto& pr : cur.activeTransitionIndices) if (pr.first==a && pr.second==dur){last=true;break;} if(!last) continue; }
        else { if (!cur.finishedActivitiys.test(a)) continue; bool ok=true; for(int idx:outIdxOf[a]) if(cur.activity_nodes[idx].first<1){ok=false;break;} if(!ok) continue; }
        // UNIFIED completer set (don't separate precedence vs resource; over-generate, DR cleans
        // up; NO rem-enumeration — one ef-based rem per completer): a's finished preds PLUS
        // finished frontier activities that share a resource with a.
        std::vector<int> comps;
        for (short x : RCPSPex.backword_dependencies[a-1]) if (cur.finishedActivitiys.test(x)) comps.push_back(x);
        const auto& aDem = RCPSPex.activities[a-1].resource_demands;
        for (int c=1;c<=NT;c++){ if(c==a||!cur.finishedActivitiys.test(c)) continue;
            if (RCPSPex.activities[c-1].duration<=0) continue;
            if (std::find(comps.begin(),comps.end(),c)!=comps.end()) continue;
            bool front=false; for(int idx:outIdxOf[c]) if(cur.activity_nodes[idx].first>0){front=true;break;} if(!front) continue;
            bool share=false; for(auto&d:RCPSPex.activities[c-1].resource_demands) if(d.second>0 && aDem.count(d.first) && aDem.at(d.first)>0){share=true;break;}
            if (share) comps.push_back(c);
        }
        int L = -1; for (int x : comps) if (L < 0 || g_ef[x] > g_ef[L]) L = x;
        auto makeReact = [&](int D){ std::map<int,int> r;
            if (L>=0) for (int x:comps){ int dx=std::max(0, D-(g_ef[L]-g_ef[x])); if (dx>0) r[x]=dx; } return r; };
        // initial Δ guess: gap to previous start = min positive elapsed among OTHER actives; else dur(L)/dur(a)
        int D = 0, minEl = INT_MAX; bool other=false;
        for (auto& pr : cur.activeTransitionIndices) { if (pr.first==a) continue; other=true;
            int el=(int)RCPSPex.activities[pr.first-1].duration - pr.second; if (el>0) minEl=std::min(minEl, el); }
        if (other && minEl!=INT_MAX) D=minEl; else if (L>=0) D=RCPSPex.activities[L-1].duration; else D=dur;
        // scalar fixpoint: rebuild pre and correct Δ to the forward-model's actual delay for a
        RCPSPState_TT2 pre; bool ok=false;
        for (int iter=0; iter<5; ++iter) {
            if (!buildPre(cur, a, D, makeReact(D), pre)) { ok=false; break; }
            unstartedBuf.clear();
            for (int i=1;i<=NT;i++) if (!pre.finishedActivitiys.test(i)) unstartedBuf.push_back((short)i);
            auto av = getAvailableTransitionIndices_TT2(unstartedBuf, pre.finishedActivitiys,
                          pre.resource_nodes, pre.activity_nodes, pre.activeTransitionIndices);
            int e=-1; for (auto& pr:av) if (pr.first==a){ e=pr.second; break; }
            if (e<0) { ok=false; break; }         // a not fireable from this pre → invalid reconstruction
            if (e==D) { ok=true; break; }          // consistent: Δ equals forward's own delay
            D=e;                                    // correct and retry
        }
        if (!ok) continue;
        if (g_bwd_fullverify) {                     // byte-identity guarantee: forward-fire must reproduce cur
            RCPSPState_TT2 child(pre, (short)a, (short)D, 1);
            if (!(child == cur)) continue;
        }
        pre.g = (short)(cur.g + D);
        ++g_succ_added;
        out.push_back(pre);
    }
    return out;
}
// ENUMERATING un-fire: generate ALL valid predecessors (the few reverses), not just one.
// Candidates bounded to a's finished predecessors (the precedence-linked window completers)
// × their rem in [1,D] × D in [0,maxDur]; each candidate is BYTE-VERIFIED (forward-fire ==
// cur), so every output is a true forward predecessor. This is the multi-reverse generator
// (RCPSP_BWD_ENUM=1). Measures whether precedence-linked completers suffice for completeness.
static bool g_bwd_enum = false;
inline std::vector<RCPSPState_TT2> reverseSuccessorsEnum(const RCPSPState_TT2& cur) {
    std::vector<RCPSPState_TT2> out;
    bool savedFV = g_bwd_fullverify; g_bwd_fullverify = true;   // enum path always byte-verifies
    for (int a = 1; a <= NT; ++a) {
        short dur = RCPSPex.activities[a-1].duration;
        if (dur > 0) { bool last=false; for (auto& pr : cur.activeTransitionIndices) if (pr.first==a && pr.second==dur){last=true;break;} if(!last) continue; }
        else { if (!cur.finishedActivitiys.test(a)) continue; bool ok=true; for(int idx:outIdxOf[a]) if(cur.activity_nodes[idx].first<1){ok=false;break;} if(!ok) continue; }
        std::vector<int> frontier;   // UNIFIED completer set: a's finished preds + finished frontier resource-sharers
        for (short x : RCPSPex.backword_dependencies[a-1])
            if (cur.finishedActivitiys.test(x) && RCPSPex.activities[x-1].duration > 0) frontier.push_back(x);
        const auto& aDem2 = RCPSPex.activities[a-1].resource_demands;
        for (int c=1;c<=NT;c++){ if(c==a||!cur.finishedActivitiys.test(c)||RCPSPex.activities[c-1].duration<=0) continue;
            if (std::find(frontier.begin(),frontier.end(),c)!=frontier.end()) continue;
            bool front=false; for(int idx:outIdxOf[c]) if(cur.activity_nodes[idx].first>0){front=true;break;} if(!front) continue;
            bool share=false; for(auto&d:RCPSPex.activities[c-1].resource_demands) if(d.second>0 && aDem2.count(d.first) && aDem2.at(d.first)>0){share=true;break;}
            if (share) frontier.push_back(c);
        }
        // SOUND bound on the rollback e: a surviving active y (rem<dur) can be rolled back at
        // most (dur-rem) before its remaining would exceed its duration. So e <= min room.
        int emax = g_maxDur;
        for (auto& pr : cur.activeTransitionIndices) { if (pr.first==a) continue;
            int room = (int)RCPSPex.activities[pr.first-1].duration - (int)pr.second; if (room < emax) emax = room; }
        if (emax < 0) emax = 0;
        std::set<uint64_t> seen;
        for (int D = 0; D <= emax; ++D) { std::map<int,int> chosen; enumReact(cur, a, D, frontier, 0, chosen, out, seen); }
    }
    g_bwd_fullverify = savedFV;
    return out;
}
// Cutset (DR5) dominance on backward states. OFF by default: the forward DR5 criterion
// is UNSOUND here — distinct penultimates share a cutset (finished∪active), so it prunes
// states that lead to different (needed) root paths. Needs a backward-specific criterion.
// RCPSP_BWD_DR=1 enables it (experimental / measurement only).
static bool g_bwd_dr = false;
static bool g_bwd_h = true;    // backward critical-path heuristic (getBackwardHcost); RCPSP_BWD_H0=1 disables
class BackwardMeetEnv : public SearchEnvironment<RCPSPState_TT2,int> {
public:
    RCPSPState_TT2 root;
    void GetSuccessors(const RCPSPState_TT2 &s, std::vector<RCPSPState_TT2> &nb) const override {
        nb = g_bwd_enum ? reverseSuccessorsEnum(s) : backwardSuccessors(s);
        if (g_bwd_dr) {
            std::vector<RCPSPState_TT2> keep; keep.reserve(nb.size());
            for (auto& c : nb) if (!get_tt2_dominance_table().check_and_insert(c)) keep.push_back(c);
            nb.swap(keep);
        }
    }
    bool GoalTest(const RCPSPState_TT2 &n, const RCPSPState_TT2 &) const override { return n == root; }
    double GCost(const RCPSPState_TT2 &a, const RCPSPState_TT2 &b) const override { return (double)(b.g - a.g); }
    double GCost(const RCPSPState_TT2 &, const int &) const override { return 0; }
    double HCost(const RCPSPState_TT2 &s, const RCPSPState_TT2 &) const override {
        if (!g_bwd_h) return 0.0;                      // RCPSP_BWD_H0=1 => pure Dijkstra
        // remaining backward work = activities not yet un-fired (still finished OR active);
        // critical-path lower bound on the makespan still to be un-scheduled (same bound the old backward used).
        std::vector<short> rem;
        for (int i = 1; i <= NT; ++i) {
            bool act=false; for (auto& p : s.activeTransitionIndices) if (p.first==i){act=true;break;}
            if (s.finishedActivitiys.test(i) || act) rem.push_back((short)i);
        }
        // admissible: critical-path (+ resource) LB on the makespan of the still-to-un-schedule set.
        return std::max(getForwardHcost(rem, s.activeTransitionIndices),
                        getforwardResource(rem, s.activeTransitionIndices));
    }
    int GetAction(const RCPSPState_TT2 &, const RCPSPState_TT2 &) const override { return 0; }
    void GetActions(const RCPSPState_TT2 &, std::vector<int> &) const override {}
    void ApplyAction(RCPSPState_TT2 &, int) const override {}
    bool InvertAction(int &) const override { return false; }
    uint64_t GetActionHash(int) const override { return 0; }
    uint64_t GetStateHash(const RCPSPState_TT2 &n) const override { return hashState(n); }
};
} // namespace BWMEET

int solveRCPSP_TT2_BackwardMeet(int group, int exam, const std::string& filename, const std::string& ptype="j30") {
    std::cout << "started BACKWARD-MEET TT2: " << group << ":" << exam << std::endl;
    getPetri(petri, group, exam, ptype);
    getRCPSP(RCPSPex, group, exam, ptype);
    BWMEET::precompute();
    BWMEET::g_bwd_fullverify = (std::getenv("RCPSP_BWD_FULLVERIFY") != nullptr);
    BWMEET::g_bwd_dr = (std::getenv("RCPSP_BWD_DR") != nullptr);
    BWMEET::g_bwd_enum = (std::getenv("RCPSP_BWD_ENUM") != nullptr);
    BWMEET::g_bwd_h  = (std::getenv("RCPSP_BWD_H0") == nullptr);
    get_tt2_dominance_table().clear();   // DR table is per-instance
    g_instance_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(astar_timeout_seconds);
    g_instance_deadline_set = true;

    RCPSPState_TT2 root;                       // default = source token, nothing finished, resources full
    BWMEET::deriveResources(root.activeTransitionIndices, root.resource_nodes);  // match backward representation
    BWMEET::BackwardMeetEnv env; env.root = root;
    RCPSPState_TT2 goal;                       // START of the backward search = forward goal marking
    for (int i = 1; i <= BWMEET::NT; ++i) goal.finishedActivitiys.set(i);
    for (auto& an : goal.activity_nodes) an = {0,0};
    if (BWMEET::g_sinkOutIdx >= 0) goal.activity_nodes[BWMEET::g_sinkOutIdx] = {1,0};
    goal.activeTransitionIndices.clear();
    BWMEET::deriveResources(goal.activeTransitionIndices, goal.resource_nodes);
    goal.g = 0;

    if (std::getenv("RCPSP_BWDBG")) {
        std::cout << "[dbg] NT=" << BWMEET::NT << " maxDur=" << BWMEET::g_maxDur
                  << " sinkOutIdx=" << BWMEET::g_sinkOutIdx << " actNodes=" << goal.activity_nodes.size() << std::endl;
        std::cout << "[dbg] goal tokens:"; for (size_t k=0;k<goal.activity_nodes.size();++k) if (goal.activity_nodes[k].first>0) std::cout << " ["<<k<<"]="<<goal.activity_nodes[k].first<<"^"<<goal.activity_nodes[k].second<<"(from="<<BWMEET::idx2ft[k].first<<",to="<<BWMEET::idx2ft[k].second<<")"; std::cout << std::endl;
        int sinkT = BWMEET::g_sinkOutIdx>=0 ? BWMEET::idx2ft[BWMEET::g_sinkOutIdx].first : -1;
        std::cout << "[dbg] sinkTrans=" << sinkT << " outIdxOf[sink]={";
        if (sinkT>=1) for (int idx : BWMEET::outIdxOf[sinkT]) std::cout << idx << "(tok="<<goal.activity_nodes[idx].first<<") ";
        std::cout << "} inIdxOf[sink]={";
        if (sinkT>=1) for (int idx : BWMEET::inIdxOf[sinkT]) std::cout << idx << " ";
        std::cout << "}" << std::endl;
        auto succ = BWMEET::backwardSuccessors(goal);
        std::cout << "[dbg] goal successors: " << succ.size() << std::endl;
        // walk the successor[0] chain to find where it dead-ends
        RCPSPState_TT2 cur = goal;
        for (int step = 0; step < 60; ++step) {
            auto sc = BWMEET::backwardSuccessors(cur);
            int fin = 0; for (int i=1;i<=BWMEET::NT;i++) if (cur.finishedActivitiys.test(i)) ++fin;
            std::cout << "[walk] step=" << step << " g=" << cur.g << " finished=" << fin
                      << " active={"; for (auto& p:cur.activeTransitionIndices) std::cout << p.first << ":" << p.second << " ";
            std::cout << "} nSucc=" << sc.size() << std::endl;
            if (cur == env.root) { std::cout << "[walk] REACHED ROOT" << std::endl; break; }
            if (sc.empty()) { std::cout << "[walk] DEAD-END (no successors)" << std::endl; break; }
            cur = sc[0];
        }
    }
    TemplateAStar<RCPSPState_TT2, int, BWMEET::BackwardMeetEnv> astar;
    std::vector<RCPSPState_TT2> path;
    auto t0 = std::chrono::high_resolution_clock::now();
    astar.GetPath(&env, goal, root, path);
    auto t1 = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> el = t1 - t0;
    int makespan = path.empty() ? -1 : path.back().g;
    bool solved = !path.empty();
    std::cout << "BWD-GEN buildPre_calls=" << BWMEET::g_buildpre_calls
              << " succ_added=" << BWMEET::g_succ_added
              << " per_expansion=" << (astar.GetNodesExpanded()? BWMEET::g_buildpre_calls/std::max<uint64_t>(1,astar.GetNodesExpanded()):0) << std::endl;
    std::cout << "BACKWARD-MEET " << group << ":" << exam << " makespan=" << makespan
              << " solved=" << (solved?"True":"False") << " pathLen=" << path.size()
              << " expanded=" << astar.GetNodesExpanded() << " time=" << el.count() << "s" << std::endl;
    std::ofstream file(filename, std::ios::app);
    file << group << "," << exam << "," << el.count() << "," << (solved?"True":"False") << ","
         << makespan << "," << astar.GetNodesExpanded() << "," << astar.GetNodesTouched() << ","
         << path.size() << ",TT2BWD," << ptype << "\n";
    return makespan;
}

// ── Meet-in-the-middle validation ─────────────────────────────────────────────
// Confirms forward and the reversed-instance backward MEET: at a shared cut, the
// forward-finished set = reversed-untouched set, the active set is shared, and each
// active activity satisfies θ_fwd + θ_bwd = τ. The meet key is (forward-finished set,
// {active id : θ_fwd}) — a COMPLETE forward state identity (activity/resource tokens
// are derivable from it). We collect every closed state of both searches and check
// min(g_fwd + g_bwd) over matching keys == forward optimum.
static std::string meetKeyForward(const RCPSPState_TT2& s, int NT) {
    std::string k = "F:";
    for (int i = 1; i <= NT; i++) if (s.finishedActivitiys.test(i)) { k += std::to_string(i); k += ','; }
    k += "|A:";
    std::vector<std::pair<int,int>> av(s.activeTransitionIndices.begin(), s.activeTransitionIndices.end());
    std::sort(av.begin(), av.end());
    for (auto& p : av) { k += std::to_string(p.first); k += ':'; k += std::to_string(p.second); k += ';'; }
    return k;
}
// Translate a reversed-instance state to the forward meet key: forward-finished =
// activities neither reversed-finished nor reversed-active; active ids are shared with
// θ_fwd = τ − θ_bwd. NT and durations are identical across the (precedence-only) reversal.
static std::string meetKeyReversedTranslated(const RCPSPState_TT2& b, int NT) {
    std::set<int> ract; for (auto& p : b.activeTransitionIndices) ract.insert(p.first);
    std::string k = "F:";
    for (int i = 1; i <= NT; i++) if (!b.finishedActivitiys.test(i) && !ract.count(i)) { k += std::to_string(i); k += ','; }
    k += "|A:";
    std::vector<std::pair<int,int>> av;
    for (auto& p : b.activeTransitionIndices) {
        int tau = RCPSPex.activities[p.first-1].duration;
        av.push_back({(int)p.first, tau - (int)p.second});
    }
    std::sort(av.begin(), av.end());
    for (auto& p : av) { k += std::to_string(p.first); k += ':'; k += std::to_string(p.second); k += ';'; }
    return k;
}
// ── CUT-MARKING NORMALISATION (re-anchoring the two frames) ──────────────────
// The two searches sit on DIFFERENT event types: a forward state is anchored at a
// START event (t = last start time), a reversed-instance state at a reversed-start,
// which is a forward FINISH event. At a shared instant T_c the true cut marking is
//   finished  = {x : f_x <= T_c}
//   active    = {x : s_x <  T_c <  f_x}, rem = f_x - T_c
//   unstarted = {x : s_x >= T_c}
// and each side violates it at exactly ONE boundary:
//   forward : an activity STARTED at T_c is active with rem == dur   -> cut says UNSTARTED
//   reversed: an activity FINISHED at T_c is active with th_fwd == 0 -> cut says FINISHED
// (forward actives always have rem >= 1 and reversed actives always have th_bwd >= 1, so
// each side needs exactly one of the two rules). Normalising both makes the keys identical
// whenever the two sides share an instant -- and shared instants are the COMMON case, since
// every start with wait e>0 lands exactly on a finish (e is attained by a completer's
// remaining), and e==0 starts chain back to such a time.
// ON by default: it is free (expansions identical to within 1-3 nodes on j30) and it is the
// whole fix -- without it the two frames straddle every event and meet only at the endpoints.
// Set RCPSP_MEET_NORM=0 to get the old un-normalised key back for comparison.
static bool g_meet_norm = [](){ const char* e=std::getenv("RCPSP_MEET_NORM"); return e? (std::atoi(e)!=0) : true; }();
static std::string meetKeyForwardNorm(const RCPSPState_TT2& s, int NT) {
    if (!g_meet_norm) return meetKeyForward(s, NT);
    std::string k = "F:";
    for (int i = 1; i <= NT; i++) if (s.finishedActivitiys.test(i)) { k += std::to_string(i); k += ','; }
    k += "|A:";
    std::vector<std::pair<int,int>> av;
    for (auto& p : s.activeTransitionIndices) {
        if ((int)p.second == (int)RCPSPex.activities[p.first-1].duration) continue;   // started AT T_c => unstarted
        av.push_back({(int)p.first, (int)p.second});
    }
    std::sort(av.begin(), av.end());
    for (auto& p : av) { k += std::to_string(p.first); k += ':'; k += std::to_string(p.second); k += ';'; }
    return k;
}
static std::string meetKeyReversedTranslatedNorm(const RCPSPState_TT2& b, int NT) {
    if (!g_meet_norm) return meetKeyReversedTranslated(b, NT);
    std::set<int> ract; for (auto& p : b.activeTransitionIndices) ract.insert(p.first);
    std::set<int> fin;
    for (int i = 1; i <= NT; i++) if (!b.finishedActivitiys.test(i) && !ract.count(i)) fin.insert(i);
    std::vector<std::pair<int,int>> av;
    for (auto& p : b.activeTransitionIndices) {
        int tau = RCPSPex.activities[p.first-1].duration, tf = tau - (int)p.second;
        if (tf == 0) { fin.insert((int)p.first); continue; }                          // finished AT T_c => finished
        av.push_back({(int)p.first, tf});
    }
    std::string k = "F:";
    for (int i : fin) { k += std::to_string(i); k += ','; }
    k += "|A:";
    std::sort(av.begin(), av.end());
    for (auto& p : av) { k += std::to_string(p.first); k += ':'; k += std::to_string(p.second); k += ';'; }
    return k;
}
int solveRCPSP_TT2_Meet(int group, int exam, const std::string& ptype="j30") {
    std::cout << "=== MEET validation " << group << ":" << exam << " ===" << std::endl;
    int NT = 0, fwdOpt = -1;
    // DR is NOT meet-preserving: it prunes exactly the cut states the other side needs.
    // bit0 = keep DR on the forward side, bit1 = on the reversed side (default 3 = both).
    const bool ordr5=g_tt2_dr5, ordr4=g_tt2_dr4;
    int drSide = 3; if (const char* dse=std::getenv("RCPSP_MEET_DRSIDE")) drSide=std::atoi(dse);
    // RCPSP_MEET_PRUNEIDX=1: also index states that DR PRUNES. They are never expanded, but
    // they are real states with real path costs, so they are legitimate meeting cuts — this
    // is what lets us keep full DR on BOTH sides and still meet in the interior.
    const bool pruneIdx = std::getenv("RCPSP_MEET_PRUNEIDX")!=nullptr;
    uint64_t prunedIndexedF=0, prunedIndexedB=0;
    std::unordered_map<std::string,int> fwdMap;   // meet key -> min g_fwd over closed states
    auto setRS = [&](){ static const double RSL[4]={0.2,0.5,0.7,1.0}; g_instance_rs=RSL[((group-1)%4+4)%4]; };
    g_instance_deadline = std::chrono::steady_clock::now()+std::chrono::seconds(astar_timeout_seconds); g_instance_deadline_set=true;
    // ---- FORWARD ----
    {
        getPetri(petri, group, exam, ptype); getRCPSP(RCPSPex, group, exam, ptype);
        NT = (int)petri.Transitions.size(); setRS(); get_tt2_dominance_table().clear();
        g_tt2_dr5=ordr5&&(drSide&1); g_tt2_dr4=ordr4&&(drSide&1);
        RCPSP_TT2 env; RCPSPState_TT2 first; RCPSPState_TT2 last=first; last.g=HCost_TT2(last,first);
        TemplateAStar<RCPSPState_TT2,int,RCPSP_TT2> astar; std::vector<RCPSPState_TT2> path;
        if (pruneIdx) g_tt2_dr_prune_observer = [&](const RCPSPState_TT2& s){
            std::string kk=meetKeyForwardNorm(s, NT); int gg=(int)s.g; ++prunedIndexedF;
            auto f=fwdMap.find(kk); if (f==fwdMap.end()||gg<f->second) fwdMap[kk]=gg; };
        astar.GetPath(&env, first, last, path);
        g_tt2_dr_prune_observer = nullptr;
        fwdOpt = path.empty()? -1 : (int)path.back().g;
        int closed=0;
        for (int i=0;i<astar.GetNumItems();i++){ const auto& it=astar.GetItem(i); if (it.where!=kClosedList) continue; ++closed;
            std::string kk=meetKeyForwardNorm(it.data, NT); int gg=(int)it.g;
            auto f=fwdMap.find(kk); if (f==fwdMap.end()||gg<f->second) fwdMap[kk]=gg; }
        std::cout << "  forward opt=" << fwdOpt << " closed=" << closed << " keys=" << fwdMap.size() << std::endl;
    }
    // ---- REVERSED ----
    int meetMin = INT_MAX, revOpt=-1;
    std::unordered_map<std::string,int> matchedBestGb;   // matching key -> min g_b (dedup by cut)
    {
        getPetri(petri, group, exam, ptype); getRCPSP(RCPSPex, group, exam, ptype);
        reverseLoadedInstance(); setRS(); get_tt2_dominance_table().clear();
        g_tt2_dr5=ordr5&&(drSide&2); g_tt2_dr4=ordr4&&(drSide&2);
        RCPSP_TT2 env; RCPSPState_TT2 first; RCPSPState_TT2 last=first; last.g=HCost_TT2(last,first);
        TemplateAStar<RCPSPState_TT2,int,RCPSP_TT2> astar; std::vector<RCPSPState_TT2> path;
        if (pruneIdx) g_tt2_dr_prune_observer = [&](const RCPSPState_TT2& s){
            std::string kk=meetKeyReversedTranslatedNorm(s, NT); int gb=(int)s.g; ++prunedIndexedB;
            if (fwdMap.count(kk)){ auto m=matchedBestGb.find(kk); if (m==matchedBestGb.end()||gb<m->second) matchedBestGb[kk]=gb; } };
        astar.GetPath(&env, first, last, path);
        g_tt2_dr_prune_observer = nullptr;
        revOpt = path.empty()? -1 : (int)path.back().g;
        for (int i=0;i<astar.GetNumItems();i++){ const auto& it=astar.GetItem(i); if (it.where!=kClosedList) continue;
            std::string kk=meetKeyReversedTranslatedNorm(it.data, NT); int gb=(int)it.g;
            if (fwdMap.count(kk)){ auto m=matchedBestGb.find(kk); if (m==matchedBestGb.end()||gb<m->second) matchedBestGb[kk]=gb; } }
    }
    // classify matches: INTERIOR (g_f>0 AND g_b>0) = a real middle meet; else an endpoint
    // (source g_f=0 / all-finished goal g_b=0), which every method shares trivially.
    uint64_t matches=matchedBestGb.size(), interior=0;
    int meetMinInt=INT_MAX, gfMinInt=INT_MAX, gfMaxInt=-1;
    for (auto& kv : matchedBestGb) {
        int gf=fwdMap[kv.first], gb=kv.second, s=gf+gb;
        if (s<meetMin) meetMin=s;
        if (gf>0 && gb>0) { ++interior; if (s<meetMinInt) meetMinInt=s;
            gfMinInt=std::min(gfMinInt,gf); gfMaxInt=std::max(gfMaxInt,gf); }
    }
    int mm = (meetMin==INT_MAX)? -1 : meetMin;
    int mmInt = (meetMinInt==INT_MAX)? -1 : meetMinInt;
    std::cout << "  reversed opt=" << revOpt << " matchingCuts=" << matches
              << " interior=" << interior;
    if (interior) std::cout << " interiorMinSum=" << mmInt << " interior g_f range=[" << gfMinInt << ".." << gfMaxInt << "]";
    std::cout << std::endl;
    if (pruneIdx) std::cout << "  prune-indexed states: fwd=" << prunedIndexedF << " rev=" << prunedIndexedB << std::endl;
    std::cout << "MEET " << group << ":" << exam << " min(g_f+g_b)=" << mm
              << " (interior=" << mmInt << ") forwardOpt=" << fwdOpt
              << "  => " << ((interior>0 && mmInt==fwdOpt)?"REAL-MIDDLE-MEET" : (mm==fwdOpt?"ENDPOINTS-ONLY":"MISMATCH")) << std::endl;
    return mm;
}

// ── Reverse branching factor (in-degree in the forward search graph) ──────────
// "How many reverses does a state have" = how many DISTINCT forward states fire into it.
// If small (≈ forward out-degree) the reverse is cheap to generate+verify; if large it's
// the enumeration blow-up. Measured on the REAL reachable graph (states the forward
// actually closed), not the naive all-countdown-values count.
int solveRCPSP_TT2_InDegree(int group, int exam, const std::string& ptype="j30") {
    std::cout << "=== IN-DEGREE (reverse branching) " << group << ":" << exam << " ===" << std::endl;
    getPetri(petri, group, exam, ptype); getRCPSP(RCPSPex, group, exam, ptype);
    { static const double RSL[4]={0.2,0.5,0.7,1.0}; g_instance_rs=RSL[((group-1)%4+4)%4]; }
    g_instance_deadline = std::chrono::steady_clock::now()+std::chrono::seconds(astar_timeout_seconds); g_instance_deadline_set=true;
    bool sDR5=g_tt2_dr5,sDR4=g_tt2_dr4,sB=g_tt2_batch;
    bool searchDR = std::getenv("RCPSP_INDEG_DR")!=nullptr;   // measure the PRACTICAL (DR'd) graph's in-degree
    g_tt2_dr5=searchDR; g_tt2_dr4=searchDR; g_tt2_batch=false; // batch off always (single-fire); DR on the SEARCH only if asked
    get_tt2_dominance_table().clear();
    RCPSP_TT2 env; RCPSPState_TT2 first; RCPSPState_TT2 last=first; last.g=HCost_TT2(last,first);
    long cap = 60000; if (const char* c=std::getenv("RCPSP_INDEG_CAP")) cap=std::atol(c);
    TemplateAStar<RCPSPState_TT2,int,RCPSP_TT2> astar; std::vector<RCPSPState_TT2> path;
    astar.InitializeSearch(&env, first, last, path);
    long steps=0; bool solved=false;
    while (astar.GetNumOpenItems()>0 && steps<cap) {
        if (g_instance_deadline_set && std::chrono::steady_clock::now()>g_instance_deadline) break;
        if (astar.DoSingleSearchStep(path)) { solved=true; break; }
        ++steps;
    }
    std::cout << "  explored steps=" << steps << " solvedFully=" << (solved?"yes":"no(capped)") << std::endl;
    // Identity = the MARKING = (finished bitset, active list with remainings) = meetKeyForward,
    // which is exactly what GetStateHash mixes (tokens are derived; == is defunct). Compare
    // that DIRECTLY (collision-free), so a hash collision cannot fake a same-transition merge.
    int NTk=(int)petri.Transitions.size();
    std::vector<RCPSPState_TT2> closed; std::unordered_set<std::string> closedKeys;
    for (int i=0;i<astar.GetNumItems();i++){ const auto& it=astar.GetItem(i); if (it.where!=kClosedList) continue;
        closed.push_back(it.data); closedKeys.insert(meetKeyForward(it.data, NTk)); }
    // regen with DR OFF so GetSuccessors returns raw successors (else children are already in the table => pruned).
    g_tt2_dr5=false; g_tt2_dr4=false;
    std::unordered_map<std::string,int> inDeg;
    std::unordered_map<std::string, std::map<int, std::set<std::string>>> firedBy;   // key(S) -> firedA -> {distinct FULL marking key(A)}
    std::unordered_map<std::string, std::map<int, std::set<std::string>>> firedById; // key(S) -> firedA -> {distinct ID-SET key(A): finished+active ids, NO rems}
    uint64_t edges=0, outSum=0;
    auto committed=[&](const RCPSPState_TT2& s){ std::bitset<128> b=s.finishedActivitiys; for(auto&p:s.activeTransitionIndices) b.set(p.first); return b; };
    auto idKey=[&](const RCPSPState_TT2& s){ std::string k="F"; for(int i=1;i<=NTk;i++) if(s.finishedActivitiys.test(i)){k+=std::to_string(i);k+=',';} k+="|A"; std::vector<int> ids; for(auto&p:s.activeTransitionIndices) ids.push_back(p.first); std::sort(ids.begin(),ids.end()); for(int id:ids){k+=std::to_string(id);k+=';';} return k; };
    // e-RULE CHECK. Over every REAL forward edge P --a--> S with wait e, test the structural
    // claims: (P1) P was itself made by a fire => some active in P carries rem == its full
    // duration; (CRIT) e is ATTAINED by a completer's rem in P (never by a survivor, whose
    // rem in P is rem^S+e > e); (PIN) e lies in the O(#activities) candidate set
    // {0} U {minSlack over survivors of S} U {dur(c) : c a completer}. A LEAK means e is an
    // inherited event time not readable off S -- that is the only case the rule misses.
    uint64_t ecTot=0, ec0=0, ecA=0, ecB=0, ecLeak=0, critOK=0, critBad=0, p1ok=0, p1viol=0;
    double remProdSum=0, remProdMax=0; uint64_t rp1=0,rp4=0,rp16=0,rp64=0,rp256=0,rpBig=0,compSum=0; int compMax=0;
    double nbProdSum=0, nbProdMax=0, smartSum=0; uint64_t blkSum=0, nblkSum=0, nbFree=0, np1=0,np4=0,np16=0,npBig=0;
    bool doValid = std::getenv("RCPSP_INDEG_VALID")!=nullptr; if (doValid) BWMEET::precompute();
    std::unordered_set<std::string> vsSeen; uint64_t vsEdges=0, vsByteSum=0, vsLightSum=0, vsZero=0; long vsByteMax=0; double vsCandSum=0;
    uint64_t rSum=0, c1ok=0, c1viol=0, fr16=0, fr256=0, fr64k=0, frBig=0; int rMax=0; double rProdSum=0, rProdMax=0;
    for (auto& A0 : closed){ RCPSPState_TT2 A=A0; A.transitionsCached=false; A.AvailableTransitionIndices_TT2.clear(); A.hKnown=false;
        std::bitset<128> ca=committed(A); std::string ka=meetKeyForward(A0, NTk);
        // wait e for each fireable activity, from the forward model's OWN availability rule
        std::vector<short> uns; for(int i=1;i<=NTk;i++) if(!A0.finishedActivitiys.test(i)) uns.push_back((short)i);
        auto av = getAvailableTransitionIndices_TT2(uns, A0.finishedActivitiys, A0.resource_nodes,
                                                   A0.activity_nodes, A0.activeTransitionIndices);
        std::map<int,int> waitOf; for(auto&pr:av) waitOf[pr.first]=pr.second;
        bool p1=false; for(auto&p:A0.activeTransitionIndices)
            if (p.second==RCPSPex.activities[p.first-1].duration) { p1=true; break; }
        if (p1) ++p1ok; else ++p1viol;
        std::vector<RCPSPState_TT2> nb; env.GetSuccessors(A,nb); outSum+=nb.size();
        for (auto& S : nb){ std::string ks=meetKeyForward(S, NTk); if (!closedKeys.count(ks)) continue; inDeg[ks]++; ++edges;
            std::bitset<128> d = committed(S) & ~ca;
            if (d.count()==1){ int fa=0; for(int i=1;i<=NTk;i++) if(d.test(i)){fa=i;break;} firedBy[ks][fa].insert(ka); firedById[ks][fa].insert(idKey(A0));
                auto wi=waitOf.find(fa); if (wi==waitOf.end()) continue; int ee=wi->second; ++ecTot;
                // minSlack over SURVIVORS of S (active in S, not the fired activity)
                int minSlack=INT_MAX;
                for(auto&p:S.activeTransitionIndices){ if(p.first==fa) continue;
                    int sl=RCPSPex.activities[p.first-1].duration - p.second; if(sl<minSlack) minSlack=sl; }
                // completers = active in P, finished in S
                bool critHit=false, durHit=false;
                for(auto&p:A0.activeTransitionIndices){ if(!S.finishedActivitiys.test(p.first)) continue;
                    if(p.second==ee) critHit=true;
                    if(RCPSPex.activities[p.first-1].duration==ee) durHit=true; }
                if (ee>0){ if(critHit) ++critOK; else ++critBad; }
                if (ee==0) ++ec0;
                else if (minSlack!=INT_MAX && ee==minSlack) ++ecA;
                else if (durHit) ++ecB;
                else ++ecLeak;
                // COST of over-generating the completers' remainings once (e,C) are pinned:
                // rem_c ranges over [1, min(dur_c, e)], so the product is the candidate count
                // this edge would cost a complete generator. e==0 => no completers => exact.
                if (ee>0){ double prod=1; int nc=0;
                    // BLOCKER split: c can affect getAvailable(P)[a] ONLY if c is a predecessor of
                    // a, or c holds a resource a demands (c's return token lands in a place a reads).
                    // Everything else is provably invisible to verify() => pure over-generation.
                    const auto& aPred = RCPSPex.backword_dependencies[fa-1];
                    const auto& aDem  = RCPSPex.activities[fa-1].resource_demands;
                    double prodB=1, prodBno=1, prodNb=1; int nb=0, nnb=0;
                    for(auto&p:A0.activeTransitionIndices){ if(!S.finishedActivitiys.test(p.first)) continue;
                        int c=p.first, m=std::min((int)RCPSPex.activities[c-1].duration, ee);
                        ++nc; prod *= m;
                        bool blk = std::find(aPred.begin(),aPred.end(),(short)c)!=aPred.end();
                        if (!blk) for(auto&d:RCPSPex.activities[c-1].resource_demands)
                            if (d.second>0 && aDem.count(d.first) && aDem.at(d.first)>0){ blk=true; break; }
                        if (blk){ ++nb; prodB*=m; prodBno*=(m-(m==ee?1:0)); } else { ++nnb; prodNb*=m; }
                    }
                    // blockers still owe "max == e", so only prodB-prodBno of their assignments survive
                    double smart = std::max(0.0, prodB-prodBno) * prodNb;
                    remProdSum += prod; if (prod>remProdMax) remProdMax=prod;
                    nbProdSum += prodNb; if (prodNb>nbProdMax) nbProdMax=prodNb;
                    smartSum  += smart;
                    blkSum += nb; nblkSum += nnb; if (nnb==0) ++nbFree;
                    if (prodNb<=1) ++np1; else if (prodNb<=4) ++np4; else if (prodNb<=16) ++np16; else ++npBig;
                    if (prod<=1) ++rp1; else if (prod<=4) ++rp4; else if (prod<=16) ++rp16;
                    else if (prod<=64) ++rp64; else if (prod<=256) ++rp256; else ++rpBig;
                    compSum += nc; if (nc>compMax) compMax=nc;
                    // REVERSIBLE FRONTIER R (C1): c can have been active in P only if none of its
                    // successors has started in S (except `a`, the one that starts on this edge).
                    // C must be a subset of R, so |R| controls the C-subset branching, and the
                    // full generator cost is prod over R of (1 + min(dur_c, e)) ["stay finished"
                    // or one of the rem values]. Also validates C1: is every TRUE completer in R?
                    { std::bitset<128> started=S.finishedActivitiys; for(auto&p:S.activeTransitionIndices) started.set(p.first);
                      std::bitset<128> inR; int nr=0; double rprod=1;
                      for(int c=1;c<=NTk;c++){ if(!S.finishedActivitiys.test(c)) continue;
                          if(RCPSPex.activities[c-1].duration<=0) continue;
                          bool ok=true; for(short s:RCPSPex.dependencies[c-1]){ if(s==fa) continue; if(started.test(s)){ok=false;break;} }
                          if(!ok) continue; inR.set(c); ++nr; rprod *= (1.0+std::min((int)RCPSPex.activities[c-1].duration, ee)); }
                      rSum+=nr; if(nr>rMax) rMax=nr; rProdSum+=rprod; if(rprod>rProdMax) rProdMax=rprod;
                      if(rprod<=16) ++fr16; else if(rprod<=256) ++fr256; else if(rprod<=65536) ++fr64k; else ++frBig;
                      bool viol=false; for(auto&p:A0.activeTransitionIndices)
                          if(S.finishedActivitiys.test(p.first) && !inR.test(p.first)) viol=true;
                      if(viol) ++c1viol; else ++c1ok; }
                    // VALID-vs-REACHABLE: with the TRUE (e,C) fixed, how many rem-assignments are
                    // genuine un-fires (fire back to exactly S)? If ~1, the availability equation
                    // pins the remainings and the generator just has to solve it. If >>1, the
                    // un-fire backward drowns in valid-but-unreachable states and the route is dead.
                    if (doValid && prod<=256.0 && vsEdges<4000){
                      std::string pk=ks+"#"+std::to_string(fa);
                      if (vsSeen.insert(pk).second){
                        std::vector<int> cid,cmx;
                        for(auto&p:A0.activeTransitionIndices){ if(!S.finishedActivitiys.test(p.first)) continue;
                            cid.push_back(p.first); cmx.push_back(std::min((int)RCPSPex.activities[p.first-1].duration, ee)); }
                        std::vector<int> od(cid.size(),1); long okB=0, okL=0;
                        while(true){
                            std::map<int,int> rr; for(size_t k=0;k<cid.size();++k) rr[cid[k]]=od[k];
                            RCPSPState_TT2 pre;
                            if (BWMEET::buildPre(S,fa,ee,rr,pre)){
                                std::vector<short> u2; for(int i=1;i<=NTk;i++) if(!pre.finishedActivitiys.test(i)) u2.push_back((short)i);
                                auto a2=getAvailableTransitionIndices_TT2(u2,pre.finishedActivitiys,pre.resource_nodes,
                                                                         pre.activity_nodes,pre.activeTransitionIndices);
                                int dd=-1; for(auto&pr:a2) if(pr.first==fa){ dd=pr.second; break; }
                                if (dd==ee){ ++okL; RCPSPState_TT2 ch(pre,(short)fa,(short)ee,1);
                                             if (meetKeyForward(ch,NTk)==ks) ++okB; }
                            }
                            size_t k=0; while(k<od.size() && ++od[k]>cmx[k]){ od[k]=1; ++k; }
                            if (k>=od.size()) break;
                        }
                        ++vsEdges; vsByteSum+=okB; vsLightSum+=okL; vsCandSum+=prod;
                        if (okB>vsByteMax) vsByteMax=okB; if (okB==0) ++vsZero;
                      }
                    }
                }
            } } }
    std::cout << "  e-RULE: edges=" << ecTot << "  e==0:" << ec0 << "  CaseA(e==minSlack):" << ecA
              << "  CaseB(e==dur(completer)):" << ecB << "  LEAK:" << ecLeak
              << "  covered=" << (ecTot? 100.0*(ec0+ecA+ecB)/ecTot : 0) << "%" << std::endl;
    std::cout << "  e-ATTAINED-by-completer (e>0): ok=" << critOK << " bad=" << critBad
              << "   P1(some active has rem==dur): ok=" << p1ok << " viol=" << p1viol << std::endl;
    { uint64_t ne=critOK+critBad;
      std::cout << "  rem-OVERGEN (e>0 edges=" << ne << "): candidates/edge avg=" << (ne? remProdSum/ne : 0)
                << " max=" << remProdMax << "  |C| avg=" << (ne? (double)compSum/ne : 0) << " max=" << compMax
                << "  hist 1:" << rp1 << " 2-4:" << rp4 << " 5-16:" << rp16 << " 17-64:" << rp64
                << " 65-256:" << rp256 << " >256:" << rpBig << std::endl;
      std::cout << "  BLOCKER-SPLIT: |C| blockers avg=" << (ne? (double)blkSum/ne : 0)
                << " non-blockers avg=" << (ne? (double)nblkSum/ne : 0)
                << "  edges with NO non-blocker=" << nbFree << " (" << (ne? 100.0*nbFree/ne : 0) << "%)"
                << "  nonblk-cands/edge avg=" << (ne? nbProdSum/ne : 0) << " max=" << nbProdMax
                << "  hist 1:" << np1 << " 2-4:" << np4 << " 5-16:" << np16 << " >16:" << npBig << std::endl;
      std::cout << "  SMART-GEN (blockers pinned by max==e, non-blockers free): cands/edge avg="
                << (ne? smartSum/ne : 0) << "   vs dumb " << (ne? remProdSum/ne : 0)
                << "   speedup=" << (smartSum>0? remProdSum/smartSum : 0) << "x" << std::endl;
      std::cout << "  FRONTIER R (C1): |R| avg=" << (ne? (double)rSum/ne : 0) << " max=" << rMax
                << "  full-gen cands/edge avg=" << (ne? rProdSum/ne : 0) << " max=" << rProdMax
                << "  hist <=16:" << fr16 << " <=256:" << fr256 << " <=64k:" << fr64k << " >64k:" << frBig
                << "  C1 ok=" << c1ok << " VIOL=" << c1viol << std::endl;
      if (doValid) std::cout << "  VALID-vs-REACHABLE (true e,C fixed; " << vsEdges << " sampled pairs): "
                << "byte-valid un-fires/pair avg=" << (vsEdges? (double)vsByteSum/vsEdges : 0)
                << " max=" << vsByteMax << " zero=" << vsZero
                << " | light-valid avg=" << (vsEdges? (double)vsLightSum/vsEdges : 0)
                << " | candidates avg=" << (vsEdges? vsCandSum/vsEdges : 0)
                << " => yield " << (vsCandSum>0? 100.0*vsByteSum/vsCandSum : 0) << "%" << std::endl; }
    // SAME-TRANSITION MERGE = a state with >1 DISTINCT-marking predecessor firing the SAME activity.
    uint64_t distinctPairs=0, sameTransStates=0; size_t maxSameDup=0;
    for (auto& kv : firedBy){ distinctPairs += kv.second.size(); bool dup=false;
        for (auto& p : kv.second){ if (p.second.size()>1){ dup=true; if(p.second.size()>maxSameDup) maxSameDup=p.second.size(); } }
        if (dup) ++sameTransStates; }
    std::cout << "  [marking] edges=" << edges << " distinct(state,firedA)=" << distinctPairs
              << " edges/pairs=" << (distinctPairs? (double)edges/distinctPairs : 0)
              << "  SAME-TRANS-MERGE maxDup=" << maxSameDup << " states=" << sameTransStates << std::endl;
    // STRUCTURAL vs SLACK: for multi-predecessor (state,firedA) groups, do the distinct
    // predecessors have distinct ID-SETS (structural) or the SAME id-set with different
    // remainings (slack = ∝-duration case)? markings/id-sets ≈ 1 => structural; > 1 => slack.
    uint64_t multiMark=0, multiIds=0;
    for (auto& kv : firedBy){ for (auto& p : kv.second){ if (p.second.size()>1){
        multiMark += p.second.size(); multiIds += firedById[kv.first][p.first].size(); }}}
    std::cout << "  STRUCT-vs-SLACK: multiPred markings=" << multiMark << " id-sets=" << multiIds
              << " markings/id-sets=" << (multiIds? (double)multiMark/multiIds : 0)
              << "  (~1 => STRUCTURAL, >1 => SLACK/same-set-diff-rem)" << std::endl;
    long h0=0,h1=0,h2=0,h3=0,h4=0,h5=0,hmore=0; int mx=0; long multi=0;
    for (auto& A : closed){ std::string k=meetKeyForward(A, NTk); int d=0; auto it=inDeg.find(k); if (it!=inDeg.end()) d=it->second;
        if (d==0)h0++; else if(d==1)h1++; else if(d==2)h2++; else if(d==3)h3++; else if(d==4)h4++; else if(d==5)h5++; else hmore++;
        if (d>1) multi++; if (d>mx) mx=d; }
    long N=(long)closed.size();
    std::cout << "  closed states=" << N << " (distinct-marking " << closedKeys.size() << ")  out-degree avg=" << (N? (double)outSum/N : 0) << std::endl;
    std::cout << "  in-degree: 0=" << h0 << " 1=" << h1 << " 2=" << h2 << " 3=" << h3
              << " 4=" << h4 << " 5=" << h5 << " >5=" << hmore << "  MAX=" << mx << std::endl;
    std::cout << "INDEG " << group << ":" << exam << " N=" << N << " maxRev=" << mx
              << " sameTransMergeMax=" << maxSameDup << " sameTransStates=" << sameTransStates << std::endl;
    g_tt2_dr5=sDR5; g_tt2_dr4=sDR4; g_tt2_batch=sB;
    return mx;
}

// ── Meet-test for the UN-FIRE backward (#1), not the reversed instance ────────
// #1's states are already forward-frame (the un-fire reconstructs the forward marking
// before the activity was activated), so we key them with meetKeyForward directly — NO
// translation, NO straddle. This tests whether the un-fire design meets forward at
// INTERIOR cuts (it should, by construction). #1 may dead-end before the source on j30
// (clamp/multi-completer); we still collect whatever it closed and count interior meets.
int solveRCPSP_TT2_MeetUnfire(int group, int exam, const std::string& ptype="j30") {
    std::cout << "=== UN-FIRE MEET (#1) " << group << ":" << exam << " ===" << std::endl;
    getPetri(petri, group, exam, ptype); getRCPSP(RCPSPex, group, exam, ptype);
    int NT = (int)petri.Transitions.size();
    { static const double RSL[4]={0.2,0.5,0.7,1.0}; g_instance_rs=RSL[((group-1)%4+4)%4]; }
    g_instance_deadline = std::chrono::steady_clock::now()+std::chrono::seconds(astar_timeout_seconds); g_instance_deadline_set=true;
    // ---- FORWARD closed set (no DR, so we keep every interior cut) ----
    std::unordered_map<std::string,int> fwdMap; int fwdOpt=-1, fClosed=0;
    {
        get_tt2_dominance_table().clear();
        RCPSP_TT2 env; RCPSPState_TT2 first; RCPSPState_TT2 last=first; last.g=HCost_TT2(last,first);
        TemplateAStar<RCPSPState_TT2,int,RCPSP_TT2> astar; std::vector<RCPSPState_TT2> path;
        astar.GetPath(&env, first, last, path);
        fwdOpt = path.empty()? -1 : (int)path.back().g;
        for (int i=0;i<astar.GetNumItems();i++){ const auto& it=astar.GetItem(i); if (it.where!=kClosedList) continue; ++fClosed;
            std::string kk=meetKeyForward(it.data, NT); int gg=(int)it.g;
            auto f=fwdMap.find(kk); if (f==fwdMap.end()||gg<f->second) fwdMap[kk]=gg; }
        std::cout << "  forward opt=" << fwdOpt << " closed=" << fClosed << " keys=" << fwdMap.size() << std::endl;
    }
    // ---- UN-FIRE backward (#1) from the all-finished goal ----
    BWMEET::precompute();
    RCPSPState_TT2 root; BWMEET::deriveResources(root.activeTransitionIndices, root.resource_nodes);
    BWMEET::BackwardMeetEnv env; env.root = root;
    RCPSPState_TT2 goal;
    for (int i=1;i<=BWMEET::NT;i++) goal.finishedActivitiys.set(i);
    for (auto& an: goal.activity_nodes) an={0,0};
    if (BWMEET::g_sinkOutIdx>=0) goal.activity_nodes[BWMEET::g_sinkOutIdx]={1,0};
    goal.activeTransitionIndices.clear();
    BWMEET::deriveResources(goal.activeTransitionIndices, goal.resource_nodes);
    goal.g=0;
    TemplateAStar<RCPSPState_TT2,int,BWMEET::BackwardMeetEnv> astar; std::vector<RCPSPState_TT2> path;
    astar.GetPath(&env, goal, root, path);
    bool bwdComplete = !path.empty();
    // collect #1 closed states, key with meetKeyForward (already forward-frame), match forward
    std::unordered_map<std::string,int> matchedBestGb; int bClosed=0;
    for (int i=0;i<astar.GetNumItems();i++){ const auto& it=astar.GetItem(i); if (it.where!=kClosedList) continue; ++bClosed;
        std::string kk=meetKeyForward(it.data, NT); int gb=(int)it.g;   // #1 g grows from 0 at goal = opt - t
        if (fwdMap.count(kk)){ auto m=matchedBestGb.find(kk); if (m==matchedBestGb.end()||gb<m->second) matchedBestGb[kk]=gb; } }
    uint64_t matches=matchedBestGb.size(), interior=0; int meetMin=INT_MAX, meetMinInt=INT_MAX, gfMinInt=INT_MAX, gfMaxInt=-1;
    for (auto& kv : matchedBestGb){ int gf=fwdMap[kv.first], gb=kv.second, s=gf+gb;
        if (s<meetMin) meetMin=s;
        if (gf>0 && gb>0){ ++interior; if (s<meetMinInt) meetMinInt=s; gfMinInt=std::min(gfMinInt,gf); gfMaxInt=std::max(gfMaxInt,gf); } }
    std::cout << "  #1 backward closed=" << bClosed << " reachedSource=" << (bwdComplete?"yes":"NO(dead-end)")
              << " matchingCuts=" << matches << " interior=" << interior;
    if (interior) std::cout << " interiorMinSum=" << meetMinInt << " interior g_f range=[" << gfMinInt << ".." << gfMaxInt << "]";
    std::cout << std::endl;
    std::cout << "UNFIRE-MEET " << group << ":" << exam << " forwardOpt=" << fwdOpt
              << " interiorMeets=" << interior
              << "  => " << (interior>0 ? "REAL-INTERIOR-MEET" : "endpoints-only") << std::endl;
    return (int)interior;
}

// ── Bidirectional meet-in-the-middle (BAE*-style) ─────────────────────────────
// Forward A* on the net N and a backward A* on the reversed net N^R, meeting via the
// translated key (θ_fwd = τ − θ_bwd). Incumbent U = min(g_f + g_b) over matching closed
// keys. Sound termination: stop when min(f_F, f_B) ≥ U — because for any state s,
// g_f(s)+g_b(s) ≥ g_f(s)+h_F(s) = f_F(s) (h_F admissible ≤ true s→sink cost ≤ g_b(s)),
// so once both frontiers' min-f ≥ U no unexpanded meet can beat U. petri/RCPSPex are
// globals read by the env, so we keep both instances and O(1) std::swap between sides.
// Per-instance BAE* stats, filled by solveRCPSP_TT2_BAE so the benchmark writer can emit
// a row without re-running anything. g_bae_proven distinguishes a PROVEN optimum from a
// deadline exit that merely holds a meet-derived incumbent (a valid UB, not a proof).
static uint64_t g_bae_expF=0, g_bae_expB=0, g_bae_meets=0;
static long     g_bae_firstMeet=-1;
static int      g_bae_uMeet=-1, g_bae_biDR4=0;
static bool     g_bae_proven=false;
static double   g_bae_time=0.0;
int solveRCPSP_TT2_BAE(int group, int exam, const std::string& ptype="j30") {
    std::cout << "=== BAE* bidirectional " << group << ":" << exam << " ===" << std::endl;
    getPetri(petri, group, exam, ptype); getRCPSP(RCPSPex, group, exam, ptype);
    int NT = (int)petri.Transitions.size();
    { static const double RSL[4]={0.2,0.5,0.7,1.0}; g_instance_rs=RSL[((group-1)%4+4)%4]; }
    g_instance_deadline = std::chrono::steady_clock::now()+std::chrono::seconds(astar_timeout_seconds);
    g_instance_deadline_set = true;
    bool sDR5=g_tt2_dr5,sDR4=g_tt2_dr4,sB=g_tt2_batch;
    // DR4 OFF by default: DR5 and DR4 TOGETHER leave the two sides' surviving cut states
    // DISJOINT, so the middle meet never fires (measured on j30: DR5-only 7/7 real middle
    // meets at the optimum, DR4-only 5/7, both 2/7). DR5 is also the far bigger lever
    // (10-143x vs ~2.4x), so this costs almost nothing. RCPSP_BI_DR4=1 restores it.
    // batch OFF: multi-fire edges likewise land the two directions on disjoint cut sets.
    // RCPSP_BI_DR4 is a PER-SIDE bitmask (bit0=forward, bit1=reversed). DR4 canonicalises
    // toward early starts going forward and toward late finishes going backward, so running
    // it on BOTH sides drives them to different canonical schedules and the shared cut states
    // vanish. One-sided DR4 may keep the meet while recovering part of its pruning.
    // DEFAULT 2 = DR4 on the REVERSED side only: measured best of the four (j30) — it keeps
    // real middle meets on 7/7 while costing only 0-10% more expansions than DR4-on-both on
    // 5/7, because the reversed side is the expensive one (expandedB 68554->24581 on 1:1).
    // DR4 on BOTH sides collapses the meet back to the trivial endpoint on 5/7. Use 0 when
    // the goal is the EARLIEST optimal upper bound (first meet at 14-69% vs 40-99%).
    int biDR4 = 2; if (const char* b4=std::getenv("RCPSP_BI_DR4")) biDR4 = std::atoi(b4);
    g_tt2_dr5=true; g_tt2_dr4=false; g_tt2_batch=false;
    g_tt2_dom_side=0; get_tt2_dominance_table().clear();
    g_tt2_dom_side=1; get_tt2_dominance_table().clear();
    // Stash holds the non-current side; start with global=forward, stash=reversed.
    P_RCPSP::PetriExample petriStash; RCPSP_example rcpspStash;
    petriStash = petri; rcpspStash = RCPSPex;   // copy forward into stash
    reverseLoadedInstance();                     // global := reversed
    std::swap(petri, petriStash); std::swap(RCPSPex, rcpspStash);  // global := forward, stash := reversed
    bool curFwd = true;
    auto useFwd = [&](bool want){ if (want!=curFwd){ std::swap(petri,petriStash); std::swap(RCPSPex,rcpspStash); curFwd=want; }
        g_tt2_dom_side = want?0:1; g_tt2_dr4 = (want ? (biDR4&1) : (biDR4&2)) != 0; };

    RCPSP_TT2 env;                               // stateless; reads globals
    useFwd(true);  RCPSPState_TT2 firstF; RCPSPState_TT2 lastF=firstF;
    useFwd(false); RCPSPState_TT2 firstB; RCPSPState_TT2 lastB=firstB;
    TemplateAStar<RCPSPState_TT2,int,RCPSP_TT2> astarF, astarB;
    std::vector<RCPSPState_TT2> pathF, pathB;
    useFwd(true);  astarF.InitializeSearch(&env, firstF, lastF, pathF);
    useFwd(false); astarB.InitializeSearch(&env, firstB, lastB, pathB);

    std::unordered_map<std::string,int> fClosed, bClosed;
    int U = INT_MAX; bool doneF=false, doneB=false; uint64_t expF=0, expB=0;
    uint64_t meets=0; long firstMeetExp=-1; int uMeet=INT_MAX;   // did the middle meet fire, and when?
    bool proven=false;   // true only if we exited with optimality PROVEN (not on the deadline)
    const double INF = 1e18;
    auto t0 = std::chrono::high_resolution_clock::now();
    while (true) {
        if (g_instance_deadline_set && std::chrono::steady_clock::now() > g_instance_deadline) break;
        // frontier f on each live side
        double fF=INF, fB=INF, gF=INF, gB=INF;
        useFwd(true);
        if (!doneF) { if (astarF.GetNumOpenItems()==0) doneF=true;
            else { RCPSPState_TT2 n=astarF.CheckNextNode(); double g=0; astarF.GetOpenListGCost(n,g); gF=g; fF=g+env.HCost(n,lastF); } }
        useFwd(false);
        if (!doneB) { if (astarB.GetNumOpenItems()==0) doneB=true;
            else { RCPSPState_TT2 n=astarB.CheckNextNode(); double g=0; astarB.GetOpenListGCost(n,g); gB=g; fB=g+env.HCost(n,lastB); } }
        if (doneF && doneB) { proven=true; break; }
        if (std::min(fF,fB) >= (double)U) { proven=true; break; }   // optimal: U proven
        // Balance by smaller g (Nicholson): push both frontiers toward the middle cut
        // (g_f≈g_b≈opt/2), instead of feeding the low-f (weak-heuristic) side.
        bool goFwd = !doneF && (doneB || gF <= gB);
        if (goFwd) {                               // expand forward
            useFwd(true);
            RCPSPState_TT2 n = astarF.CheckNextNode();
            doneF = astarF.DoSingleSearchStep(pathF); ++expF;
            double g=0; astarF.GetClosedListGCost(n,g);
            std::string k = meetKeyForwardNorm(n, NT);
            auto it=fClosed.find(k); if (it==fClosed.end()||(int)g<it->second) fClosed[k]=(int)g;
            auto jt=bClosed.find(k); if (jt!=bClosed.end()){ ++meets; int c=(int)g+jt->second;
                if (firstMeetExp<0) firstMeetExp=(long)(expF+expB); if (c<uMeet) uMeet=c; U=std::min(U,c); }
            if (!pathF.empty()) { U=std::min(U,(int)pathF.back().g); proven=true; break; }   // forward solved its direction => optimal
        } else {                                   // expand backward
            useFwd(false);
            RCPSPState_TT2 n = astarB.CheckNextNode();
            doneB = astarB.DoSingleSearchStep(pathB); ++expB;
            double g=0; astarB.GetClosedListGCost(n,g);
            std::string k = meetKeyReversedTranslatedNorm(n, NT);
            auto it=bClosed.find(k); if (it==bClosed.end()||(int)g<it->second) bClosed[k]=(int)g;
            auto jt=fClosed.find(k); if (jt!=fClosed.end()){ ++meets; int c=(int)g+jt->second;
                if (firstMeetExp<0) firstMeetExp=(long)(expF+expB); if (c<uMeet) uMeet=c; U=std::min(U,c); }
            if (!pathB.empty()) { U=std::min(U,(int)pathB.back().g); proven=true; break; }   // backward solved its direction => optimal
        }
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> el = t1 - t0;
    int mk = (U==INT_MAX)? -1 : U;
    std::cout << "BAE* " << group << ":" << exam << " makespan=" << mk
              << " expandedF=" << expF << " expandedB=" << expB << " total=" << (expF+expB)
              << " time=" << el.count() << "s"
              << "  norm=" << (g_meet_norm?1:0) << " biDR4=" << biDR4 << " meets=" << meets
              << " firstMeetAtExp=" << firstMeetExp << " U_from_meet=" << (uMeet==INT_MAX?-1:uMeet) << std::endl;
    g_bae_expF=expF; g_bae_expB=expB; g_bae_meets=meets; g_bae_firstMeet=firstMeetExp;
    g_bae_uMeet=(uMeet==INT_MAX?-1:uMeet); g_bae_proven=proven; g_bae_time=el.count(); g_bae_biDR4=biDR4;
    g_tt2_dr5=sDR5; g_tt2_dr4=sDR4; g_tt2_batch=sB;
    return mk;
}

// ── Meet-in-the-Middle (MM, Holte et al.) over forward N and reversed N^R ──────
// Custom bidirectional A* with MM priority pr(n)=max(g+h, 2g). Expands the side with
// smaller prmin; stops when U ≤ C=min(prminF,prminB). MM guarantees neither side expands
// a node with g > C*/2, which caps the (harder) reversed half — the win the f-balanced
// tt2bae lacks. Per-side DR via g_tt2_dom_side; meet via the translated key. Consistent
// heuristic (paper Prop. 2-4) ⇒ no reopening; lazy-deleted stale PQ entries.
int solveRCPSP_TT2_MM(int group, int exam, const std::string& ptype="j30") {
    std::cout << "=== MM bidirectional " << group << ":" << exam << " ===" << std::endl;
    getPetri(petri, group, exam, ptype); getRCPSP(RCPSPex, group, exam, ptype);
    int NT = (int)petri.Transitions.size();
    { static const double RSL[4]={0.2,0.5,0.7,1.0}; g_instance_rs=RSL[((group-1)%4+4)%4]; }
    g_instance_deadline = std::chrono::steady_clock::now()+std::chrono::seconds(astar_timeout_seconds); g_instance_deadline_set=true;
    bool sDR5=g_tt2_dr5,sDR4=g_tt2_dr4,sB=g_tt2_batch;
    g_tt2_dr5=true; g_tt2_dr4=(std::getenv("RCPSP_BI_DR4")!=nullptr); g_tt2_batch=false;   // see BAE*: DR4 disjoints the cut sets
    g_tt2_dom_side=0; get_tt2_dominance_table().clear();
    g_tt2_dom_side=1; get_tt2_dominance_table().clear();
    P_RCPSP::PetriExample petriStash; RCPSP_example rcpspStash;
    petriStash=petri; rcpspStash=RCPSPex; reverseLoadedInstance();
    std::swap(petri,petriStash); std::swap(RCPSPex,rcpspStash);
    bool curFwd=true;
    auto useFwd=[&](bool w){ if(w!=curFwd){std::swap(petri,petriStash);std::swap(RCPSPex,rcpspStash);curFwd=w;} g_tt2_dom_side=w?0:1; };
    RCPSP_TT2 env; RCPSPState_TT2 dummy;

    struct PQE { double pr; int g; int idx; };
    struct Cmp { bool operator()(const PQE&a,const PQE&b)const{ return a.pr>b.pr; } };  // min-heap on pr
    std::priority_queue<PQE,std::vector<PQE>,Cmp> openF, openB;
    std::vector<RCPSPState_TT2> poolF, poolB;
    std::unordered_map<uint64_t,int> closedF, closedB;      // raw state hash -> best closed g
    std::unordered_map<std::string,int> meetF, meetB;       // translated meet key -> min closed g
    auto allFin=[&](const RCPSPState_TT2& s){ int c=0; for(int i=1;i<=NT;i++) if(s.finishedActivitiys.test(i))++c; return c==NT; };
    const double INF=1e18;

    useFwd(true);  { RCPSPState_TT2 r; double h=env.HCost(r,dummy); poolF.push_back(r); openF.push({std::max(h,0.0),0,0}); }
    useFwd(false); { RCPSPState_TT2 r; double h=env.HCost(r,dummy); poolB.push_back(r); openB.push({std::max(h,0.0),0,0}); }

    int U=INT_MAX; uint64_t expF=0, expB=0;
    auto t0=std::chrono::high_resolution_clock::now();
    auto peekPr=[&](std::priority_queue<PQE,std::vector<PQE>,Cmp>& pq, std::unordered_map<uint64_t,int>& cl, std::vector<RCPSPState_TT2>& pool)->double{
        while(!pq.empty()){ const PQE& e=pq.top(); auto it=cl.find(env.GetStateHash(pool[e.idx]));
            if(it!=cl.end() && it->second<=e.g){ pq.pop(); continue; } return e.pr; }
        return INF;
    };
    auto expand=[&](bool fwd){
        useFwd(fwd);
        auto& pq=fwd?openF:openB; auto& pool=fwd?poolF:poolB; auto& cl=fwd?closedF:closedB;
        auto& meMap=fwd?meetF:meetB; auto& otMap=fwd?meetB:meetF;
        PQE e=pq.top(); pq.pop();
        RCPSPState_TT2 s=pool[e.idx];
        uint64_t hh=env.GetStateHash(s);
        auto cit=cl.find(hh); if(cit!=cl.end() && cit->second<=e.g) return;
        cl[hh]=e.g; if(fwd)++expF; else ++expB;
        std::string k = fwd?meetKeyForwardNorm(s,NT):meetKeyReversedTranslatedNorm(s,NT);
        auto mit=meMap.find(k); if(mit==meMap.end()||e.g<mit->second) meMap[k]=e.g;
        auto ot=otMap.find(k); if(ot!=otMap.end()) U=std::min(U,e.g+ot->second);
        if(allFin(s)) U=std::min(U,e.g);
        std::vector<RCPSPState_TT2> nbrs; env.GetSuccessors(s,nbrs);
        for(auto& c:nbrs){ int cg=(int)c.g; uint64_t ch=env.GetStateHash(c);
            auto xit=cl.find(ch); if(xit!=cl.end() && xit->second<=cg) continue;
            double h=env.HCost(c,dummy); if((double)cg+h>=(double)U) continue;
            pool.push_back(c); pq.push({std::max((double)cg+h, 2.0*cg),cg,(int)pool.size()-1}); }
    };
    while(true){
        if(g_instance_deadline_set && std::chrono::steady_clock::now()>g_instance_deadline) break;
        useFwd(true);  double cF=peekPr(openF,closedF,poolF);
        useFwd(false); double cB=peekPr(openB,closedB,poolB);
        double C=std::min(cF,cB);
        if((double)U<=C) break;
        if(cF>=INF && cB>=INF) break;
        expand(cF<=cB);
    }
    auto t1=std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> el=t1-t0;
    int mk=(U==INT_MAX)?-1:U;
    std::cout << "MM " << group << ":" << exam << " makespan=" << mk
              << " expandedF=" << expF << " expandedB=" << expB << " total=" << (expF+expB)
              << " time=" << el.count() << "s" << std::endl;
    g_tt2_dr5=sDR5; g_tt2_dr4=sDR4; g_tt2_batch=sB;
    return mk;
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
    // Optional cap on dominance-table entries per table (memory bound); the table
    // keeps checking above the cap, it just stops storing. 0 or negative = unlimited.
    if (const char* c = std::getenv("RCPSP_DOM_CAP")) {
        long cap = std::atol(c);
        g_dom_store_cap = (cap > 0) ? cap : std::numeric_limits<long>::max();
    }
    // Per-instance wall-clock budget (seconds); default 300 as on the server.
    // Local testing uses smaller values so heavy instances stay cheap.
    if (const char* t = std::getenv("RCPSP_TIMEOUT_S")) {
        long long ts = std::atoll(t);
        if (ts > 0) { astar_timeout_seconds = ts; std::cout << "Timeout: " << ts << "s\n"; }
    }
    // Experiment flags (all default off; see Globals.h)
    if (const char* e = std::getenv("RCPSP_UB"))        g_use_ub        = std::atoi(e) != 0;
    if (const char* e = std::getenv("RCPSP_LEFTSHIFT")) g_use_leftshift = std::atoi(e) != 0;
    if (const char* e = std::getenv("RCPSP_BIDIR"))     g_use_bidir     = std::atoi(e) != 0;
    if (const char* e = std::getenv("RCPSP_HYBRID"))    g_use_hybrid    = std::atoi(e) != 0;
    if (const char* e = std::getenv("RCPSP_HYBRID_T"))  g_hybrid_threshold = (float)std::atof(e);
    if (const char* e = std::getenv("RCPSP_LEAN"))      g_use_lean      = std::atoi(e) != 0;
    if (const char* e = std::getenv("RCPSP_HGREED"))    setting.use_greed_conflic_resultion_asstimation = std::atoi(e) != 0;
    if (const char* e = std::getenv("RCPSP_INLINE"))    g_use_inline    = std::atoi(e) != 0;
    if (const char* e = std::getenv("RCPSP_SKYLINE"))   g_dom_skyline   = std::atoi(e) != 0;
    if (const char* e = std::getenv("RCPSP_LAZY"))      g_use_lazy      = std::atoi(e) != 0;
    if (const char* e = std::getenv("RCPSP_TT2_DR5"))   g_tt2_dr5       = std::atoi(e) != 0;
    if (const char* e = std::getenv("RCPSP_TT2_BATCH")) g_tt2_batch     = std::atoi(e) != 0;
    if (const char* e = std::getenv("RCPSP_TT2_DR4"))   g_tt2_dr4       = std::atoi(e) != 0;
    if (const char* e = std::getenv("RCPSP_TT2_IMMSEL")) g_tt2_immsel   = std::atoi(e) != 0;
    if (const char* e = std::getenv("RCPSP_TT2_FREEFIRE")) g_tt2_freefire = std::atoi(e) != 0;
    if (const char* e = std::getenv("RCPSP_TT2_BATCH_CAP")) g_tt2_batch_cap = std::atol(e);
    if (const char* e = std::getenv("RCPSP_TT2_SYM"))   g_tt2_sym       = std::atoi(e);
    if (const char* e = std::getenv("RCPSP_TT2_SYMTB"))  g_tt2_sym_tiebreak = std::atoi(e) != 0;
    if (const char* e = std::getenv("RCPSP_TT2_GENDESC")) g_tt2_gendesc   = std::atoi(e) != 0;
    if (const char* e = std::getenv("RCPSP_TT2_THETA"))  g_tt2_theta     = std::atoi(e) != 0;
    if (const char* e = std::getenv("RCPSP_TT2_RSADAPT"))         g_tt2_rsadapt         = std::atoi(e) != 0;
    if (const char* e = std::getenv("RCPSP_TT2_RS_THRESH"))       g_tt2_rs_threshold    = std::atof(e);
    if (const char* e = std::getenv("RCPSP_TT2_SINGLERES"))         g_tt2_singleres         = std::atoi(e) != 0;
    if (const char* e = std::getenv("RCPSP_TT2_SINGLERES_EXPAND"))  g_tt2_singleres_expand  = std::atol(e);
    if (const char* e = std::getenv("RCPSP_TT2_SINGLERES_REL"))     g_tt2_singleres_rel     = std::atoi(e) != 0;  // relative-time residual (no abs_start); validate makespans first
    if (const char* e = std::getenv("RCPSP_TT2_SINGLERES_MAXSIZE")) g_tt2_singleres_maxsize = std::atoi(e);
    if (const char* e = std::getenv("RCPSP_TT2_SINGLERES_VIACBS"))   { if (std::atoi(e)!=0) g_subsolver = 1; }  // legacy alias
    if (const char* e = std::getenv("RCPSP_TT2_SINGLERES_VIATT2"))   { if (std::atoi(e)!=0) g_subsolver = 2; }  // legacy alias
    if (const char* e = std::getenv("RCPSP_TT2_UB"))                g_tt2_ub                = std::atoi(e) != 0;
    if (const char* e = std::getenv("RCPSP_TT2_HIERRS"))            g_tt2_hierrs            = std::atoi(e) != 0;
    if (const char* e = std::getenv("RCPSP_TT2_HIERRS_EXPAND"))     g_tt2_hierrs_expand     = std::atol(e);
    if (const char* e = std::getenv("RCPSP_HIERRS_TARGET"))         g_hierrs_target_override = std::atof(e);
    if (g_tt2_ub)        std::cout << "TT2 UB pruning ON: SGS-seeded incumbent, drop children with f > incumbent (memory)\n";
    if (g_tt2_hierrs)    std::cout << "TT2 bound: hierarchical RS-relaxation LB ON (next-RS inflated caps; expand=" << g_tt2_hierrs_expand << ")\n";
    if (g_tt2_rsadapt)   std::cout << "TT2 RS-adaptive gating ON: expensive bounds fire only if RS<=" << g_tt2_rs_threshold << "\n";
    if (g_tt2_singleres) std::cout << "TT2 bound: single-resource max LB ON (expand=" << g_tt2_singleres_expand
                                   << " maxsize=" << g_tt2_singleres_maxsize << ")\n";
    if (const char* e = std::getenv("RCPSP_WARMSTART"))          g_use_warmstart     = std::atoi(e) != 0;
    if (const char* e = std::getenv("RCPSP_WARMSTART_K"))        g_warmstart_k       = std::atof(e);
    if (const char* e = std::getenv("RCPSP_WARMSTART_BUDGET_S")) g_warmstart_budget_s = std::atoll(e);
    if (const char* e = std::getenv("RCPSP_WARMSTART_DIR"))      g_warmstart_dir     = std::atoi(e);
    if (const char* e = std::getenv("RCPSP_WARMSTART_REORDER"))  g_warmstart_reorder = std::atoi(e) != 0;
    if (const char* e = std::getenv("RCPSP_WARMSTART_RS"))       g_warmstart_rs      = std::atof(e);
    if (const char* e = std::getenv("RCPSP_SETDELAY"))           g_use_setdelay      = std::atoi(e) != 0;
    if (const char* e = std::getenv("RCPSP_DR4"))                g_use_dr4           = std::atoi(e) != 0;
    if (const char* e = std::getenv("RCPSP_NMD"))                g_use_nmd           = std::atoi(e) != 0;
    if (const char* e = std::getenv("RCPSP_ORDERSWAP"))          g_orderswap         = std::atoi(e) != 0;
    if (const char* e = std::getenv("RCPSP_MDA_RECURSE"))        setting.use_mda_recursive_delay = std::atoi(e) != 0;
    if (const char* e = std::getenv("RCPSP_NMD_PREC"))           setting.use_nmd_precedence      = std::atoi(e) != 0;
    if (const char* e = std::getenv("RCPSP_NMD_PREC_REC"))       setting.use_nmd_prec_record     = std::atoi(e) != 0;
    if (const char* e = std::getenv("RCPSP_STAR")) {             // optimal-reachability tracer
        g_star.clear();
        std::string s(e); size_t p = 0;
        while (p < s.size()) { size_t c = s.find(',', p);
            g_star.push_back((short)std::atoi(s.substr(p, c==std::string::npos?c:c-p).c_str()));
            if (c==std::string::npos) break; p = c+1; }
        std::cout << "STAR tracer: loaded " << g_star.size() << " optimal starts\n";
    }
    // Expose the non-minimal delay to the sweep/single-config runner (previously
    // only reachable via `nmd_test`). Improvement 2 (recursive MDA split) requires it.
    setting.use_non_minimal_delay = g_use_nmd;
    if (setting.use_mda_recursive_delay && !g_use_nmd)
        std::cout << "WARNING: RCPSP_MDA_RECURSE=1 has no effect without RCPSP_NMD=1\n";
    if (g_use_nmd) std::cout << "Delay policy: NON-MINIMAL"
                             << (setting.use_mda_recursive_delay ? " + recursive MDA split (Improvement 2)" : "")
                             << "\n";
    if (const char* e = std::getenv("RCPSP_CBS_THETA"))          g_cbs_theta         = std::atoi(e) != 0;
    if (const char* e = std::getenv("RCPSP_CBS_SUBSET"))         g_cbs_subset        = std::atoi(e) != 0;
    if (const char* e = std::getenv("RCPSP_CBS_SUBSET_EXPAND"))  g_cbs_subset_expand = std::atol(e);
    if (const char* e = std::getenv("RCPSP_CBS_SUBSET_HOPS"))    g_cbs_subset_hops   = std::atoi(e);
    if (const char* e = std::getenv("RCPSP_CBS_SUBSET_SIZE"))    g_cbs_subset_size   = std::atoi(e);
    if (const char* e = std::getenv("RCPSP_CBS_SUBSET_MAXDEPTH")) g_cbs_subset_maxdepth = std::atoi(e);
    if (const char* e = std::getenv("RCPSP_CBS_SUBSET_GAP"))     g_cbs_subset_gap    = std::atoi(e);
    if (const char* e = std::getenv("RCPSP_CBS_MINCUT"))          g_cbs_mincut          = std::atoi(e) != 0;
    if (const char* e = std::getenv("RCPSP_CBS_MINCUT_MAXDEPTH")) g_cbs_mincut_maxdepth = std::atoi(e);
    if (const char* e = std::getenv("RCPSP_CBS_MINCUT_GAP"))      g_cbs_mincut_gap      = std::atoi(e);
    if (const char* e = std::getenv("RCPSP_CBS_RSADAPT"))         g_cbs_rsadapt         = std::atoi(e) != 0;
    if (const char* e = std::getenv("RCPSP_CBS_RS_THRESH"))       g_cbs_rs_threshold    = std::atof(e);
    if (const char* e = std::getenv("RCPSP_CBS_SINGLERES"))          g_cbs_singleres          = std::atoi(e) != 0;
    if (const char* e = std::getenv("RCPSP_CBS_SINGLERES_MAXDEPTH")) g_cbs_singleres_maxdepth = std::atoi(e);
    if (const char* e = std::getenv("RCPSP_CBS_SINGLERES_GAP"))      g_cbs_singleres_gap      = std::atoi(e);
    if (const char* e = std::getenv("RCPSP_SUBSET_DOM"))             g_subset_dom             = std::atoi(e) != 0;
    if (const char* e = std::getenv("RCPSP_SUBSOLVER")) {            // ext|cbs|tt2 (or 0|1|2)
        std::string v(e);
        g_subsolver = (v=="cbs"||v=="1") ? 1 : (v=="tt2"||v=="2") ? 2 : 0;
    }
    if (const char* e = std::getenv("RCPSP_SUBSOLVE_HCBS_OFF"))      g_subsolve_hcbs_off = std::atoi(e) != 0;
    if (const char* e = std::getenv("RCPSP_SHADOW"))                 g_shadow_cache      = std::atoi(e) != 0;  // measure-only cache-hit estimator
    if (const char* e = std::getenv("RCPSP_SUBSOLVE_SUCC_CACHE"))    g_subsolve_succ_cache = std::atoi(e) != 0; // TT2 sub-solve successor cache (parked)
    if (const char* e = std::getenv("RCPSP_SUCC_CACHE_CAP"))         g_succ_cache_cap    = std::atol(e);       // 0 = unlimited
    if (const char* e = std::getenv("RCPSP_SUBSOLVE_H_CACHE"))       g_h_cache           = std::atoi(e) != 0;  // TT2 sub-solve h-value cache (the light lever)
    if (const char* e = std::getenv("RCPSP_SUBSOLVE_H_CACHE_SOLVED")) g_h_cache_solved   = std::atoi(e) != 0;  // cache only solved (exact h*) path nodes
    if (const char* e = std::getenv("RCPSP_CBS_SINGLERES_VIACBS"))   { if (std::atoi(e)!=0) g_subsolver = 1; }  // legacy alias
    if (const char* e = std::getenv("RCPSP_CBS_SINGLERES_EXPAND"))   g_cbs_singleres_expand   = std::atol(e);
    if (const char* e = std::getenv("RCPSP_CBS_SINGLERES_MAXSIZE"))  g_cbs_singleres_maxsize  = std::atoi(e);
    if (const char* e = std::getenv("RCPSP_CBS_HIERRS"))             g_cbs_hierrs             = std::atoi(e) != 0;
    if (const char* e = std::getenv("RCPSP_CBS_HIERRS_MAXDEPTH"))    g_cbs_hierrs_maxdepth    = std::atoi(e);
    if (const char* e = std::getenv("RCPSP_CBS_HIERRS_GAP"))         g_cbs_hierrs_gap         = std::atoi(e);
    if (const char* e = std::getenv("RCPSP_CBS_HIERRS_EXPAND"))      g_cbs_hierrs_expand      = std::atol(e);
    if (g_tt2_sym)    std::cout << "TT2 symmetry breaking: ON (canonical increasing-id, non-batch)\n";
    if (g_tt2_dr5)    std::cout << "TT2 dominance: DR5 cutset ON\n";
    if (g_tt2_theta)  std::cout << "TT2 bound: Theta-tree ECT resource bound ON (max-combined)\n";
    if (g_tt2_batch)  std::cout << "TT2 expansion: BATCH (feasible subsets, cap=" << g_tt2_batch_cap << ")\n";
    if (g_tt2_dr4)    std::cout << "TT2 DR4 delayed-start dominance: ON\n";
    if (g_tt2_immsel) std::cout << "TT2 immediate selection (branching reduction): ON\n";
    if (g_tt2_freefire) std::cout << "TT2 free-activity force-fire (sub-solve only): ON\n";
    if (g_use_lazy)   std::cout << "Search: LazyAStarCBS (deferred-heuristic A*)\n";
    if (g_cbs_theta)  std::cout << "CBS bound: Theta-tree ECT resource floor ON (max with HCBS)\n";
    if (g_cbs_subset) std::cout << "CBS bound: conflict-subset look-ahead LB ON (expand=" << g_cbs_subset_expand
                                << " hops=" << g_cbs_subset_hops << " size=" << g_cbs_subset_size
                                << " | gate: depth<=" << g_cbs_subset_maxdepth << " OR gap<=" << g_cbs_subset_gap << ")\n";
    if (g_cbs_mincut) std::cout << "CBS bound: min-cut resource LB ON (max-flow energetic; gate: depth<="
                                << g_cbs_mincut_maxdepth << " OR gap<=" << g_cbs_mincut_gap << ")\n";
    if (g_cbs_rsadapt) std::cout << "CBS RS-adaptive gating ON: expensive bounds fire only if RS<=" << g_cbs_rs_threshold << "\n";
    if (g_cbs_singleres) std::cout << "CBS bound: single-resource max LB ON (per-resource capped sub-solve; gate: depth<="
                                << g_cbs_singleres_maxdepth << " expand=" << g_cbs_singleres_expand
                                << " maxsize=" << g_cbs_singleres_maxsize << ")\n";
    if (g_cbs_hierrs) std::cout << "CBS bound: hierarchical RS-relaxation LB ON (next-RS inflated caps; expand="
                                << g_cbs_hierrs_expand << " | gate: depth<=" << g_cbs_hierrs_maxdepth
                                << " OR gap<=" << g_cbs_hierrs_gap << ")\n";
    if (g_dom_skyline) std::cout << "Dominance: SKYLINE bucket compaction ON\n";
    if (g_use_ub || g_use_leftshift || g_use_bidir || g_use_hybrid)
        std::cout << "Experiments: UB=" << g_use_ub << " LEFTSHIFT=" << g_use_leftshift
                  << " BIDIR=" << g_use_bidir << " HYBRID=" << g_use_hybrid << std::endl;
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
                 << "useHeuristic,useMDASets,useMDACache,useStrongConstraints,useMDABAB,cardinalityRatio,useDR5,domRule,useUB,useHybrid,hybridT,useLeftshift,useBidir,ubPruned,leftshiftPruned,domPruned,domChecks,domStored,useLazy,useSkyline,lazyEvals,lazyReinserts,useNonMinimalDelay,useAncestorBranching,useDominanceSib,usePairDecomp,useHGreed,useLean,useInline,domCap,timeoutS,useWarmStart,warmStartK,warmStartBudgetS,warmStartDir,useSetDelay,warmStartRS,warmstartEngaged,warmstartInflMk,rootF,provenLB,warmstartSec,useDR4,useThetaBound,thetaBoundBetter,useSubsetLB,subsetBetter,subsetSolves,subsetExpandsTotal,subsetCapped,subsetMaxExpands,subsetCacheHits,useMdaRecursive,imp2Fires,imp2MaxDepth,useNmdPrecedence,orderSwapCand,useCbsMinCut,minCutBetter,minCutCalls,useRSAdapt,rsThreshold,instanceRS,useSingleRes,singleResBetter,singleResCalls,heuristicLowRS,heuristicHighRS,rootH,heurTimeSec,heurCalls,singleResTimeSec,minCutTimeSec,useHierRS,hierRsBetter,hierRsCalls,hierRsTimeSec,hierRsCacheHits,subsolver,shadowProbes,shadowHits\n"; }
            solveRCPSP_CBS(std::atoi(argv[3]), std::atoi(argv[4]), f, argv[2]);
            std::cout << "prune pairs -> " << g_dom_dump_path << std::endl;
        } else if (arg1 == "verifydom") {
            // verifydom <type> <group> <exam> <cfg> <pairsfile>
            if (argc < 7) { std::cerr << "Usage: verifydom <type> <group> <exam> <cfg> <pairsfile>\n"; return 1; }
            runVerifyDom(argv[2], std::atoi(argv[3]), std::atoi(argv[4]), std::atoi(argv[5]), argv[6]);
        } else if (arg1 == "startrace") {
            // startrace <type> <group> <exam>  (needs RCPSP_STAR=... and RCPSP_NMD=1)
            if (argc < 5) { std::cerr << "Usage: startrace <type> <group> <exam> (set RCPSP_STAR)\n"; return 1; }
            applyConfigNum(8);
            traceStarLoss(argv[2], std::atoi(argv[3]), std::atoi(argv[4]));
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
        } else if (arg1 == "tt2one") {
            // tt2one <type> <group> <exam> — single TT2 instance (validation/measurement)
            if (argc < 5) { std::cerr << "Usage: tt2one <type> <group> <exam>\n"; return 1; }
            std::string ptype = argv[2];
            std::string f = getNextFilename("new_results", "output_tt2one_" + ptype + "_", ".csv");
            { std::ofstream h(f); h << "group,exam,time,solved,makespan,expandNumber,generatedNumber,depth,model,problemType,maxMem,LB,useTT2DR5,useTT2Batch,tt2BatchCap,domPruned,domThinned,domChecks,domInserts,domMaxBucket,useTT2Sym,symPruned,useTT2Gendesc,useTT2SymTB,timeoutS,trivialAtRoot,useThetaBound,thetaBoundBetter,useTT2DR4,dr4Pruned,useTT2ImmSel,immSelFired,useTT2RSAdapt,tt2RsThreshold,instanceRS,useTT2SingleRes,tt2SingleResBetter,tt2SingleResCalls,heuristicLowRS,heuristicHighRS,rootH,heurTimeSec,heurCalls,singleResTimeSec,subsolver,shadowProbes,shadowHits,useTT2UB,ubPruned,useTT2HierRS,tt2HierRsBetter,tt2HierRsCalls,tt2HierRsTimeSec,tt2HierRsCacheHits\n"; }
            solveRCPSP_TT2(std::atoi(argv[3]), std::atoi(argv[4]), f, ptype);
        } else if (arg1 == "tt2oldbwd") {
            // tt2oldbwd <type> <group> <exam> — the pre-existing backward (mirror-frame), for baseline timing
            if (argc < 5) { std::cerr << "Usage: tt2oldbwd <type> <group> <exam>\n"; return 1; }
            std::string ptype = argv[2];
            std::string f = getNextFilename("new_results", "output_tt2oldbwd_" + ptype + "_", ".csv");
            { std::ofstream h(f); h << "group,exam\n"; }
            auto t0 = std::chrono::high_resolution_clock::now();
            int mk = solveRCPSP_TT2_Backward(std::atoi(argv[3]), std::atoi(argv[4]), f, ptype);
            auto t1 = std::chrono::high_resolution_clock::now();
            std::cout << "OLDBWD result makespan=" << mk << " time="
                      << std::chrono::duration<double>(t1-t0).count() << "s" << std::endl;
        } else if (arg1 == "tt2mm") {
            // tt2mm <type> <group> <exam> — Meet-in-the-Middle bidirectional (pr=max(f,2g))
            if (argc < 5) { std::cerr << "Usage: tt2mm <type> <group> <exam>\n"; return 1; }
            solveRCPSP_TT2_MM(std::atoi(argv[3]), std::atoi(argv[4]), argv[2]);
        } else if (arg1 == "tt2bae") {
            // tt2bae <type> <group> <exam> — bidirectional meet-in-the-middle (BAE*-style)
            if (argc < 5) { std::cerr << "Usage: tt2bae <type> <group> <exam>\n"; return 1; }
            solveRCPSP_TT2_BAE(std::atoi(argv[3]), std::atoi(argv[4]), argv[2]);
        } else if (arg1 == "tt2indeg") {
            // tt2indeg <type> <group> <exam> — reverse branching (in-degree) in the forward graph
            if (argc < 5) { std::cerr << "Usage: tt2indeg <type> <group> <exam>\n"; return 1; }
            solveRCPSP_TT2_InDegree(std::atoi(argv[3]), std::atoi(argv[4]), argv[2]);
        } else if (arg1 == "tt2meetuf") {
            // tt2meetuf <type> <group> <exam> — interior-meet test for the UN-FIRE backward (#1)
            if (argc < 5) { std::cerr << "Usage: tt2meetuf <type> <group> <exam>\n"; return 1; }
            solveRCPSP_TT2_MeetUnfire(std::atoi(argv[3]), std::atoi(argv[4]), argv[2]);
        } else if (arg1 == "tt2meet") {
            // tt2meet <type> <group> <exam> — validate that forward & reversed backward meet
            if (argc < 5) { std::cerr << "Usage: tt2meet <type> <group> <exam>\n"; return 1; }
            solveRCPSP_TT2_Meet(std::atoi(argv[3]), std::atoi(argv[4]), argv[2]);
        } else if (arg1 == "tt2rev") {
            // tt2rev <type> <group> <exam> — reversed-instance backward via the fast forward solver
            if (argc < 5) { std::cerr << "Usage: tt2rev <type> <group> <exam>\n"; return 1; }
            std::string ptype = argv[2];
            std::string f = getNextFilename("new_results", "output_tt2rev_" + ptype + "_", ".csv");
            { std::ofstream h(f); h << "group,exam,time,solved,makespan,expandNumber,generatedNumber,depth,model,problemType,maxMem,LB,useTT2DR5,useTT2Batch,tt2BatchCap,domPruned,domThinned,domChecks,domInserts,domMaxBucket,useTT2Sym,symPruned,useTT2Gendesc,useTT2SymTB,timeoutS,trivialAtRoot,useThetaBound,thetaBoundBetter,useTT2DR4,dr4Pruned,useTT2ImmSel,immSelFired,useTT2RSAdapt,tt2RsThreshold,instanceRS,useTT2SingleRes,tt2SingleResBetter,tt2SingleResCalls,heuristicLowRS,heuristicHighRS,rootH,heurTimeSec,heurCalls,singleResTimeSec,subsolver,shadowProbes,shadowHits,useTT2UB,ubPruned,useTT2HierRS,tt2HierRsBetter,tt2HierRsCalls,tt2HierRsTimeSec,tt2HierRsCacheHits\n"; }
            g_tt2_reverse = true;
            solveRCPSP_TT2(std::atoi(argv[3]), std::atoi(argv[4]), f, ptype);
            g_tt2_reverse = false;
        } else if (arg1 == "tt2bwd") {
            // tt2bwd <type> <group> <exam> — single backward-meet TT2 instance
            if (argc < 5) { std::cerr << "Usage: tt2bwd <type> <group> <exam>\n"; return 1; }
            std::string ptype = argv[2];
            std::string f = getNextFilename("new_results", "output_tt2bwd_" + ptype + "_", ".csv");
            { std::ofstream h(f); h << "group,exam,time,solved,makespan,expandNumber,touched,pathLen,model,problemType\n"; }
            solveRCPSP_TT2_BackwardMeet(std::atoi(argv[3]), std::atoi(argv[4]), f, ptype);
        } else if (arg1 == "cbsinitf") {
            // cbsinitf <type> — root-state f (earliest-start makespan + HCBS h) for
            // every instance, NO search. For heuristic-quality evaluation vs LB/optimum.
            if (argc < 3) { std::cerr << "Usage: cbsinitf <type>\n"; return 1; }
            runCbsInitF(argv[2]);
        } else if (arg1 == "tt2initf") {
            // tt2initf <type> — TT2 root-state f (= HCost_TT2 at the root, g=0) for
            // every instance, NO search.
            if (argc < 3) { std::cerr << "Usage: tt2initf <type>\n"; return 1; }
            runTt2InitF(argv[2]);
        } else if (arg1.rfind("tt2bae_", 0) == 0) {
            // BAE* bidirectional, full benchmark for one size: "tt2bae_j30" etc.
            std::string problemType = arg1.substr(7);
            runBenchmarkTT2BAE(problemType);
        } else if (arg1.rfind("tt2rev_", 0) == 0) {
            // Reversed-instance backward, full benchmark for one size: "tt2rev_j30" etc.
            // Same fast solver on the reversed net (same optima); writes output_tt2rev_<size>_*.csv.
            std::string problemType = arg1.substr(7);
            g_tt2_reverse = true;
            runBenchmarkTT2(problemType);
            g_tt2_reverse = false;
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


// ── Root-state f for heuristic evaluation (no search) ─────────────────────────
// For every instance, build the CBS root and record: rootMakespan (earliest-start
// schedule = the f you get with h=0/baseline), rootH (HCBS heuristic at the root),
// rootF = rootMakespan + rootH (the f the CBS search starts from with the heuristic
// on), and the known LB/UB/optimum. No expansion happens — this is pure heuristic
// measurement. Compare rootF to lb/ub to judge how tight the initial bound is.
template<int N>
void runCbsInitF_impl(const std::string& ptype, std::ofstream& file) {
    setting.heuristic = HeuristicType::HCBS;   // evaluate the CBS heuristic at the root
    for (int group = 1; group <= 48; group++) {
        for (int exam = 1; exam <= 10; exam++) {
            getRCPSP(RCPSPex, group, exam, ptype);
            resource_info.clear(); downstream.clear(); upstream.clear();
            precomputeDownstream(); precomputeUpstream(); precomputeResourceInfo();

            { static const double RSL[4]={0.2,0.5,0.7,1.0}; g_instance_rs = RSL[((group-1)%4+4)%4]; }
            g_instance_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(astar_timeout_seconds); g_instance_deadline_set = true;
            if (g_cbs_hierrs) precomputeRSInflation();
            g_heur_time_sec = g_singleres_time_sec = g_mincut_time_sec = g_hierrs_time_sec = 0.0; g_heur_calls = 0; g_root_h = -1.0;
            g_cbs_hierrs_better = g_cbs_hierrs_calls = g_cbs_hierrs_cache_hits = 0; g_cbs_hierrs_cache.clear();
            g_incumbent = std::numeric_limits<short>::max();   // no incumbent at root => nearUB arm off; shallow arm fires
            RCPSPState_CBS<N> first;                       // default ctor = earliest-start root
            short rootMk = first.start_times[g_sink_id];   // rootG (CBS g != 0)
            RCPSP_CBS<N> env;                              // real env => HCost applies the bound floors
            short h      = (short)env.HCost(first, first); // h WITH whatever bounds are flagged on
            int   rootF  = (int)rootMk + (int)h;

            int lb, ub; bool optKnown;
            if (ptype == "j30") {
                int opt = getOptimalMakespan(group, exam, ptype);
                lb = ub = opt; optKnown = true;
            } else {
                Bounds b = getBounds(group, exam, ptype);
                lb = b.lb; ub = b.ub; optKnown = b.optimal_known;
            }
            file << group << "," << exam << "," << g_instance_rs << "," << (int)rootMk << "," << (int)h << ","
                 << rootF << "," << g_heur_time_sec << "," << g_singleres_time_sec << "," << g_mincut_time_sec << ","
                 << lb << "," << ub << "," << (optKnown ? "True" : "False") << "," << subsolver_name() << "," << g_shadow_probes << "," << g_shadow_hits << "\n";
        }
    }
}

void runCbsInitF(const std::string& ptype) {
    setProblemSize(ptype);
    std::string f = getNextFilename("new_results", "initF_cbs_" + ptype + "_", ".csv");
    { std::ofstream h(f); h << "group,exam,RS,rootMakespan,rootH,rootF,heurTimeSec,singleResTimeSec,minCutTimeSec,lb,ub,optKnown,subsolver,shadowProbes,shadowHits\n"; }
    std::ofstream file(f, std::ios::app);
    if      (ptype == "j30")  runCbsInitF_impl<32>(ptype, file);
    else if (ptype == "j60")  runCbsInitF_impl<62>(ptype, file);
    else if (ptype == "j90")  runCbsInitF_impl<92>(ptype, file);
    else if (ptype == "j120") runCbsInitF_impl<122>(ptype, file);
    else { std::cerr << "cbsinitf: unknown type " << ptype << "\n"; return; }
    std::cout << "CBS initF -> " << f << std::endl;
}

// TT2 counterpart of runCbsInitF: root-state f only, no search. Root g = 0, so the
// first node's f = HCost_TT2(first, last) (the same heuristic the TT2 search starts
// from — critical-path via getForwardHcost_TT).
void runTt2InitF(const std::string& ptype) {
    setProblemSize(ptype);
    std::string f = getNextFilename("new_results", "initF_tt2main_" + ptype + "_", ".csv");
    { std::ofstream h(f); h << "group,exam,RS,rootF,heurTimeSec,singleResTimeSec,subsolver,shadowProbes,shadowHits\n"; }
    std::ofstream file(f, std::ios::app);
    for (int group = 1; group <= 48; group++) {
        for (int exam = 1; exam <= 10; exam++) {
            getPetri(petri, group, exam, ptype);
            getRCPSP(RCPSPex, group, exam, ptype);
            { static const double RSL[4]={0.2,0.5,0.7,1.0}; g_instance_rs = RSL[((group-1)%4+4)%4]; }
            g_instance_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(astar_timeout_seconds); g_instance_deadline_set = true;
            if (g_tt2_singleres || g_tt2_hierrs) { resource_info.clear(); upstream.clear(); downstream.clear(); precomputeResourceInfo(); precomputeUpstream(); precomputeDownstream(); }
            if (g_tt2_hierrs) precomputeRSInflation();
            g_heur_time_sec = g_singleres_time_sec = g_mincut_time_sec = g_hierrs_time_sec = 0.0; g_heur_calls = 0; g_root_h = -1.0; g_shadow_set.clear(); g_shadow_probes = g_shadow_hits = 0; g_succ_probes = g_succ_hits = g_hcache_probes = g_hcache_hits = 0; if (g_subsolve_succ_cache || g_h_cache) get_tt2_cache().clear();
            g_tt2_hierrs_better = g_tt2_hierrs_calls = g_cbs_hierrs_cache_hits = 0; g_cbs_hierrs_cache.clear();
            RCPSPState_TT2 first;
            RCPSPState_TT2 last = first;
            RCPSP_TT2 env;                              // real env => HCost METHOD applies the bound floors (single-res/theta)
            int rootF = (int)env.HCost(first, last);    // g(root)=0 -> f = h
            file << group << "," << exam << "," << g_instance_rs << "," << rootF << ","
                 << g_heur_time_sec << "," << g_singleres_time_sec << "," << subsolver_name() << "," << g_shadow_probes << "," << g_shadow_hits << "\n";
        }
    }
    std::cout << "TT2 initF -> " << f << std::endl;
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
    std::string filename = outName(folder, baseName, ".csv");
    std::ofstream file(filename);
    if (!file.is_open()) { std::cerr << "Cannot open " << filename << "\n"; return; }
    file << "group,exam,time,makespan,correct,setType,model,optimalOrLB,UB,NC,RF,RS,"
         << "finished,expandNumber,generatedNumber,depth,maxMem,"
         << "useFirst,useConflictPrioritization,useHeuristic,useMDASets,useMDACache,useStrongConstraints,useMDABAB,cardinalityRatio,useDR5,domRule,useUB,useHybrid,hybridT,useLeftshift,useBidir,ubPruned,leftshiftPruned,domPruned,domChecks,domStored,useLazy,useSkyline,lazyEvals,lazyReinserts,useNonMinimalDelay,useAncestorBranching,useDominanceSib,usePairDecomp,useHGreed,useLean,useInline,domCap,timeoutS,useWarmStart,warmStartK,warmStartBudgetS,warmStartDir,useSetDelay,warmStartRS,warmstartEngaged,warmstartInflMk,rootF,provenLB,warmstartSec,useDR4,useThetaBound,thetaBoundBetter,useSubsetLB,subsetBetter,subsetSolves,subsetExpandsTotal,subsetCapped,subsetMaxExpands,subsetCacheHits,useMdaRecursive,imp2Fires,imp2MaxDepth,useNmdPrecedence,orderSwapCand,useCbsMinCut,minCutBetter,minCutCalls,useRSAdapt,rsThreshold,instanceRS,useSingleRes,singleResBetter,singleResCalls,heuristicLowRS,heuristicHighRS,rootH,heurTimeSec,heurCalls,singleResTimeSec,minCutTimeSec,useHierRS,hierRsBetter,hierRsCalls,hierRsTimeSec,hierRsCacheHits,subsolver,shadowProbes,shadowHits"
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
    std::string filename = outName(folder, baseName, ".csv");

    // Write header to the new file so it is self-contained
    { std::ofstream hdr(filename);
      if (!hdr.is_open()) { std::cerr << "Cannot open " << filename << "\n"; return; }
      hdr << "group,exam,time,makespan,correct,setType,model,optimalOrLB,UB,NC,RF,RS,"
          << "finished,expandNumber,generatedNumber,depth,maxMem,"
          << "useFirst,useConflictPrioritization,useHeuristic,useMDASets,useMDACache,"
          << "useStrongConstraints,useMDABAB,cardinalityRatio,useDR5,domRule,useUB,useHybrid,hybridT,useLeftshift,useBidir,ubPruned,leftshiftPruned,domPruned,domChecks,domStored,useLazy,useSkyline,lazyEvals,lazyReinserts,useNonMinimalDelay,useAncestorBranching,useDominanceSib,usePairDecomp,useHGreed,useLean,useInline,domCap,timeoutS,useWarmStart,warmStartK,warmStartBudgetS,warmStartDir,useSetDelay,warmStartRS,warmstartEngaged,warmstartInflMk,rootF,provenLB,warmstartSec,useDR4,useThetaBound,thetaBoundBetter,useSubsetLB,subsetBetter,subsetSolves,subsetExpandsTotal,subsetCapped,subsetMaxExpands,subsetCacheHits,useMdaRecursive,imp2Fires,imp2MaxDepth,useNmdPrecedence,orderSwapCand,useCbsMinCut,minCutBetter,minCutCalls,useRSAdapt,rsThreshold,instanceRS,useSingleRes,singleResBetter,singleResCalls,heuristicLowRS,heuristicHighRS,rootH,heurTimeSec,heurCalls,singleResTimeSec,minCutTimeSec,useHierRS,hierRsBetter,hierRsCalls,hierRsTimeSec,hierRsCacheHits,subsolver,shadowProbes,shadowHits\n"; }

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
    std::string filename = outName(folder, baseName, extension);
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error opening file!" << std::endl;
        return;
    }
    file << "group,exam,time,makespan,correct,setType,model,optimalOrLB,UB,NC,RF,RS,"
         << "finished,expandNumber,generatedNumber,depth,maxMem,"
         << "useFirst,useConflictPrioritization,useHeuristic,useMDASets,useMDACache,useStrongConstraints,useMDABAB,cardinalityRatio,useDR5,domRule,useUB,useHybrid,hybridT,useLeftshift,useBidir,ubPruned,leftshiftPruned,domPruned,domChecks,domStored,useLazy,useSkyline,lazyEvals,lazyReinserts,useNonMinimalDelay,useAncestorBranching,useDominanceSib,usePairDecomp,useHGreed,useLean,useInline,domCap,timeoutS,useWarmStart,warmStartK,warmStartBudgetS,warmStartDir,useSetDelay,warmStartRS,warmstartEngaged,warmstartInflMk,rootF,provenLB,warmstartSec,useDR4,useThetaBound,thetaBoundBetter,useSubsetLB,subsetBetter,subsetSolves,subsetExpandsTotal,subsetCapped,subsetMaxExpands,subsetCacheHits,useMdaRecursive,imp2Fires,imp2MaxDepth,useNmdPrecedence,orderSwapCand,useCbsMinCut,minCutBetter,minCutCalls,useRSAdapt,rsThreshold,instanceRS,useSingleRes,singleResBetter,singleResCalls,heuristicLowRS,heuristicHighRS,rootH,heurTimeSec,heurCalls,singleResTimeSec,minCutTimeSec,useHierRS,hierRsBetter,hierRsCalls,hierRsTimeSec,hierRsCacheHits,subsolver,shadowProbes,shadowHits"
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
    std::string baseName = std::string("output_tt2") + (g_tt2_reverse ? "rev" : "") + "_" + problemType + "_";
    std::string filename = outName(folder, baseName, ".csv");
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error opening file: " << filename << std::endl;
        return;
    }
    // Header matches what solveRCPSP_TT2 writes per row
    file << "group,exam,time,solved,makespan,expandNumber,generatedNumber,depth,model,problemType,maxMem,LB,useTT2DR5,useTT2Batch,tt2BatchCap,domPruned,domThinned,domChecks,domInserts,domMaxBucket,useTT2Sym,symPruned,useTT2Gendesc,useTT2SymTB,timeoutS,trivialAtRoot,useThetaBound,thetaBoundBetter,useTT2DR4,dr4Pruned,useTT2ImmSel,immSelFired,useTT2RSAdapt,tt2RsThreshold,instanceRS,useTT2SingleRes,tt2SingleResBetter,tt2SingleResCalls,heuristicLowRS,heuristicHighRS,rootH,heurTimeSec,heurCalls,singleResTimeSec,subsolver,shadowProbes,shadowHits,useTT2UB,ubPruned,useTT2HierRS,tt2HierRsBetter,tt2HierRsCalls,tt2HierRsTimeSec,tt2HierRsCacheHits"
         << std::endl;
    file.close();

    std::cout << "\n=== TT2 | " << problemType << " ===" << std::endl;
    runConfigTT2(filename, problemType);
    std::cout << "\nTT2 benchmark done (" << problemType << ") -> " << filename << std::endl;
}

// ── BAE* full benchmark for one size (modes "tt2bae_j30" / "_j60" / "_j90") ───
// One row per instance, 48 groups x 10 exams. `solved` is True ONLY when optimality was
// PROVEN — the loop exited on min(f_F,f_B) >= U, on a side reaching its own goal, or on
// both frontiers being exhausted. A deadline exit holding a meet-derived incumbent is a
// valid UPPER BOUND but not a proof, so it is reported as solved=False with the bound in
// `ubFound` and makespan=-1. Flags are written into the row (self-documenting run).
void runBenchmarkTT2BAE(const std::string& problemType) {
    std::string filename = outName("new_results", "output_tt2bae_" + problemType + "_", ".csv");
    { std::ofstream f(filename);
      if (!f.is_open()) { std::cerr << "Error opening file: " << filename << std::endl; return; }
      f << "group,exam,time,solved,makespan,expandNumber,expandedF,expandedB,model,problemType,"
           "timeoutS,instanceRS,useTT2DR5,biDR4,meetNorm,meets,firstMeetAtExp,uFromMeet,ubFound"
        << std::endl; }
    std::cout << std::endl << "=== TT2 BAE* | " << problemType << " ===" << std::endl;
    for (int i = 1; i <= 48; i++) {
        for (int j = 1; j <= 10; j++) {
            int mk = solveRCPSP_TT2_BAE(i, j, problemType);
            std::ofstream f(filename, std::ios::app);
            f << i << "," << j << "," << g_bae_time << ","
              << (g_bae_proven ? "True" : "False") << ","
              << (g_bae_proven ? mk : -1) << ","
              << (g_bae_expF + g_bae_expB) << "," << g_bae_expF << "," << g_bae_expB << ","
              << "TT2BAE," << problemType << "," << astar_timeout_seconds << ","
              << g_instance_rs << ",1," << g_bae_biDR4 << "," << (g_meet_norm ? 1 : 0) << ","
              << g_bae_meets << "," << g_bae_firstMeet << "," << g_bae_uMeet << "," << mk
              << std::endl;
        }
    }
    std::cout << std::endl << "BAE* benchmark done (" << problemType << ") -> " << filename << std::endl;
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

    // 3. Sort (Priority: PetriType -> SetslSize -> Group -> Exam)
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
