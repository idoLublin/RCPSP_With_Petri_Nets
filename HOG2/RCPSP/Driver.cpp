//
// Created by idol on 29/12/2024.
//
// Your First C++ Program
#include <queue>
#include "Globals.h"
#include <iostream>
#include "RCPSPState.h"
#include "RCPSPState.cpp"
#include "../../HOG2/generic/TemplateAStar.h"
#include "../../HOG2/generic/BAE.h"
#include "../../HOG2/generic/EPEAStar.h"
#include "BAE_RCPSP.h"   // local copy of HOG2 BAE* with a separate meet hash
#include "AStarCompare.h"

#include <filesystem>
#include <ctime>            // timestamped, self-describing run filenames
namespace fs = std::filesystem;
#include "RCPSP.h"
#include "HDAStarTT2.h"   // RCPSP_HDA: hash-distributed A* for TT2 (mode tt2hda_<size>)
#include "RootBounds.h"     // RCPSP_ROOT_DLB / RCPSP_ROOT_SEED: root deadline lower bound + heuristic incumbent
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
#include <iomanip>
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
void runBenchmarkTT2HBAE(const std::string& problemType);
int solveRCPSP_TT2_Trace(int group, int exam, const std::string& ptype = "j30");
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

// Derive the source/sink place names for the CURRENTLY LOADED net.
// initialstatename / finalstatename are GLOBALS (Globals.h). RCPSPState_TT2's constructor
// WRITES them as it scans places and READS them in the same pass, so any place seen before
// the real source is matched against whatever the PREVIOUS construction left behind. On the
// reversed net that stale value is the forward source's name -- and reverseLoadedInstance
// deliberately tokens the new source BY NAME MATCH -- so a stale global seeds a spurious
// token at the new sink. Call this after loading/reversing and before building a root.
static void deriveEndpointNames(std::string& ini, std::string& fin) {
    for (const auto& pl : petri.places) {
        if (pl.arcs_in.empty())  ini = pl.name;
        if (pl.arcs_out.empty()) fin = pl.name;
    }
}
// RCPSP_TT2_PROF columns, appended after `direction` on every TT2 row. Timers are 0 unless
// RCPSP_TT2_PROF=1; counters and the f-bound checkpoints are always filled. On trivial-root
// rows A* never ran, so the search columns are 0 / -1.
//   profAstarSec   TemplateAStar::GetPath wall time      profSuccSec  whole GetSuccessors
//   profAvailSec   available-transition scan             profGenSec   child construction (DR4, batch)
//   profDR5Sec     DR5 check_and_insert                  profHashSec  GetStateHash
//   profHeurCPSec / profHeurResSec  the two heuristic parts (heurTimeSec is their total + extras)
//   open-list/bookkeeping time = profAstarSec - profSuccSec - heurTimeSec - profHashSec
//   maxFExpanded   largest f expanded = proven lower bound; fAt*s the same at 1/10/60/120/240 s
static void writeTT2ProfCols(std::ostream& o, uint64_t openAtEnd, uint64_t itemsAtEnd, uint64_t uniqueExp, bool timedOut) {
    auto& dt = get_tt2_dominance_table();
    o << "," << (g_tt2_prof ? 1 : 0) << "," << g_prof_astar_sec << "," << g_prof_succ_sec << "," << g_prof_succ_calls
      << "," << g_prof_avail_sec << "," << g_prof_gen_sec << "," << g_prof_dr5_sec << "," << g_prof_hash_sec << "," << g_prof_hash_calls
      << "," << g_prof_hcp_sec << "," << g_prof_hres_sec << "," << g_prof_h_calls_all << "," << g_prof_h_known << "," << g_prof_h_deltazero
      << "," << g_prof_children_raw << "," << g_prof_children_kept << "," << dt.bucketCount() << "," << dt.comparisons
      << "," << openAtEnd << "," << itemsAtEnd << "," << uniqueExp << "," << g_prof_maxf;
    for (int v : g_prof_f_at) o << "," << v;
    o << "," << (timedOut ? 1 : 0) << "," << (cpFastOn() ? 1 : 0) << "," << (resFastOn() ? 1 : 0) << "," << (TT2DominanceTable::FastOn() ? 1 : 0) << "," << (TT2DominanceTable::SortedOn() ? 1 : 0) << "," << (genFastOn() ? 1 : 0) << "," << (TT2DominanceTable::MtfOn() ? 1 : 0)
      << "," << (g_tt2_wait ? 1 : 0) << "," << g_tt2_wait_edges << "," << g_tt2_wait_cut << "," << dt.waitExempt << "," << g_tt2_wait_pass << "," << (g_tt2_waitjump ? 1 : 0) << "," << g_tt2_wait_jumped;
    o << "," << (rootb::DlbOn() ? 1 : 0) << "," << (rootb::SeedOn() ? 1 : 0) << "," << rootb::info().lbPlain << "," << rootb::info().lb
      << "," << rootb::info().lbSec << "," << (rootb::info().budgetHit ? 1 : 0) << "," << rootb::info().seed << "," << rootb::info().seedSec
      << "," << (rootb::info().closed ? 1 : 0)   // RootBounds.h
      << "," << (rootb::ProbeOn() ? 1 : 0) << "," << rootb::info().probeSeedIn << "," << rootb::info().probeBest << "," << rootb::info().probeNodes
      << "," << rootb::info().probeExhausted << "," << rootb::info().probeSec << "," << (tt2nb::On() ? 1 : 0) << "," << tt2nb::st().calls << "," << tt2nb::st().raised << "," << tt2nb::st().raiseSum << "," << tt2nb::st().tt0Fail << "," << tt2nb::st().propFail << "," << tt2nb::st().sec << "," << tt2nb::st().winPruned << "," << (tt2nb::WinOn() ? 1 : 0) << "," << (tt2nb::WinLsOn() ? 1 : 0) << "," << (tt2nb::SerOn() ? 1 : 0) << "," << tt2nb::st().serFail << "," << (g_tt2_ub_ge ? 1 : 0) << "," << (g_tt2_ub_closed ? 1 : 0) << "," << rootb::LnsMode() << "," << rootb::info().lnsSeedIn << "," << rootb::info().lnsBest << "," << rootb::info().lnsIters << "," << rootb::info().lnsImpCp << "," << rootb::info().lnsImpSgs << "," << rootb::info().lnsSec << "," << (tt2TbSpec().empty() ? "-" : tt2TbSpec());
    o << "," << g_tt2_compact_on << "," << g_tt2_compact_rebuilds << "," << g_tt2_compact_checked;   // tt2Compact,compactRebuilds,compactChecked
    o << "," << g_tt2_precheck << "," << g_tt2_precheck_cut;                                          // tt2Precheck,precheckCut
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

    deriveEndpointNames(initialstatename, finalstatename);   // before ANY root construction
    RCPSPState_TT2 first;
    RCPSPState_TT2 last = first;


    last.g = HCost_TT2(last, first);

    // RCPSP_TT2_COMPACT: tables for rebuilding the token lists on this net; 0 for this instance if it is not supported
    g_tt2_compact_on = (g_tt2_compact && tt2CompactPrepare(first)) ? g_tt2_compact : 0;
    g_tt2_compact_rebuilds = g_tt2_compact_checked = 0;
    get_tt2_dominance_table().clear();   // DR5 table is per-instance (RCPSP_TT2_DR5=1)
    g_tt2_sym_pruned = 0;                // symmetry-breaking counter is per-instance
    g_tt2_dr4_pruned = 0;                // DR4 delayed-start prune counter is per-instance
    g_tt2_dead_expanded = 0; for (long& d : g_tt2_dup) d = 0;   // RCPSP_TT2_KILL / RCPSP_TT2_DUPPROBE counters are per-instance
    g_tt2_precheck_cut = 0;
    g_tt2_wait_edges = 0; g_tt2_wait_cut = 0; g_tt2_wait_pass = 0; g_tt2_wait_jumped = 0;   // explicit-wait model counters are per-instance
    g_tt2_immsel_fired = 0;              // immediate-selection counter is per-instance
    g_tt2_freefire_fired = 0;            // free-fire counter is per-instance
    g_tt2_theta_better = 0;              // Θ-tree "bound beat existing" counter is per-instance
    g_tt2_singleres_better = g_tt2_singleres_calls = 0;   // single-resource LB telemetry, per-instance
    g_tt2_hierrs_better = g_tt2_hierrs_calls = g_cbs_hierrs_cache_hits = 0;   // hier-RS LB telemetry, per-instance
    g_hierrs_time_sec = 0.0; g_cbs_hierrs_cache.clear();   // hier-RS timer + result cache, per-instance
    g_heur_time_sec = g_singleres_time_sec = g_mincut_time_sec = 0.0; g_heur_calls = 0; g_root_h = -1.0; g_shadow_set.clear(); g_shadow_probes = g_shadow_hits = 0; g_succ_probes = g_succ_hits = g_hcache_probes = g_hcache_hits = 0; if (g_subsolve_succ_cache || g_h_cache) get_tt2_cache().clear();  // always-on instrumentation, per-instance
    // RCPSP_TT2_PROF: per-instance phase timers and counters (Globals.h)
    g_prof_succ_sec = g_prof_avail_sec = g_prof_gen_sec = g_prof_dr5_sec = g_prof_hash_sec = 0.0;
    g_prof_hcp_sec = g_prof_hres_sec = g_prof_astar_sec = 0.0;
    g_prof_succ_calls = g_prof_hash_calls = g_prof_children_raw = g_prof_children_kept = 0;
    g_prof_h_calls_all = g_prof_h_known = g_prof_h_deltazero = 0;
    g_prof_maxf = -1; for (int& v : g_prof_f_at) v = -1;
    g_prof_group = group; g_prof_exam = exam; g_prof_size = problemType.c_str(); g_prof_next_trace = 5.0;
    g_prof_t0 = std::chrono::steady_clock::now();
    // TT2 UB (RCPSP_TT2_UB=1): seed the incumbent from a REAL feasible primal (serial
    // SGS, not the datasheet-optima cheat). Off => incumbent stays MAX => no pruning.
    g_ub_pruned = 0; g_incumbent = std::numeric_limits<short>::max(); g_tt2_ub_closed = false;
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
        rootb::reset();
        tt2nb::reset();
        bool closedByRootBounds = false;
        bool trivOK = tryTrivialRootSchedule_TT2(trivEst, trivMakespan);
        const bool rootRan = !trivOK && (rootb::DlbOn() || rootb::SeedOn() || rootb::ProbeOn()) && rootb::computeRootBounds();
        if (g_tt2_ub && rootRan && rootb::info().seed > 0 && rootb::info().seed < (int)g_incumbent)
            g_incumbent = (short)rootb::info().seed;          // honest primal (SGS+probe+LNS), no datasheet
        if (rootRan) {
            // RCPSP_ROOT_SEED schedule meets the root lower bound => optimal without A*
            closedByRootBounds = true; trivOK = true;
            trivMakespan = rootb::info().seed;
            trivEst.assign(RCPSPex.activities.size() + 1, 0);
            for (size_t i = 0; i < rootb::info().seedStart.size(); ++i) trivEst[i + 1] = rootb::info().seedStart[i];
        }
        if (trivOK) {
            auto tend = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> telapsed = tend - tstart;
            long peakMemKB = getPeakMemoryKB();

            if (closedByRootBounds)
                std::cout << "TT2 closed at root: seed schedule " << trivMakespan << " <= root lower bound " << std::max(rootb::info().lb, rootb::info().lbPlain) << " => ";
            else
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
                 << (g_tt2_wait ? (g_tt2_reverse ? "TT2Wbwd" : "TT2Wfwd") : (g_tt2_reverse ? "TT2bwd" : "TT2fwd")) << ","
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
                 << (closedByRootBounds ? 0 : 1) << ","   // trivialAtRoot (0 when closed by RootBounds.h: see closedAtRoot)
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
                 << "," << (g_tt2_reverse ? "bwd" : "fwd");   // direction: which net was searched
            writeTT2ProfCols(file, 0, 0, 0, false);   // RCPSP_TT2_PROF columns (A* never ran)
            file << "\n";
            g_tt2_compact_on = 0;
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
   g_prof_t0 = std::chrono::steady_clock::now(); g_prof_next_trace = 5.0;
   const auto rootSavedTO = astar_timeout_seconds;   // root bounds already used part of the per-instance budget
   if (rootb::seconds() > 0) astar_timeout_seconds = std::max<decltype(rootSavedTO)>(1, rootSavedTO - (decltype(rootSavedTO))std::ceil(rootb::seconds()));
   g_tt2_kill_armed = g_tt2_dr5 ? g_tt2_kill : 0;   // dead nodes come from DR5 thinning; this search only
   if (g_tt2_dupprobe)
       g_tt2_dup_probe = [&astar](const RCPSPState_TT2& c) {
           double g;
           if (astar.GetOpenListGCost(c, g))   return c.g < g ? 1 : 2;
           if (astar.GetClosedListGCost(c, g)) return c.g < g ? 3 : 4;
           return 0;
       };
   astar.GetPath(&as1, first, last, path);
   g_tt2_kill_armed = 0; g_tt2_dup_probe = nullptr;
   astar_timeout_seconds = rootSavedTO;
   g_prof_astar_sec = secs_since(g_prof_t0);
   if (g_tt2_compact_on == 1) for (auto& s : path) if (tt2CompactStripped(s)) tt2CompactRebuild(s);   // RCPSP_TT2_COMPACT: the path states get their token lists back (the dumps below read them)
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
    // RCPSP_TT2_UB_GE: an empty path can now mean SUCCESS - OPEN ran out with every f >= U pruned, which
    // proves no schedule shorter than the incumbent exists. Tell it apart from a timeout by the same deadline
    // TemplateAStar uses: a timed-out run is always past it, an exhausted one is not.
    g_tt2_ub_closed = path.empty() && g_tt2_ub && g_tt2_ub_ge
                      && g_incumbent < std::numeric_limits<short>::max()
                      && std::chrono::steady_clock::now() <= timeout;
    if (g_tt2_ub_closed) {
        makespan = (int)g_incumbent;
        std::cout << "TT2 UB-closed: OPEN exhausted with every f >= " << makespan
                  << " pruned => the incumbent is optimal" << std::endl;
    }

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
     else if (!g_tt2_ub_closed) {
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
        if (g_tt2_kill)
            std::cout << "TT2 KILL: mode=" << g_tt2_kill << " deadRecords=" << dt.deadCount()
                      << " deadExpanded=" << g_tt2_dead_expanded << std::endl;
        if (g_tt2_precheck)
            std::cout << "TT2 PRECHECK: pruned before construction=" << g_tt2_precheck_cut << std::endl;
    }
    if (g_tt2_dupprobe)
        std::cout << "TT2 DUP: new=" << g_tt2_dup[0] << " openLower=" << g_tt2_dup[1] << " openNotLower=" << g_tt2_dup[2]
                  << " closedLower=" << g_tt2_dup[3] << " closedNotLower=" << g_tt2_dup[4] << std::endl;
    if (g_tt2_sym)
        std::cout << "TT2 SYM: successors skipped by canonical order = " << g_tt2_sym_pruned << std::endl;
    if (g_tt2_theta)
        std::cout << "TT2 THETA: bound beat existing at " << g_tt2_theta_better << " states" << std::endl;
    if (g_tt2_dr4)
        std::cout << "TT2 DR4: successors pruned = " << g_tt2_dr4_pruned << std::endl;
    if (g_tt2_immsel)
        std::cout << "TT2 IMMSEL: forced-single-branch nodes = " << g_tt2_immsel_fired << std::endl;
    if (g_tt2_prof)
        std::cout << "TT2 PROF: astar=" << g_prof_astar_sec << "s succ=" << g_prof_succ_sec << " (avail=" << g_prof_avail_sec
                  << " gen=" << g_prof_gen_sec << " dr5=" << g_prof_dr5_sec << ") heur=" << g_heur_time_sec
                  << " (cp=" << g_prof_hcp_sec << " res=" << g_prof_hres_sec << ") hash=" << g_prof_hash_sec
                  << " other=" << (g_prof_astar_sec - g_prof_succ_sec - g_heur_time_sec - g_prof_hash_sec)
                  << "s maxF=" << g_prof_maxf << " children raw/kept=" << g_prof_children_raw << "/" << g_prof_children_kept << std::endl;

    std::ofstream file(filename, std::ios::app);
    file << group << "," << exam << "," << (elapsed.count() + rootb::seconds()) << ","   // + RootBounds.h time
         << ((!path.empty() || g_tt2_ub_closed) ? "True" : "False") << ","
         << makespan << ","
         << astar.GetNodesExpanded() << ","
         << astar.GetNodesTouched() << ","
         << path.size() << ","
        << (g_tt2_wait ? (g_tt2_reverse ? "TT2Wbwd" : "TT2Wfwd") : (g_tt2_reverse ? "TT2bwd" : "TT2fwd")) << ","
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
                 << "," << (g_tt2_reverse ? "bwd" : "fwd");   // direction: which net was searched
    writeTT2ProfCols(file, astar.GetNumOpenItems(), (uint64_t)astar.GetNumItems(), astar.GetUniqueNodesExpanded(), path.empty());
    file << "\n";

    g_tt2_compact_on = 0;
    return 0;
}

 // ── HDA* forward TT2 (mode "tt2hda_<size>", HDAStarTT2.h) ───────────────────────
// Same instance setup and trivial-root shortcut as solveRCPSP_TT2, then the search runs on
// RCPSP_HDA_THREADS workers (default: hardware threads). Flags whose state is not per-thread
// (sub-solves, root bounds, node deadline reasoning, dead-node kill) are refused.
std::string outName(const std::string& folder, const std::string& baseName, const std::string& extension);   // defined below
static bool onlyInstance(const std::string& size, int g, int e);                                               // defined below
static int hdaThreads() {
    if (const char* e = std::getenv("RCPSP_HDA_THREADS")) { int n = std::atoi(e); if (n > 0) return n; }
    return std::max(1u, std::thread::hardware_concurrency());
}
void solveRCPSP_TT2_HDA(int group, int exam, const std::string& filename, const std::string& problemType) {
    if (g_tt2_singleres || g_tt2_hierrs || g_subsolve_succ_cache || g_h_cache || g_tt2_reverse || g_tt2_dupprobe
        || tt2nb::On() || tt2nb::WinOn() || rootb::DlbOn() || rootb::SeedOn() || rootb::ProbeOn()) {
        std::cerr << "tt2hda: a flag that is not thread-safe yet is set (SINGLERES/HIERRS/DUPPROBE/sub-solve caches/"
                     "reverse/TT2_TT/root bounds); refusing." << std::endl;
        std::exit(1);
    }
    const int N = hdaThreads();
    g_tt2_hdz_fix = true;   // HDA* re-opens and lowers g far more often than serial A*; see Globals.h
    std::cout << "started solving TT2-HDA (" << N << " threads): " << group << ":" << exam << std::endl;
    count = 0; LB = 0;
    getPetri(petri, group, exam, problemType);
    getRCPSP(RCPSPex, group, exam, problemType);
    { static const double RSL[4] = {0.2, 0.5, 0.7, 1.0}; g_instance_rs = RSL[((group - 1) % 4 + 4) % 4]; }
    if (g_tt2_ub) { precomputeResourceInfo(); precomputeUpstream(); precomputeDownstream(); }
    deriveEndpointNames(initialstatename, finalstatename);
    RCPSPState_TT2 first;
    RCPSPState_TT2 last = first;
    last.g = HCost_TT2(last, first);
    g_tt2_compact_on = (g_tt2_compact && tt2CompactPrepare(first)) ? g_tt2_compact : 0;   // tables built here, read-only in the workers
    g_heur_time_sec = 0.0; g_heur_calls = 0; g_root_h = -1.0;
    g_incumbent = std::numeric_limits<short>::max();
    int seedUB = 0;
    if (g_tt2_ub) { int sgs = serialSGS_makespan(); if (sgs > 0 && sgs < (int)std::numeric_limits<short>::max()) { g_incumbent = (short)sgs; seedUB = sgs; } }

    auto t0 = std::chrono::steady_clock::now();
    std::vector<int> trivEst; int trivMakespan = 0;
    hda::Result r;
    bool trivial = false;
    rootb::reset(); tt2nb::reset();
    if (tryTrivialRootSchedule_TT2(trivEst, trivMakespan)) { trivial = true; r.proven = true; r.makespan = trivMakespan; r.lb = trivMakespan; }
    else {
        g_tt2_kill_armed = g_tt2_dr5 ? g_tt2_kill : 0;   // RCPSP_TT2_KILL: each worker skips nodes its own DR5 table retired
        r = hda::Solve(first, last, N, (double)astar_timeout_seconds, seedUB);
        g_tt2_kill_armed = 0;
    }
    const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();

    uint64_t exp = 0, expMax = 0; long sent = 0, recv = 0, dr5P = 0, dr5I = 0; double heur = 0;
    for (const auto& w : r.stats) { exp += w.expanded; expMax = std::max<uint64_t>(expMax, w.expanded); sent += w.sentStates; recv += w.recvStates; dr5P += w.dr5Pruned; dr5I += w.dr5Inserts; heur += w.heurSec; }
    std::cout << "TT2-HDA: " << (r.proven ? "optimal" : "not proven") << " makespan=" << r.makespan << " LB=" << r.lb
              << " expanded=" << exp << " (max/worker " << expMax << ") routed=" << sent << " dr5Pruned=" << dr5P
              << (r.timedOut ? " TIMEOUT" : "") << " time=" << secs << "s" << std::endl;
    std::ofstream file(filename, std::ios::app);
    file << group << "," << exam << "," << secs << "," << (r.proven ? "True" : "False") << "," << (r.proven ? r.makespan : -1) << ","
         << r.makespan << "," << r.lb << "," << exp << "," << expMax << "," << sent << "," << recv << "," << dr5P << "," << dr5I << ","
         << heur << "," << N << "," << (trivial ? 1 : 0) << "," << (r.timedOut ? 1 : 0) << "," << problemType << ","
         << getPeakMemoryKB() << "," << astar_timeout_seconds << "," << g_tt2_compact_on << "\n";
    g_tt2_compact_on = 0;
}

void runBenchmarkTT2HDA(const std::string& problemType) {
    std::string filename = outName("new_results", "output_tt2hda_" + problemType + "_", ".csv");
    { std::ofstream f(filename);
      if (!f.is_open()) { std::cerr << "Error opening file: " << filename << std::endl; return; }
      f << "group,exam,time,solved,makespan,ub,lb,expandNumber,expandMaxWorker,routedStates,receivedStates,domPruned,domInserts,"
           "heurTimeSec,threads,trivialAtRoot,timedOut,problemType,maxMem,timeoutS,tt2Compact" << std::endl; }
    std::cout << "\n=== TT2 HDA* (" << hdaThreads() << " threads) | " << problemType << " ===" << std::endl;
    for (int i = 1; i <= 48; i++)
        for (int j = 1; j <= 10; j++) {
            if (!onlyInstance(problemType, i, j)) continue;
            solveRCPSP_TT2_HDA(i, j, filename, problemType);
        }
    std::cout << "\nTT2 HDA* benchmark done (" << problemType << ") -> " << filename << std::endl;
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

// RCPSP_CBS_MIRROR (Globals.h): mirror the loaded project in place. Old activity i (0-based) becomes n-1-i, so the
// old sink is the new source and ids stay topological; successor and predecessor lists swap roles (their entries
// are 1-based ids: s -> n+1-s). Applying it twice restores the original.
static void mirrorLoadedInstanceForCBS() {
    const int n = (int)RCPSPex.activities.size();
    std::vector<Activity> acts(RCPSPex.activities.rbegin(), RCPSPex.activities.rend());
    std::vector<std::vector<short>> succ(n), pred(n);
    for (int i = 0; i < n; ++i) {
        const int ni = n - 1 - i;
        if (i < (int)RCPSPex.dependencies.size())
            for (short s : RCPSPex.dependencies[i])          pred[ni].push_back((short)(n + 1 - s));
        if (i < (int)RCPSPex.backword_dependencies.size())
            for (short p : RCPSPex.backword_dependencies[i]) succ[ni].push_back((short)(n + 1 - p));
        std::sort(pred[ni].begin(), pred[ni].end()); std::sort(succ[ni].begin(), succ[ni].end());
    }
    RCPSPex.activities = std::move(acts);
    RCPSPex.dependencies = std::move(succ);
    RCPSPex.backword_dependencies = std::move(pred);
}
// Everything the CBS search derives from the loaded project; call after its orientation changed.
static void cbsRebuildInstanceTables() {
    resource_info.clear(); downstream.clear(); upstream.clear();
    precomputeDownstream(); precomputeUpstream(); precomputeResourceInfo();
}

template<int N>
int solveRCPSP_CBS_impl(int group, int exam, const std::string& filename, const std::string& problemType) {
    debug_cardinal_num = 0;
    reset_mda_cache<N>();
    get_cbs_dominance_table<N>().clear();   // DR5 table is per-instance
    getRCPSP(RCPSPex, group, exam, problemType);
    g_cbs_mirror_now = 0;
    if (g_cbs_mirror) { mirrorLoadedInstanceForCBS(); g_cbs_mirror_now = 1; }   // RCPSP_CBS_MIRROR, before anything is derived
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
    cpcbs::resetInstance();   // CpCBS.h hooks stay off (built=false) while warm start swaps RCPSPex
    if (g_use_warmstart) computeWarmstartOrder<N>();

    // ── Upper-bound pruning: per-instance reset + SGS seed ──
    g_incumbent = std::numeric_limits<short>::max();
    g_ub_pruned = 0;
    g_leftshift_pruned = 0;
    g_lazy_hcost_evals = 0;
    g_lazy_reinserts = 0;
    g_cbs_pathmax_raised = 0;
    g_cbs_tb_seq = 0;
    g_cbs_restart_stage = 0;
    g_cbs_lb_floor = 0; g_lw_tries.fill(0); g_lw_raise.fill(0);   // RCPSP_CBS_RESTART_KEEPLB / RCPSP_CBS_LEARN, per instance
    g_cbs_learn_tries = g_cbs_learn_raises = g_cbs_learn_pickdiff = 0;
    g_cbs_fastfirst_skipped = 0;
    rootb::propCalls() = 0; rootb::propRounds() = 0;
    g_cbs_adapt_singular = 0;
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
    g_rel_infeasible = g_rel_fallback = g_rel_maxlen = 0; g_far_blocked = g_far_applied = 0;   // RCPSP_CBS_DELAY telemetry, per-instance
    g_cbs_freefix_kids = g_cbs_freefix_resolved = g_cbs_freefix_finalfail = g_cbs_freefix_sgs_calls = 0; g_ffd_n = g_ffd_same = g_ffd_goal = g_ffd_up = g_ffd_upsum = 0;   // RCPSP_CBS_FREEFIX telemetry, per-instance
    { int hz = 0; for (const auto& a : RCPSPex.activities) hz += a.duration; g_rel_horizon = hz; }
    g_orderswap_cand = 0;                     // order-swap measurement (RCPSP_ORDERSWAP), per-instance
    g_nmd_overshoot_events = 0; g_nmd_lost_siblings = 0;   // NMD overshoot diagnostic, per-instance
    LB = 0;   // proven-LB accumulator (TemplateAStar: LB = max(LB, popped f))
    // CpCBS.h step 1: with RootBounds.h flags the incumbent is the verified root seed, not the datasheet.
    cpcbs::resetInstance();
    rootb::computeRootBounds();   // no-op (and 0 s) when RCPSP_ROOT_DLB / _SEED / _PROBE are all off
    if (cpcbs::RootOn()) {
        const auto& ri = rootb::info();
        if (ri.seed > 0) {
            g_use_ub = true; g_incumbent = (short)ri.seed;
            cpcbs::st().realInc = true; cpcbs::st().ubSource = 2;
        }
        cpcbs::st().closedAtRoot = ri.closed;
        std::cout << "UB seed (root bounds): " << ri.seed << " root LB " << std::max(ri.lb, ri.lbPlain)
                  << (ri.closed ? " -> closed at root" : "") << std::endl;
    } else if (g_use_ub && !g_ub_seed) {
        // RCPSP_UB_SEED=0 (Globals.h): no seed. The bound is set by the feasible schedules the search generates, and
        // each of them stays on OPEN, so a state with makespan >= the bound can be discarded (B&P lines 4-6).
        cpcbs::st().realInc = true; cpcbs::st().ubSource = 3;
        std::cout << "UB: infinite at the start, set by schedules found during the search" << std::endl;
    } else if (g_use_ub) {
        int ub = getDatasheetUB(group, exam, problemType);
        if (ub > 0) { g_incumbent = (short)ub; cpcbs::st().ubSource = 1; }
        std::cout << "UB seed (datasheet): " << ub << std::endl;
    }
    if (cpcbs::RootOn() || cpcbs::TTOn())
        cpcbs::buildInstance(rootb::DlbOn() ? std::max(rootb::info().lb, rootb::info().lbPlain) : -1);

    RCPSP_CBS<N> as1;

    RCPSPState_CBS<N> first;
    RCPSPState_CBS<N> last = first;
    last.start_times[g_sink_id]=0;
    // RCPSP_CBS_ROOTES: tighten the root with the deadline propagation (see CpCBS.h)
    {
        const int rs = cpcbs::shiftRootToPropagatedEs(first.start_times);
        if (rs < 0) {
            cpcbs::st().closedAtRoot = true;          // no schedule beats the incumbent => it is optimal
            std::cout << "CBS root refuted at incumbent-1 => incumbent " << (int)g_incumbent << " is optimal" << std::endl;
        } else if (rs > 0) {
            (void)first.compute_h_and_RVS();   // recompute conflicts + h for the shifted schedule
            std::cout << "CBS root shifted to propagated earliest starts: " << cpcbs::st().rootEsMoved
                      << " activities, root makespan " << (int)first.start_times[g_sink_id] << std::endl;
        }
    }
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
    const auto cbsSavedTO = astar_timeout_seconds;   // root bounds already used part of the per-instance budget
    if (rootb::seconds() > 0) astar_timeout_seconds = std::max<decltype(cbsSavedTO)>(1, cbsSavedTO - (decltype(cbsSavedTO))std::ceil(rootb::seconds()));
    if (cpcbs::st().closedAtRoot) {
        // seed <= root LB: optimal without search; the UB-exhaust block below reports the incumbent
    } else if (!g_cbs_restart_stages.empty() && !first.rvs_activities_pool.empty()) {
        // (2026-10-07) the stages run on LazyAStarCBS (RCPSP_LAZY=1) or, with RCPSP_LAZY=0, on the eager TemplateAStar.
        // RCPSP_CBS_RESTART (Globals.h): complete A* runs in sequence, each with its own plateau order / threshold.
        const std::string savedSpec = g_cbs_tb_spec; const float savedT = g_hybrid_threshold;
        const int savedFreefix = g_cbs_freefix;
        const long long budget = astar_timeout_seconds;
        const auto t0 = std::chrono::steady_clock::now();
        const size_t nStages = g_cbs_restart_stages.size();
        for (size_t si = 0; si < nStages; ++si) {
            const long long used = (long long)std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - t0).count();
            const long long remaining = budget - used;
            if (remaining <= 0) break;
            if (si > 0) {                       // throw the previous stage's search state away
                get_cbs_dominance_table<N>().clear();
                reset_mda_cache<N>();
                g_cbs_tb_seq = 0;
                // ... but keep what it proved (the lower bound) and, with RCPSP_CBS_LEARN=1, what it observed
                if (g_cbs_restart_keeplb) g_cbs_lb_floor = std::max(g_cbs_lb_floor, root_mk_csv + (int)LB);
                if (g_cbs_learn == 2) { g_lw_tries.fill(0); g_lw_raise.fill(0); }
            }
            const CbsRestartStage& st = g_cbs_restart_stages[si];
            g_cbs_tb_spec = st.spec; g_hybrid_threshold = st.T;
            g_cbs_freefix = st.freefix ? std::max(1, savedFreefix) : savedFreefix;   // stage flag F
            {   // RCPSP_CBS_MIRROR: a stage marked /M runs on the mirrored project (relative to the global flag)
                const int want = (g_cbs_mirror ? 1 : 0) ^ (st.mirror ? 1 : 0);
                if (want != g_cbs_mirror_now) {
                    mirrorLoadedInstanceForCBS(); g_cbs_mirror_now = want;
                    cbsRebuildInstanceTables();
                    if (cpcbs::RootOn() || cpcbs::TTOn())
                        cpcbs::buildInstance(rootb::DlbOn() ? std::max(rootb::info().lb, rootb::info().lbPlain) : -1);
                    get_cbs_dominance_table<N>().clear(); reset_mda_cache<N>();
                    const int nA = std::min((int)RCPSPex.activities.size(), 128);   // the learned counters follow the activities
                    std::reverse(g_lw_tries.begin(), g_lw_tries.begin() + nA); std::reverse(g_lw_raise.begin(), g_lw_raise.begin() + nA);
                }
            }
            const long long stageBudget = (si + 1 < nStages) ? std::min<long long>(st.sec, remaining) : remaining;
            astar_timeout_seconds = stageBudget;
            g_cbs_restart_stage = (int)si + 1;
            RCPSP_CBS<N> envS;
            RCPSPState_CBS<N> rootS;            // a fresh root, evaluated under this stage's settings
            const auto ts = std::chrono::steady_clock::now();
            if (g_use_lazy) {
                LazyAStarCBS<RCPSPState_CBS<N>, int, RCPSP_CBS<N>> lazyAstar;
                lazyAstar.GetPath(&envS, rootS, last, path);
                nExpanded += (long)lazyAstar.GetNodesExpanded();
                nTouched  += (long)lazyAstar.GetNodesTouched();
            } else {                            // RCPSP_LAZY=0: same stage, every generated node is evaluated at once
                TemplateAStar<RCPSPState_CBS<N>, int, RCPSP_CBS<N>> eagerAstar;
                eagerAstar.GetPath(&envS, rootS, last, path);
                nExpanded += (long)eagerAstar.GetNodesExpanded();
                nTouched  += (long)eagerAstar.GetNodesTouched();
            }
            if (!path.empty()) break;           // solved: optimal
            const double stageSec = std::chrono::duration<double>(std::chrono::steady_clock::now() - ts).count();
            if (stageSec < (double)stageBudget - 0.5) break;   // OPEN exhausted, not a timeout: nothing to restart
        }
        g_cbs_tb_spec = savedSpec; g_hybrid_threshold = savedT; astar_timeout_seconds = budget; g_cbs_freefix = savedFreefix;
    } else if (g_use_lazy) {
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

    astar_timeout_seconds = cbsSavedTO;   // restored only after the UB-exhaust timeout test above

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
    if (cpcbs::RootOn() || cpcbs::TTOn())
        std::cout << "CPCBS ubSource=" << cpcbs::st().ubSource << " closedAtRoot=" << cpcbs::st().closedAtRoot
                  << " ttCalls=" << cpcbs::st().ttCalls << " ttRaised=" << cpcbs::st().ttRaised
                  << " prunedAtExpand=" << cpcbs::st().ttPrunedAtExpand << " ngLearned=" << cpcbs::st().ngLearned
                  << " ngHits=" << cpcbs::st().ngHits << " ttSec=" << cpcbs::st().ttSec << " rootSec=" << rootb::seconds() << std::endl;

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
            if (g_cbs_freefix == 2) std::cout << "FREEFIX DIAG: extra=" << g_ffd_n << " sameF=" << g_ffd_same << " goal=" << g_ffd_goal << " fUp=" << g_ffd_up << " meanRise=" << (g_ffd_up ? (double)g_ffd_upsum / g_ffd_up : 0.0) << std::endl;
            std::cout << "Lazy A*: hcost_evals(scans@pop)=" << g_lazy_hcost_evals
                      << " reinserts=" << g_lazy_reinserts
                      << "  vs expanded=" << nExpanded << " touched=" << nTouched << std::endl;
    }

    std::ofstream file(filename, std::ios::app);
    InstanceParams p = getParams(group);

    file << group << ","
         << exam << ","
         << (elapsed.count() + rootb::seconds()) << ","   // + RootBounds.h time
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
         << std::min(root_mk_csv + (int)LB, cpcbs::st().realInc ? (int)g_incumbent : INT_MAX) << ","   // provenLB; with a real incumbent, nodes popped at f >= incumbent are pruned, not bounds
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
         << g_cbs_hierrs_cache_hits << "," << subsolver_name() << "," << g_shadow_probes << "," << g_shadow_hits   // hierRsCacheHits,subsolver,shadowProbes,shadowHits
         << "," << cpcbs::st().ubSource << "," << (rootb::DlbOn() ? 1 : 0) << "," << (rootb::SeedOn() ? 1 : 0) << "," << (rootb::ProbeOn() ? 1 : 0)
         << "," << std::max(rootb::info().lb, rootb::info().lbPlain) << "," << rootb::info().seed << "," << rootb::seconds() << "," << (cpcbs::st().closedAtRoot ? 1 : 0)
         << "," << (cpcbs::TTOn() ? 1 : 0) << "," << (cpcbs::NGOn() ? 1 : 0) << "," << cpcbs::TTSteps() << "," << cpcbs::st().ttCalls << "," << cpcbs::st().ttRaised
         << "," << cpcbs::st().ttPrunedAtExpand << "," << cpcbs::st().ttSec << "," << cpcbs::st().ngLearned << "," << cpcbs::st().ngHits << "," << cpcbs::ctx().ng.size() << "," << (cpcbs::FloorOn() ? 1 : 0) << "," << (cpcbs::TTFull() ? 1 : 0) << "," << rootb::LnsMode() << "," << rootb::info().lnsSeedIn << "," << rootb::info().lnsBest << "," << rootb::info().lnsIters << "," << rootb::info().lnsImpCp << "," << rootb::info().lnsImpSgs << "," << rootb::info().lnsSec << "," << (cpcbs::LsPropOn() ? 1 : 0) << "," << cpcbs::st().lsCalls << "," << cpcbs::st().lsFail << "," << cpcbs::st().lsTighter << "," << cpcbs::st().lsScoreDiff << "," << cpcbs::st().lsPickDiff << "," << cpcbs::st().lsSec << "," << (cpcbs::RootEsOn() ? 1 : 0) << "," << cpcbs::st().rootEsMoved << "," << (cpcbs::st().rootEsRefuted ? 1 : 0) << "," << (cpcbs::WinOn() ? 1 : 0) << "," << cpcbs::st().winChecks << "," << cpcbs::st().winPruned << "," << g_cbs_tb << "," << g_lb_raise_exp << "," << (g_cbs_pathmax ? 1 : 0) << "," << g_cbs_pathmax_raised << "," << (g_cbs_gate_depth ? 1 : 0) << "," << g_cbs_sel << "," << (cpcbs::MdaRefOn() ? 1 : 0) << "," << cpcbs::st().mdaRef << "," << cpcbs::st().mdaRefChecks << "," << rootb::PropIters() << "," << rootb::propCalls() << "," << rootb::propRounds() << "," << (cpcbs::ErOn() ? 1 : 0) << "," << cpcbs::ErMaxRS() << "," << cpcbs::st().erRefuted << "," << g_cbs_adapt_branch << "," << g_cbs_adapt_singular << "," << (g_cbs_pathmax_insert ? 1 : 0) << "," << (g_cbs_tb_spec.empty() ? "-" : g_cbs_tb_spec) << "," << (g_cbs_restart.empty() ? std::string("-") : [] { std::string r = g_cbs_restart; for (char& ch : r) if (ch == ',') ch = ';'; return r; }()) << "," << g_cbs_restart_stage << "," << (g_cbs_restart_keeplb ? 1 : 0) << "," << g_cbs_lb_floor << "," << g_cbs_learn << "," << g_cbs_learn_prior << "," << g_cbs_learn_tries << "," << g_cbs_learn_raises << "," << g_cbs_learn_pickdiff << "," << g_cbs_fastfirst << "," << g_cbs_fastfirst_skipped << "," << (g_bap_mrvs ? 1 : 0) << "," << (g_ub_seed ? 1 : 0) << "," << (g_leftshift_global ? 1 : 0) << "," << (g_dom_bp_setkey ? 1 : 0) << "," << (g_cbs_mirror ? 1 : 0) << "," << rootb::PropFast() << "," << g_cbs_delay << "," << g_cbs_delay_maxkids << "," << g_rel_infeasible << "," << g_rel_fallback << "," << g_rel_maxlen << "," << g_far_blocked << "," << g_far_applied << "," << rootb::PropInc() << "," << g_cbs_freefix << "," << g_cbs_freefix_kids << "," << g_cbs_freefix_resolved << "," << g_cbs_freefix_prop << "," << g_cbs_freefix_max << "," << g_cbs_freefix_tries
         << "," << g_cbs_freefix_test << "," << g_cbs_freefix_share << "," << g_cbs_freefix_lazy << "," << g_cbs_freefix_finalfail << "," << g_cbs_freefix_stop << "," << g_cbs_freefix_goal << "," << g_cbs_freefix_maxover
         << "," << g_cbs_freefix_sgs << "," << g_cbs_freefix_maxconf << "," << g_cbs_freefix_sgs_calls
         << "," << g_cbs_freefix_rand << "," << g_cbs_freefix_randconf << "\n";   // CpCBS.h
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
// Feature tags for result file names, one per solver family, from the flags actually in force.
static std::string cbsFeatureTag() {
    std::string t;
    auto add = [&](const std::string& s){ if(!t.empty()) t += "-"; t += s; };
    if (setting.use_dr5) add(g_dom_rule==DOM_BP?"bp":g_dom_rule==DOM_DR5?"dr5":g_dom_rule==DOM_DR5S?"dr5s":"both");
    if (g_use_hybrid)    add("hyb");
    if (g_use_hybrid && std::lround(g_hybrid_threshold * 10) != 5) {   // hybrid threshold, tenths: t03, t11
        const long v = std::lround(g_hybrid_threshold * 10);
        add(std::string("t") + (v < 10 ? "0" : "") + std::to_string(v));
    }
    if (g_use_dr4)       add("dr4");
    if (g_use_ub)        add(g_ub_seed ? "ub" : "ub0");
    if (g_use_lazy)      add("lazy");
    if (g_dom_skyline)   add("sky");
    if (g_use_leftshift) add(g_leftshift_global ? "ls2" : "ls");
    if (g_bap_mrvs)      add("mrvs");
    if (g_dom_bp_setkey) add("setkey");
    if (g_use_bidir)     add("bidir");
    if (g_use_lean)      add("lean");
    if (g_use_inline)    add("inl");
    if (g_use_setdelay)  add("setdelay");
    if (g_cbs_tb)        add("tb" + std::to_string(g_cbs_tb));
    if (!g_cbs_tb_spec.empty()) add("tbs" + g_cbs_tb_spec);
    if (!g_cbs_restart_stages.empty()) add("rst" + std::to_string(g_cbs_restart_stages.size()));
    if (g_cbs_restart_keeplb) add("klb");
    if (g_cbs_learn)     add("lrn" + std::to_string(g_cbs_learn));
    if (g_cbs_fastfirst) add("ff" + std::to_string(g_cbs_fastfirst));
    if (g_cbs_mirror)    add("mir");
    if (g_cbs_delay)     add(std::string("dly") + (g_cbs_delay == 1 ? "prec" : g_cbs_delay == 2 ? "pair" : g_cbs_delay == 3 ? "wait" : "far"));
    if (rootb::PropFast()) add("pf" + std::to_string(rootb::PropFast()));
    if (rootb::PropInc())  add("inc" + std::to_string(rootb::PropInc()));
    if (g_cbs_freefix)     add(std::string("freefix") + (g_cbs_freefix_prop ? "" : "-noprop"));
    if (g_cbs_pathmax)   add("pmax");
    if (g_cbs_pathmax_insert) add("pmaxins");
    if (g_cbs_gate_depth) add("gatedepth");
    if (g_cbs_sel)       add("sel" + std::to_string(g_cbs_sel));
    if (cpcbs::MdaRefOn()) add("mdaref");
    if (cpcbs::ErOn())   add("er");
    if (g_cbs_adapt_branch) add("adapt" + std::to_string(g_cbs_adapt_branch));
    if (g_use_warmstart) add("warmstart");
    if (g_use_nmd)       add("nmd");
    if (rootb::DlbOn())  add("rootdlb");
    if (rootb::SeedOn()) add("seedub");
    if (rootb::ProbeOn()) add("probe");
    if (rootb::PairOn()) add("pair");
    if (tt2nb::On())     add("nodett");
    if (tt2nb::WinOn())  add("win");
    if (tt2nb::WinLsOn()) add("winls");
    if (tt2nb::SerOn())  add("ser");
    if (g_tt2_ub_ge)     add("ubge");
    if (rootb::LnsOn())  add(rootb::LnsMode() == 1 ? "lnscp" : rootb::LnsMode() == 2 ? "lnssgs" : "lns");
    if (cpcbs::LsPropOn()) add("lsprop");
    if (cpcbs::RootEsOn()) add("rootes");
    if (cpcbs::WinOn())    add("win");
    if (std::getenv("RCPSP_ONLY") && *std::getenv("RCPSP_ONLY")) add("only");
    if (cpcbs::TTOn())   add("tt");
    if (cpcbs::NGOn())   add("ng");
    if (!cpcbs::FloorOn()) add("nofloor");
    if (cpcbs::TTOn() && !cpcbs::TTFull()) add("tt0");
    return t;
}
static std::string tt2FeatureTag() {
    std::string t;
    auto add = [&](const std::string& s){ if(!t.empty()) t += "-"; t += s; };
    if (g_tt2_wait)      add(g_tt2_waitjump ? "wait" : "wait-nojump");
    if (g_tt2_dr5)       add("dr5");
    if (g_tt2_dr4)       add("dr4");
    if (g_tt2_batch)     add("batch");
    if (g_tt2_kill)      add("kill" + std::to_string(g_tt2_kill));
    if (g_tt2_dupprobe)  add("dupprobe");
    if (g_tt2_precheck)  add(g_tt2_precheck == 2 ? "precheckchk" : "precheck");
    if (g_tt2_sym)       add("sym");
    if (!tt2TbSpec().empty()) add("tbs" + tt2TbSpec());
    if (g_tt2_gendesc)   add("gendesc");
    if (g_tt2_theta)     add("theta");
    if (g_tt2_prof)      add("prof");
    if (g_tt2_compact)   add(g_tt2_compact == 2 ? "compactchk" : "compact");
    if (rootb::DlbOn())  add("rootdlb");
    if (rootb::SeedOn()) add("seedub");
    if (rootb::ProbeOn()) add("probe");
    if (rootb::PairOn()) add("pair");
    if (tt2nb::On())     add("nodett");
    if (tt2nb::WinOn())  add("win");
    if (tt2nb::WinLsOn()) add("winls");
    if (tt2nb::SerOn())  add("ser");
    if (g_tt2_ub_ge)     add("ubge");
    if (rootb::LnsOn())  add(rootb::LnsMode() == 1 ? "lnscp" : rootb::LnsMode() == 2 ? "lnssgs" : "lns");
    if (cpcbs::LsPropOn()) add("lsprop");
    if (cpcbs::RootEsOn()) add("rootes");
    if (cpcbs::WinOn())    add("win");
    if (std::getenv("RCPSP_ONLY") && *std::getenv("RCPSP_ONLY")) add("only");
    return t;
}
// HBAE / BAE read their settings from RCPSP_BI_* at solve time, so the tag reads the same variables
// with the same defaults.
static std::string biFeatureTag() {
    auto env = [](const char* k, const char* d){ const char* e = std::getenv(k); return std::string(e ? e : d); };
    std::string t = (env("RCPSP_BI_PRIO", "0") != "0") ? "fgh" : "b2g";
    if (env("RCPSP_TT2_WAIT", "0") != "0") {   t += (env("RCPSP_TT2_WAITJUMP", "1") != "0") ? "-wait" : "-wait-nojump";
        const std::string mo = env("RCPSP_BI_MEETONLY", "both"); if (mo != "both") t += "-meetonly" + mo; }
    const std::string m = env("RCPSP_BI_DR5", "3");
    if (m != "0") t += "-dr5" + std::string(m == "3" ? "both" : m == "1" ? "fwd" : m == "2" ? "bwd" : m);
    if (env("RCPSP_BI_DR4", "0") != "0") {     t += "-dr4";
        const std::string s4 = env("RCPSP_BI_DR4SIDE", "both"); if (s4 == "fwd" || s4 == "bwd") t += s4; }
    if (env("RCPSP_BI_BATCH", "0") != "0")     t += "-batch";
    if (env("RCPSP_BI_NIP", "1") == "0")       t += "-nonip";
    if (env("RCPSP_BI_DRAINCERT", "0") != "0") t += "-draincert";
    if (m == "0" && env("RCPSP_BI_DR4", "0") == "0") t += "-nodr";
    if (env("RCPSP_BI_ALT", "0") != "0")        t += "-alt";
    if (env("RCPSP_BI_ACTIVETB", "0") != "0")   t += "-activetb";
    if (!tt2TbSpec().empty())                   t += "-tbs" + tt2TbSpec();
    if (env("RCPSP_BI_HFRESH", "1") != "0")     t += "-hfresh";
    if (env("RCPSP_BI_DZFIRST", "1") != "0")    t += "-dzfirst";
    if (rootb::DlbOn())                         t += "-rootdlb";
    if (rootb::SeedOn())                        t += "-seedub";
    if (rootb::ProbeOn())                       t += "-probe";
    if (rootb::PairOn())                        t += "-pair";
    if (tt2nb::On())                            t += "-nodett";
    if (rootb::LnsOn())                         t += std::string("-") + (rootb::LnsMode() == 1 ? "lnscp" : rootb::LnsMode() == 2 ? "lnssgs" : "lns");
    if (std::getenv("RCPSP_ONLY") && *std::getenv("RCPSP_ONLY"))                        t += "-only";
    if (env("RCPSP_BI_MINF_HEAP", "1") == "0")  t += "-minfscan";
    { const std::string bw = env("RCPSP_BI_BWD", "");
      if (bw == "unfire" || bw == "1") { t += "-unfire"; if (env("RCPSP_BWD_FULLVERIFY", "1") == "0") t += "-lightverify"; } }
    if (env("RCPSP_BI_HFUSE", "0") != "0")  t += "-hfuse1";
    if (env("RCPSP_BI_STRHASH", "0") != "0") t += "-strhash";
    if (env("RCPSP_CP_FAST", "1") == "0")    t += "-cpmap";
    if (env("RCPSP_RES_FAST", "1") == "0")   t += "-resmap";
    if (env("RCPSP_DR5_FAST", "1") == "0")   t += "-dr5vec";
    if (env("RCPSP_DR5_SORTED", "0") != "0") t += "-dr5sorted";
    if (env("RCPSP_GEN_FAST", "1") == "0")   t += "-genref";
    if (env("RCPSP_DR5_MTF", "1") == "0")    t += "-dr5nomtf";
    if (env("RCPSP_BI_STOP", "avg") == "max") t += "-stopmax";
    if (env("RCPSP_BI_AUDIT", "0") != "0")      t += "-audit";
    if (env("RCPSP_BI_DOMMEET", "0") != "0") { t += "-dommeet"; if (env("RCPSP_BI_MEET_GHOST", "0") != "0") t += "-ghost";
                                                if (env("RCPSP_BI_SHIFTMEET", "0") != "0") t += "-shift"; }
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

// ── RESULT FILES: one folder, readable names ─────────────────────────────────
// FOLDER: every CSV goes to ONE place whatever directory the run starts in: RCPSP_RESULTS_DIR if set,
// else <repo>/new_results, the repo root being the first ancestor of the working directory that
// contains HOG2/RCPSP (j30 runs start in HOG2/RCPSP, j60/j90 at the root). An absolute folder argument
// is kept as given.
static std::string resultsDir(const std::string& folder) {
    if (const char* e = std::getenv("RCPSP_RESULTS_DIR")) if (*e) return e;
    if (fs::path(folder).is_absolute()) return folder;
    std::error_code ec;
    fs::path p = fs::current_path(ec);
    for (int i = 0; i < 6 && !ec && !p.empty(); ++i) {
        if (fs::exists(p / "HOG2" / "RCPSP", ec)) return (p / folder).string();
        if (p == p.parent_path()) break;
        p = p.parent_path();
    }
    return folder;
}

// NAME: <solver>_<size>_<info>_vNN.csv, e.g. TT2fwd_j60_dr5-dr4-batch-prof_v01.csv or
// HBAE_j30_fgh-dr5both-dr4-nonip-draincert_v01.csv. The solver and size come from the call site's
// baseName ("output_tt2rev_j60_" -> TT2bwd, j60); <info> is any remaining baseName tokens plus the
// feature tag; vNN is the first free version. The file is created at once so parallel runs cannot
// take the same name.
std::string getNextFilename(const std::string& folder, const std::string& baseName, const std::string& extension) {
    const std::string dir = resultsDir(folder);
    std::error_code ec;
    fs::create_directories(dir, ec);

    std::vector<std::string> tok;
    { std::string cur;
      for (char ch : baseName) { if (ch == '_') { if (!cur.empty()) tok.push_back(cur); cur.clear(); } else cur += ch; }
      if (!cur.empty()) tok.push_back(cur); }
    if (!tok.empty() && tok[0] == "output") tok.erase(tok.begin());

    std::string size;
    for (size_t i = 0; i < tok.size(); ++i) {
        const std::string& x = tok[i];
        bool isSize = x.size() >= 2 && x[0] == 'j';
        for (size_t k = 1; isSize && k < x.size(); ++k) isSize = (x[k] >= '0' && x[k] <= '9');
        if (isSize) { size = x; tok.erase(tok.begin() + i); break; }
    }

    static const std::map<std::string, std::string> solverOf = {
        {"tt2", "TT2fwd"}, {"tt2rev", "TT2bwd"}, {"tt2one", "TT2one"}, {"tt2oldbwd", "TT2oldbwd"},
        {"tt2bwd", "TT2bwdmeet"}, {"tt2bae", "BAE"}, {"tt2hbae", "HBAE"}, {"dumpdom", "CBSdumpdom"}};
    std::string solver = "CBS", family = "cbs";
    if (!tok.empty() && tok[0] == "initF") {
        const bool cbs = tok.size() > 1 && tok[1] == "cbs";
        solver = cbs ? "initFCBS" : "initFTT2"; family = cbs ? "cbs" : "tt2";
        tok.erase(tok.begin(), tok.begin() + std::min<size_t>(2, tok.size()));
    } else if (!tok.empty() && solverOf.count(tok[0])) {
        solver = solverOf.at(tok[0]);
        family = (solver == "HBAE" || solver == "BAE") ? "bi" : (solver.rfind("TT2", 0) == 0) ? "tt2" : "cbs";
        tok.erase(tok.begin());
    }

    std::string info;
    for (const auto& x : tok) info += (info.empty() ? "" : "-") + x;
    const std::string tag = (family == "bi") ? biFeatureTag() : (family == "tt2") ? tt2FeatureTag() : cbsFeatureTag();
    if (!tag.empty()) info += (info.empty() ? "" : "-") + tag;
    if (info.empty()) info = "base";
    const std::string stem = solver + (size.empty() ? "" : "_" + size) + "_" + info;

    for (int v = 1; ; ++v) {
        char vb[16]; std::snprintf(vb, sizeof vb, "_v%02d", v);
        const std::string f = dir + "/" + stem + vb + extension;
        if (fs::exists(f, ec)) continue;
        { std::ofstream reserve(f); }
        return f;
    }
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
         << "useFirst,useConflictPrioritization,useHeuristic,useMDASets,useMDACache,useStrongConstraints,useMDABAB,cardinalityRatio,useDR5,domRule,useUB,useHybrid,hybridT,useLeftshift,useBidir,ubPruned,leftshiftPruned,domPruned,domChecks,domStored,useLazy,useSkyline,lazyEvals,lazyReinserts,useNonMinimalDelay,useAncestorBranching,useDominanceSib,usePairDecomp,useHGreed,useLean,useInline,domCap,timeoutS,useWarmStart,warmStartK,warmStartBudgetS,warmStartDir,useSetDelay,warmStartRS,warmstartEngaged,warmstartInflMk,rootF,provenLB,warmstartSec,useDR4,useThetaBound,thetaBoundBetter,useSubsetLB,subsetBetter,subsetSolves,subsetExpandsTotal,subsetCapped,subsetMaxExpands,subsetCacheHits,useMdaRecursive,imp2Fires,imp2MaxDepth,useNmdPrecedence,orderSwapCand,useCbsMinCut,minCutBetter,minCutCalls,useRSAdapt,rsThreshold,instanceRS,useSingleRes,singleResBetter,singleResCalls,heuristicLowRS,heuristicHighRS,rootH,heurTimeSec,heurCalls,singleResTimeSec,minCutTimeSec,useHierRS,hierRsBetter,hierRsCalls,hierRsTimeSec,hierRsCacheHits,subsolver,shadowProbes,shadowHits,cbsUbSource,rootDlbOn,rootSeedOn,rootProbeOn,rootLB,seedUB,rootSec,closedAtRoot,cbsTT,cbsNG,ttSteps,ttCalls,ttRaised,ttPrunedAtExpand,ttSec,ngLearned,ngHits,ngStored,cbsRootFloor,ttFull,rootLnsOn,lnsSeedIn,lnsBest,lnsIters,lnsImpCp,lnsImpSgs,lnsSec,lsPropOn,lsCalls,lsFail,lsTighter,lsScoreDiff,lsPickDiff,lsSec,cbsRootEs,rootEsMoved,rootEsRefuted,cbsWin,winChecks,winPruned,cbsTB,lbRaiseExp,cbsPathmax,pathmaxRaised,cbsGateDepth,cbsSel,cbsMdaRef,mdaRefRaised,mdaRefChecks,propIters,propCalls,propRounds,cbsTtEr,cbsTtErMaxRS,erRefuted,cbsAdaptBranch,adaptSingular,cbsPathmaxInsert,cbsTbSpec,cbsRestart,restartStage,cbsRestartKeepLB,restartFloor,cbsLearn,cbsLearnPrior,learnTries,learnRaises,learnPickDiff,cbsFastFirst,fastFirstSkipped,bapMrvs,ubSeed,leftshiftGlobal,domBpSetKey,cbsMirror,propFast,cbsDelay,cbsDelayMaxKids,relInfeasible,relFallback,relMaxLen,farBlocked,farApplied,propInc,cbsFreeFix,freeFixKids,freeFixResolved,freeFixProp,freeFixMax,freeFixTries,freeFixTest,freeFixShare,freeFixLazy,freeFixFinalFail,freeFixStop,freeFixGoal,freeFixMaxOver,freeFixSgs,freeFixMaxConf,freeFixSgsCalls,freeFixRand,freeFixRandConf"
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
         << "useFirst,useConflictPrioritization,useHeuristic,useMDASets,useMDACache,useStrongConstraints,useMDABAB,cardinalityRatio,useDR5,domRule,useUB,useHybrid,hybridT,useLeftshift,useBidir,ubPruned,leftshiftPruned,domPruned,domChecks,domStored,useLazy,useSkyline,lazyEvals,lazyReinserts,useNonMinimalDelay,useAncestorBranching,useDominanceSib,usePairDecomp,useHGreed,useLean,useInline,domCap,timeoutS,useWarmStart,warmStartK,warmStartBudgetS,warmStartDir,useSetDelay,warmStartRS,warmstartEngaged,warmstartInflMk,rootF,provenLB,warmstartSec,useDR4,useThetaBound,thetaBoundBetter,useSubsetLB,subsetBetter,subsetSolves,subsetExpandsTotal,subsetCapped,subsetMaxExpands,subsetCacheHits,useMdaRecursive,imp2Fires,imp2MaxDepth,useNmdPrecedence,orderSwapCand,useCbsMinCut,minCutBetter,minCutCalls,useRSAdapt,rsThreshold,instanceRS,useSingleRes,singleResBetter,singleResCalls,heuristicLowRS,heuristicHighRS,rootH,heurTimeSec,heurCalls,singleResTimeSec,minCutTimeSec,useHierRS,hierRsBetter,hierRsCalls,hierRsTimeSec,hierRsCacheHits,subsolver,shadowProbes,shadowHits,cbsUbSource,rootDlbOn,rootSeedOn,rootProbeOn,rootLB,seedUB,rootSec,closedAtRoot,cbsTT,cbsNG,ttSteps,ttCalls,ttRaised,ttPrunedAtExpand,ttSec,ngLearned,ngHits,ngStored,cbsRootFloor,ttFull,rootLnsOn,lnsSeedIn,lnsBest,lnsIters,lnsImpCp,lnsImpSgs,lnsSec,lsPropOn,lsCalls,lsFail,lsTighter,lsScoreDiff,lsPickDiff,lsSec,cbsRootEs,rootEsMoved,rootEsRefuted,cbsWin,winChecks,winPruned,cbsTB,lbRaiseExp,cbsPathmax,pathmaxRaised,cbsGateDepth,cbsSel,cbsMdaRef,mdaRefRaised,mdaRefChecks,propIters,propCalls,propRounds,cbsTtEr,cbsTtErMaxRS,erRefuted,cbsAdaptBranch,adaptSingular,cbsPathmaxInsert,cbsTbSpec,cbsRestart,restartStage,cbsRestartKeepLB,restartFloor,cbsLearn,cbsLearnPrior,learnTries,learnRaises,learnPickDiff,cbsFastFirst,fastFirstSkipped,bapMrvs,ubSeed,leftshiftGlobal,domBpSetKey,cbsMirror,propFast,cbsDelay,cbsDelayMaxKids,relInfeasible,relFallback,relMaxLen,farBlocked,farApplied,propInc,cbsFreeFix,freeFixKids,freeFixResolved,freeFixProp,freeFixMax,freeFixTries,freeFixTest,freeFixShare,freeFixLazy,freeFixFinalFail,freeFixStop,freeFixGoal,freeFixMaxOver,freeFixSgs,freeFixMaxConf,freeFixSgsCalls,freeFixRand,freeFixRandConf"
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
          << "useStrongConstraints,useMDABAB,cardinalityRatio,useDR5,domRule,useUB,useHybrid,hybridT,useLeftshift,useBidir,ubPruned,leftshiftPruned,domPruned,domChecks,domStored,useLazy,useSkyline,lazyEvals,lazyReinserts,useNonMinimalDelay,useAncestorBranching,useDominanceSib,usePairDecomp,useHGreed,useLean,useInline,domCap,timeoutS,useWarmStart,warmStartK,warmStartBudgetS,warmStartDir,useSetDelay,warmStartRS,warmstartEngaged,warmstartInflMk,rootF,provenLB,warmstartSec,useDR4,useThetaBound,thetaBoundBetter,useSubsetLB,subsetBetter,subsetSolves,subsetExpandsTotal,subsetCapped,subsetMaxExpands,subsetCacheHits,useMdaRecursive,imp2Fires,imp2MaxDepth,useNmdPrecedence,orderSwapCand,useCbsMinCut,minCutBetter,minCutCalls,useRSAdapt,rsThreshold,instanceRS,useSingleRes,singleResBetter,singleResCalls,heuristicLowRS,heuristicHighRS,rootH,heurTimeSec,heurCalls,singleResTimeSec,minCutTimeSec,useHierRS,hierRsBetter,hierRsCalls,hierRsTimeSec,hierRsCacheHits,subsolver,shadowProbes,shadowHits,cbsUbSource,rootDlbOn,rootSeedOn,rootProbeOn,rootLB,seedUB,rootSec,closedAtRoot,cbsTT,cbsNG,ttSteps,ttCalls,ttRaised,ttPrunedAtExpand,ttSec,ngLearned,ngHits,ngStored,cbsRootFloor,ttFull,rootLnsOn,lnsSeedIn,lnsBest,lnsIters,lnsImpCp,lnsImpSgs,lnsSec,lsPropOn,lsCalls,lsFail,lsTighter,lsScoreDiff,lsPickDiff,lsSec,cbsRootEs,rootEsMoved,rootEsRefuted,cbsWin,winChecks,winPruned,cbsTB,lbRaiseExp,cbsPathmax,pathmaxRaised,cbsGateDepth,cbsSel,cbsMdaRef,mdaRefRaised,mdaRefChecks,propIters,propCalls,propRounds,cbsTtEr,cbsTtErMaxRS,erRefuted,cbsAdaptBranch,adaptSingular,cbsPathmaxInsert,cbsTbSpec,cbsRestart,restartStage,cbsRestartKeepLB,restartFloor,cbsLearn,cbsLearnPrior,learnTries,learnRaises,learnPickDiff,cbsFastFirst,fastFirstSkipped,bapMrvs,ubSeed,leftshiftGlobal,domBpSetKey,cbsMirror,propFast,cbsDelay,cbsDelayMaxKids,relInfeasible,relFallback,relMaxLen,farBlocked,farApplied,propInc,cbsFreeFix,freeFixKids,freeFixResolved,freeFixProp,freeFixMax,freeFixTries,freeFixTest,freeFixShare,freeFixLazy,freeFixFinalFail,freeFixStop,freeFixGoal,freeFixMaxOver,freeFixSgs,freeFixMaxConf,freeFixSgsCalls,freeFixRand,freeFixRandConf\n"; }

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
    // trace the non-minimal generator by default; RCPSP_NMD=0 traces the minimal-delay one (control)
    { const char* e = std::getenv("RCPSP_NMD"); setting.use_non_minimal_delay = !(e && std::atoi(e) == 0); }
    std::cout << "startrace generator: " << (setting.use_non_minimal_delay ? "NON-MINIMAL" : "MINIMAL") << " delay\n";
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
        << "useHeuristic,useMDASets,useMDACache,useStrongConstraints,useMDABAB,cardinalityRatio,useDR5,domRule,useUB,useHybrid,hybridT,useLeftshift,useBidir,ubPruned,leftshiftPruned,domPruned,domChecks,domStored,useLazy,useSkyline,lazyEvals,lazyReinserts,useNonMinimalDelay,useAncestorBranching,useDominanceSib,usePairDecomp,useHGreed,useLean,useInline,domCap,timeoutS,useWarmStart,warmStartK,warmStartBudgetS,warmStartDir,useSetDelay,warmStartRS,warmstartEngaged,warmstartInflMk,rootF,provenLB,warmstartSec,useDR4,useThetaBound,thetaBoundBetter,useSubsetLB,subsetBetter,subsetSolves,subsetExpandsTotal,subsetCapped,subsetMaxExpands,subsetCacheHits,useMdaRecursive,imp2Fires,imp2MaxDepth,useNmdPrecedence,orderSwapCand,useCbsMinCut,minCutBetter,minCutCalls,useRSAdapt,rsThreshold,instanceRS,useSingleRes,singleResBetter,singleResCalls,heuristicLowRS,heuristicHighRS,rootH,heurTimeSec,heurCalls,singleResTimeSec,minCutTimeSec,useHierRS,hierRsBetter,hierRsCalls,hierRsTimeSec,hierRsCacheHits,subsolver,shadowProbes,shadowHits,cbsUbSource,rootDlbOn,rootSeedOn,rootProbeOn,rootLB,seedUB,rootSec,closedAtRoot,cbsTT,cbsNG,ttSteps,ttCalls,ttRaised,ttPrunedAtExpand,ttSec,ngLearned,ngHits,ngStored,cbsRootFloor,ttFull,rootLnsOn,lnsSeedIn,lnsBest,lnsIters,lnsImpCp,lnsImpSgs,lnsSec,lsPropOn,lsCalls,lsFail,lsTighter,lsScoreDiff,lsPickDiff,lsSec,cbsRootEs,rootEsMoved,rootEsRefuted,cbsWin,winChecks,winPruned,cbsTB,lbRaiseExp,cbsPathmax,pathmaxRaised,cbsGateDepth,cbsSel,cbsMdaRef,mdaRefRaised,mdaRefChecks,propIters,propCalls,propRounds,cbsTtEr,cbsTtErMaxRS,erRefuted,cbsAdaptBranch,adaptSingular,cbsPathmaxInsert,cbsTbSpec,cbsRestart,restartStage,cbsRestartKeepLB,restartFloor,cbsLearn,cbsLearnPrior,learnTries,learnRaises,learnPickDiff,cbsFastFirst,fastFirstSkipped,bapMrvs,ubSeed,leftshiftGlobal,domBpSetKey,cbsMirror,propFast,cbsDelay,cbsDelayMaxKids,relInfeasible,relFallback,relMaxLen,farBlocked,farApplied,propInc,cbsFreeFix,freeFixKids,freeFixResolved,freeFixProp,freeFixMax,freeFixTries,freeFixTest,freeFixShare,freeFixLazy,freeFixFinalFail,freeFixStop,freeFixGoal,freeFixMaxOver,freeFixSgs,freeFixMaxConf,freeFixSgsCalls,freeFixRand,freeFixRandConf\n"; }
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
// ── ADMISSIBILITY PROBE ON THE OPTIMAL PATH ─────────────────────────────────
// For any node n on an OPTIMAL path, g*(n) + h*(n) = C*, so an admissible h must give
// g(n) + h(n) <= C* at EVERY node of that path. HBAE prunes anything with f >= U (the
// incumbent). If some optimal-path node reads f > C*, HBAE discards it, the side can
// never complete the optimal schedule, it drains, and the drain certifies the stale
// incumbent -- exactly the j30 3:9 failure (drains at 66, true optimum 65).
// Runs each side's own A* to optimality, then walks the returned path.
// tt2admis <type> <group> <exam>
static void tt2AdmisSide(const char* label, bool reversed, int group, int exam, const std::string& ptype) {
    getPetri(petri, group, exam, ptype); getRCPSP(RCPSPex, group, exam, ptype);
    if (reversed) reverseLoadedInstance();
    { static const double RSL[4]={0.2,0.5,0.7,1.0}; g_instance_rs = RSL[((group-1)%4+4)%4]; }
    deriveEndpointNames(initialstatename, finalstatename);
    get_tt2_dominance_table().clear();
    RCPSP_TT2 env; RCPSPState_TT2 first; RCPSPState_TT2 last = first;
    last.g = HCost_TT2(last, first);
    TemplateAStar<RCPSPState_TT2, int, RCPSP_TT2> astar;
    std::vector<RCPSPState_TT2> path;
    astar.GetPath(&env, first, last, path);
    if (path.empty()) { std::cout << "  " << label << ": no path (timeout)" << std::endl; return; }
    const int C = (int)path.back().g;
    int worst = INT_MIN, worstAt = -1, violations = 0;
    std::cout << "  " << label << " optimum C*=" << C << "  path length=" << path.size() << std::endl;
    for (size_t i = 0; i < path.size(); ++i) {
        RCPSPState_TT2 x = path[i], d0 = path[i];
        const int g = (int)path[i].g, h = (int)env.HCost(x, d0), f = g + h;
        if (f > C) { ++violations;
            if (violations <= 10)
                std::cout << "    VIOLATION node " << i << ": g=" << g << " h=" << h
                          << " f=" << f << " > C*=" << C << std::endl; }
        if (f - C > worst) { worst = f - C; worstAt = (int)i; }
    }
    std::cout << "  " << label << ": violations=" << violations << "/" << path.size()
              << "   worst f-C* = " << worst << " (node " << worstAt << ")"
              << (violations ? "   => HEURISTIC OVERESTIMATES ON THE OPTIMAL PATH"
                             : "   => admissible along this path") << std::endl;
}
int solveRCPSP_TT2_Admis(int group, int exam, const std::string& ptype="j30") {
    std::cout << "=== ADMISSIBILITY ON THE OPTIMAL PATH " << group << ":" << exam << " ===" << std::endl;
    g_instance_deadline = std::chrono::steady_clock::now()
                        + std::chrono::seconds(astar_timeout_seconds);
    g_instance_deadline_set = true;
    bool s5=g_tt2_dr5, s4=g_tt2_dr4, sb=g_tt2_batch;
    tt2AdmisSide("FORWARD ", false, group, exam, ptype);
    tt2AdmisSide("REVERSED", true,  group, exam, ptype);
    g_tt2_dr5=s5; g_tt2_dr4=s4; g_tt2_batch=sb;
    return 0;
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
    double rootH = -1;
    std::vector<std::tuple<std::string,int,int>> pathCuts;   // optimal-path cuts: (key, g, #started)
    std::unordered_map<std::string,int> bAll;           // EVERY reversed closed key -> min g_b
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
        { RCPSPState_TT2 r, d0; rootH = env.HCost(r, d0); }
        // DEPTH, not time, is what the "two shallower searches" argument is about: one long
        // activity can eat most of the makespan while the search barely advances. So position a
        // cut by how many activities have STARTED (finished + active), not by g.
        for (auto& st : path) {
            int started = (int)st.activeTransitionIndices.size();
            for (int q=1; q<=NT; q++) if (st.finishedActivitiys.test(q)) ++started;
            pathCuts.push_back({meetKeyForwardNorm(st, NT), (int)st.g, started});
        }
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
            { auto q=bAll.find(kk); if (q==bAll.end()||gb<q->second) bAll[kk]=gb; }
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
    // WHERE ALONG THE SCHEDULE can we meet? Only cuts on an OPTIMAL path can produce a meet at
    // the optimum, so walk the forward optimal path and ask, per cut, whether the reversed search
    // ever closed the matching state. Position = g_f/opt: 0 = at the source, 1 = at the goal,
    // 0.5 = the true middle, which is what the "two shallower searches" argument needs.
    if (fwdOpt > 0 && !pathCuts.empty()) {
        int nCut=0, nMatch=0; std::vector<double> posD, posG;
        int dec[10]={0,0,0,0,0,0,0,0,0,0};
        for (auto& pc : pathCuts) {
            const std::string& k=std::get<0>(pc); int gf=std::get<1>(pc), nst=std::get<2>(pc);
            if (nst<=0 || nst>=NT) continue;          // skip the two trivial endpoints
            ++nCut;
            if (!bAll.count(k)) continue;
            ++nMatch;
            double rd=(double)nst/(double)NT; posD.push_back(rd);
            posG.push_back(fwdOpt>0 ? (double)gf/(double)fwdOpt : 0.0);
            int d=(int)(rd*10); if(d<0)d=0; if(d>9)d=9; dec[d]++;
        }
        std::sort(posD.begin(), posD.end()); std::sort(posG.begin(), posG.end());
        std::cout << "  OPT-PATH CUTS: rootH=" << rootH << " opt=" << fwdOpt
                  << " gap=" << (fwdOpt - rootH)
                  << " interior=" << nCut << " meetable=" << nMatch;
        if (nMatch) {
            std::cout << "  DEPTH started/NT min=" << posD.front()
                      << " median=" << posD[posD.size()/2] << " max=" << posD.back()
                      << "  (time g/opt median=" << posG[posG.size()/2] << ")"
                      << "  depth deciles:";
            for (int d=0; d<10; d++) std::cout << " " << dec[d];
        }
        std::cout << std::endl;
    }
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
static int      g_bae_uMeet=-1, g_bae_biDR4=0, g_bae_dr4mask=0;
static bool     g_bae_proven=false;
static double   g_bae_time=0.0;
int solveRCPSP_TT2_BAE(int group, int exam, const std::string& ptype="j30") {
    std::cout << "=== BAE* bidirectional " << group << ":" << exam << " ===" << std::endl;
    getPetri(petri, group, exam, ptype); getRCPSP(RCPSPex, group, exam, ptype);
    int NT = (int)petri.Transitions.size();
    { static const double RSL[4]={0.2,0.5,0.7,1.0}; g_instance_rs=RSL[((group-1)%4+4)%4]; }
    g_instance_deadline = std::chrono::steady_clock::now()+std::chrono::seconds(astar_timeout_seconds);
    g_instance_deadline_set = true;
    // ── PARITY with the forward benchmark preamble (solveRCPSP_TT2) ─────────────
    // BAE* was skipping instance-level work the forward path always does, which made
    // any head-to-head unfair: the forward returns easy (high-RF / loose-RS) instances
    // in ZERO expansions via the trivial-root shortcut, while BAE* ran a full
    // bidirectional search on them. Same shortcut, same resource precomputes, same
    // SGS-seeded incumbent. Seeding U is sound: U is an achievable schedule length, and
    // min(f_F,f_B) >= U still means no cheaper solution exists, so a stop at U is optimal.
    // ── SIDE-SYMMETRY SAFETY ────────────────────────────────────────────────────
    // Three things in the TT2 pipeline are keyed to ONE net and are therefore unsafe when
    // two nets share the globals:
    //  (a) resource_info / upstream / downstream — net-specific precomputes. The reversed
    //      side would read the FORWARD net's tables, so the single-resource and hier-RS
    //      bounds would be wrong (possibly inadmissible) on that side. Disabled here.
    //  (b) the sub-solve h-cache and successor cache — keyed by MARKING, and a forward and
    //      a reversed marking can coincide numerically while having different h. Disabled.
    //  (c) the SGS UB seed is SAFE: it needs only the incumbent value, which is the same
    //      makespan for both nets, so it is kept.
    if (g_tt2_singleres || g_tt2_hierrs) {
        std::cout << "  [BAE] disabling singleres/hierRS: their precomputes are per-net "
                     "and the two sides share globals" << std::endl;
        g_tt2_singleres = false; g_tt2_hierrs = false;
    }
    if (g_h_cache || g_subsolve_succ_cache) {
        std::cout << "  [BAE] disabling sub-solve caches: keyed by marking, shared across "
                     "two different nets" << std::endl;
        g_h_cache = false; g_subsolve_succ_cache = false;
    }
    get_tt2_cache().clear();
    if (g_tt2_ub) { precomputeResourceInfo(); precomputeUpstream(); precomputeDownstream(); }
    g_tt2_dr4_pruned = 0; g_tt2_sym_pruned = 0; g_ub_pruned = 0;   // per-instance telemetry
    { std::vector<int> trivEst; int trivMk = 0;
      if (tryTrivialRootSchedule_TT2(trivEst, trivMk)) {
          std::cout << "BAE* " << group << ":" << exam << " makespan=" << trivMk
                    << " expandedF=0 expandedB=0 total=0 time=0s"
                    << "  norm=" << (g_meet_norm?1:0) << " trivialAtRoot=1" << std::endl;
          g_bae_expF=0; g_bae_expB=0; g_bae_meets=0; g_bae_firstMeet=-1; g_bae_uMeet=-1;
          g_bae_proven=true; g_bae_time=0.0; g_bae_biDR4=0; g_bae_dr4mask=0;
          return trivMk;
      } }
    int seedUB = INT_MAX;
    g_incumbent = std::numeric_limits<short>::max();
    if (g_tt2_ub) { int sgs = serialSGS_makespan();
        if (sgs > 0 && sgs < (int)std::numeric_limits<short>::max()) { g_incumbent=(short)sgs; seedUB=sgs; } }
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
    // biDR4 == 4 = ADAPTIVE. DR4 must stay OFF on one side or the two searches canonicalise
    // onto different schedules and the middle meet dies; but WHICH side can afford to lose it
    // is instance-dependent (j30 1:1 is backward-heavy 14784/68554, 2:2 is forward-heavy
    // 321092/3936, and fixed DR4-on-reversed costs 15x there). So: probe with DR4 off both
    // sides, then commit it to whichever side is doing more work and leave the cheap side
    // DR4-free. Toggling is sound - DR4 is a local per-expansion left-shift test, not state.
    long probeN = 2000; if (const char* pn=std::getenv("RCPSP_BI_PROBE")) probeN = std::atol(pn);
    int dr4mask = (biDR4==4) ? 0 : biDR4;
    bool dr4committed = (biDR4!=4);
    // batch was ruled out with the OLD (un-normalised) key, where multi-fire edges landed
    // the two directions on disjoint cut sets. Worth re-testing under the normalised key,
    // since batch is part of the forward BEST config -- RCPSP_BI_BATCH=1 to try it.
    // batch ON by default, matching the forward BEST config. The old "batch kills the meet"
    // verdict was an artifact of the UN-normalised key; under the normalised cut-marking key
    // batch keeps the middle meet AND cuts nodes hard (j30 2:2 325028->94267, 3:2 35965->18516).
    // RCPSP_BI_BATCH=0 to disable.
    { const char* bb=std::getenv("RCPSP_BI_BATCH"); g_tt2_batch = bb? (std::atoi(bb)!=0) : true; }
    g_tt2_dr5=true; g_tt2_dr4=false;
    g_tt2_dom_side=0; get_tt2_dominance_table().clear();
    g_tt2_dom_side=1; get_tt2_dominance_table().clear();
    // Stash holds the non-current side; start with global=forward, stash=reversed.
    P_RCPSP::PetriExample petriStash; RCPSP_example rcpspStash;
    petriStash = petri; rcpspStash = RCPSPex;   // copy forward into stash
    reverseLoadedInstance();                     // global := reversed
    std::swap(petri, petriStash); std::swap(RCPSPex, rcpspStash);  // global := forward, stash := reversed
    bool curFwd = true;
    auto useFwd = [&](bool want){ if (want!=curFwd){ std::swap(petri,petriStash); std::swap(RCPSPex,rcpspStash); curFwd=want; }
        g_tt2_dom_side = want?0:1; g_tt2_dr4 = (want ? (dr4mask&1) : (dr4mask&2)) != 0; };

    RCPSP_TT2 env;                               // stateless; reads globals
    useFwd(true);  RCPSPState_TT2 firstF; RCPSPState_TT2 lastF=firstF;
    useFwd(false); RCPSPState_TT2 firstB; RCPSPState_TT2 lastB=firstB;
    TemplateAStar<RCPSPState_TT2,int,RCPSP_TT2> astarF, astarB;
    std::vector<RCPSPState_TT2> pathF, pathB;
    useFwd(true);  astarF.InitializeSearch(&env, firstF, lastF, pathF);
    useFwd(false); astarB.InitializeSearch(&env, firstB, lastB, pathB);

    std::unordered_map<std::string,int> fClosed, bClosed;
    int U = seedUB; bool doneF=false, doneB=false; uint64_t expF=0, expB=0;
    uint64_t meets=0; long firstMeetExp=-1; int uMeet=INT_MAX;   // did the middle meet fire, and when?
    bool proven=false;   // true only if we exited with optimality PROVEN (not on the deadline)
    const double INF = 1e18;
    auto t0 = std::chrono::high_resolution_clock::now();
    while (true) {
        if (g_instance_deadline_set && std::chrono::steady_clock::now() > g_instance_deadline) break;
        if (!dr4committed && (long)(expF+expB) >= probeN) {          // probe over: arm the heavy side
            dr4mask = (expF > expB) ? 1 : 2; dr4committed = true;
        }
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
              << "  norm=" << (g_meet_norm?1:0) << " batch=" << (g_tt2_batch?1:0) << " biDR4=" << biDR4 << " dr4mask=" << dr4mask << " meets=" << meets
              << " firstMeetAtExp=" << firstMeetExp << " U_from_meet=" << (uMeet==INT_MAX?-1:uMeet) << std::endl;
    g_bae_expF=expF; g_bae_expB=expB; g_bae_meets=meets; g_bae_firstMeet=firstMeetExp;
    g_bae_uMeet=(uMeet==INT_MAX?-1:uMeet); g_bae_proven=proven; g_bae_time=el.count(); g_bae_biDR4=biDR4; g_bae_dr4mask=dr4mask;
    g_tt2_dr5=sDR5; g_tt2_dr4=sDR4; g_tt2_batch=sB;
    return mk;
}

// ── Meet-in-the-Middle (MM, Holte et al.) over forward N and reversed N^R ──────
// Custom bidirectional A* with MM priority pr(n)=max(g+h, 2g). Expands the side with
// smaller prmin; stops when U ≤ C=min(prminF,prminB). MM guarantees neither side expands
// a node with g > C*/2, which caps the (harder) reversed half — the win the f-balanced
// tt2bae lacks. Per-side DR via g_tt2_dom_side; meet via the translated key. Consistent
// heuristic (paper Prop. 2-4) ⇒ no reopening; lazy-deleted stale PQ entries.
// ── PROPER BAE* (Shperberg/Felner et al.) over N and its reversal N^R ─────────
// WHY NOT HOG2/generic/BAE.h: it uses env->GetStateHash for TWO different jobs —
// the within-queue node IDENTITY and the cross-queue MEET lookup. Our two sides
// need those to be DIFFERENT keys:
//   identity MUST be the FULL marking. Collapsing forward states onto the cut key
//     would merge states that differ only in what has just been committed at this
//     instant, and "start a at delay 0" would then map a state onto ITSELF — a
//     zero-cost self-loop that breaks the search space.
//   the meet MUST be on the NORMALISED CUT key, the only key both frames can agree
//     on (forward sits on START events, reversed on FINISH events).
// Making BAE.h separate the two would mean editing HOG2 core, so the ALGORITHM is
// implemented here instead, over the existing pieces: env.GetSuccessors (so DR5 /
// DR4 / batch all still apply), HCost_TT2, and the per-side dominance tables.
// What this adds over solveRCPSP_TT2_BAE's hand-rolled f-balance:
//   * the real BAE* priority  b(n) = 2g(n) + h(n) - h_opp(n)
//   * alternating expansion (the original BAE* policy)
//   * the (min b_F + min b_B)/2 lower bound, and termination at U <= LB
// h_opp is a genuine admissible bound on the distance already travelled: an activity
// x active with remaining r started at t-(dur_x-r) >= EST(x), so t >= EST(x)+dur_x-r;
// a finished x gives t >= EST(x)+dur_x. The max over started activities lower-bounds g.
static std::vector<int> computeESTforCurrentNet() {
    int n = (int)RCPSPex.activities.size();
    std::vector<int> est(n + 2, 0);
    bool changed = true; int guard = 0;
    while (changed && guard++ < n + 2) {           // DAG relaxation to fixpoint
        changed = false;
        for (int i = 1; i <= n; i++) {
            int m = 0;
            for (short p : RCPSPex.backword_dependencies[i - 1]) {
                int v = est[p] + RCPSPex.activities[p - 1].duration;
                if (v > m) m = v;
            }
            if (m > est[i]) { est[i] = m; changed = true; }
        }
    }
    return est;
}
static int hOppOf(const RCPSPState_TT2& s, const std::vector<int>& est, int NT) {
    int m = 0;
    for (int i = 1; i <= NT; i++)
        if (s.finishedActivitiys.test(i)) {
            int v = est[i] + RCPSPex.activities[i - 1].duration; if (v > m) m = v;
        }
    for (const auto& p : s.activeTransitionIndices) {
        int v = est[p.first] + RCPSPex.activities[p.first - 1].duration - p.second;
        if (v > m) m = v;
    }
    return m;
}

// ── HOG2 BAE* over the two nets: state / environment / heuristic wrapper ──────
// The algorithm is HOG2's own (HOG2/RCPSP/BAE_RCPSP.h, a copy of generic/BAE.h whose only
// structural change is that the cross-queue MEET lookup goes through a separate hash).
// Everything below is glue so that one BAE* instance can drive TWO different nets:
//   * the state carries which side it belongs to; successors inherit it
//   * GetSuccessors swaps the globals to that side's net before delegating to RCPSP_TT2,
//     so DR5/DR4/batch and the heuristic all behave exactly as in the unidirectional search
//   * GetStateHash = FULL marking (+ side bit) -> correct node identity within a queue
//   * GetMeetHash  = NORMALISED CUT key       -> frame-independent, so the two frontiers
//     collide on the same cut even though forward states sit on START events and reversed
//     states on FINISH events
struct RCPSPState_BI {
    RCPSPState_TT2 s;
    bool rev = false;                 // false = forward net, true = reversed net
    bool pass = false;                // RCPSP_TT2_WAITJUMP: a skipped wait node re-created only for the meet (never expanded; not identity)
    RCPSPState_BI() {}
    RCPSPState_BI(const RCPSPState_TT2& st, bool r) : s(st), rev(r) {}
    RCPSPState_BI(RCPSPState_TT2&& st, bool r) : s(std::move(st)), rev(r) {}
};
static int g_bi_NT = 0;               // activity count, set once per instance
// RCPSP_BI_BWD=unfire: the backward side un-fires on the FORWARD net (BWMEET::backwardSuccessors)
// instead of searching the reversed net. Both sides then hold forward-frame states.
static bool g_bi_unfire = false;
static inline uint64_t biFnv(const std::string& k) {
    uint64_t h = 1469598103934665603ULL;
    for (unsigned char c : k) { h ^= c; h *= 1099511628211ULL; }
    return h;
}
// FULL identity. RCPSPState_TT2::operator== compares FOUR things: finishedActivitiys,
// activity_nodes, resource_nodes, activeTransitionIndices. meetKeyForward covers only the
// first and last -- it is the CUT key, deliberately blind to token markings. Using it as the
// BI identity made GetStateHash lossy, so two states differing only in their activity/resource
// tokens hashed alike, and AStarOpenClosed silently merged them: the second was dropped as a
// duplicate. That deletes states from the bidirectional frontier that the unidirectional
// search keeps, which drains a queue early and lets a drain certify a non-optimal incumbent.
// Mirrors tt2VerifyHash in RCPSP.h, which documents the same full-identity field list.
// ── OPTIMAL-PATH TRACER ──────────────────────────────────────────────────────
// Marks the states of a KNOWN optimal path and reports, per state, what the
// bidirectional search did with it: generated / added to open / expanded / rejected
// by the f>=U cut / skipped by lazy nipping / never seen at all. Turns "something
// removes nodes" into a specific state and a specific branch. Off unless a trace is
// loaded, and it only ever reads -- it never changes the search.
enum PPEvent { PP_GEN = 0, PP_EXPAND = 1, PP_ADD = 2, PP_REJECT = 3, PP_NIPPED = 4 };
struct PathProbe {
    std::vector<std::string> keys;                 // identity keys, in path order
    std::vector<int> g;                            // g along the optimal path
    std::unordered_map<std::string,int> idx;       // key -> position
    std::vector<std::array<int,5>> ev;             // per position, count per PPEvent
    std::vector<double> rejG, rejH, rejU;          // first rejection seen
    void reset(size_t n) { ev.assign(n, {0,0,0,0,0});
        rejG.assign(n,-1); rejH.assign(n,-1); rejU.assign(n,-1); }
};
static PathProbe g_pp[2];        // [0] = forward net, [1] = reversed net
static bool g_pp_on = false;
static long long g_pp_calls[2] = {0,0};       // how many states the search offered per side
static std::string g_pp_sample[2];            // first key the search offered, for comparison
static inline std::string ppIdKey(const RCPSPState_TT2& st, int NT) {
    std::string k = meetKeyForward(st, NT);
    k += "|N:";
    for (const auto& q : st.activity_nodes) {
        k += std::to_string(q.first); k += ':'; k += std::to_string(q.second); k += ';'; }
    k += "|R:";
    for (int r = 0; r < 4; ++r) { k += '#';
        for (const auto& q : st.resource_nodes[r]) {
            k += std::to_string(q.first); k += ':'; k += std::to_string(q.second); k += ';'; } }
    if (st.waitSlack != -1) { k += "|W:"; k += std::to_string(st.waitSlack); }   // explicit-wait model with DR4
    return k;
}
static inline void ppMark(int side, const RCPSPState_TT2& st, int NT, PPEvent e,
                          double g = -1, double h = -1, double U = -1) {
    if (!g_pp_on) return;
    PathProbe& P = g_pp[side];
    const std::string k = ppIdKey(st, NT);
    ++g_pp_calls[side];
    if (g_pp_sample[side].empty()) g_pp_sample[side] = k;
    auto it = P.idx.find(k);
    if (it == P.idx.end()) return;
    P.ev[it->second][e]++;
    if (e == PP_REJECT && P.rejU[it->second] < 0) {
        P.rejG[it->second] = g; P.rejH[it->second] = h; P.rejU[it->second] = U; }
}
// FULL identity, matching RCPSPState_TT2::operator== (finished, activity_nodes,
// resource_nodes, activeTransitionIndices). meetKeyForward alone is the CUT key and is
// deliberately blind to token markings, so it cannot serve as the identity.
static inline std::string biIdKey(const RCPSPState_BI& a) { return ppIdKey(a.s, g_bi_NT); }
static inline std::string biCutKey(const RCPSPState_BI& a) {
    return (a.rev && !g_bi_unfire) ? meetKeyReversedTranslatedNorm(a.s, g_bi_NT)
                                   : meetKeyForwardNorm(a.s, g_bi_NT);
}

// ── NUMERIC identity / meet hashes ───────────────────────────────────────────
// Same fields and the same normalisation as biIdKey / biCutKey, mixed straight from the state
// instead of being printed to a std::string first (~15% of HBAE time). Equal keys give equal
// hashes and different keys differ with 64-bit probability, so the search is unchanged.
// RCPSP_BI_STRHASH=1 restores the string keys (A/B check).
static const bool g_bi_strhash = [](){ const char* e = std::getenv("RCPSP_BI_STRHASH"); return e && std::atoi(e) != 0; }();
static inline const std::bitset<128>& biFinMask() {          // bits 1..NT, as the string keys use
    static std::bitset<128> m; static int nt = -1;
    if (nt != g_bi_NT) { m.reset(); for (int i = 1; i <= g_bi_NT && i < 128; ++i) m.set(i); nt = g_bi_NT; }
    return m;
}
static inline uint64_t biMix(uint64_t h, uint64_t v) {
    uint64_t x = h ^ (v + 0x9e3779b97f4a7c15ULL);
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}
static inline uint64_t biPair(const std::pair<short,short>& p) { return ((uint64_t)(uint16_t)p.first << 16) | (uint16_t)p.second; }
// finished set + (id, value) pairs sorted, i.e. exactly the "F:...|A:..." part of the string keys
static inline uint64_t biHashCut(const std::bitset<128>& fin, std::pair<short,short>* av, int n) {
    static const std::bitset<128> LOW(~0ULL);
    std::sort(av, av + n);
    uint64_t h = biMix(0x243f6a8885a308d3ULL, (fin & LOW).to_ullong());
    h = biMix(h, (fin >> 64).to_ullong());
    h = biMix(h, (uint64_t)n);
    for (int i = 0; i < n; ++i) h = biMix(h, biPair(av[i]));
    return h;
}
static inline uint64_t biCutHashFwd(const RCPSPState_TT2& s) {               // = meetKeyForwardNorm
    std::pair<short,short> av[128]; int n = 0;
    for (const auto& p : s.activeTransitionIndices) {
        if ((int)p.second == (int)RCPSPex.activities[p.first-1].duration) continue;   // started AT T_c => unstarted
        if (n < 128) av[n++] = p;
    }
    return biHashCut(s.finishedActivitiys & biFinMask(), av, n);
}
static inline uint64_t biCutHashRevTranslated(const RCPSPState_TT2& b) {    // = meetKeyReversedTranslatedNorm
    std::bitset<128> fin = biFinMask() & ~b.finishedActivitiys;
    std::pair<short,short> av[128]; int n = 0;
    for (const auto& p : b.activeTransitionIndices) {
        const int tf = (int)RCPSPex.activities[p.first-1].duration - (int)p.second;
        if (tf == 0) { fin.set(p.first); continue; }                          // finished AT T_c => finished
        fin.reset(p.first);
        if (n < 128) av[n++] = {p.first, (short)tf};
    }
    return biHashCut(fin, av, n);
}
static inline uint64_t biIdHash(const RCPSPState_TT2& s) {                   // = ppIdKey
    std::pair<short,short> av[128]; int n = 0;
    for (const auto& p : s.activeTransitionIndices) if (n < 128) av[n++] = p;
    uint64_t h = biHashCut(s.finishedActivitiys & biFinMask(), av, n);
    h = biMix(h, 0xA11CE000ULL + (uint64_t)s.activity_nodes.size());
    for (const auto& q : s.activity_nodes) h = biMix(h, biPair(q));
    for (int r = 0; r < 4; ++r) {
        h = biMix(h, 0x5E500000ULL + (uint64_t)r * 131ULL + ((uint64_t)s.resource_nodes[r].size() << 24));
        for (const auto& q : s.resource_nodes[r]) h = biMix(h, biPair(q));
    }
    if (s.waitSlack != -1) h = biMix(h, 0x5A170000ULL + (uint64_t)(uint16_t)s.waitSlack);   // explicit-wait model with DR4
    return h;
}
// BAE* compares states for equality (start==goal, and childData.data==middleNode). Identity
// is the full marking plus the side, matching GetStateHash.
static inline bool operator==(const RCPSPState_BI& a, const RCPSPState_BI& b) {
    return a.rev == b.rev && biIdKey(a) == biIdKey(b);
}
static inline bool operator!=(const RCPSPState_BI& a, const RCPSPState_BI& b) { return !(a == b); }
static inline std::ostream& operator<<(std::ostream& o, const RCPSPState_BI& a) {
    return o << (a.rev ? "B:" : "F:") << biIdKey(a);
}

// Holds the non-current net and swaps the globals. petri/RCPSPex are globals, so the swap is
// how "which net am I on" is expressed; std::swap on these is O(1).
struct BiSides {
    P_RCPSP::PetriExample petriStash; RCPSP_example rcpspStash; bool curFwd = true;
    // initialstatename / finalstatename are GLOBALS (Globals.h) that RCPSPState_TT2's
    // constructor both writes and READS inside the same loop over places: a place seen
    // BEFORE the real source is compared against whatever value the previous construction
    // left behind, and gets a spurious token if the names match. They are per-net, so they
    // must be swapped with petri/RCPSPex. Without this the backward root was built on the
    // reversed net while still holding the FORWARD source name, giving it an extra token
    // at the sink -- so HBAE's backward search started from a marking no reversed state
    // can ever equal, and could not meet the forward side on any true cut.
    std::string initF, finalF, initB, finalB;
    void use(bool wantFwd) {
        const bool net = g_bi_unfire ? true : wantFwd;      // un-fire backward states are forward-net states
        if (net != curFwd) { std::swap(petri, petriStash); std::swap(RCPSPex, rcpspStash); curFwd = net; }
        initialstatename = net ? initF : initB;
        finalstatename   = net ? finalF : finalB;
        g_tt2_dom_side = wantFwd ? 0 : 1;      // per-side DR5 table
        // per-side DR5 enable: bit0 = forward, bit1 = reversed
        g_tt2_dr5 = (dr5Mask & (wantFwd ? 1 : 2)) != 0;
        // per-side DR4 (RCPSP_BI_DR4SIDE): with DR4 on both sides the forward keeps left-shifted and the reversed
        // net right-shifted schedules, so the two may share no optimal schedule; on one side the other side still
        // contains every schedule the DR4 side keeps
        g_tt2_dr4 = dr4On && (dr4Mask & (wantFwd ? 1 : 2)) != 0;
    }
    int dr5Mask = 3;
    bool dr4On = false; int dr4Mask = 3;
};

// HBAE hashing cost. Node identity and meet keys are built as strings, the suspected main extra
// per-node cost over unidirectional TT2. Calls always counted; time only with RCPSP_TT2_PROF=1.
static double   g_hb_bihash_sec = 0.0, g_hb_meethash_sec = 0.0;
static uint64_t g_hb_bihash_calls = 0, g_hb_meethash_calls = 0;

// RCPSP_BI_MEETONLY = both (default) | fwd | bwd | none: which side re-creates the wait nodes a pass-through jump skipped.
// ONE side is enough for the meeting guarantee: that side holds a common schedule's cut at every integer time up to its
// progress, and the other side's progress point on that schedule is a real node (a jump's landing node or a firing), so
// once the frontiers cross, that time is held by both. "none" gives the guarantee up (experiments only).
static int biMeetOnlyMask() {
    static const int m = [](){ const char* e = std::getenv("RCPSP_BI_MEETONLY"); const std::string v = e ? e : "both";
                               return v == "fwd" ? 1 : v == "bwd" ? 2 : v == "none" ? 0 : 3; }();
    return m;
}
class RCPSP_BiEnv {
public:
    RCPSP_BiEnv(BiSides* sd) : sides(sd) {}
    void GetSuccessors(const RCPSPState_BI& st, std::vector<RCPSPState_BI>& nb) {
        sides->use(!st.rev);
        rawBuf.clear();
        if (g_bi_unfire && st.rev) rawBuf = BWMEET::backwardSuccessors(st.s);   // un-fire, one predecessor per activity
        else env.GetSuccessors(st.s, rawBuf);          // RCPSP_TT2::GetSuccessors only reads its parent: no copy
        if (g_pp_on) { const int sd = st.rev ? 1 : 0;
            ppMark(sd, st.s, g_bi_NT, PP_EXPAND);
            for (auto& c : rawBuf) ppMark(sd, c, g_bi_NT, PP_GEN); }
        nb.clear(); nb.reserve(rawBuf.size());
        for (auto& c : rawBuf) {
            // RCPSP_TT2_WAITJUMP: a wait edge longer than one unit skipped nodes whose only successor was the next wait.
            // Re-create them, in time order and before the landing node, as meet-only nodes (BAE_RCPSP.h adds them
            // CLOSED), with the remaining waits they would have had, so this side still holds a cut at every time.
            const int span = (int)c.g - (int)st.s.g;
            if (g_tt2_wait && c.lastTransitionId == 0 && span > 1 && !(g_bi_unfire && st.rev) && (biMeetOnlyMask() & (st.rev ? 2 : 1))) {
                for (int j = 1; j < span; ++j) {
                    RCPSPState_TT2 m(st.s, (short)j, TT2WaitTag{});
                    m.waitSlack = (c.waitSlack < 0) ? (short)-1 : (c.waitSlack >= 32767) ? (short)32767 : (short)(c.waitSlack + (span - j));
                    nb.emplace_back(std::move(m), st.rev);
                    nb.back().pass = true;
                }
                g_tt2_wait_meetonly += span - 1;
            }
            nb.emplace_back(std::move(c), st.rev);   // successors inherit the side
        }
    }
    bool IsPassThrough(const RCPSPState_BI& st) const { return st.pass; }
    std::vector<RCPSPState_TT2> rawBuf;

    // ── DOMINANCE MEET (RCPSP_BI_DOMMEET) ─────────────────────────────────────
    // A cut is (finished set, active activities with their FORWARD remaining times), normalised exactly like
    // the meet key: a forward activity started at the cut instant counts as not started, a reversed one ending
    // at the cut instant counts as finished. Forward cut F and backward cut B with the same scheduled set
    // (finished + active) JOIN when F dominates B in the DR5 sense: every activity active in F finishes no later
    // than in B (0 if B has it finished). Then B's suffix replays from F with no later releases, so
    // g_F + g_B is the length of a real schedule. Exact cut matches are the special case of equal releases.
    // Per side, a bucket keeps only non-redundant entries: forward F1 makes F2 redundant if g1 <= g2 and F1
    // dominates F2; backward B1 makes B2 redundant if g1 <= g2 and B2 dominates B1.
    using DmKey = TT2DominanceTable::Key;
    struct DmEntry : TT2DominanceTable::FEntry { uint64_t ref = UINT64_MAX; };   // ref: its queue node (RCPSP_BI_AUDIT); none for a DR5-pruned state
    std::unordered_map<DmKey, std::vector<DmEntry>, TT2DominanceTable::KeyHash> dmTab[2];
    uint64_t dmInserts = 0, dmSkipped = 0, dmThinned = 0, dmJoinCmp = 0;
    uint64_t dmLastRef = UINT64_MAX; int dmLastShift = 0;   // the other side's entry and the shift of the join DomMeet last returned
    void DomMeetReset() { dmTab[0].clear(); dmTab[1].clear(); dmInserts = dmSkipped = dmThinned = dmJoinCmp = 0; dmShiftBest = 0; }
    size_t DomMeetEntries(int side) const { size_t n = 0; for (const auto& kv : dmTab[side]) n += kv.second.size(); return n; }
    // forward-frame normalised cut of st: key = scheduled set, (ids, rels) sorted by id, R = max(0, max rel)
    int domMeetSig(const RCPSPState_BI& st, DmKey& key, short* ids, short* rels, int& R) {
        std::bitset<128> sched;
        int n = 0;
        if (!st.rev || g_bi_unfire) {
            sched = st.s.finishedActivitiys & biFinMask();
            for (const auto& p : st.s.activeTransitionIndices) {
                if ((int)p.second == (int)RCPSPex.activities[p.first - 1].duration) continue;   // started at the cut
                if (n < 128) { ids[n] = p.first; rels[n] = p.second; ++n; }
            }
        } else {
            sched = biFinMask() & ~st.s.finishedActivitiys;          // not reversed-finished ...
            for (const auto& p : st.s.activeTransitionIndices) {
                const int tf = (int)RCPSPex.activities[p.first - 1].duration - (int)p.second;
                if (tf == 0) { sched.set(p.first); continue; }         // ends at the cut: finished
                sched.reset(p.first);
                if (n < 128) { ids[n] = p.first; rels[n] = (short)tf; ++n; }
            }
        }
        for (int i = 1; i < n; ++i) {                                   // insertion sort by id (n is small)
            short a = ids[i], r = rels[i]; int j = i - 1;
            while (j >= 0 && ids[j] > a) { ids[j + 1] = ids[j]; rels[j + 1] = rels[j]; --j; }
            ids[j + 1] = a; rels[j + 1] = r;
        }
        R = 0;
        for (int i = 0; i < n; ++i) { sched.set(ids[i]); if (rels[i] > R) R = rels[i]; }
        static const std::bitset<128> low64(~0ULL);
        key.lo = (sched & low64).to_ullong();
        key.hi = (sched >> 64).to_ullong();
        return n;
    }
    // ── SHIFT MEET (RCPSP_BI_SHIFTMEET, with RCPSP_BI_DOMMEET) ─────────────────
    // Every forward cut F and backward cut B with the same scheduled set join, not only when F dominates B:
    // run F's prefix, then B's whole suffix shifted LATER by
    //   delta = max(0, max over i active in F of  rem_F(i) - (i active in B ? rem_B(i) : 0)),
    // so every activity still running in F ends by the time the shifted suffix expects it (precedence and
    // resource releases no later than in B's own schedule). Cost g_F + g_B + delta, a real schedule. Never
    // shifted earlier: B's suffix may start something right after its cut. Dominance is the delta = 0 case.
    static bool ShiftMeetOn() { static const bool on = [](){ const char* e = std::getenv("RCPSP_BI_SHIFTMEET");
                                                            return e && std::atoi(e) != 0; }();
                                return on; }
    uint64_t dmShiftBest = 0;      // improving joins that needed a positive shift
    // lateness of A against B: max(0, max over i active in A of relA(i) - (i active in B ? relB(i) : 0))
    static int shiftOver(int nA, const short* idA, const short* relA, int nB, const short* idB, const short* relB) {
        int dlt = 0, ib = 0;
        for (int ia = 0; ia < nA; ++ia) {
            while (ib < nB && idB[ib] < idA[ia]) ++ib;
            const int bound = (ib < nB && idB[ib] == idA[ia]) ? (int)relB[ib] : 0;
            const int late = (int)relA[ia] - bound;
            if (late > dlt) dlt = late;
        }
        return dlt;
    }
    // Insert st (path cost g on its own side) and return the cheapest join with the other side (DBL_MAX if none)
    double DomMeet(const RCPSPState_BI& st, double g, double U, uint64_t ref = UINT64_MAX) {
        sides->use(!st.rev);                                            // durations of st's own net
        short ids[128], rels[128]; int R = 0; DmKey key;
        const int n = domMeetSig(st, key, ids, rels, R);
        const int side = st.rev ? 1 : 0;
        const bool shift = ShiftMeetOn();
        const auto dom = [](int nA, int RA, const short* idA, const short* relA, int nB, int RB, const short* idB, const short* relB) {
            return TT2DominanceTable::domFast(0, nA, RA, idA, relA, 0, nB, RB, idB, relB);
        };
        double best = DBL_MAX; bool bestShifted = false;
        dmLastRef = UINT64_MAX; dmLastShift = 0;
        auto oit = dmTab[1 - side].find(key);
        if (oit != dmTab[1 - side].end()) {
            for (const DmEntry& e : oit->second) {
                const double base = g + e.g;
                if (!(base < U && base < best)) continue;
                ++dmJoinCmp;
                if (shift) {
                    const int RF = (side == 0) ? R : e.R, RB = (side == 0) ? e.R : R;
                    if (!(base + std::max(0, RF - RB) < std::min(U, best))) continue;          // delta >= RF - RB
                    const int dlt = (side == 0) ? shiftOver(n, ids, rels, e.n, e.ids(), e.rels())
                                                : shiftOver(e.n, e.ids(), e.rels(), n, ids, rels);
                    const double c = base + dlt;
                    if (c < U && c < best) { best = c; bestShifted = (dlt > 0); dmLastRef = e.ref; dmLastShift = dlt; }
                } else {
                    const bool ok = (side == 0) ? dom(n, R, ids, rels, e.n, e.R, e.ids(), e.rels())     // F (new) dominates B
                                                : dom(e.n, e.R, e.ids(), e.rels(), n, R, ids, rels);    // F (entry) dominates B (new)
                    if (ok) { best = base; bestShifted = false; dmLastRef = e.ref; dmLastShift = 0; }
                }
            }
        }
        if (best < U && bestShifted) ++dmShiftBest;
        auto& bucket = dmTab[side][key];
        const short gs = (short)g;
        // Redundancy. Forward: E makes F redundant if g_E + lateness(E over F) <= g_F.
        // Backward: E makes B redundant if g_E + lateness(B over E) <= g_B. (Without shift: the dominance forms.)
        for (const DmEntry& e : bucket) {
            if (e.g > gs) continue;
            bool redundant;
            if (shift) {
                const int s = (side == 0) ? shiftOver(e.n, e.ids(), e.rels(), n, ids, rels)
                                          : shiftOver(n, ids, rels, e.n, e.ids(), e.rels());
                redundant = (e.g + s <= gs);
            } else {
                redundant = (side == 0) ? dom(e.n, e.R, e.ids(), e.rels(), n, R, ids, rels)
                                        : dom(n, R, ids, rels, e.n, e.R, e.ids(), e.rels());
            }
            if (redundant) { ++dmSkipped; return best; }
        }
        size_t w = 0;
        for (size_t i = 0; i < bucket.size(); ++i) {
            const DmEntry& e = bucket[i];
            bool drop = false;
            if (gs <= e.g) {
                if (shift) {
                    const int s = (side == 0) ? shiftOver(n, ids, rels, e.n, e.ids(), e.rels())
                                              : shiftOver(e.n, e.ids(), e.rels(), n, ids, rels);
                    drop = (gs + s <= e.g);
                } else {
                    drop = (side == 0) ? dom(n, R, ids, rels, e.n, e.R, e.ids(), e.rels())
                                       : dom(e.n, e.R, e.ids(), e.rels(), n, R, ids, rels);
                }
            }
            if (drop) { ++dmThinned; continue; }
            if (w != i) bucket[w] = std::move(bucket[i]);
            ++w;
        }
        bucket.resize(w);
        bucket.emplace_back();
        DmEntry& ne = bucket.back();
        ne.g = gs; ne.side = (short)side; ne.n = (short)n; ne.R = (short)R; ne.ref = ref;
        if (n <= TT2DominanceTable::K) { for (int j = 0; j < n; ++j) { ne.id[j] = ids[j]; ne.rel[j] = rels[j]; } }
        else { ne.big.reset(new short[2 * n]); for (int j = 0; j < n; ++j) { ne.big[j] = ids[j]; ne.big[n + j] = rels[j]; } }
        ++dmInserts;
        return best;
    }
    // Called from BAE_RCPSP.h so the tracer can see what the SEARCH did, not just what
    // the successor function produced.
    void Probe(const RCPSPState_BI& st, int ev, double g, double h, double U) {
        if (g_pp_on) ppMark(st.rev ? 1 : 0, st.s, g_bi_NT, (PPEvent)ev, g, h, U);
    }
    static uint64_t idHashOf(const RCPSPState_BI& st) {
        return (g_bi_strhash ? biFnv(biIdKey(st)) : biIdHash(st.s)) ^ (st.rev ? 0x9e3779b97f4a7c15ULL : 0ULL);
    }
    static uint64_t cutHashOf(const RCPSPState_BI& st) {
        if (g_bi_strhash || !g_meet_norm) return biFnv(biCutKey(st));
        return (st.rev && !g_bi_unfire) ? biCutHashRevTranslated(st.s) : biCutHashFwd(st.s);
    }
    uint64_t GetStateHash(const RCPSPState_BI& st) {
        ++g_hb_bihash_calls;
        if (!g_tt2_prof) return idHashOf(st);
        const auto t0 = std::chrono::steady_clock::now();
        const uint64_t h = idHashOf(st);
        g_hb_bihash_sec += secs_since(t0);
        return h;
    }
    uint64_t GetMeetHash(const RCPSPState_BI& st) {   // frame-independent: no side bit
        ++g_hb_meethash_calls;
        sides->use(!st.rev);                          // durations are read from the current net
        if (!g_tt2_prof) return cutHashOf(st);
        const auto t0 = std::chrono::steady_clock::now();
        const uint64_t h = cutHashOf(st);
        g_hb_meethash_sec += secs_since(t0);
        return h;
    }
    // BAE* relaxes the NODE's g (childData.g) but the stored STATE keeps its own short g,
    // set when it was generated. RCPSP_TT2::GetSuccessors derives each child's g from the
    // PARENT STATE's g, and DR5 dominates on s.g -- so once the two diverge, dominance is
    // comparing inflated costs and prunes states it must keep. TemplateAStar never showed
    // this because it does not relax the way BAE* does. Keep them in sync.
    static uint64_t gDesyncs;
    void SetStateG(RCPSPState_BI& st, double g) {
        if ((short)g != st.s.g) { ++gDesyncs; st.s.g = (short)g; }
    }
    // Activities started on the state's OWN net (active + finished). Read by BAE_RCPSP.h for the
    // maxDepthF / maxDepthB columns only.
    int Progress(const RCPSPState_BI& st) const {
        const int started = (int)st.s.activeTransitionIndices.size() + (int)st.s.finishedActivitiys.count()
                          - (st.s.finishedActivitiys.test(0) ? 1 : 0);
        return (g_bi_unfire && st.rev) ? g_bi_NT - started : started;
    }
    double GCost(const RCPSPState_BI& a, const RCPSPState_BI& b) {
        double d = (double)b.s.g - (double)a.s.g;
        return d < 0 ? 0 : d;                         // e==0 fires are legitimate zero-cost edges
    }
    // RCPSP_BI_HFUSE (default 0). TT2's HCost writes h into the state it scores; under TemplateAStar that lets
    // a zero-time child reuse its parent's h (predessesor_h). This wrapper scores the state and then RESTORES
    // h, which is the search HBAE has always run (stored states keep h = 0, so zero-time children, ~19% of
    // heuristic requests on j30, score h = 0) -- now without the two full state copies the old wrapper made.
    // HFUSE=1 keeps the written h: still optimal, but MORE expansions locally (j30 1:1 17k -> 34k, 1:4 93k ->
    // 133k, 3:9 2.2k -> 3.5k; likely the reused parent h makes f non-monotone along zero-cost edges).
    // RCPSP_BI_HFRESH (default 1): with h restored to 0 in stored states, the zero-delay shortcut in RCPSP_TT2::HCost
    // scored those children h = 0 (f = g) -- admissible but inconsistent (min f per side fell by 30-40 mid-run).
    // HFRESH sets g_tt2_hfresh for the instance, so every child gets a fresh h. HFRESH=0 restores the old search.
    static bool HFuseOn() { static const bool on = [](){ const char* e = std::getenv("RCPSP_BI_HFUSE");
                                                         return e ? (std::atoi(e) != 0) : false; }();
                            return on; }
    double HCost(const RCPSPState_BI& a, const RCPSPState_BI&) {
        sides->use(!a.rev);
        if (HFuseOn()) return env.HCost(a.s, a.s);
        const short h0 = a.s.h, nc0 = a.s.nextCritical;
        const double r = env.HCost(a.s, a.s);
        a.s.h = h0; a.s.nextCritical = nc0;
        return r;
    }
    bool GoalTest(const RCPSPState_BI& a, const RCPSPState_BI&) {
        int fin = 0; for (int i = 1; i <= g_bi_NT; i++) if (a.s.finishedActivitiys.test(i)) ++fin;
        return fin == g_bi_NT;
    }
    BiSides* sides;
    RCPSP_TT2 env;
};

// One heuristic object per direction. BAE* calls heuristic->HCost(n, target) on nodes of its
// own side, and reverse_heuristic->HCost(n, source) on those SAME nodes to get h_opp. The
// forward/backward distinction is carried by the state, so both delegate to the same code and
// the reverse one is the admissible "distance already travelled" bound (EST-based).
// BAE* uses EACH heuristic object in BOTH roles. For the forward side it calls
// forwardHeuristic as the own cost-to-go and backwardHeuristic as h_opp; for the backward
// side it calls backwardHeuristic as the own cost-to-go and forwardHeuristic as h_opp.
// So an object cannot be "the cost-to-go one" or "the h_opp one" -- it must DISPATCH ON THE
// STATE'S SIDE: own cost-to-go for its own side, h_opp for the other. Getting this wrong made
// the backward search use h_opp (distance ALREADY TRAVELLED, ~= g) as its cost-to-go, which
// grossly overestimates: on j30 3:9 an optimal-path node at g=35 with a true cost-to-go of 30
// was scored h=35, so f=70 >= U=66 and the node was discarded along with the whole remainder
// of the optimal path. The backward frontier then drained and certified a non-optimal 66.
uint64_t RCPSP_BiEnv::gDesyncs = 0;

class BiHeuristic : public Heuristic<RCPSPState_BI> {
public:
    // ownSideRev = the side for which THIS object supplies the own cost-to-go.
    BiHeuristic(RCPSP_BiEnv* e, bool ownSideRev, std::vector<int>* estF, std::vector<int>* estB)
        : env(e), ownRev(ownSideRev), eF(estF), eB(estB) {}
    double HCost(const RCPSPState_BI& a, const RCPSPState_BI& b) const override {
        static const bool zeroHopp = [](){ const char* e = std::getenv("RCPSP_BI_HOPP");
                                           return e && std::atoi(e) == 0; }();
        if (g_bi_unfire) {
            // Both sides hold FORWARD-frame states, so there are exactly two admissible estimates:
            // H_end = TT2 heuristic (time still to go), H_start = hOppOf on the forward net (time since
            // the start). Forward: h = H_end, h_opp = H_start. Un-fire backward: h = H_start, h_opp = H_end.
            const bool own = (a.rev == ownRev);
            if (!own && zeroHopp) return 0.0;
            const bool toEnd = own ? !a.rev : a.rev;
            if (toEnd) return const_cast<RCPSP_BiEnv*>(env)->HCost(a, b);
            env->sides->use(true);
            return (double)hOppOf(a.s, *eF, g_bi_NT);
        }
        if (a.rev == ownRev)                                    // own side => cost-to-go
            return const_cast<RCPSP_BiEnv*>(env)->HCost(a, b);
        if (zeroHopp) return 0.0;      // consistent by construction => bound stays valid
        env->sides->use(!a.rev);                                // other side => h_opp
        return (double)hOppOf(a.s, a.rev ? *eB : *eF, g_bi_NT);
    }
    RCPSP_BiEnv* env; bool ownRev; std::vector<int>* eF; std::vector<int>* eB;
};

// Per-instance HBAE telemetry for the benchmark writer.
// g_hb_proven is the one that matters: a drained queue under DOMINANCE cannot certify
// optimality (dominance discards states the opposite side needed to meet on), so those rows
// carry an upper bound only. See g_hb_stop.
static uint64_t g_hb_exp=0, g_hb_unique=0, g_hb_touched=0, g_hb_necessary=0;
static uint64_t g_hb_reopened=0, g_hb_solUpd=0;
static int      g_hb_mk=-1, g_hb_ub=-1, g_hb_pathLen=0;
static bool     g_hb_proven=false, g_hb_trivial=false;
static double   g_hb_time=0.0, g_hb_meetDepth=-1;
static const char* g_hb_stop = "none";
static const char* g_hb_meetAt = "none";
static int      g_hb_dr5mask=0, g_hb_dr4f=0, g_hb_batchf=0;   // ACTUAL flags used
static int      g_hb_nip=1, g_hb_noub=1, g_hb_fbound=1, g_hb_draincert=0;   // stop/certify switches
// Extra per-instance diagnostics (column list in runBenchmarkTT2HBAE). Side index 0 = forward, 1 = backward.
static uint64_t g_hb_expF=0, g_hb_expB=0, g_hb_openF=0, g_hb_openB=0, g_hb_itemsF=0, g_hb_itemsB=0, g_hb_meetIdxF=0, g_hb_meetIdxB=0;
static double   g_hb_timeF=0.0, g_hb_timeB=0.0, g_hb_minFF=-1, g_hb_minFB=-1;
static double   g_hb_minFAt[3][2] = {{-1, -1}, {-1, -1}, {-1, -1}};   // min open f at 60 / 120 / 240 s
static int64_t  g_hb_expAtMax=-1;
static double   g_hb_uAtMax=-1, g_hb_rssMB=0.0;
static uint64_t g_hb_domPrunedF=0, g_hb_domPrunedB=0, g_hb_domInsertsF=0, g_hb_domInsertsB=0;
// Where/when the frontiers met (the FINAL incumbent unless named first*) and how deep each side got.
static int64_t  g_hb_meetExp=-1, g_hb_firstMeetExp=-1;
static double   g_hb_firstU=-1, g_hb_meetGF=-1, g_hb_meetGB=-1, g_hb_meetTimeFrac=-1;
static const char* g_hb_meetSide = "none";
static uint64_t g_hb_meetHits=0, g_hb_fBoundCalls=0;
static double   g_hb_maxDepthF=-1, g_hb_maxDepthB=-1, g_hb_maxGF=-1, g_hb_maxGB=-1, g_hb_fBoundSec=0.0;
static int      g_hb_minfHeap=1;
static int      g_hb_hfuse=1, g_hb_strhash=0;
static int      g_hb_hfresh=1;                       // RCPSP_BI_HFRESH (CSV hFresh)
static uint64_t g_hb_fDropF=0, g_hb_fDropB=0;        // children with lower f than their parent, per side (CSV fDropF/fDropB)
static double   g_hb_fDropMax=0;                     // largest such drop (CSV fDropMax)
static double   g_hb_bx[5] = {0, 0, 0, 0, 0};   // Expand split: pop, successors, child loop, heuristic calls, add to OPEN
static int      g_hb_domMeet=0, g_hb_meetGhost=0;
static uint64_t g_hb_dmUpdates=0, g_hb_dmInserts=0, g_hb_dmSkipped=0, g_hb_dmThinned=0, g_hb_dmJoinCmp=0, g_hb_dmEntF=0, g_hb_dmEntB=0;
static int      g_hb_shiftMeet=0, g_hb_stopMax=0, g_hb_maxStop=0;
static uint64_t g_hb_dmShiftBest=0;
// explicit-wait model (RCPSP_TT2_WAIT) and the meeting audit (RCPSP_BI_AUDIT); filled by solveRCPSP_TT2_HBAE
static int      g_hb_dr4side=0;
static uint64_t g_hb_waitExemptF=0, g_hb_waitExemptB=0;
static const char* g_hb_auditStatus = "off";
static int      g_hb_auditHorizon=-1, g_hb_auditBoth=-1, g_hb_auditFwd=-1, g_hb_auditBwd=-1, g_hb_auditBothDom=-1, g_hb_auditMissed=-1;
static int64_t  g_hb_auditCrossExp=-1, g_hb_auditMeetExp=-1;
static int      g_hb_auditFwdMaxT=-1, g_hb_auditBwdMinT=-1, g_hb_auditFwdHoles=-1, g_hb_auditBwdHoles=-1;
int solveRCPSP_TT2_HBAE(int group, int exam, const std::string& ptype = "j30") {
    std::cout << "=== HOG2 BAE* " << group << ":" << exam << " ===" << std::endl;
    { const char* bw = std::getenv("RCPSP_BI_BWD");
      g_bi_unfire = bw && (std::string(bw) == "unfire" || std::string(bw) == "1"); }
    getPetri(petri, group, exam, ptype); getRCPSP(RCPSPex, group, exam, ptype);
    g_bi_NT = (int)petri.Transitions.size();
    { static const double RSL[4]={0.2,0.5,0.7,1.0}; g_instance_rs=RSL[((group-1)%4+4)%4]; }
    g_instance_deadline = std::chrono::steady_clock::now()+std::chrono::seconds(astar_timeout_seconds);
    g_instance_deadline_set = true;
    if (g_tt2_singleres || g_tt2_hierrs) { g_tt2_singleres=false; g_tt2_hierrs=false; }
    if (g_h_cache || g_subsolve_succ_cache) { g_h_cache=false; g_subsolve_succ_cache=false; }
    get_tt2_cache().clear();
    // per-instance diagnostics: the TT2 phase timers are shared with the unidirectional solver
    g_prof_succ_sec = g_prof_avail_sec = g_prof_gen_sec = g_prof_dr5_sec = g_prof_hash_sec = 0.0;
    g_prof_hcp_sec = g_prof_hres_sec = g_prof_astar_sec = 0.0;
    g_prof_succ_calls = g_prof_hash_calls = g_prof_children_raw = g_prof_children_kept = 0;
    g_prof_h_calls_all = g_prof_h_known = g_prof_h_deltazero = 0; g_prof_maxf = -1;
    g_prof_t0 = std::chrono::steady_clock::now(); g_prof_next_trace = 5.0;
    g_heur_time_sec = 0.0; g_heur_calls = 0; g_tt2_dr4_pruned = 0;
    g_hb_bihash_sec = g_hb_meethash_sec = 0.0; g_hb_bihash_calls = g_hb_meethash_calls = 0;
    g_hb_expF = g_hb_expB = g_hb_openF = g_hb_openB = g_hb_itemsF = g_hb_itemsB = g_hb_meetIdxF = g_hb_meetIdxB = 0;
    g_hb_timeF = g_hb_timeB = 0.0; g_hb_minFF = g_hb_minFB = -1;
    for (auto& r : g_hb_minFAt) { r[0] = -1; r[1] = -1; }
    g_hb_expAtMax = -1; g_hb_uAtMax = -1; g_hb_rssMB = 0.0;
    g_hb_domPrunedF = g_hb_domPrunedB = g_hb_domInsertsF = g_hb_domInsertsB = 0;
    g_hb_meetExp = g_hb_firstMeetExp = -1; g_hb_firstU = g_hb_meetGF = g_hb_meetGB = g_hb_meetTimeFrac = -1;
    g_hb_meetSide = "none"; g_hb_meetHits = g_hb_fBoundCalls = 0; g_hb_fBoundSec = 0.0;
    g_hb_maxDepthF = g_hb_maxDepthB = g_hb_maxGF = g_hb_maxGB = -1;
    g_hb_minfHeap = BAE_RCPSP<RCPSPState_BI, int, RCPSP_BiEnv>::MinFHeapOn() ? 1 : 0;
    g_hb_hfuse = RCPSP_BiEnv::HFuseOn() ? 1 : 0; g_hb_strhash = g_bi_strhash ? 1 : 0;
    { const char* e = std::getenv("RCPSP_BI_HFRESH"); g_hb_hfresh = (e && std::atoi(e) == 0) ? 0 : 1; }
    g_hb_fDropF = g_hb_fDropB = 0; g_hb_fDropMax = 0;
    struct HFreshGuard { bool saved; explicit HFreshGuard(bool on) : saved(g_tt2_hfresh) { g_tt2_hfresh = on; }
                         ~HFreshGuard() { g_tt2_hfresh = saved; } } hfreshGuard(g_hb_hfresh != 0);
    for (double& x : g_hb_bx) x = 0.0;
    g_hb_domMeet = BAE_RCPSP<RCPSPState_BI, int, RCPSP_BiEnv>::DomMeetOn() ? 1 : 0;
    { const char* e = std::getenv("RCPSP_BI_MEET_GHOST"); g_hb_meetGhost = (g_hb_domMeet && e && std::atoi(e) != 0) ? 1 : 0; }
    g_hb_dmUpdates = g_hb_dmInserts = g_hb_dmSkipped = g_hb_dmThinned = g_hb_dmJoinCmp = g_hb_dmEntF = g_hb_dmEntB = 0;
    g_hb_shiftMeet = (g_hb_domMeet && RCPSP_BiEnv::ShiftMeetOn()) ? 1 : 0; g_hb_dmShiftBest = 0;
    { const char* e = std::getenv("RCPSP_BI_STOP"); g_hb_stopMax = (e && std::string(e) == "max") ? 1 : 0; } g_hb_maxStop = 0;
    g_tt2_wait_edges = 0; g_tt2_wait_cut = 0; g_tt2_wait_pass = 0; g_tt2_wait_jumped = 0; g_tt2_wait_meetonly = 0; g_hb_waitExemptF = g_hb_waitExemptB = 0;
    g_hb_auditStatus = BAE_RCPSP<RCPSPState_BI, int, RCPSP_BiEnv>::AuditOn() ? "trivial" : "off";
    g_hb_auditHorizon = g_hb_auditBoth = g_hb_auditFwd = g_hb_auditBwd = g_hb_auditBothDom = g_hb_auditMissed = -1;
    g_hb_auditCrossExp = g_hb_auditMeetExp = -1;
    g_hb_auditFwdMaxT = g_hb_auditBwdMinT = g_hb_auditFwdHoles = g_hb_auditBwdHoles = -1;
    rootb::reset();   // RootBounds.h values are per instance (also on the trivial-root row)
    { std::vector<int> te; int tm=0;
      if (tryTrivialRootSchedule_TT2(te, tm)) {
          std::cout << "HBAE " << group << ":" << exam << " makespan=" << tm
                    << " expanded=0 trivialAtRoot=1" << std::endl;
          g_hb_exp=g_hb_unique=g_hb_touched=g_hb_necessary=0; g_hb_reopened=g_hb_solUpd=0;
          g_hb_mk=g_hb_ub=tm; g_hb_pathLen=0; g_hb_proven=true; g_hb_trivial=true;
          g_hb_time=0.0; g_hb_meetDepth=-1; g_hb_stop="trivial"; g_hb_meetAt="none";
          return tm; } }
    tt2nb::reset();                 // per-instance counters + instance cache (HBAE alternates nets)
    // RCPSP_ROOT_DLB / RCPSP_ROOT_SEED (RootBounds.h), computed on the forward instance before the nets are swapped
    if (rootb::computeRootBounds()) {
        const int tm = rootb::info().seed;
        std::cout << "HBAE " << group << ":" << exam << " makespan=" << tm << " expanded=0 closedAtRoot=1 (seed <= root LB "
                  << std::max(rootb::info().lb, rootb::info().lbPlain) << ")" << std::endl;
        g_hb_exp=g_hb_unique=g_hb_touched=g_hb_necessary=0; g_hb_reopened=g_hb_solUpd=0;
        g_hb_mk=g_hb_ub=tm; g_hb_pathLen=0; g_hb_proven=true; g_hb_trivial=false;
        g_hb_time=rootb::seconds(); g_hb_meetDepth=-1; g_hb_stop="rootclosed"; g_hb_meetAt="none";
        return tm;
    }
    bool sDR5=g_tt2_dr5, sDR4=g_tt2_dr4, sB=g_tt2_batch;
    int dr5MaskCfg = 3; if (const char* d5=std::getenv("RCPSP_BI_DR5")) dr5MaskCfg = std::atoi(d5);
    if (g_bi_unfire) dr5MaskCfg &= 1;   // the forward DR5 rule is unsound on un-fire states (see BWMEET)
    g_tt2_dr5 = (dr5MaskCfg != 0);
    { const char* b4=std::getenv("RCPSP_BI_DR4"); g_tt2_dr4 = b4 ? (std::atoi(b4)!=0) : false; }
    g_hb_dr5mask = dr5MaskCfg; g_hb_dr4f = g_tt2_dr4 ? 1 : 0;
    // RCPSP_BI_DR4SIDE = both (default) | fwd | bwd: the side(s) DR4 applies to (BiSides::use). 0 when DR4 is off.
    { const char* ds = std::getenv("RCPSP_BI_DR4SIDE"); const std::string v = ds ? ds : "both";
      g_hb_dr4side = !g_tt2_dr4 ? 0 : (v == "fwd") ? 1 : (v == "bwd") ? 2 : 3; }
    g_tt2_wait_edges = 0; g_tt2_wait_cut = 0;
    { const char* bb=std::getenv("RCPSP_BI_BATCH"); g_tt2_batch = bb ? (std::atoi(bb)!=0) : false; }
    g_hb_batchf = g_tt2_batch ? 1 : 0;
    g_tt2_dom_side=0; get_tt2_dominance_table().clear();
    const uint64_t domP0F = (uint64_t)get_tt2_dominance_table().pruned, domI0F = (uint64_t)get_tt2_dominance_table().inserts;
    g_tt2_dom_side=1; get_tt2_dominance_table().clear();
    const uint64_t domP0B = (uint64_t)get_tt2_dominance_table().pruned, domI0B = (uint64_t)get_tt2_dominance_table().inserts;

    BiSides sides;
    sides.dr5Mask = dr5MaskCfg;
    sides.dr4On = (g_hb_dr4side != 0); sides.dr4Mask = g_hb_dr4side;
    sides.petriStash = petri; sides.rcpspStash = RCPSPex;
    deriveEndpointNames(sides.initF, sides.finalF);      // globals currently hold the FORWARD net
    reverseLoadedInstance();
    deriveEndpointNames(sides.initB, sides.finalB);      // ... and now the REVERSED one
    std::swap(petri, sides.petriStash); std::swap(RCPSPex, sides.rcpspStash);  // global = forward
    sides.curFwd = true;
    if (g_bi_unfire) {
        sides.use(true);
        BWMEET::precompute();
        { const char* fv = std::getenv("RCPSP_BWD_FULLVERIFY");       // default ON: every un-fire edge is
          BWMEET::g_bwd_fullverify = fv ? (std::atoi(fv) != 0) : true; } // re-fired and must reproduce the state
        BWMEET::g_bwd_enum = false; BWMEET::g_bwd_dr = false;
    }

    sides.use(true);  std::vector<int> estF = computeESTforCurrentNet();
    sides.use(false); std::vector<int> estB = computeESTforCurrentNet();

    RCPSP_BiEnv benv(&sides);
    sides.use(true);  RCPSPState_TT2 rf; rf.g = 0; RCPSPState_BI start(rf, false);
    sides.use(false); RCPSPState_TT2 rb; rb.g = 0;
    if (g_bi_unfire) {                     // backward root = the forward GOAL marking: all finished, sink token
        for (int i = 1; i <= BWMEET::NT; ++i) rb.finishedActivitiys.set(i);
        for (auto& an : rb.activity_nodes) an = {0,0};
        if (BWMEET::g_sinkOutIdx >= 0) rb.activity_nodes[BWMEET::g_sinkOutIdx] = {1,0};
        rb.activeTransitionIndices.clear();
        BWMEET::deriveResources(rb.activeTransitionIndices, rb.resource_nodes);
        rb.transitionsCached = false; rb.AvailableTransitionIndices_TT2.clear(); rb.hKnown = false; rb.g = 0;
    }
    RCPSPState_BI goal (rb, true);

    // Each object serves BOTH roles, selected by the state's side (see BiHeuristic).
    BiHeuristic hSideF(&benv, false, &estF, &estB);  // cost-to-go for FORWARD states, h_opp for backward
    BiHeuristic hSideB(&benv, true,  &estF, &estB);  // cost-to-go for BACKWARD states, h_opp for forward

    // epsilon = 0: e==0 fires make zero-cost edges legal here. gcd = 1: costs are integers.
    // RCPSP_BI_ALT=1 strict alternation (original BAE*), =0 Pohl cardinality (default):
    // alternation drags the cheap side up to match the expensive one, which hurts whenever
    // the two directions are asymmetric.
    int altMode = 0; if (const char* am=std::getenv("RCPSP_BI_ALT")) altMode = std::atoi(am);
    BAE_RCPSP<RCPSPState_BI, int, RCPSP_BiEnv> bae(altMode != 0, 0.0, 1.0);
    std::vector<RCPSPState_BI> path;
    // RCPSP_BI_MEET_GHOST (needs RCPSP_BI_DOMMEET): states DR5 prunes are real states reached by a real path, so
    // they are offered to the dominance-meet tables too (their g is an actual path cost). They are never expanded.
    if (g_hb_meetGhost) {
        g_tt2_dr_prune_observer = [&](const RCPSPState_TT2& s) {
            const bool rev = (g_tt2_dom_side == 1);
            RCPSPState_BI gs(s, rev);
            const double c = benv.DomMeet(gs, (double)s.g, bae.GetSolutionCost());
            if (c < bae.GetSolutionCost()) bae.OfferSolution(c, gs, !rev, (double)s.g);
        };
    }
    bae.SetRootBounds(rootb::info().seed > 0 ? (double)rootb::info().seed : DBL_MAX,
                      rootb::DlbOn() ? (double)std::max(rootb::info().lb, rootb::info().lbPlain) : 0.0);
    const auto rootSavedTO = astar_timeout_seconds;   // root bounds already used part of the per-instance budget
    if (rootb::seconds() > 0) astar_timeout_seconds = std::max<decltype(rootSavedTO)>(1, rootSavedTO - (decltype(rootSavedTO))std::ceil(rootb::seconds()));
    auto t0 = std::chrono::high_resolution_clock::now();
    bae.GetPath(&benv, start, goal, &hSideF, &hSideB, path);
    auto t1 = std::chrono::high_resolution_clock::now();
    astar_timeout_seconds = rootSavedTO;
    g_tt2_dr_prune_observer = nullptr;
    std::chrono::duration<double> el = t1 - t0;

    double cost = bae.GetSolutionCost();
    int ub = (cost >= DBL_MAX/2) ? -1 : (int)cost;
    // A drained queue proves optimality when nothing unsound was removed from THAT side.
    // DR5 is per-side (bit0 forward, bit1 reversed); DR4 is global, so it taints both.
    const int drained = bae.GetDrainedSide();
    // A DRAIN (one queue empty) certifies optimality only when that side provably exhausted a
    // search that must contain an optimal path ending at its goal, whose meet with the opposite
    // ROOT (indexed at initialisation) records U <= C*. That needs:
    //  * lazy nipping OFF (RCPSP_BI_NIP=0): nipping closes a node WITHOUT expanding it on a
    //    many-to-one cut-key match with an opposite state that need not be the cheapest for that
    //    cut yet -- so a side can lose its optimal path and still drain;
    //  * no discard (RCPSP_BI_NOUB, default on);
    //  * that side's dominance off, OR RCPSP_BI_DRAINCERT=1: DR5/DR4/batch are sound for a side's
    //    own search, so an optimal path survives in it. Retrospective (nipping on): all 367
    //    drained-unproven rows in four j30 gates had ub == C*.
    // TIGHTENED 2026-09-10: a dominance-free drain used to certify even with nipping on.
    static const bool nipOffC = [](){ const char* e = std::getenv("RCPSP_BI_NIP");
                                      return e && std::atoi(e) == 0; }();
    static const bool noubC   = [](){ const char* e = std::getenv("RCPSP_BI_NOUB");
                                      return e ? (std::atoi(e) != 0) : true; }();
    static const bool drainC  = [](){ const char* e = std::getenv("RCPSP_BI_DRAINCERT");
                                      return e && std::atoi(e) != 0; }();
    const bool drainedSideClean = nipOffC && noubC && !(g_bi_unfire && drained == 2) &&
        ((drained == 1 && ((!(dr5MaskCfg & 1) && !(g_hb_dr4side & 1)) || drainC)) ||
         (drained == 2 && ((!(dr5MaskCfg & 2) && !(g_hb_dr4side & 2)) || drainC)));
    const bool proven = (ub >= 0) && (bae.StoppedByBound() || drainedSideClean);
    int mk = proven ? ub : -1;
    double meetDepth = -1; const char* meetWhere = "none";
    if (ub >= 0 && bae.GetMeetFromFwd() < 0) meetWhere = "seed";   // RCPSP_ROOT_SEED incumbent never improved by a meet
    if (ub >= 0 && bae.GetMeetFromFwd() >= 0) {   // an incumbent from a meet
        const RCPSPState_BI& m = bae.GetMiddleNode();
        int started;
        if (!m.rev || g_bi_unfire) { started = (int)m.s.activeTransitionIndices.size();
                      for (int q = 1; q <= g_bi_NT; q++) if (m.s.finishedActivitiys.test(q)) ++started; }
        else        { int rf = 0; for (int q = 1; q <= g_bi_NT; q++) if (m.s.finishedActivitiys.test(q)) ++rf;
                      started = g_bi_NT - rf; }
        meetDepth = (double)started / (double)g_bi_NT;
        meetWhere = (started <= 1) ? "AT-SOURCE" : (started >= g_bi_NT - 1) ? "AT-GOAL" : "MIDDLE";
    }
    // where and when the frontiers met, and how deep each side got (see BAE_RCPSP.h)
    g_hb_meetExp = bae.GetMeetExp(); g_hb_firstMeetExp = bae.GetFirstMeetExp(); g_hb_firstU = bae.GetFirstU();
    g_hb_meetHits = bae.GetMeetHits();
    g_hb_meetSide = (ub < 0 || bae.GetMeetFromFwd() < 0) ? "none" : (bae.GetMeetFromFwd() ? "fwd" : "bwd");
    g_hb_meetGF = bae.GetMeetGF(); g_hb_meetGB = bae.GetMeetGB();
    g_hb_meetTimeFrac = (ub > 0 && g_hb_meetGF >= 0) ? g_hb_meetGF / (double)ub : -1;
    g_hb_maxDepthF = bae.GetMaxProg(true)  >= 0 ? (double)bae.GetMaxProg(true)  / (double)g_bi_NT : -1;
    g_hb_maxDepthB = bae.GetMaxProg(false) >= 0 ? (double)bae.GetMaxProg(false) / (double)g_bi_NT : -1;
    g_hb_maxGF = bae.GetMaxG(true); g_hb_maxGB = bae.GetMaxG(false);
    g_hb_fBoundCalls = bae.GetFBoundCalls(); g_hb_fBoundSec = bae.GetFBoundSec();
    for (int i = 0; i < 5; ++i) g_hb_bx[i] = bae.GetExpandSplit(i);
    g_hb_dmUpdates = bae.GetDomMeetUpdates(); g_hb_dmInserts = benv.dmInserts; g_hb_dmSkipped = benv.dmSkipped;
    g_hb_dmThinned = benv.dmThinned; g_hb_dmJoinCmp = benv.dmJoinCmp;
    if (g_hb_domMeet) { g_hb_dmEntF = benv.DomMeetEntries(0); g_hb_dmEntB = benv.DomMeetEntries(1); }
    g_hb_dmShiftBest = benv.dmShiftBest;
    g_hb_maxStop = bae.StoppedByMax() ? 1 : 0;
    // DR5 wait-chain exemptions per side (explicit-wait model)
    g_tt2_dom_side=0; g_hb_waitExemptF = get_tt2_dominance_table().waitExempt;
    g_tt2_dom_side=1; g_hb_waitExemptB = get_tt2_dominance_table().waitExempt;
    g_tt2_dom_side=0;
    // ── MEETING AUDIT (RCPSP_BI_AUDIT=1) ─────────────────────────────────────
    // Rebuild the schedule S of the best exact meet (the forward path to one node, the reversed path to the other),
    // check that S is feasible with makespan C, then for every integer t in [0, C] look up S's cut at t (what started
    // before t; running activities with their remaining time) in both sides' meet indexes, which keep the best-g node
    // per cut:
    //   auditFwdExact / auditBwdExact / auditBothExact  t with a node of that cut at g = t (forward), g = C - t (reversed)
    //   auditCrossExp   the first expansion at which BOTH sides held S's cut at the same t
    //   auditMeetExp    the expansion at which the incumbent reached C
    //   auditMissed     1 if auditMeetExp > auditCrossExp: both sides held a common cut of S and no meet was recorded
    //                   when the second one appeared (what the explicit-wait model must never show)
    //   auditBothDom    t whose scheduled set holds a forward and a reversed dominance-meet entry joining at <= C
    // Status INCONSISTENT / INFEASIBLE means the meet produced something that is not a schedule of cost C (a bug).
    if (BAE_RCPSP<RCPSPState_BI, int, RCPSP_BiEnv>::AuditOn()) {
        uint64_t fid = 0, bid = 0; double ec = DBL_MAX; int shift = 0; const int C = (int)cost;
        if (!proven) g_hb_auditStatus = "unproven";
        else if (g_bi_unfire) g_hb_auditStatus = "unfire";
        else if (!bae.GetExactMeet(fid, bid, shift, ec) || ec != cost) g_hb_auditStatus = "nojoin";   // e.g. found by a DR5-pruned state
        else {
            const int NT = g_bi_NT;
            sides.use(true);
            std::vector<int> dur(NT + 1, 0);
            for (int i = 1; i <= NT; ++i) dur[i] = RCPSPex.activities[i - 1].duration;
            std::vector<int> st(NT + 1, INT_MIN);
            const auto setStart = [&](int id, int s) { if (id >= 1 && id <= NT && st[id] == INT_MIN) st[id] = s; };
            std::vector<std::pair<RCPSPState_BI, double>> pth;
            // The join runs the forward path up to node fid (time T), then the reversed suffix of node bid (length GB)
            // shifted `shift` later. The forward side places what its node started before T (finished, or running and
            // not started exactly at T: the cut key counts that as not started); the suffix places everything else.
            bae.PathFromID(true, fid, pth);
            const int T = (int)pth.back().second;
            std::vector<char> own(NT + 1, 0);
            { const auto& xs = pth.back().first.s;
              for (int i = 1; i <= NT; ++i) if (xs.finishedActivitiys.test(i)) own[i] = 1;
              for (const auto& a : xs.activeTransitionIndices) if (a.first >= 1 && a.first <= NT && a.second != dur[a.first]) own[a.first] = 1; }
            for (const auto& pr : pth) {        // running (id, r) at node time g started at g - (p - r)
                const int gg = (int)pr.second;
                for (const auto& a : pr.first.s.activeTransitionIndices)
                    if (a.first >= 1 && a.first <= NT && own[a.first]) setStart(a.first, gg - (dur[a.first] - a.second));
                for (int i = 1; i <= NT; ++i) if (dur[i] == 0 && own[i] && pr.first.s.finishedActivitiys.test(i)) setStart(i, gg);
            }
            bae.PathFromID(false, bid, pth);
            const int GB = (int)pth.back().second;
            for (const auto& pr : pth) {        // reversed clock: started at sigma = tau - (p - r), which on the forward
                const int gg = (int)pr.second;  // clock is a start at T + shift + GB - sigma - p
                for (const auto& a : pr.first.s.activeTransitionIndices)
                    if (a.first >= 1 && a.first <= NT && !own[a.first]) setStart(a.first, T + shift + GB - (gg - (dur[a.first] - a.second)) - dur[a.first]);
                for (int i = 1; i <= NT; ++i) if (dur[i] == 0 && !own[i] && pr.first.s.finishedActivitiys.test(i)) setStart(i, T + shift + GB - gg);
            }
            bool complete = true;
            for (int i = 1; i <= NT; ++i) if (st[i] == INT_MIN) complete = false;
            bool feasible = complete && (T + shift + GB == C);
            if (feasible) {
                int mkS = 0;
                for (int i = 1; i <= NT; ++i) { if (st[i] < 0) feasible = false; mkS = std::max(mkS, st[i] + dur[i]); }
                if (mkS != C) feasible = false;
                for (int a = 1; a <= NT && feasible; ++a)
                    for (int p : RCPSPex.backword_dependencies[a - 1])
                        if (p >= 1 && p <= NT && st[p] + dur[p] > st[a]) { feasible = false; break; }
                for (const auto& [resName, cap] : RCPSPex.resources) {
                    if (!feasible) break;
                    std::vector<int> use(C + 1, 0);
                    for (int a = 1; a <= NT; ++a)
                        for (const auto& [rn, dem] : RCPSPex.activities[a - 1].resource_demands)
                            if (rn == resName && dem > 0)
                                for (int t = std::max(0, st[a]); t < st[a] + dur[a] && t <= C; ++t) use[t] += dem;
                    for (int t = 0; t <= C; ++t) if (use[t] > cap) { feasible = false; break; }
                }
            }
            if (!complete) g_hb_auditStatus = "incomplete";
            else if (!feasible) g_hb_auditStatus = "INFEASIBLE";
            else {
                g_hb_auditStatus = "ok";
                g_hb_auditHorizon = C + 1; g_hb_auditFwd = g_hb_auditBwd = g_hb_auditBoth = g_hb_auditBothDom = 0;
                g_hb_auditMeetExp = g_hb_meetExp;
                for (int t = 0; t <= C; ++t) {
                    bool hasF = false, hasB = false; int64_t eF = -1, eB = -1;
                    std::bitset<128> sched;
                    for (int variant = 0; variant < 2; ++variant) {   // 1: zero-duration activities starting at t count as finished
                        std::bitset<128> fin; std::pair<short, short> av[128]; int n = 0; bool zeroAt = false;
                        for (int i = 1; i <= NT; ++i) {
                            const int s = st[i], e = st[i] + dur[i];
                            if (dur[i] == 0 && s == t) { zeroAt = true; if (variant == 1) fin.set(i); continue; }
                            if (s < t && e <= t) fin.set(i);
                            else if (s < t && t < e && n < 128) av[n++] = {(short)i, (short)(e - t)};
                        }
                        if (variant == 1 && !zeroAt) break;
                        if (variant == 0) { sched = fin; for (int k = 0; k < n; ++k) sched.set(av[k].first); }
                        const uint64_t h = biHashCut(fin & biFinMask(), av, n);
                        double gg; int64_t ee;
                        if (bae.MeetNode(true, h, gg, ee) && (int)gg == t && (!hasF || ee < eF)) { hasF = true; eF = ee; }
                        if (bae.MeetNode(false, h, gg, ee) && (int)gg == C - t && (!hasB || ee < eB)) { hasB = true; eB = ee; }
                    }
                    if (hasF) { ++g_hb_auditFwd; g_hb_auditFwdMaxT = t; }
                    if (hasB) { ++g_hb_auditBwd; if (g_hb_auditBwdMinT < 0) g_hb_auditBwdMinT = t; }
                    if (hasF && hasB) {
                        ++g_hb_auditBoth;
                        const int64_t both = std::max(eF, eB);
                        if (g_hb_auditCrossExp < 0 || both < g_hb_auditCrossExp) g_hb_auditCrossExp = both;
                    }
                    if (g_hb_domMeet) {
                        static const std::bitset<128> low64(~0ULL);
                        RCPSP_BiEnv::DmKey key; key.lo = (sched & low64).to_ullong(); key.hi = (sched >> 64).to_ullong();
                        auto itF = benv.dmTab[0].find(key), itB = benv.dmTab[1].find(key);
                        bool joined = false;
                        if (itF != benv.dmTab[0].end() && itB != benv.dmTab[1].end())
                            for (const auto& ef : itF->second) {
                                for (const auto& eb : itB->second)
                                    if ((int)ef.g + (int)eb.g + RCPSP_BiEnv::shiftOver(ef.n, ef.ids(), ef.rels(), eb.n, eb.ids(), eb.rels()) <= C) { joined = true; break; }
                                if (joined) break;
                            }
                        if (joined) ++g_hb_auditBothDom;
                    }
                }
                g_hb_auditMissed = (g_hb_auditCrossExp >= 0 && g_hb_auditMeetExp > g_hb_auditCrossExp) ? 1 : 0;
                g_hb_auditFwdHoles = (g_hb_auditFwdMaxT >= 0) ? (g_hb_auditFwdMaxT + 1 - g_hb_auditFwd) : 0;
                g_hb_auditBwdHoles = (g_hb_auditBwdMinT >= 0) ? (C - g_hb_auditBwdMinT + 1 - g_hb_auditBwd) : 0;
            }
        }
        std::cout << "AUDIT " << group << ":" << exam << " status=" << g_hb_auditStatus << " C=" << C << " joinShift=" << shift
                  << " fwdExact=" << g_hb_auditFwd << " bwdExact=" << g_hb_auditBwd << " bothExact=" << g_hb_auditBoth
                  << " crossExp=" << g_hb_auditCrossExp << " meetExp=" << g_hb_auditMeetExp
                  << " missed=" << g_hb_auditMissed << " bothDom=" << g_hb_auditBothDom
                  << " fwdMaxT=" << g_hb_auditFwdMaxT << " bwdMinT=" << g_hb_auditBwdMinT
                  << " fwdHoles=" << g_hb_auditFwdHoles << " bwdHoles=" << g_hb_auditBwdHoles << std::endl;
        if (g_hb_auditMissed == 1) std::cout << "!! AUDIT MISSED MEET " << group << ":" << exam << std::endl;
    }
    std::cout << "HBAE " << group << ":" << exam << " makespan=" << mk
              << " ub=" << ub << " proven=" << (proven ? 1 : 0)
              << " stop=" << (bae.StoppedByBound() ? "bound"
                              : drained == 1 ? "fwd-drained"
                              : drained == 2 ? "bwd-drained" : "deadline")
              << " expanded=" << bae.GetNodesExpanded()
              << " unique=" << bae.GetUniqueNodesExpanded()
              << " touched=" << bae.GetNodesTouched()
              << " necessary=" << bae.GetNecessaryExpansions()
              << " pathLen=" << path.size()
              << " reopened=" << bae.GetReopenedNodes()
              << " fDrop=" << bae.GetFDrop(true) << "/" << bae.GetFDrop(false) << " fDropMax=" << bae.GetFDropMax() << " hfresh=" << g_hb_hfresh
              << " solUpdates=" << bae.GetSolutionUpdates()
              << " meetDepth=" << meetDepth << " meetAt=" << meetWhere
              << " meetSide=" << g_hb_meetSide << " gF=" << g_hb_meetGF << " gB=" << g_hb_meetGB
              << " meetExp=" << g_hb_meetExp << " maxDepthF=" << g_hb_maxDepthF << " maxDepthB=" << g_hb_maxDepthB
              << " dmUpdates=" << bae.GetDomMeetUpdates() << " stopRule=" << (g_hb_stopMax ? "max" : "avg")
              << " maxStop=" << (bae.StoppedByMax() ? 1 : 0) << " minfHeap=" << g_hb_minfHeap << " fBoundCalls=" << g_hb_fBoundCalls << " fBoundSec=" << g_hb_fBoundSec
              << " bwd=" << (g_bi_unfire ? "unfire" : "reversed")
              << " alt=" << altMode
              << " batch=" << (g_tt2_batch?1:0) << " dr4=" << g_hb_dr4f << " dr4side=" << g_hb_dr4side << " wait=" << (g_tt2_wait?1:0)
              << " time=" << el.count() << "s" << std::endl;
    g_hb_exp=bae.GetNodesExpanded(); g_hb_unique=bae.GetUniqueNodesExpanded();
    g_hb_touched=bae.GetNodesTouched(); g_hb_necessary=bae.GetNecessaryExpansions();
    g_hb_reopened=bae.GetReopenedNodes(); g_hb_solUpd=bae.GetSolutionUpdates();
    g_hb_fDropF=bae.GetFDrop(true); g_hb_fDropB=bae.GetFDrop(false); g_hb_fDropMax=bae.GetFDropMax();
    g_hb_mk=mk; g_hb_ub=ub; g_hb_pathLen=(int)path.size(); g_hb_proven=proven;
    g_hb_trivial=false; g_hb_time=el.count() + rootb::seconds(); g_hb_meetDepth=meetDepth; g_hb_meetAt=meetWhere;
    g_hb_stop = bae.StoppedByFloor() ? "rootfloor" : bae.StoppedByBound() ? "bound"
              : drained == 1 ? "fwd-drained"
              : drained == 2 ? "bwd-drained" : "deadline";
    g_hb_expF = bae.GetExpanded(true);       g_hb_expB = bae.GetExpanded(false);
    g_hb_timeF = bae.GetSideSec(true);       g_hb_timeB = bae.GetSideSec(false);
    g_hb_openF = bae.GetOpenCount(true);     g_hb_openB = bae.GetOpenCount(false);
    g_hb_itemsF = bae.GetItemCount(true);    g_hb_itemsB = bae.GetItemCount(false);
    g_hb_meetIdxF = bae.GetMeetIndexSize(true); g_hb_meetIdxB = bae.GetMeetIndexSize(false);
    g_hb_minFF = bae.GetMinFOpenNow(true);   g_hb_minFB = bae.GetMinFOpenNow(false);
    for (int i = 0; i < 3; i++) { g_hb_minFAt[i][0] = bae.GetMinFAt(i, true); g_hb_minFAt[i][1] = bae.GetMinFAt(i, false); }
    g_hb_expAtMax = bae.GetExpAtMaxBound();  g_hb_uAtMax = bae.GetUAtMaxBound();
    g_hb_rssMB = (double)std::max(bae.GetPeakRSSKB(), bae_rcpsp_rss_kb()) / 1024.0;
    g_tt2_dom_side=0; g_hb_domPrunedF = (uint64_t)get_tt2_dominance_table().pruned - domP0F; g_hb_domInsertsF = (uint64_t)get_tt2_dominance_table().inserts - domI0F;
    g_tt2_dom_side=1; g_hb_domPrunedB = (uint64_t)get_tt2_dominance_table().pruned - domP0B; g_hb_domInsertsB = (uint64_t)get_tt2_dominance_table().inserts - domI0B;
    g_tt2_dom_side=0;
    g_tt2_dr5=sDR5; g_tt2_dr4=sDR4; g_tt2_batch=sB;
    return mk;
}

// ── Does BAE2's successor generation match the UNIDIRECTIONAL searches? ───────
// The concern is real: BAE2 keeps BOTH nets alive in the same globals and std::swaps
// between them, so a stale global would silently give one side a different graph from
// the one tt2one / tt2rev would search. This compares the two directly: build the same
// root three ways (standalone forward, standalone reversed, and BAE2's stash+swap) and
// compare the SET of states reachable within k fires, keyed by (marking, g). DR is
// forced OFF so we compare raw generation, not table history.
static std::set<std::string> biReach(RCPSP_TT2& env, const RCPSPState_TT2& root, int NT, int depth) {
    std::set<std::string> seen;
    std::vector<RCPSPState_TT2> frontier{root};
    for (int d = 0; d < depth; ++d) {
        std::vector<RCPSPState_TT2> next;
        for (auto& s : frontier) {
            RCPSPState_TT2 c = s; c.transitionsCached = false;
            c.AvailableTransitionIndices_TT2.clear(); c.hKnown = false;
            std::vector<RCPSPState_TT2> nb; env.GetSuccessors(c, nb);
            for (auto& ch : nb) {
                std::string k = meetKeyForward(ch, NT) + "#g" + std::to_string((int)ch.g);
                if (seen.insert(k).second) next.push_back(ch);
            }
        }
        frontier.swap(next);
    }
    return seen;
}
int solveRCPSP_TT2_BiCheck(int group, int exam, const std::string& ptype = "j30") {
    int depth = 3; if (const char* d = std::getenv("RCPSP_BICHECK_DEPTH")) depth = std::atoi(d);
    std::cout << "=== BI-GENERATION CHECK " << group << ":" << exam
              << " (depth " << depth << ") ===" << std::endl;
    bool s5=g_tt2_dr5, s4=g_tt2_dr4, sb=g_tt2_batch;
    g_tt2_dr5=false; g_tt2_dr4=false;   // raw generation; batch left as the caller set it
    RCPSP_TT2 env; int NT = 0;
    double hF0 = 0, hB0 = 0;

    // A: standalone forward, exactly what tt2one searches
    std::set<std::string> A;
    { getPetri(petri, group, exam, ptype); getRCPSP(RCPSPex, group, exam, ptype);
      NT = (int)petri.Transitions.size();
      RCPSPState_TT2 r, d0; hF0 = env.HCost(r, d0); A = biReach(env, r, NT, depth); }

    // B: standalone reversed, exactly what tt2rev searches
    std::set<std::string> B;
    { getPetri(petri, group, exam, ptype); getRCPSP(RCPSPex, group, exam, ptype);
      reverseLoadedInstance();
      RCPSPState_TT2 r, d0; hB0 = env.HCost(r, d0); B = biReach(env, r, NT, depth); }

    // C: BAE2's own setup — stash the reversed net, swap between sides
    std::set<std::string> Cf, Cb; double hF1 = 0, hB1 = 0;
    { getPetri(petri, group, exam, ptype); getRCPSP(RCPSPex, group, exam, ptype);
      P_RCPSP::PetriExample petriStash; RCPSP_example rcpspStash;
      petriStash = petri; rcpspStash = RCPSPex;
      reverseLoadedInstance();
      std::swap(petri, petriStash); std::swap(RCPSPex, rcpspStash);   // global = forward
      bool curFwd = true;
      auto useFwd = [&](bool want){
          if (want != curFwd) { std::swap(petri, petriStash); std::swap(RCPSPex, rcpspStash); curFwd = want; }
          g_tt2_dom_side = want ? 0 : 1; };
      // interleave the two sides the way the real search does, to catch stale globals
      useFwd(true);  { RCPSPState_TT2 r, d0; hF1 = env.HCost(r, d0); }
      useFwd(false); { RCPSPState_TT2 r, d0; hB1 = env.HCost(r, d0); }
      useFwd(true);  { RCPSPState_TT2 r; Cf = biReach(env, r, NT, depth); }
      useFwd(false); { RCPSPState_TT2 r; Cb = biReach(env, r, NT, depth); }
      useFwd(true);  { RCPSPState_TT2 r; std::set<std::string> again = biReach(env, r, NT, depth);
                       if (again != Cf) std::cout << "  !! forward generation CHANGED after touching"
                                                     " the reversed side (stale global)" << std::endl; } }

    bool okF = (A == Cf), okB = (B == Cb);
    // CPM critical path is DIRECTION-INVARIANT: reversing a DAG preserves longest paths.
    // So if the heuristic disagrees with CPM on the reversed net, the heuristic is wrong
    // there, not the net. getForwardHcost relaxes in ASCENDING activity-ID order and reads
    // rbegin() (highest id) as the answer — valid only while ID order is topological, which
    // PSPLIB guarantees for the FORWARD net and which reversal destroys.
    double cpmF = 0, cpmB = 0;
    { getPetri(petri, group, exam, ptype); getRCPSP(RCPSPex, group, exam, ptype);
      std::vector<int> e = computeESTforCurrentNet();
      for (int i = 1; i <= NT; i++) cpmF = std::max(cpmF, (double)(e[i] + RCPSPex.activities[i-1].duration)); }
    { getPetri(petri, group, exam, ptype); getRCPSP(RCPSPex, group, exam, ptype);
      reverseLoadedInstance();
      std::vector<int> e = computeESTforCurrentNet();
      for (int i = 1; i <= NT; i++) cpmB = std::max(cpmB, (double)(e[i] + RCPSPex.activities[i-1].duration)); }
    std::cout << "  CPM critical path: forward=" << cpmF << " reversed=" << cpmB
              << (cpmF == cpmB ? "  (invariant, as expected)" : "  ** NOT INVARIANT **") << std::endl;
    std::cout << "  heuristic vs CPM : forward h=" << hF0 << "/" << cpmF
              << "  reversed h=" << hB0 << "/" << cpmB
              << (hB0 < cpmB ? "   ** reversed heuristic LOSES the critical path **" : "") << std::endl;
    std::cout << "  root h: forward standalone=" << hF0 << " in-BAE2=" << hF1
              << (hF0==hF1 ? "  MATCH" : "  ** MISMATCH **") << std::endl;
    std::cout << "  root h: reversed standalone=" << hB0 << " in-BAE2=" << hB1
              << (hB0==hB1 ? "  MATCH" : "  ** MISMATCH **") << std::endl;
    std::cout << "  forward reachable<=" << depth << " fires: standalone=" << A.size()
              << " in-BAE2=" << Cf.size() << (okF ? "  MATCH" : "  ** MISMATCH **") << std::endl;
    std::cout << "  reversed reachable<=" << depth << " fires: standalone=" << B.size()
              << " in-BAE2=" << Cb.size() << (okB ? "  MATCH" : "  ** MISMATCH **") << std::endl;
    if (!okF || !okB) {
        std::set<std::string> d1, d2;
        std::set_difference(A.begin(),A.end(),Cf.begin(),Cf.end(),std::inserter(d1,d1.begin()));
        std::set_difference(Cf.begin(),Cf.end(),A.begin(),A.end(),std::inserter(d2,d2.begin()));
        std::cout << "  forward: only-standalone=" << d1.size() << " only-BAE2=" << d2.size() << std::endl;
        d1.clear(); d2.clear();
        std::set_difference(B.begin(),B.end(),Cb.begin(),Cb.end(),std::inserter(d1,d1.begin()));
        std::set_difference(Cb.begin(),Cb.end(),B.begin(),B.end(),std::inserter(d2,d2.begin()));
        std::cout << "  reversed: only-standalone=" << d1.size() << " only-BAE2=" << d2.size() << std::endl;
    }
    std::cout << "BICHECK " << group << ":" << exam
              << (okF && okB && hF0==hF1 && hB0==hB1 ? " ALL-MATCH" : " ** MISMATCH **") << std::endl;
    g_tt2_dr5=s5; g_tt2_dr4=s4; g_tt2_batch=sb;
    return (okF && okB) ? 0 : 1;
}

// ── Is the TT2 heuristic CONSISTENT? (mode tt2consist) ───────────────────────
// Unidirectional A* only needs ADMISSIBILITY. BAE*'s lower bound
// (min b_F + min b_B)/2 needs CONSISTENCY of BOTH heuristics: the proof requires
// h(u) - h(v) <= c(u,v) on every edge. So a heuristic that is fine for the forward
// search can still make the bidirectional bound overshoot and terminate early with a
// SUB-OPTIMAL answer (j30 13:1 returned 59/60/62 against a true optimum of 58).
// This walks real edges of both nets and counts violations of h(u) <= c(u,v) + h(v).
// It also checks h_opp, which we already know jumps across zero-cost (e==0) edges.
int solveRCPSP_TT2_Consist(int group, int exam, const std::string& ptype = "j30") {
    std::cout << "=== CONSISTENCY CHECK " << group << ":" << exam << " ===" << std::endl;
    long cap = 20000; if (const char* c = std::getenv("RCPSP_CONSIST_CAP")) cap = std::atol(c);
    for (int side = 0; side < 2; ++side) {
        getPetri(petri, group, exam, ptype); getRCPSP(RCPSPex, group, exam, ptype);
        int NT = (int)petri.Transitions.size();
        { static const double RSL[4]={0.2,0.5,0.7,1.0}; g_instance_rs=RSL[((group-1)%4+4)%4]; }
        if (side == 1) reverseLoadedInstance();
        std::vector<int> est = computeESTforCurrentNet();
        bool s5=g_tt2_dr5, s4=g_tt2_dr4, sb=g_tt2_batch;
        g_tt2_dr5=false; g_tt2_dr4=false; g_tt2_batch=false;   // raw edges
        RCPSP_TT2 env;
        long edges=0, viol=0, violZero=0, oViol=0, oViolZero=0;
        double worst=0, oWorst=0;
        std::vector<RCPSPState_TT2> frontier; { RCPSPState_TT2 r; frontier.push_back(r); }
        std::set<std::string> seen;
        while (!frontier.empty() && edges < cap) {
            std::vector<RCPSPState_TT2> next;
            for (auto& u : frontier) {
                if (edges >= cap) break;
                RCPSPState_TT2 uu = u; uu.transitionsCached=false;
                uu.AvailableTransitionIndices_TT2.clear(); uu.hKnown=false;
                RCPSPState_TT2 d0 = uu;
                double hu = env.HCost(uu, d0);
                double ou = (double)hOppOf(uu, est, NT);
                std::vector<RCPSPState_TT2> nb; env.GetSuccessors(uu, nb);
                for (auto& v : nb) {
                    RCPSPState_TT2 vv = v; RCPSPState_TT2 d1 = vv;
                    double c  = (double)vv.g - (double)uu.g;
                    double hv = env.HCost(vv, d1);
                    double ov = (double)hOppOf(vv, est, NT);
                    ++edges;
                    if (hu > c + hv + 1e-9) { ++viol; if (c == 0) ++violZero;
                        if (hu - (c + hv) > worst) worst = hu - (c + hv); }
                    if (ov > ou + c + 1e-9) { ++oViol; if (c == 0) ++oViolZero;
                        if (ov - (ou + c) > oWorst) oWorst = ov - (ou + c); }
                    std::string k = meetKeyForward(v, NT);
                    if (seen.insert(k).second && (long)next.size() < 4000) next.push_back(v);
                }
            }
            frontier.swap(next);
        }
        std::cout << "  " << (side ? "REVERSED" : "FORWARD ") << " net: edges=" << edges
                  << "  h inconsistent=" << viol << " (of which zero-cost edges=" << violZero
                  << ", worst excess=" << worst << ")"
                  << "  h_opp inconsistent=" << oViol << " (zero-cost=" << oViolZero
                  << ", worst=" << oWorst << ")" << std::endl;
        g_tt2_dr5=s5; g_tt2_dr4=s4; g_tt2_batch=sb;
    }
    return 0;
}

int solveRCPSP_TT2_MM(int group, int exam, const std::string& ptype="j30") {
    std::cout << "=== MM bidirectional " << group << ":" << exam << " ===" << std::endl;
    getPetri(petri, group, exam, ptype); getRCPSP(RCPSPex, group, exam, ptype);
    int NT = (int)petri.Transitions.size();
    { static const double RSL[4]={0.2,0.5,0.7,1.0}; g_instance_rs=RSL[((group-1)%4+4)%4]; }
    g_instance_deadline = std::chrono::steady_clock::now()+std::chrono::seconds(astar_timeout_seconds); g_instance_deadline_set=true;
    bool sDR5=g_tt2_dr5,sDR4=g_tt2_dr4,sB=g_tt2_batch;
    g_tt2_dr5=true; g_tt2_dr4=[]{const char*b=std::getenv("RCPSP_BI_DR4");return b&&std::atoi(b)!=0;}(); g_tt2_batch=false;   // see BAE*: DR4 disjoints the cut sets
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
    if (const char* e = std::getenv("RCPSP_LEFTSHIFT")) { g_use_leftshift = std::atoi(e) != 0; g_leftshift_global = std::atoi(e) >= 2; }
    if (const char* e = std::getenv("RCPSP_UB_SEED"))        g_ub_seed       = std::atoi(e) != 0;
    if (const char* e = std::getenv("RCPSP_BAP_MRVS"))       g_bap_mrvs      = std::atoi(e) != 0;
    if (const char* e = std::getenv("RCPSP_DOM_BP_SETKEY"))  g_dom_bp_setkey = std::atoi(e) != 0;
    if (const char* e = std::getenv("RCPSP_BIDIR"))     g_use_bidir     = std::atoi(e) != 0;
    if (const char* e = std::getenv("RCPSP_HYBRID"))    g_use_hybrid    = std::atoi(e) != 0;
    if (const char* e = std::getenv("RCPSP_HYBRID_T"))  g_hybrid_threshold = (float)std::atof(e);
    if (const char* e = std::getenv("RCPSP_LEAN"))      g_use_lean      = std::atoi(e) != 0;
    if (const char* e = std::getenv("RCPSP_HGREED"))    setting.use_greed_conflic_resultion_asstimation = std::atoi(e) != 0;
    if (const char* e = std::getenv("RCPSP_INLINE"))    g_use_inline    = std::atoi(e) != 0;
    if (const char* e = std::getenv("RCPSP_SKYLINE"))   g_dom_skyline   = std::atoi(e) != 0;
    if (const char* e = std::getenv("RCPSP_LAZY"))      g_use_lazy      = std::atoi(e) != 0;
    if (const char* e = std::getenv("RCPSP_TT2_DR5"))   g_tt2_dr5       = std::atoi(e) != 0;
    if (const char* e = std::getenv("RCPSP_TT2_PROF"))  g_tt2_prof      = std::atoi(e) != 0;   // phase timers (Globals.h)
    if (const char* e = std::getenv("RCPSP_TT2_PROF_TRACE")) {   // progress snapshot every 5 s, appended
        g_prof_trace = std::fopen(e, "a");
        if (g_prof_trace) { std::fseek(g_prof_trace, 0, SEEK_END);
            if (std::ftell(g_prof_trace) == 0)
                std::fprintf(g_prof_trace, "size,group,exam,elapsedSec,expansions,maxF,childrenRaw,childrenKept,succSec,heurSec,dr5Sec\n"); }
    }
    if (const char* e = std::getenv("RCPSP_TT2_BATCH")) g_tt2_batch     = std::atoi(e) != 0;
    if (const char* e = std::getenv("RCPSP_TT2_DR4"))   g_tt2_dr4       = std::atoi(e) != 0;
    if (const char* e = std::getenv("RCPSP_TT2_KILL"))  g_tt2_kill      = std::atoi(e);        // dead-node count (1) / skip (2), Globals.h
    if (const char* e = std::getenv("RCPSP_CBS_FREEFIX"))     g_cbs_freefix     = std::atoi(e);   // extra free-resolution child (Globals.h)
    if (const char* e = std::getenv("RCPSP_CBS_FREEFIX_MAX")) g_cbs_freefix_max = std::max(1, std::atoi(e));
    if (const char* e = std::getenv("RCPSP_CBS_FREEFIX_PROP")) g_cbs_freefix_prop = std::atoi(e);
    if (const char* e = std::getenv("RCPSP_CBS_FREEFIX_TRIES")) g_cbs_freefix_tries = std::max(1, std::atoi(e));
    if (const char* e = std::getenv("RCPSP_CBS_FREEFIX_TEST"))  g_cbs_freefix_test  = std::atoi(e);
    if (const char* e = std::getenv("RCPSP_CBS_FREEFIX_SHARE")) g_cbs_freefix_share = std::atoi(e);
    if (const char* e = std::getenv("RCPSP_CBS_FREEFIX_LAZY"))  g_cbs_freefix_lazy  = std::atoi(e);
    if (const char* e = std::getenv("RCPSP_CBS_FREEFIX_STOP"))  g_cbs_freefix_stop  = std::atoi(e);
    if (const char* e = std::getenv("RCPSP_CBS_FREEFIX_GOAL"))  g_cbs_freefix_goal  = std::atoi(e);
    if (const char* e = std::getenv("RCPSP_CBS_FREEFIX_MAXOVER")) g_cbs_freefix_maxover = std::atoi(e);
    if (const char* e = std::getenv("RCPSP_CBS_FREEFIX_SGS"))     g_cbs_freefix_sgs     = std::atoi(e);
    if (const char* e = std::getenv("RCPSP_CBS_FREEFIX_MAXCONF")) g_cbs_freefix_maxconf = std::atoi(e);
    if (const char* e = std::getenv("RCPSP_CBS_FREEFIX_RAND"))    g_cbs_freefix_rand    = std::atoi(e);
    if (const char* e = std::getenv("RCPSP_CBS_FREEFIX_RANDCONF")) g_cbs_freefix_randconf = std::atoi(e);
    if (const char* e = std::getenv("RCPSP_TT2_HDZFIX")) g_tt2_hdz_fix = std::atoi(e) != 0; // zero-delay h shortcut fix, Globals.h
    if (const char* e = std::getenv("RCPSP_TT2_DUPPROBE")) g_tt2_dupprobe = std::atoi(e) != 0; // A* duplicate-lookup census, Globals.h
    if (const char* e = std::getenv("RCPSP_TT2_PRECHECK")) g_tt2_precheck = std::atoi(e); // DR5 before a child is built, Globals.h
    if (const char* e = std::getenv("RCPSP_TT2_WAIT"))  g_tt2_wait      = std::atoi(e) != 0;   // explicit-wait model (Globals.h)
    if (const char* e = std::getenv("RCPSP_TT2_WAITJUMP")) g_tt2_waitjump = std::atoi(e) != 0; // pass-through wait jumps (Globals.h)
    if (const char* e = std::getenv("RCPSP_TT2_COMPACT")) g_tt2_compact = std::max(0, std::min(2, std::atoi(e)));   // stored TT2 nodes without token lists (Globals.h)
    if (const char* e = std::getenv("RCPSP_TT2_HFRESH")) g_tt2_hfresh = std::atoi(e) != 0;   // fresh h on zero-delay children (Globals.h)
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
    if (const char* e = std::getenv("RCPSP_TT2_UB_GE"))             g_tt2_ub_ge             = std::atoi(e) != 0;
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
    if (const char* e = std::getenv("RCPSP_CBS_TB"))             g_cbs_tb            = std::atoi(e);
    if (const char* e = std::getenv("RCPSP_CBS_TBSPEC"))         g_cbs_tb_spec       = e;
    if (const char* e = std::getenv("RCPSP_CBS_RESTART")) {      // "<sec>/<TBSPEC>/<T>,..." (see Globals.h)
        g_cbs_restart = e; g_cbs_restart_stages.clear();
        std::stringstream ss(g_cbs_restart); std::string tok;
        while (std::getline(ss, tok, ',')) {
            if (tok.empty()) continue;
            const size_t a = tok.find('/'), b = (a == std::string::npos) ? a : tok.find('/', a + 1);
            if (a == std::string::npos || b == std::string::npos) {
                std::cerr << "RCPSP_CBS_RESTART: bad stage '" << tok << "' (want <sec>/<TBSPEC>/<T>)" << std::endl; std::exit(1);
            }
            CbsRestartStage st; st.sec = std::atol(tok.substr(0, a).c_str());
            st.spec = tok.substr(a + 1, b - a - 1); if (st.spec == "-") st.spec.clear();
            st.T = (float)std::atof(tok.substr(b + 1).c_str());
            {   // optional 4th field: M = this stage runs on the mirrored project, F = it adds the FREEFIX extra children
                const std::string rest = tok.substr(b + 1); const size_t m = rest.find('/');
                if (m != std::string::npos) {
                    const std::string f4 = rest.substr(m + 1);
                    if (f4.empty()) { std::cerr << "RCPSP_CBS_RESTART: bad stage '" << tok << "' (empty 4th field)" << std::endl; std::exit(1); }
                    for (char ch : f4) {
                        if (ch == 'M' || ch == 'm') st.mirror = true;
                        else if (ch == 'F' || ch == 'f') st.freefix = true;
                        else { std::cerr << "RCPSP_CBS_RESTART: bad stage '" << tok << "' (the 4th field takes M and/or F)" << std::endl; std::exit(1); }
                    }
                }
            }
            g_cbs_restart_stages.push_back(st);
        }
        for (size_t i = 0; i + 1 < g_cbs_restart_stages.size(); ++i)
            if (g_cbs_restart_stages[i].sec < 1) { std::cerr << "RCPSP_CBS_RESTART: stage " << i + 1 << " needs >= 1 s" << std::endl; std::exit(1); }
        if (!g_cbs_restart_stages.empty()) {
            std::cout << "CBS restart sequence:";
            for (size_t i = 0; i < g_cbs_restart_stages.size(); ++i)
                std::cout << "  [" << i + 1 << "] " << (i + 1 < g_cbs_restart_stages.size() ? std::to_string(g_cbs_restart_stages[i].sec) + " s" : std::string("rest"))
                          << " order=" << (g_cbs_restart_stages[i].spec.empty() ? "-" : g_cbs_restart_stages[i].spec) << " T=" << g_cbs_restart_stages[i].T << (g_cbs_restart_stages[i].mirror ? " MIRROR" : "");
            std::cout << std::endl;
        }
    }
    if (const char* e = std::getenv("RCPSP_CBS_RESTART_KEEPLB")) g_cbs_restart_keeplb = std::atoi(e) != 0;
    if (const char* e = std::getenv("RCPSP_CBS_LEARN"))          g_cbs_learn         = std::atoi(e);
    if (const char* e = std::getenv("RCPSP_CBS_FASTFIRST"))      g_cbs_fastfirst     = std::max(0, std::min(3, std::atoi(e)));
    if (const char* e = std::getenv("RCPSP_CBS_MIRROR"))         g_cbs_mirror        = std::atoi(e) != 0;
    if (const char* e = std::getenv("RCPSP_CBS_DELAY")) {        // Globals.h: 0 | prec | pair | wait | far
        const std::string v = e;
        g_cbs_delay = (v == "prec" || v == "1") ? 1 : (v == "pair" || v == "2") ? 2 : (v == "wait" || v == "3") ? 3
                    : (v == "far" || v == "4") ? 4
                    : (v.empty() || v == "0" || v == "off") ? 0 : -1;
        if (g_cbs_delay < 0) { std::cerr << "RCPSP_CBS_DELAY: want 0, prec, pair, wait or far" << std::endl; std::exit(1); }
    }
    if (const char* e = std::getenv("RCPSP_CBS_DELAY_MAXKIDS"))  g_cbs_delay_maxkids = std::max(0, std::atoi(e));
    if (g_cbs_delay == 4) std::cout << "RCPSP_CBS_DELAY=4 (far): further delay where safe; a node is still its schedule, dominance unchanged\n";
    if (g_cbs_delay >= 1 && g_cbs_delay <= 3) {
        // A node is no longer just its schedule: only the Bell & Park table (which compares the conditions over
        // unscheduled activities) stays sound, and the reverse direction marks schedules, not nodes.
        g_dom_rule = DOM_BP; g_use_bidir = false;
        std::cout << "RCPSP_CBS_DELAY=" << g_cbs_delay << " (1 prec, 2 pair, 3 wait), max kids " << g_cbs_delay_maxkids
                  << ": dominance rule forced to bp, reverse direction off\n";
    }
    if (const char* e = std::getenv("RCPSP_CBS_LEARN_PRIOR"))    g_cbs_learn_prior   = std::max(1, std::atoi(e));
    if (g_cbs_learn < 0 || g_cbs_learn > 2) { std::cerr << "RCPSP_CBS_LEARN: want 0, 1 or 2" << std::endl; std::exit(1); }
    if (g_cbs_restart_keeplb || g_cbs_learn)
        std::cout << "CBS restart keeps: lower bound=" << (g_cbs_restart_keeplb ? 1 : 0) << " learned selection=" << g_cbs_learn
                  << " (prior " << g_cbs_learn_prior << ")" << std::endl;
    if (const char* e = std::getenv("RCPSP_CBS_PATHMAX"))        g_cbs_pathmax       = std::atoi(e) != 0;
    if (const char* e = std::getenv("RCPSP_CBS_PATHMAX_INSERT")) g_cbs_pathmax_insert = std::atoi(e) != 0;
    if (const char* e = std::getenv("RCPSP_CBS_GATE_DEPTH"))     g_cbs_gate_depth    = std::atoi(e) != 0;
    if (const char* e = std::getenv("RCPSP_CBS_SEL"))            g_cbs_sel           = std::atoi(e);
    if (const char* e = std::getenv("RCPSP_CBS_ADAPT_BRANCH"))   g_cbs_adapt_branch  = std::atoi(e);
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
        // RCPSP_TT2_COMPACT is for the plain TT2 search. The other TT2 modes (HBAE, BAE, the backward and meet
        // searches, the diagnostic modes) store and read states of their own, and the single-resource bound with the
        // TT2 sub-solver restarts a search from a stored node's marking: those keep the token lists.
        if (g_tt2_compact) {
            const bool plainTT2 = arg1 == "tt2one" || arg1 == "tt2rev" || arg1.rfind("tt2rev_", 0) == 0 || arg1.rfind("tt2_", 0) == 0
                               || arg1.rfind("tt2hda_", 0) == 0;   // HDA*: workers expand through RCPSP_TT2::GetSuccessors (rebuild per expansion)
            if (!plainTT2) {
                std::cout << "RCPSP_TT2_COMPACT: forced off (mode " << arg1 << "; it applies to tt2_<size>, tt2rev_<size>, tt2hda_<size>, tt2one and tt2rev)\n";
                g_tt2_compact = 0;
            } else if (g_tt2_singleres && g_subsolver == 2) {
                std::cout << "RCPSP_TT2_COMPACT: forced off (RCPSP_TT2_SINGLERES with the TT2 sub-solver reads the token lists of stored nodes)\n";
                g_tt2_compact = 0;
            } else
                std::cout << "TT2 stored nodes: COMPACT" << (g_tt2_compact == 2 ? " CHECK (token lists kept, rebuilt and compared)" : " (token lists rebuilt at expansion)") << "\n";
        }
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
                 << "useHeuristic,useMDASets,useMDACache,useStrongConstraints,useMDABAB,cardinalityRatio,useDR5,domRule,useUB,useHybrid,hybridT,useLeftshift,useBidir,ubPruned,leftshiftPruned,domPruned,domChecks,domStored,useLazy,useSkyline,lazyEvals,lazyReinserts,useNonMinimalDelay,useAncestorBranching,useDominanceSib,usePairDecomp,useHGreed,useLean,useInline,domCap,timeoutS,useWarmStart,warmStartK,warmStartBudgetS,warmStartDir,useSetDelay,warmStartRS,warmstartEngaged,warmstartInflMk,rootF,provenLB,warmstartSec,useDR4,useThetaBound,thetaBoundBetter,useSubsetLB,subsetBetter,subsetSolves,subsetExpandsTotal,subsetCapped,subsetMaxExpands,subsetCacheHits,useMdaRecursive,imp2Fires,imp2MaxDepth,useNmdPrecedence,orderSwapCand,useCbsMinCut,minCutBetter,minCutCalls,useRSAdapt,rsThreshold,instanceRS,useSingleRes,singleResBetter,singleResCalls,heuristicLowRS,heuristicHighRS,rootH,heurTimeSec,heurCalls,singleResTimeSec,minCutTimeSec,useHierRS,hierRsBetter,hierRsCalls,hierRsTimeSec,hierRsCacheHits,subsolver,shadowProbes,shadowHits,cbsUbSource,rootDlbOn,rootSeedOn,rootProbeOn,rootLB,seedUB,rootSec,closedAtRoot,cbsTT,cbsNG,ttSteps,ttCalls,ttRaised,ttPrunedAtExpand,ttSec,ngLearned,ngHits,ngStored,cbsRootFloor,ttFull,rootLnsOn,lnsSeedIn,lnsBest,lnsIters,lnsImpCp,lnsImpSgs,lnsSec,lsPropOn,lsCalls,lsFail,lsTighter,lsScoreDiff,lsPickDiff,lsSec,cbsRootEs,rootEsMoved,rootEsRefuted,cbsWin,winChecks,winPruned,cbsTB,lbRaiseExp,cbsPathmax,pathmaxRaised,cbsGateDepth,cbsSel,cbsMdaRef,mdaRefRaised,mdaRefChecks,propIters,propCalls,propRounds,cbsTtEr,cbsTtErMaxRS,erRefuted,cbsAdaptBranch,adaptSingular,cbsPathmaxInsert,cbsTbSpec,cbsRestart,restartStage,cbsRestartKeepLB,restartFloor,cbsLearn,cbsLearnPrior,learnTries,learnRaises,learnPickDiff,cbsFastFirst,fastFirstSkipped,bapMrvs,ubSeed,leftshiftGlobal,domBpSetKey,cbsMirror,propFast,cbsDelay,cbsDelayMaxKids,relInfeasible,relFallback,relMaxLen,farBlocked,farApplied,propInc,cbsFreeFix,freeFixKids,freeFixResolved,freeFixProp,freeFixMax,freeFixTries,freeFixTest,freeFixShare,freeFixLazy,freeFixFinalFail,freeFixStop,freeFixGoal,freeFixMaxOver,freeFixSgs,freeFixMaxConf,freeFixSgsCalls,freeFixRand,freeFixRandConf\n"; }
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
            { std::ofstream h(f); h << "group,exam,time,solved,makespan,expandNumber,generatedNumber,depth,model,problemType,maxMem,LB,useTT2DR5,useTT2Batch,tt2BatchCap,domPruned,domThinned,domChecks,domInserts,domMaxBucket,useTT2Sym,symPruned,useTT2Gendesc,useTT2SymTB,timeoutS,trivialAtRoot,useThetaBound,thetaBoundBetter,useTT2DR4,dr4Pruned,useTT2ImmSel,immSelFired,useTT2RSAdapt,tt2RsThreshold,instanceRS,useTT2SingleRes,tt2SingleResBetter,tt2SingleResCalls,heuristicLowRS,heuristicHighRS,rootH,heurTimeSec,heurCalls,singleResTimeSec,subsolver,shadowProbes,shadowHits,useTT2UB,ubPruned,useTT2HierRS,tt2HierRsBetter,tt2HierRsCalls,tt2HierRsTimeSec,tt2HierRsCacheHits,direction,useTT2Prof,profAstarSec,profSuccSec,profSuccCalls,profAvailSec,profGenSec,profDR5Sec,profHashSec,profHashCalls,profHeurCPSec,profHeurResSec,hCallsAll,hKnownHits,hDeltaZeroHits,childrenRaw,childrenKept,domBuckets,domComparisons,openAtEnd,itemsAtEnd,uniqueExpanded,maxFExpanded,fAt1s,fAt10s,fAt60s,fAt120s,fAt240s,timedOut,cpFast,resFast,dr5Fast,dr5Sorted,genFast,dr5Mtf,waitModel,waitEdges,waitCut,dr5WaitExempt,waitPass,waitJump,waitJumped,rootDlbOn,rootSeedOn,rootLBPlain,rootLB,rootLBSec,rootLBBudgetHit,seedUB,seedSec,closedAtRoot,rootProbeOn,probeSeedIn,probeBest,probeNodes,probeExhausted,probeSec,tt2NodeTT,nodeTtCalls,nodeTtRaised,nodeTtRaiseSum,nodeTtTt0Fail,nodeTtPropFail,nodeTtSec,nodeTtWinPruned,tt2Win,tt2WinLs,tt2Ser,nodeTtSerFail,tt2UbGe,tt2UbClosed,rootLnsOn,lnsSeedIn,lnsBest,lnsIters,lnsImpCp,lnsImpSgs,lnsSec,tt2TbSpec,tt2Compact,compactRebuilds,compactChecked,tt2Precheck,precheckCut\n"; }
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
        } else if (arg1 == "tt2bicheck") {
            if (argc < 5) { std::cerr << "Usage: tt2bicheck <type> <group> <exam>" << std::endl; return 1; }
            solveRCPSP_TT2_BiCheck(std::atoi(argv[3]), std::atoi(argv[4]), argv[2]);
        } else if (arg1 == "rootbounds") {
            // rootbounds <size> [g1 g2]: RootBounds.h values per instance, no search (set RCPSP_ROOT_DLB / RCPSP_ROOT_SEED)
            const std::string sz = argc > 2 ? argv[2] : "j30";
            const int g1 = argc > 3 ? std::atoi(argv[3]) : 1, g2 = argc > 4 ? std::atoi(argv[4]) : 48;
            const int e1 = argc > 5 ? std::atoi(argv[5]) : 1, e2 = argc > 5 ? e1 : 10;   // optional single exam
            for (int g = g1; g <= g2; ++g) for (int e = e1; e <= e2; ++e) {
                getPetri(petri, g, e, sz); getRCPSP(RCPSPex, g, e, sz);
                rootb::computeRootBounds();
                const auto& ri = rootb::info();
                std::cout << "ROOTB," << sz << "," << g << "," << e << "," << ri.lbPlain << "," << ri.lb << "," << ri.seed << ","
                          << ri.lbSec << "," << ri.seedSec << "," << (ri.budgetHit ? 1 : 0) << "," << (ri.closed ? 1 : 0) << ","
                          << ri.probeSeedIn << "," << ri.probeBest << "," << ri.probeNodes << "," << ri.probeExhausted << "," << ri.probeSec << "," << (tt2nb::On() ? 1 : 0) << "," << tt2nb::st().calls << "," << tt2nb::st().raised << "," << tt2nb::st().raiseSum << "," << tt2nb::st().tt0Fail << "," << tt2nb::st().propFail << "," << tt2nb::st().sec << "," << tt2nb::st().winPruned << "," << (tt2nb::WinOn() ? 1 : 0) << "," << (tt2nb::WinLsOn() ? 1 : 0) << "," << (tt2nb::SerOn() ? 1 : 0) << "," << tt2nb::st().serFail << "," << (g_tt2_ub_ge ? 1 : 0) << "," << (g_tt2_ub_closed ? 1 : 0)
                          << "," << ri.lnsSeedIn << "," << ri.lnsBest << "," << ri.lnsIters << "," << ri.lnsImpCp << "," << ri.lnsImpSgs << "," << ri.lnsSec << std::endl;
            }
        } else if (arg1 == "tt2consist") {
            if (argc < 5) { std::cerr << "Usage: tt2consist <type> <group> <exam>" << std::endl; return 1; }
            solveRCPSP_TT2_Consist(std::atoi(argv[3]), std::atoi(argv[4]), argv[2]);
        } else if (arg1 == "tt2hbae") {
            if (argc < 5) { std::cerr << "Usage: tt2hbae <type> <group> <exam>" << std::endl; return 1; }
            solveRCPSP_TT2_HBAE(std::atoi(argv[3]), std::atoi(argv[4]), argv[2]);
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
        } else if (arg1 == "tt2trace") {
            if (argc < 5) { std::cerr << "Usage: tt2trace <type> <group> <exam>" << std::endl; return 1; }
            solveRCPSP_TT2_Trace(std::atoi(argv[3]), std::atoi(argv[4]), argv[2]);
        } else if (arg1 == "tt2admis") {
            // tt2admis <type> <group> <exam> — is h admissible along each side's optimal path?
            if (argc < 5) { std::cerr << "Usage: tt2admis <type> <group> <exam>\n"; return 1; }
            solveRCPSP_TT2_Admis(std::atoi(argv[3]), std::atoi(argv[4]), argv[2]);
        } else if (arg1 == "tt2meet") {
            // tt2meet <type> <group> <exam> — validate that forward & reversed backward meet
            if (argc < 5) { std::cerr << "Usage: tt2meet <type> <group> <exam>\n"; return 1; }
            solveRCPSP_TT2_Meet(std::atoi(argv[3]), std::atoi(argv[4]), argv[2]);
        } else if (arg1 == "tt2rev") {
            // tt2rev <type> <group> <exam> — reversed-instance backward via the fast forward solver
            if (argc < 5) { std::cerr << "Usage: tt2rev <type> <group> <exam>\n"; return 1; }
            std::string ptype = argv[2];
            std::string f = getNextFilename("new_results", "output_tt2rev_" + ptype + "_", ".csv");
            { std::ofstream h(f); h << "group,exam,time,solved,makespan,expandNumber,generatedNumber,depth,model,problemType,maxMem,LB,useTT2DR5,useTT2Batch,tt2BatchCap,domPruned,domThinned,domChecks,domInserts,domMaxBucket,useTT2Sym,symPruned,useTT2Gendesc,useTT2SymTB,timeoutS,trivialAtRoot,useThetaBound,thetaBoundBetter,useTT2DR4,dr4Pruned,useTT2ImmSel,immSelFired,useTT2RSAdapt,tt2RsThreshold,instanceRS,useTT2SingleRes,tt2SingleResBetter,tt2SingleResCalls,heuristicLowRS,heuristicHighRS,rootH,heurTimeSec,heurCalls,singleResTimeSec,subsolver,shadowProbes,shadowHits,useTT2UB,ubPruned,useTT2HierRS,tt2HierRsBetter,tt2HierRsCalls,tt2HierRsTimeSec,tt2HierRsCacheHits,direction,useTT2Prof,profAstarSec,profSuccSec,profSuccCalls,profAvailSec,profGenSec,profDR5Sec,profHashSec,profHashCalls,profHeurCPSec,profHeurResSec,hCallsAll,hKnownHits,hDeltaZeroHits,childrenRaw,childrenKept,domBuckets,domComparisons,openAtEnd,itemsAtEnd,uniqueExpanded,maxFExpanded,fAt1s,fAt10s,fAt60s,fAt120s,fAt240s,timedOut,cpFast,resFast,dr5Fast,dr5Sorted,genFast,dr5Mtf,waitModel,waitEdges,waitCut,dr5WaitExempt,waitPass,waitJump,waitJumped,rootDlbOn,rootSeedOn,rootLBPlain,rootLB,rootLBSec,rootLBBudgetHit,seedUB,seedSec,closedAtRoot,rootProbeOn,probeSeedIn,probeBest,probeNodes,probeExhausted,probeSec,tt2NodeTT,nodeTtCalls,nodeTtRaised,nodeTtRaiseSum,nodeTtTt0Fail,nodeTtPropFail,nodeTtSec,nodeTtWinPruned,tt2Win,tt2WinLs,tt2Ser,nodeTtSerFail,tt2UbGe,tt2UbClosed,rootLnsOn,lnsSeedIn,lnsBest,lnsIters,lnsImpCp,lnsImpSgs,lnsSec,tt2TbSpec,tt2Compact,compactRebuilds,compactChecked,tt2Precheck,precheckCut\n"; }
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
        } else if (arg1.rfind("tt2hbae_", 0) == 0) {
            // HOG2 BAE* full benchmark for one size: "tt2hbae_j30" etc.
            runBenchmarkTT2HBAE(arg1.substr(8));
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
        } else if (arg1.rfind("tt2hda_", 0) == 0) {
            // HDA* forward TT2 on RCPSP_HDA_THREADS workers: "tt2hda_j30" etc. (HDAStarTT2.h)
            runBenchmarkTT2HDA(arg1.substr(7));
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

// RCPSP_ONLY=<file>: the batch loops (CBS config runs, tt2_<size>, tt2hbae_<size>) run only the instances listed in
// the file, one "size,group,exam" per line (e.g. j60,5,9); every other instance is skipped and gets no row. Used to
// re-test only the instances no model has solved yet. A set but empty/missing file stops the run.
static bool onlyInstance(const std::string& size, int g, int e) {
    static const std::set<std::string> keep = [] {
        std::set<std::string> s;
        const char* f = std::getenv("RCPSP_ONLY");
        if (!f || !*f) return s;
        std::ifstream in(f); std::string line;
        while (std::getline(in, line)) {
            std::string t; for (char c : line) if (c != '\r' && c != ' ' && c != '\t') t += c;
            if (!t.empty()) s.insert(t);
        }
        if (s.empty()) { std::cerr << "RCPSP_ONLY: no instances in " << f << std::endl; std::exit(1); }
        std::cout << "RCPSP_ONLY: " << s.size() << " instances from " << f << std::endl;
        return s;
    }();
    return keep.empty() || keep.count(size + "," + std::to_string(g) + "," + std::to_string(e)) > 0;
}

void runConfig(const std::string& filename, const std::string& problemType) {
    allcorrect = true;
    for (int i = 1; i <= 48; i++) {
        for (int j = 1; j <= 10; j++) {
            if (!onlyInstance(problemType, i, j)) continue;
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
         << "useFirst,useConflictPrioritization,useHeuristic,useMDASets,useMDACache,useStrongConstraints,useMDABAB,cardinalityRatio,useDR5,domRule,useUB,useHybrid,hybridT,useLeftshift,useBidir,ubPruned,leftshiftPruned,domPruned,domChecks,domStored,useLazy,useSkyline,lazyEvals,lazyReinserts,useNonMinimalDelay,useAncestorBranching,useDominanceSib,usePairDecomp,useHGreed,useLean,useInline,domCap,timeoutS,useWarmStart,warmStartK,warmStartBudgetS,warmStartDir,useSetDelay,warmStartRS,warmstartEngaged,warmstartInflMk,rootF,provenLB,warmstartSec,useDR4,useThetaBound,thetaBoundBetter,useSubsetLB,subsetBetter,subsetSolves,subsetExpandsTotal,subsetCapped,subsetMaxExpands,subsetCacheHits,useMdaRecursive,imp2Fires,imp2MaxDepth,useNmdPrecedence,orderSwapCand,useCbsMinCut,minCutBetter,minCutCalls,useRSAdapt,rsThreshold,instanceRS,useSingleRes,singleResBetter,singleResCalls,heuristicLowRS,heuristicHighRS,rootH,heurTimeSec,heurCalls,singleResTimeSec,minCutTimeSec,useHierRS,hierRsBetter,hierRsCalls,hierRsTimeSec,hierRsCacheHits,subsolver,shadowProbes,shadowHits,cbsUbSource,rootDlbOn,rootSeedOn,rootProbeOn,rootLB,seedUB,rootSec,closedAtRoot,cbsTT,cbsNG,ttSteps,ttCalls,ttRaised,ttPrunedAtExpand,ttSec,ngLearned,ngHits,ngStored,cbsRootFloor,ttFull,rootLnsOn,lnsSeedIn,lnsBest,lnsIters,lnsImpCp,lnsImpSgs,lnsSec,lsPropOn,lsCalls,lsFail,lsTighter,lsScoreDiff,lsPickDiff,lsSec,cbsRootEs,rootEsMoved,rootEsRefuted,cbsWin,winChecks,winPruned,cbsTB,lbRaiseExp,cbsPathmax,pathmaxRaised,cbsGateDepth,cbsSel,cbsMdaRef,mdaRefRaised,mdaRefChecks,propIters,propCalls,propRounds,cbsTtEr,cbsTtErMaxRS,erRefuted,cbsAdaptBranch,adaptSingular,cbsPathmaxInsert,cbsTbSpec,cbsRestart,restartStage,cbsRestartKeepLB,restartFloor,cbsLearn,cbsLearnPrior,learnTries,learnRaises,learnPickDiff,cbsFastFirst,fastFirstSkipped,bapMrvs,ubSeed,leftshiftGlobal,domBpSetKey,cbsMirror,propFast,cbsDelay,cbsDelayMaxKids,relInfeasible,relFallback,relMaxLen,farBlocked,farApplied,propInc,cbsFreeFix,freeFixKids,freeFixResolved,freeFixProp,freeFixMax,freeFixTries,freeFixTest,freeFixShare,freeFixLazy,freeFixFinalFail,freeFixStop,freeFixGoal,freeFixMaxOver,freeFixSgs,freeFixMaxConf,freeFixSgsCalls,freeFixRand,freeFixRandConf"
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
          << "useStrongConstraints,useMDABAB,cardinalityRatio,useDR5,domRule,useUB,useHybrid,hybridT,useLeftshift,useBidir,ubPruned,leftshiftPruned,domPruned,domChecks,domStored,useLazy,useSkyline,lazyEvals,lazyReinserts,useNonMinimalDelay,useAncestorBranching,useDominanceSib,usePairDecomp,useHGreed,useLean,useInline,domCap,timeoutS,useWarmStart,warmStartK,warmStartBudgetS,warmStartDir,useSetDelay,warmStartRS,warmstartEngaged,warmstartInflMk,rootF,provenLB,warmstartSec,useDR4,useThetaBound,thetaBoundBetter,useSubsetLB,subsetBetter,subsetSolves,subsetExpandsTotal,subsetCapped,subsetMaxExpands,subsetCacheHits,useMdaRecursive,imp2Fires,imp2MaxDepth,useNmdPrecedence,orderSwapCand,useCbsMinCut,minCutBetter,minCutCalls,useRSAdapt,rsThreshold,instanceRS,useSingleRes,singleResBetter,singleResCalls,heuristicLowRS,heuristicHighRS,rootH,heurTimeSec,heurCalls,singleResTimeSec,minCutTimeSec,useHierRS,hierRsBetter,hierRsCalls,hierRsTimeSec,hierRsCacheHits,subsolver,shadowProbes,shadowHits,cbsUbSource,rootDlbOn,rootSeedOn,rootProbeOn,rootLB,seedUB,rootSec,closedAtRoot,cbsTT,cbsNG,ttSteps,ttCalls,ttRaised,ttPrunedAtExpand,ttSec,ngLearned,ngHits,ngStored,cbsRootFloor,ttFull,rootLnsOn,lnsSeedIn,lnsBest,lnsIters,lnsImpCp,lnsImpSgs,lnsSec,lsPropOn,lsCalls,lsFail,lsTighter,lsScoreDiff,lsPickDiff,lsSec,cbsRootEs,rootEsMoved,rootEsRefuted,cbsWin,winChecks,winPruned,cbsTB,lbRaiseExp,cbsPathmax,pathmaxRaised,cbsGateDepth,cbsSel,cbsMdaRef,mdaRefRaised,mdaRefChecks,propIters,propCalls,propRounds,cbsTtEr,cbsTtErMaxRS,erRefuted,cbsAdaptBranch,adaptSingular,cbsPathmaxInsert,cbsTbSpec,cbsRestart,restartStage,cbsRestartKeepLB,restartFloor,cbsLearn,cbsLearnPrior,learnTries,learnRaises,learnPickDiff,cbsFastFirst,fastFirstSkipped,bapMrvs,ubSeed,leftshiftGlobal,domBpSetKey,cbsMirror,propFast,cbsDelay,cbsDelayMaxKids,relInfeasible,relFallback,relMaxLen,farBlocked,farApplied,propInc,cbsFreeFix,freeFixKids,freeFixResolved,freeFixProp,freeFixMax,freeFixTries,freeFixTest,freeFixShare,freeFixLazy,freeFixFinalFail,freeFixStop,freeFixGoal,freeFixMaxOver,freeFixSgs,freeFixMaxConf,freeFixSgsCalls,freeFixRand,freeFixRandConf\n"; }

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
         << "useFirst,useConflictPrioritization,useHeuristic,useMDASets,useMDACache,useStrongConstraints,useMDABAB,cardinalityRatio,useDR5,domRule,useUB,useHybrid,hybridT,useLeftshift,useBidir,ubPruned,leftshiftPruned,domPruned,domChecks,domStored,useLazy,useSkyline,lazyEvals,lazyReinserts,useNonMinimalDelay,useAncestorBranching,useDominanceSib,usePairDecomp,useHGreed,useLean,useInline,domCap,timeoutS,useWarmStart,warmStartK,warmStartBudgetS,warmStartDir,useSetDelay,warmStartRS,warmstartEngaged,warmstartInflMk,rootF,provenLB,warmstartSec,useDR4,useThetaBound,thetaBoundBetter,useSubsetLB,subsetBetter,subsetSolves,subsetExpandsTotal,subsetCapped,subsetMaxExpands,subsetCacheHits,useMdaRecursive,imp2Fires,imp2MaxDepth,useNmdPrecedence,orderSwapCand,useCbsMinCut,minCutBetter,minCutCalls,useRSAdapt,rsThreshold,instanceRS,useSingleRes,singleResBetter,singleResCalls,heuristicLowRS,heuristicHighRS,rootH,heurTimeSec,heurCalls,singleResTimeSec,minCutTimeSec,useHierRS,hierRsBetter,hierRsCalls,hierRsTimeSec,hierRsCacheHits,subsolver,shadowProbes,shadowHits,cbsUbSource,rootDlbOn,rootSeedOn,rootProbeOn,rootLB,seedUB,rootSec,closedAtRoot,cbsTT,cbsNG,ttSteps,ttCalls,ttRaised,ttPrunedAtExpand,ttSec,ngLearned,ngHits,ngStored,cbsRootFloor,ttFull,rootLnsOn,lnsSeedIn,lnsBest,lnsIters,lnsImpCp,lnsImpSgs,lnsSec,lsPropOn,lsCalls,lsFail,lsTighter,lsScoreDiff,lsPickDiff,lsSec,cbsRootEs,rootEsMoved,rootEsRefuted,cbsWin,winChecks,winPruned,cbsTB,lbRaiseExp,cbsPathmax,pathmaxRaised,cbsGateDepth,cbsSel,cbsMdaRef,mdaRefRaised,mdaRefChecks,propIters,propCalls,propRounds,cbsTtEr,cbsTtErMaxRS,erRefuted,cbsAdaptBranch,adaptSingular,cbsPathmaxInsert,cbsTbSpec,cbsRestart,restartStage,cbsRestartKeepLB,restartFloor,cbsLearn,cbsLearnPrior,learnTries,learnRaises,learnPickDiff,cbsFastFirst,fastFirstSkipped,bapMrvs,ubSeed,leftshiftGlobal,domBpSetKey,cbsMirror,propFast,cbsDelay,cbsDelayMaxKids,relInfeasible,relFallback,relMaxLen,farBlocked,farApplied,propInc,cbsFreeFix,freeFixKids,freeFixResolved,freeFixProp,freeFixMax,freeFixTries,freeFixTest,freeFixShare,freeFixLazy,freeFixFinalFail,freeFixStop,freeFixGoal,freeFixMaxOver,freeFixSgs,freeFixMaxConf,freeFixSgsCalls,freeFixRand,freeFixRandConf"
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
            if (!onlyInstance(problemType, i, j)) continue;
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
    file << "group,exam,time,solved,makespan,expandNumber,generatedNumber,depth,model,problemType,maxMem,LB,useTT2DR5,useTT2Batch,tt2BatchCap,domPruned,domThinned,domChecks,domInserts,domMaxBucket,useTT2Sym,symPruned,useTT2Gendesc,useTT2SymTB,timeoutS,trivialAtRoot,useThetaBound,thetaBoundBetter,useTT2DR4,dr4Pruned,useTT2ImmSel,immSelFired,useTT2RSAdapt,tt2RsThreshold,instanceRS,useTT2SingleRes,tt2SingleResBetter,tt2SingleResCalls,heuristicLowRS,heuristicHighRS,rootH,heurTimeSec,heurCalls,singleResTimeSec,subsolver,shadowProbes,shadowHits,useTT2UB,ubPruned,useTT2HierRS,tt2HierRsBetter,tt2HierRsCalls,tt2HierRsTimeSec,tt2HierRsCacheHits,direction,useTT2Prof,profAstarSec,profSuccSec,profSuccCalls,profAvailSec,profGenSec,profDR5Sec,profHashSec,profHashCalls,profHeurCPSec,profHeurResSec,hCallsAll,hKnownHits,hDeltaZeroHits,childrenRaw,childrenKept,domBuckets,domComparisons,openAtEnd,itemsAtEnd,uniqueExpanded,maxFExpanded,fAt1s,fAt10s,fAt60s,fAt120s,fAt240s,timedOut,cpFast,resFast,dr5Fast,dr5Sorted,genFast,dr5Mtf,waitModel,waitEdges,waitCut,dr5WaitExempt,waitPass,waitJump,waitJumped,rootDlbOn,rootSeedOn,rootLBPlain,rootLB,rootLBSec,rootLBBudgetHit,seedUB,seedSec,closedAtRoot,rootProbeOn,probeSeedIn,probeBest,probeNodes,probeExhausted,probeSec,tt2NodeTT,nodeTtCalls,nodeTtRaised,nodeTtRaiseSum,nodeTtTt0Fail,nodeTtPropFail,nodeTtSec,nodeTtWinPruned,tt2Win,tt2WinLs,tt2Ser,nodeTtSerFail,tt2UbGe,tt2UbClosed,rootLnsOn,lnsSeedIn,lnsBest,lnsIters,lnsImpCp,lnsImpSgs,lnsSec,tt2TbSpec,tt2Compact,compactRebuilds,compactChecked,tt2Precheck,precheckCut"
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
           "timeoutS,instanceRS,useTT2DR5,biDR4,dr4mask,meetNorm,meets,firstMeetAtExp,uFromMeet,ubFound"
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
              << g_instance_rs << ",1," << g_bae_biDR4 << "," << g_bae_dr4mask << "," << (g_meet_norm ? 1 : 0) << ","
              << g_bae_meets << "," << g_bae_firstMeet << "," << g_bae_uMeet << "," << mk
              << std::endl;
        }
    }
    std::cout << std::endl << "BAE* benchmark done (" << problemType << ") -> " << filename << std::endl;
}

// ── BAE2 full benchmark for one size (modes "tt2bae2_j30" / "_j60" / "_j90") ──
// One row per instance, 48 groups x 10 exams, with the diagnostics that decide whether
// bidirectional is worth it on this domain:
//   expandedF / expandedB and expRatioBF  — the DIRECTION ASYMMETRY. BAE* pays for the
//     harder direction while unidirectional gets to pick the cheaper one, so this ratio
//     is the single best predictor of whether BAE* can win on an instance.
//   meets, firstMeetAtExp / firstMeetFrac — did the middle meet fire, and how early.
//   bestMeetDepth                          — WHERE the meet that set the incumbent sits,
//     as #started/NT (0 = source, 1 = goal, 0.5 = true middle). Depth, not g/opt: one long
//     activity can eat most of the makespan while the search barely advances.
//   meetDepthMin/Mean/Max                  — the spread of all meeting points.
//   bestMeetGF / bestMeetGB                — the g split at that meet (time, both sides).
//   ubSource                               — "meet" (genuinely bidirectional), "goal" (a
//     side completed, i.e. it degenerated to unidirectional), "trivial", or "none".
// solved is True ONLY when optimality was PROVEN; a deadline exit still reports its
// incumbent in ub, so unsolved rows carry a valid upper bound.
// ── HBAE full benchmark for one size (modes "tt2hbae_j30" / "_j60" / "_j90") ──
// HOG2's own BAE* over the forward net N and the reversed net N^R, meeting on the normalised
// cut key. One row per instance, 48 groups x 10 exams.
// READ `solved` CAREFULLY: it is True only when optimality was PROVEN. A `queue-empty` stop
// under DOMINANCE cannot certify — dominance discards states the opposite side needed to meet
// on, so the frontier can drain without ever meeting the optimal cut, and the incumbent held
// at that moment is only an UPPER BOUND. Those rows get makespan=-1 with the bound in `ub`.
// `stopReason` (bound / queue-empty / trivial) says which happened.
// `meetDepth` is #started/NT of the node the incumbent was found at: 0 = source, 1 = goal,
// 0.5 = true middle. Depth, not g/opt, because one long activity can eat most of the makespan
// while the search barely advances.
// Known optimum for an instance, or -1 if none is on file. j30 has a full optima table;
// j60/j90 only where the bounds file marks the LB as proven optimal. Same sources
// getDatasheetUB uses -- this is validation only, never fed to the search.
// ── tt2trace <type> <group> <exam> ───────────────────────────────────────────
// Collect each side's KNOWN optimal path, then run HBAE with the tracer live and
// report what the bidirectional search did with every state on those paths.
static void ppCollect(int side, bool reversed, int group, int exam, const std::string& ptype) {
    getPetri(petri, group, exam, ptype); getRCPSP(RCPSPex, group, exam, ptype);
    if (reversed) reverseLoadedInstance();
    { static const double RSL[4]={0.2,0.5,0.7,1.0}; g_instance_rs = RSL[((group-1)%4+4)%4]; }
    const int NT = (int)petri.Transitions.size();
    deriveEndpointNames(initialstatename, finalstatename);
    get_tt2_dominance_table().clear();
    RCPSP_TT2 env; RCPSPState_TT2 first; RCPSPState_TT2 last = first;
    last.g = HCost_TT2(last, first);
    TemplateAStar<RCPSPState_TT2, int, RCPSP_TT2> astar;
    std::vector<RCPSPState_TT2> path;
    astar.GetPath(&env, first, last, path);
    PathProbe& P = g_pp[side];
    P.keys.clear(); P.g.clear(); P.idx.clear();
    for (auto& st : path) { P.keys.push_back(ppIdKey(st, NT)); P.g.push_back((int)st.g); }
    for (size_t i = 0; i < P.keys.size(); ++i) P.idx[P.keys[i]] = (int)i;
    P.reset(P.keys.size());
    std::cout << "  " << (reversed ? "REVERSED" : "FORWARD ") << " optimal path: "
              << path.size() << " states, C*="
              << (path.empty() ? -1 : (int)path.back().g) << std::endl;
}
static void ppReport(const char* label, int side) {
    PathProbe& P = g_pp[side];
    std::cout << "  --- " << label << " (" << P.keys.size() << " states on the optimal path) ---"
              << std::endl;
    if (P.keys.empty()) { std::cout << "    no path collected" << std::endl; return; }
    std::cout << "    idx    g   gen   add   exp   rej   nip   verdict" << std::endl;
    int never = 0, firstLost = -1, rejected = 0, nipped = 0, expanded = 0;
    for (size_t i = 0; i < P.keys.size(); ++i) {
        const auto& e = P.ev[i];
        const bool seen = e[PP_GEN] || e[PP_ADD] || e[PP_EXPAND];
        const char* v = e[PP_EXPAND] ? "expanded"
                      : e[PP_ADD]    ? "opened, never expanded"
                      : e[PP_REJECT] ? "REJECTED by f>=U"
                      : e[PP_NIPPED] ? "NIPPED (closed unexpanded)"
                      : e[PP_GEN]    ? "generated, never opened"
                                     : "NEVER SEEN";
        if (!seen) { ++never; if (firstLost < 0) firstLost = (int)i; }
        if (e[PP_EXPAND]) ++expanded;
        if (e[PP_REJECT]) ++rejected;
        if (e[PP_NIPPED]) ++nipped;
        std::cout << "    " << std::setw(3) << i << std::setw(5) << P.g[i]
                  << std::setw(6) << e[PP_GEN] << std::setw(6) << e[PP_ADD]
                  << std::setw(6) << e[PP_EXPAND] << std::setw(6) << e[PP_REJECT]
                  << std::setw(6) << e[PP_NIPPED] << "   " << v;
        if (e[PP_REJECT] && P.rejU[i] >= 0)
            std::cout << "  [g=" << P.rejG[i] << " h=" << P.rejH[i]
                      << " f=" << (P.rejG[i] + P.rejH[i]) << " U=" << P.rejU[i] << "]";
        std::cout << std::endl;
    }
    std::cout << "    probe calls on this side: " << g_pp_calls[side] << std::endl;
    if (never == (int)P.keys.size()) {
        std::cout << "    KEY MISMATCH -- nothing matched. Comparing:" << std::endl;
        std::cout << "      collected[0] = " << P.keys[0].substr(0, 220) << std::endl;
        std::cout << "      searched [0] = " << g_pp_sample[side].substr(0, 220) << std::endl;
    }
    std::cout << "    SUMMARY: expanded=" << expanded << "  rejected=" << rejected
              << "  nipped=" << nipped << "  never-seen=" << never;
    if (firstLost >= 0) std::cout << "   FIRST LOST AT idx " << firstLost;
    std::cout << std::endl;
}
int solveRCPSP_TT2_Trace(int group, int exam, const std::string& ptype) {
    std::cout << "=== OPTIMAL-PATH TRACE " << group << ":" << exam << " ===" << std::endl;
    bool s5 = g_tt2_dr5, s4 = g_tt2_dr4, sb = g_tt2_batch;
    // Collect with DR5 on purely for speed -- any optimal path serves as the witness,
    // and HBAE itself runs dominance-free, so nothing on it is legitimately prunable.
    g_instance_deadline = std::chrono::steady_clock::now()
                        + std::chrono::seconds(astar_timeout_seconds);
    g_instance_deadline_set = true;
    g_tt2_dr5 = true; g_tt2_dr4 = false; g_tt2_batch = false;
    ppCollect(0, false, group, exam, ptype);
    ppCollect(1, true,  group, exam, ptype);
    g_tt2_dr5 = s5; g_tt2_dr4 = s4; g_tt2_batch = sb;
    g_instance_deadline = std::chrono::steady_clock::now()
                        + std::chrono::seconds(astar_timeout_seconds);
    g_pp_on = true;
    solveRCPSP_TT2_HBAE(group, exam, ptype);
    g_pp_on = false;
    ppReport("FORWARD side", 0);
    ppReport("BACKWARD side (reversed net)", 1);
    return 0;
}

static int hbaeReferenceOptimum(int group, int exam, const std::string& ptype) {
    if (ptype == "j30") { int o = getOptimalMakespan(group, exam, ptype); return o > 0 ? o : -1; }
    Bounds b = getBounds(group, exam, ptype);
    return (b.optimal_known && b.lb > 0) ? b.lb : -1;
}

void runBenchmarkTT2HBAE(const std::string& problemType) {
    std::string filename = outName("new_results", "output_tt2hbae_" + problemType + "_", ".csv");
    { std::ofstream f(filename);
      if (!f.is_open()) { std::cerr << "Error opening file: " << filename << std::endl; return; }
      f << "group,exam,time,solved,makespan,ub,proven,stopReason,expandNumber,uniqueExpanded,"
           "touched,necessary,reopened,solUpdates,meetDepth,meetAt,pathLen,trivialAtRoot,"
           "model,problemType,timeoutS,instanceRS,useTT2DR5,dr4,batch,prio,alt,tiebreak,meetNorm,nip,noub,fbound,drainCert,expandedF,expandedB,timeFSec,timeBSec,openFEnd,openBEnd,itemsFEnd,itemsBEnd,meetIdxF,meetIdxB,minFFEnd,minFBEnd,minFF60s,minFB60s,minFF120s,minFB120s,minFF240s,minFB240s,expAtMaxBound,uAtMaxBound,peakRSSMB,dr5PrunedF,dr5PrunedB,dr5InsertsF,dr5InsertsB,dr4Pruned,useTT2Prof,heurTimeSec,heurCalls,profSuccSec,profAvailSec,profGenSec,profDR5Sec,profHeurCPSec,profHeurResSec,hDeltaZeroHits,childrenRaw,childrenKept,biHashSec,biHashCalls,meetHashSec,meetHashCalls,meetSide,meetGF,meetGB,meetTimeFrac,meetExp,firstMeetExp,firstU,meetHits,maxDepthF,maxDepthB,maxGF,maxGB,fBoundCalls,fBoundSec,minfHeap,bwdModel,bwdFullVerify,hfuse,strHash,bxPopSec,bxSuccSec,bxChildSec,bxHeurSec,bxAddSec,cpFast,resFast,dr5Fast,dr5Sorted,genFast,dr5Mtf,domMeet,meetGhost,dmUpdates,dmInserts,dmSkipped,dmThinned,dmJoinCmp,dmEntriesF,dmEntriesB,shiftMeet,dmShiftBest,stopRule,maxStop,waitModel,dr4Side,waitEdges,waitCut,waitPass,waitJump,waitJumped,waitMeetOnly,meetOnlySide,dr5WaitExemptF,dr5WaitExemptB,audit,auditStatus,auditHorizon,auditFwdExact,auditBwdExact,auditBothExact,auditCrossExp,auditMeetExp,auditMissed,auditBothDom,auditFwdMaxT,auditBwdMinT,auditFwdHoles,auditBwdHoles,activeTB,hFresh,fDropF,fDropB,fDropMax,dzFirst,rootDlbOn,rootSeedOn,rootLBPlain,rootLB,rootLBSec,rootLBBudgetHit,seedUB,seedSec,closedAtRoot,rootProbeOn,probeSeedIn,probeBest,probeNodes,probeExhausted,probeSec,tt2NodeTT,nodeTtCalls,nodeTtRaised,nodeTtRaiseSum,nodeTtTt0Fail,nodeTtPropFail,nodeTtSec,nodeTtWinPruned,tt2Win,tt2WinLs,tt2Ser,nodeTtSerFail,tt2UbGe,tt2UbClosed,rootLnsOn,lnsSeedIn,lnsBest,lnsIters,lnsImpCp,lnsImpSgs,lnsSec,tt2TbSpec"
        << std::endl; }
    std::cout << std::endl << "=== TT2 HBAE | " << problemType << " ===" << std::endl;
    auto envi = [](const char* k, int dflt){ const char* e=std::getenv(k); return e?std::atoi(e):dflt; };
    // RCPSP_BI_ASSERT: 0 = record only, 1 = (default) stop on a FALSE RESULT --
    // a certified makespan that is not the known optimum, or an incumbent BELOW it
    // (impossible: every incumbent is a real g_f+g_b concatenation, so it can only be
    // >= optimal). 2 = also stop on any instance that fails to prove optimality.
    const int assertLvl = envi("RCPSP_BI_ASSERT", 1);
    // actual stop/certification switches, recorded on every row (same parsing as BAE_RCPSP.h)
    { const char* e;
      e = std::getenv("RCPSP_BI_NIP");       g_hb_nip       = (e && std::atoi(e) == 0) ? 0 : 1;
      e = std::getenv("RCPSP_BI_NOUB");      g_hb_noub      = e ? (std::atoi(e) != 0) : 1;
      e = std::getenv("RCPSP_BI_FBOUND");    g_hb_fbound    = e ? (std::atoi(e) != 0) : 1;
      e = std::getenv("RCPSP_BI_DRAINCERT"); g_hb_draincert = (e && std::atoi(e) != 0) ? 1 : 0; }
    int checked = 0, unproven = 0;
    for (int i = 1; i <= 48; i++) {
        for (int j = 1; j <= 10; j++) {
            if (!onlyInstance(problemType, i, j)) continue;
            solveRCPSP_TT2_HBAE(i, j, problemType);
            std::ofstream f(filename, std::ios::app);
            f << i << "," << j << "," << g_hb_time << ","
              << (g_hb_proven ? "True" : "False") << ","
              << g_hb_mk << "," << g_hb_ub << "," << (g_hb_proven ? 1 : 0) << ","
              << g_hb_stop << "," << g_hb_exp << "," << g_hb_unique << ","
              << g_hb_touched << "," << g_hb_necessary << "," << g_hb_reopened << ","
              << g_hb_solUpd << "," << g_hb_meetDepth << "," << g_hb_meetAt << ","
              << g_hb_pathLen << "," << (g_hb_trivial ? 1 : 0) << (g_bi_unfire ? ",TT2bi-unfire," : g_tt2_wait ? ",TT2Wbi," : ",TT2bi,") << problemType << ","
              << astar_timeout_seconds << "," << g_instance_rs << ","
              << g_hb_dr5mask << "," << g_hb_dr4f << ","
              << g_hb_batchf << "," << envi("RCPSP_BI_PRIO", 0) << ","
              << envi("RCPSP_BI_ALT", 0) << "," << envi("RCPSP_BI_TIEBREAK", 0) << ","
              << (g_meet_norm ? 1 : 0) << "," << g_hb_nip << "," << g_hb_noub << ","
              << g_hb_fbound << "," << g_hb_draincert << ","
              << g_hb_expF << "," << g_hb_expB << "," << g_hb_timeF << "," << g_hb_timeB << ","
              << g_hb_openF << "," << g_hb_openB << "," << g_hb_itemsF << "," << g_hb_itemsB << ","
              << g_hb_meetIdxF << "," << g_hb_meetIdxB << "," << g_hb_minFF << "," << g_hb_minFB << ","
              << g_hb_minFAt[0][0] << "," << g_hb_minFAt[0][1] << "," << g_hb_minFAt[1][0] << "," << g_hb_minFAt[1][1] << ","
              << g_hb_minFAt[2][0] << "," << g_hb_minFAt[2][1] << "," << g_hb_expAtMax << "," << g_hb_uAtMax << ","
              << g_hb_rssMB << "," << g_hb_domPrunedF << "," << g_hb_domPrunedB << "," << g_hb_domInsertsF << "," << g_hb_domInsertsB << ","
              << g_tt2_dr4_pruned << "," << (g_tt2_prof ? 1 : 0) << "," << g_heur_time_sec << "," << g_heur_calls << ","
              << g_prof_succ_sec << "," << g_prof_avail_sec << "," << g_prof_gen_sec << "," << g_prof_dr5_sec << ","
              << g_prof_hcp_sec << "," << g_prof_hres_sec << "," << g_prof_h_deltazero << ","
              << g_prof_children_raw << "," << g_prof_children_kept << ","
              << g_hb_bihash_sec << "," << g_hb_bihash_calls << "," << g_hb_meethash_sec << "," << g_hb_meethash_calls << ","
              << g_hb_meetSide << "," << g_hb_meetGF << "," << g_hb_meetGB << "," << g_hb_meetTimeFrac << ","
              << g_hb_meetExp << "," << g_hb_firstMeetExp << "," << g_hb_firstU << "," << g_hb_meetHits << ","
              << g_hb_maxDepthF << "," << g_hb_maxDepthB << "," << g_hb_maxGF << "," << g_hb_maxGB << ","
              << g_hb_fBoundCalls << "," << g_hb_fBoundSec << "," << g_hb_minfHeap << ","
              << (g_bi_unfire ? "unfire" : "reversed") << "," << (g_bi_unfire ? (BWMEET::g_bwd_fullverify ? 1 : 0) : -1) << ","
              << g_hb_hfuse << "," << g_hb_strhash << "," << g_hb_bx[0] << "," << g_hb_bx[1] << ","
              << g_hb_bx[2] << "," << g_hb_bx[3] << "," << g_hb_bx[4] << "," << (cpFastOn() ? 1 : 0) << "," << (resFastOn() ? 1 : 0) << "," << (TT2DominanceTable::FastOn() ? 1 : 0) << "," << (TT2DominanceTable::SortedOn() ? 1 : 0) << "," << (genFastOn() ? 1 : 0) << "," << (TT2DominanceTable::MtfOn() ? 1 : 0) << ","
              << g_hb_domMeet << "," << g_hb_meetGhost << "," << g_hb_dmUpdates << "," << g_hb_dmInserts << ","
              << g_hb_dmSkipped << "," << g_hb_dmThinned << "," << g_hb_dmJoinCmp << "," << g_hb_dmEntF << "," << g_hb_dmEntB << "," << g_hb_shiftMeet << "," << g_hb_dmShiftBest << "," << (g_hb_stopMax ? "max" : "avg") << "," << g_hb_maxStop << ","
              << (g_tt2_wait ? 1 : 0) << "," << g_hb_dr4side << "," << g_tt2_wait_edges << "," << g_tt2_wait_cut << "," << g_tt2_wait_pass << ","
              << (g_tt2_waitjump ? 1 : 0) << "," << g_tt2_wait_jumped << "," << g_tt2_wait_meetonly << "," << biMeetOnlyMask() << ","
              << g_hb_waitExemptF << "," << g_hb_waitExemptB << "," << (BAE_RCPSP<RCPSPState_BI, int, RCPSP_BiEnv>::AuditOn() ? 1 : 0) << ","
              << g_hb_auditStatus << "," << g_hb_auditHorizon << "," << g_hb_auditFwd << "," << g_hb_auditBwd << "," << g_hb_auditBoth << ","
              << g_hb_auditCrossExp << "," << g_hb_auditMeetExp << "," << g_hb_auditMissed << "," << g_hb_auditBothDom << ","
              << g_hb_auditFwdMaxT << "," << g_hb_auditBwdMinT << "," << g_hb_auditFwdHoles << "," << g_hb_auditBwdHoles << ","
              << envi("RCPSP_BI_ACTIVETB", 0) << "," << g_hb_hfresh << "," << g_hb_fDropF << "," << g_hb_fDropB << "," << g_hb_fDropMax << "," << envi("RCPSP_BI_DZFIRST", 1)
              << "," << (rootb::DlbOn() ? 1 : 0) << "," << (rootb::SeedOn() ? 1 : 0) << "," << rootb::info().lbPlain << "," << rootb::info().lb
      << "," << rootb::info().lbSec << "," << (rootb::info().budgetHit ? 1 : 0) << "," << rootb::info().seed << "," << rootb::info().seedSec
      << "," << (rootb::info().closed ? 1 : 0)
      << "," << (rootb::ProbeOn() ? 1 : 0) << "," << rootb::info().probeSeedIn << "," << rootb::info().probeBest << "," << rootb::info().probeNodes
      << "," << rootb::info().probeExhausted << "," << rootb::info().probeSec << "," << (tt2nb::On() ? 1 : 0) << "," << tt2nb::st().calls << "," << tt2nb::st().raised << "," << tt2nb::st().raiseSum << "," << tt2nb::st().tt0Fail << "," << tt2nb::st().propFail << "," << tt2nb::st().sec << "," << tt2nb::st().winPruned << "," << (tt2nb::WinOn() ? 1 : 0) << "," << (tt2nb::WinLsOn() ? 1 : 0) << "," << (tt2nb::SerOn() ? 1 : 0) << "," << tt2nb::st().serFail << "," << (g_tt2_ub_ge ? 1 : 0) << "," << (g_tt2_ub_closed ? 1 : 0) << "," << rootb::LnsMode() << "," << rootb::info().lnsSeedIn << "," << rootb::info().lnsBest << "," << rootb::info().lnsIters << "," << rootb::info().lnsImpCp << "," << rootb::info().lnsImpSgs << "," << rootb::info().lnsSec << "," << (tt2TbSpec().empty() ? "-" : tt2TbSpec()) << std::endl;
            f.flush();

            const int refOpt = hbaeReferenceOptimum(i, j, problemType);
            const char* why = nullptr;
            if (refOpt > 0) {
                ++checked;
                if (g_hb_proven && g_hb_mk != refOpt)
                    why = (g_hb_mk > refOpt) ? "CERTIFIED A MAKESPAN WORSE THAN THE OPTIMUM"
                                             : "CERTIFIED A MAKESPAN BELOW THE OPTIMUM";
                else if (g_hb_ub >= 0 && g_hb_ub < refOpt)
                    why = "INCUMBENT BELOW THE KNOWN OPTIMUM (impossible - the meet is wrong)";
            }
            if (!g_hb_proven) ++unproven;
            if (!why && assertLvl >= 2 && !g_hb_proven)
                why = "SEARCH DID NOT FINISH (no proof of optimality)";
            if (!why && (std::string(g_hb_auditStatus) == "INFEASIBLE" || std::string(g_hb_auditStatus) == "INCONSISTENT"))
                why = "MEETING AUDIT: the schedule rebuilt from the meet is not a feasible schedule of that makespan";

            if (why) {
                const char* BAR = "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!";
                std::cout << BAR
                          << "\nFALSE RESULT  " << problemType << " " << i << ":" << j
                          << "  -- " << why
                          << "\n  reported makespan = " << g_hb_mk << "   ub = " << g_hb_ub
                          << "   known optimum = " << refOpt
                          << "\n  proven = " << (g_hb_proven ? "1" : "0")
                          << "   stopReason = " << g_hb_stop
                          << "   meetAt = " << g_hb_meetAt
                          << "\n  flags: dr5mask=" << g_hb_dr5mask << " dr4=" << g_hb_dr4f
                          << " batch=" << g_hb_batchf << " prio=" << envi("RCPSP_BI_PRIO", 0)
                          << " wait=" << (g_tt2_wait ? 1 : 0) << " dr4side=" << g_hb_dr4side << " audit=" << g_hb_auditStatus
                          << "\n  expanded = " << g_hb_exp << "   time = " << g_hb_time << "s"
                          << "\n  rows so far: " << checked << " checked, " << unproven << " unproven."
                          << "\n  partial CSV -> " << filename
                          << "\n  set RCPSP_BI_ASSERT=0 to record instead of stopping."
                          << "\n" << BAR << std::endl << std::flush;
                if (assertLvl >= 1) { f.flush(); f.close(); std::exit(3); }
            }
        }
    }
    std::cout << std::endl << "HBAE benchmark done (" << problemType << ") -> " << filename
              << "   [" << checked << " instances checked against known optima, "
              << unproven << " unproven]" << std::endl;
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
