//
// Created by idol on 29/12/2024.
//
// Your First C++ Program

#include <iostream>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <ctime>
#include "RCPSPState.cpp"
#include "../../HOG2/generic/TemplateAStar.h"
#include "../../HOG2/generic/BAE.h"

#include "RCPSP.h"

// Forward declarations for heuristic cache functions (defined in RCPSPState.cpp)
#ifdef RCPSP_ENABLE_HCACHE
void ClearHCostCache();
void PrintCacheStats();
void InitHCostCacheForProblem(const std::string& problemType, int group, int exam);
void FinalizeHCostCacheForProblem(const std::string& problemType, int group, int exam);
bool SaveHCostCacheToDisk(const std::string& problemType, int group, int exam);
bool LoadHCostCacheFromDisk(const std::string& problemType, int group, int exam);
bool HCostCacheExistsOnDisk(const std::string& problemType, int group, int exam);
void SetHCostCacheEnabled(bool enabled);
bool IsHCostCacheEnabled();
uint64_t GetHCostCacheHits();
uint64_t GetHCostCacheMisses();
size_t GetHCostCacheSize();
#else
inline void ClearHCostCache() {}
inline void PrintCacheStats() {}
inline void InitHCostCacheForProblem(const std::string&, int, int) {}
inline void FinalizeHCostCacheForProblem(const std::string&, int, int) {}
inline bool SaveHCostCacheToDisk(const std::string&, int, int) { return false; }
inline bool LoadHCostCacheFromDisk(const std::string&, int, int) { return false; }
inline bool HCostCacheExistsOnDisk(const std::string&, int, int) { return false; }
inline void SetHCostCacheEnabled(bool) {}
inline bool IsHCostCacheEnabled() { return false; }
inline uint64_t GetHCostCacheHits() { return 0; }
inline uint64_t GetHCostCacheMisses() { return 0; }
inline size_t GetHCostCacheSize() { return 0; }
#endif
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

void runBenchmark(const std::string& problemType, int groupStart, int groupEnd, int examStart, int examEnd, bool useCache);
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
namespace fs = std::filesystem;

int solveRCPSP(int group, int exam, const std::string& filename,const std::string& problemType="j30") {
    std::cout << "started solving: " << group<<":"<<exam << std::endl;

    generateTIME= std::chrono::duration<double>(0);
    avelableTIME= std::chrono::duration<double>(0);
    HTIME= std::chrono::duration<double>(0);
    hashTIME= std::chrono::duration<double>(0);
    comperTime= std::chrono::duration<double>(0);
    secssesorTIME= std::chrono::duration<double>(0);
    clock_t setupTIME = clock();

    getPetri(petri, group, exam,problemType);
    getRCPSP(RCPSPex, group, exam,problemType);

    RCPSPex.computeAndStoreDeepDependencies();

    RCPSPState first;
    RCPSPState last = first;
    //last.h = 0;

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

#ifdef RCPSP_ENABLE_HCACHE
    const bool cacheRuntimeEnabled = IsHCostCacheEnabled();
    uint64_t cacheHitsBefore = cacheRuntimeEnabled ? GetHCostCacheHits() : 0;
    uint64_t cacheMissesBefore = cacheRuntimeEnabled ? GetHCostCacheMisses() : 0;
#else
    const bool cacheRuntimeEnabled = false;
    uint64_t cacheHitsBefore = 0;
    uint64_t cacheMissesBefore = 0;
#endif

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

#ifdef RCPSP_ENABLE_HCACHE
        uint64_t cacheHitsAfter = cacheRuntimeEnabled ? GetHCostCacheHits() : 0;
        uint64_t cacheMissesAfter = cacheRuntimeEnabled ? GetHCostCacheMisses() : 0;
        size_t cacheSizeAfter = cacheRuntimeEnabled ? GetHCostCacheSize() : 0;
        uint64_t cacheHitsDelta = (cacheRuntimeEnabled && cacheHitsAfter >= cacheHitsBefore)
                                                                    ? (cacheHitsAfter - cacheHitsBefore)
                                                                    : 0;
        uint64_t cacheMissesDelta = (cacheRuntimeEnabled && cacheMissesAfter >= cacheMissesBefore)
                                                                        ? (cacheMissesAfter - cacheMissesBefore)
                                                                        : 0;
#else
        size_t cacheSizeAfter = 0;
        uint64_t cacheHitsDelta = 0;
        uint64_t cacheMissesDelta = 0;
#endif

        auto boolToString = [](bool value) { return value ? "True" : "False"; };

    std::ofstream file(filename, std::ios::app);
        file << group << "," << exam << "," << elapsed.count() << ","
                 << boolToString(!path.empty()) << ","
                 << makespan << ","
                 << astar.GetNodesExpanded() << ","
                 << astar.GetNodesTouched() << ","
                 << path.size() << ","
                 << "TP" << ","
                 << problemType << ","
                 << boolToString(useCS) << ","
                 << boolToString(cacheRuntimeEnabled) << ","
                 << cacheHitsDelta << ","
                 << cacheMissesDelta << ","
                 << cacheSizeAfter << ","
                 << 100 * generateTIME.count() / elapsed.count() << ","
                 << generateTIME.count() / astar.GetNodesTouched() << ","
                 << 100 * avelableTIME.count() / elapsed.count() << ","
                 << avelableTIME.count() / astar.GetNodesTouched() << ","
                 << 100 * hashTIME.count() / elapsed.count() << ","
                 << hashTIME.count() / astar.GetNodesTouched() << ","
                 << 100 * HTIME.count() / elapsed.count() << ","
                 << HTIME.count() / count << ","
                 << 100 * comperTime.count() / elapsed.count() << ","
                 << comperTime.count() / astar.GetNodesTouched() << ","
                 << 100 * secssesorTIME.count() / elapsed.count() << ","
                 << secssesorTIME.count() / count
                 << "\n";





    return 0;
}
    int solveRCPSP_TT(int group, int exam, const std::string& filename,const std::string& problemType="j30") {
    std::cout << "started solving: " << group<<":"<<exam << std::endl;
    count=0;
    getPetri(petri, group, exam,problemType);
    getRCPSP(RCPSPex, group, exam,problemType);

    RCPSPState_TT first;
    RCPSPState_TT last = first;



    RCPSP_TT as1;

    TemplateAStar<RCPSPState_TT, int, RCPSP_TT> astar;
    std::vector<RCPSPState_TT> path;


    std::chrono::duration<double> elapsed;

#ifdef RCPSP_ENABLE_HCACHE
    const bool cacheRuntimeEnabled = IsHCostCacheEnabled();
    uint64_t cacheHitsBefore = cacheRuntimeEnabled ? GetHCostCacheHits() : 0;
    uint64_t cacheMissesBefore = cacheRuntimeEnabled ? GetHCostCacheMisses() : 0;
#else
    const bool cacheRuntimeEnabled = false;
    uint64_t cacheHitsBefore = 0;
    uint64_t cacheMissesBefore = 0;
#endif

    auto start = std::chrono::high_resolution_clock::now();
    astar.GetPath(&as1, first, last, path);

    auto end = std::chrono::high_resolution_clock::now();
    elapsed = end - start;

    int makespan = 0;

    if (!path.empty()) {
        std::cout << "Path found!" << std::endl;

        //for (const auto& state : path) {
        RCPSPState_TT state=path.back();
            //std::cout << "g: " << state.g;

            for (const auto& [actId, startTime] : state.startedActivitiys) {
                std::cout << actId << ":" << startTime << " ";
            }

            std::cout << std::endl;
            makespan = state.g;
        //}

        std::cout << "\nFinal makespan: " << makespan << std::endl;
    }
     else {
        std::cout << "Path not found or timeout occurred.\n";
    }

    std::cout << "Nodes Expanded: " << astar.GetNodesExpanded() << std::endl;
    std::cout << "Nodes Touched: " << astar.GetNodesTouched() << std::endl;

#ifdef RCPSP_ENABLE_HCACHE
    const size_t cacheSizeAfter = cacheRuntimeEnabled ? GetHCostCacheSize() : 0;
    const uint64_t cacheHitsDelta = cacheRuntimeEnabled ? (GetHCostCacheHits() - cacheHitsBefore) : 0;
    const uint64_t cacheMissesDelta = cacheRuntimeEnabled ? (GetHCostCacheMisses() - cacheMissesBefore) : 0;
#else
    const size_t cacheSizeAfter = 0;
    const uint64_t cacheHitsDelta = 0;
    const uint64_t cacheMissesDelta = 0;
#endif

    auto boolToString = [](bool value) { return value ? "True" : "False"; };

    std::ofstream file(filename, std::ios::app);
    file << group << "," << exam << "," << elapsed.count() << ","
         << boolToString(!path.empty()) << ","
         << makespan << ","
         << astar.GetNodesExpanded() << ","
         << astar.GetNodesTouched() << ","
         << path.size() << ","
         << "TT" << ","
         << problemType << ","
         << boolToString(useCS) << ","
         << boolToString(cacheRuntimeEnabled) << ","
         << cacheHitsDelta << ","
         << cacheMissesDelta << ","
         << cacheSizeAfter << ","
         << 100 * generateTIME.count() / elapsed.count() << ","
         << generateTIME.count() / astar.GetNodesTouched() << ","
         << 100 * avelableTIME.count() / elapsed.count() << ","
         << avelableTIME.count() / astar.GetNodesTouched() << ","
         << 100 * hashTIME.count() / elapsed.count() << ","
         << hashTIME.count() / astar.GetNodesTouched() << ","
         << 100 * HTIME.count() / elapsed.count() << ","
         << HTIME.count() / count << ","
         << 100 * comperTime.count() / elapsed.count() << ","
         << comperTime.count() / astar.GetNodesTouched() << ","
         << 100 * secssesorTIME.count() / elapsed.count() << ","
         << secssesorTIME.count() / count
         << "\n";

    return 0;
}
//not working
int solveRCPSP_Bi(int group, int exam, const std::string& filename) {
    std::cout << "started solving: " << group<<":"<<exam << std::endl;

  //  generateTIME= std::chrono::duration<double>(0);
    //avelableTIME= std::chrono::duration<double>(0);
   // hashTIME= std::chrono::duration<double>(0);
  //  comperTime= std::chrono::duration<double>(0);
    //secssesorTIME= std::chrono::duration<double>(0);
    count=0;

    getPetri(petri, group, exam);
    getRCPSP(RCPSPex, group, exam);

    RCPSPState_bi first;
    first.direction=true;
    count=2;
    RCPSPState_bi last = first;
    last.direction=false;
    last.h_b = first.h_f;
    last.h_f = 0;
last.name=1;
    for (auto& pair : last.marking) {
        //set only to to 4 diffrent resources
        if (pair.first=="R1"){continue;}
        if (pair.first=="R2"){continue;}
        if (pair.first=="R3"){continue;}
        if (pair.first=="R4"){continue;}
        if (pair.second == 1) { pair.second = 0; }
        if (pair.first == finalstatename) { pair.second = 1; }
    }
    last.avilableDeTransitionIndices=getAvilableDetransitionIndices(last.marking);
    //last.avilableTransitionIndices=getAvilableTransitionIndices(last.marking);
    for (int i=1;i<petri.Transitions.size()+1;i++) {
        last.finishedActivitiys.insert(i);
        last.startedActivitiys.insert(i);
    }
    last.unstartedTransitions.clear();
    std::vector<RCPSPState_bi> path;
    ForwardRCPSPHeuristic H_F;
    BackwardRCPSPHeuristic H_B;
    RCPSP_BiGreedy bs1;


    BAE<RCPSPState_bi, int, RCPSP_BiGreedy> Bi_RCPSP;



    bool finished = false;
    bool timeout_occurred = false;
    std::chrono::duration<double> elapsed;

    // Create a flag for thread completion

    auto start = std::chrono::high_resolution_clock::now();
    Bi_RCPSP.GetPath(&bs1, first, last,&H_F,&H_B ,path);



    // Record end time and calculate elapsed time
    auto end = std::chrono::high_resolution_clock::now();
    elapsed = end - start;


    // Output results
    double max_f = 0;
    double max_b = 0;

    if (!path.empty()) {
        finished=true;
        std::cout << "Path found!" << std::endl;
        for (const auto& state : path) {
            std::cout << "g_f: " << state.g_f << std::endl;
            std::cout << "g_b: " << state.g_b << std::endl;

            std::cout << "active: ";
            for (const auto& [transIdx, duration] : state.activeTransitionIndices)
                std::cout << " " << transIdx;
            std::cout << std::endl;

            std::cout << "available: ";
            for (int transIdx : state.avilableTransitionIndices)
                std::cout << " " << transIdx;
            std::cout << std::endl << std::endl;

            max_f = std::max(max_f,state.g_f);
            max_b = std::max(max_b,state.g_b);
        }
    } else {
        std::cout << "Path not found or timeout occurred.\n";
    }

    std::cout << "Nodes Expanded: " << Bi_RCPSP.GetNodesExpanded() << std::endl;
    std::cout << "Nodes Touched: " << Bi_RCPSP.GetNodesTouched() << std::endl;
    // Save to file
    std::ofstream file(filename, std::ios::app);
    file << group << "," << exam << "," << elapsed.count() << ","
         << (finished ? "True" : "False") << ","
         << max_f+max_b << ","
         << Bi_RCPSP.GetNodesExpanded() << ","
        //  << ","<<100*generateTIME.count()/elapsed.count()<< ","<<generateTIME.count()
         //    << ","<<100*avelableTIME.count()/elapsed.count()<< ","<<avelableTIME.count()
          //       << ","<<100*hashTIME.count()/elapsed.count()<< ","<<hashTIME.count()<<
                    // ","<<100*comperTime.count()/elapsed.count()<< ","<<comperTime.count()/astar.GetNodesTouched()<<
             "\n";

    return 0;
}



std::string buildResultsFilename(const std::string& folder,
                                 const std::string& problemType,
                                 int groupStart,
                                 int groupEnd,
                                 int examStart,
                                 int examEnd,
                                 bool cacheEnabled) {
    if (!fs::exists(folder)) {
        fs::create_directories(folder);
    }

    auto now = std::chrono::system_clock::now();
    std::time_t nowTimeT = std::chrono::system_clock::to_time_t(now);
    std::tm nowTm{};
#ifdef _WIN32
    localtime_s(&nowTm, &nowTimeT);
#else
    nowTm = *std::localtime(&nowTimeT);
#endif

    std::ostringstream prefix;
    prefix << folder << "/results_"
           << std::put_time(&nowTm, "%Y%m%d-%H%M%S")
           << '_' << problemType
           << "_g" << groupStart << '-' << groupEnd
           << "_e" << examStart << '-' << examEnd
           << "_cache-" << (cacheEnabled ? "on" : "off");

    std::string base = prefix.str();
    std::string filename = base + ".csv";
    int suffix = 1;
    while (fs::exists(filename)) {
        filename = base + "_" + std::to_string(suffix) + ".csv";
        ++suffix;
    }

    return filename;
}

//TODO: merge the redundant code here with the code in solveRCPSP function starting in the "// Reset and load problem" comment
// Precompute h-costs for a range of problems and save to disk
void precomputeHCosts(const std::string& problemType, int groupStart, int groupEnd, int examStart, int examEnd) {
#ifdef RCPSP_ENABLE_HCACHE
    std::cout << "=== H-Cost Precomputation Mode ===" << std::endl;
    std::cout << "Problem type: " << problemType << std::endl;
    std::cout << "Groups: " << groupStart << " to " << groupEnd << std::endl;
    std::cout << "Exams: " << examStart << " to " << examEnd << std::endl;
    std::cout << "=================================" << std::endl;
    
    for (int i = groupStart; i <= groupEnd; i++) {
        for (int j = examStart; j <= examEnd; j++) {
            std::cout << "\n--- Precomputing " << problemType << " group " << i << " exam " << j << " ---" << std::endl;
            
            // Check if cache already exists
            if (HCostCacheExistsOnDisk(problemType, i, j)) {
                std::cout << "[HCache] Cache already exists, loading to verify..." << std::endl;
                InitHCostCacheForProblem(problemType, i, j);
                PrintCacheStats();
                continue; // Skip to next problem
            }
            
            // Reset and load problem
            petri.reset();
            RCPSPex.reset();
            ClearHCostCache();
            
            getPetri(petri, i, j, problemType);
            getRCPSP(RCPSPex, i, j, problemType);
            RCPSPex.computeAndStoreDeepDependencies();
            
            // Set up initial and goal states
            RCPSPState first;
            RCPSPState last = first;
            
            for (int k = 0; k < last.marking.size(); ++k) {
                if (last.marking[k] == 1) {
                    last.marking[k] = 0;
                }
            }
            int finalID = petri.place_name_to_id.at(finalstatename);
            last.marking[finalID] = 1;
            
            // Run A* search to populate cache
            RCPSP as1;
            TemplateAStar<RCPSPState, int, RCPSP> astar;
            std::vector<RCPSPState> path;
            
            auto start = std::chrono::high_resolution_clock::now();
            astar.GetPath(&as1, first, last, path);
            auto end = std::chrono::high_resolution_clock::now();
            
            std::chrono::duration<double> elapsed = end - start;
            
            std::cout << "Solved in " << elapsed.count() << "s" << std::endl;
            std::cout << "Nodes expanded: " << astar.GetNodesExpanded() << std::endl;
            PrintCacheStats();
            
            // Save to disk
            SaveHCostCacheToDisk(problemType, i, j);
        }
    }
    
    std::cout << "\n=== Precomputation Complete ===" << std::endl;
#else
    std::cerr << "Error: H-cost caching is not enabled. Rebuild with -DRCPSP_ENABLE_HCACHE=ON" << std::endl;
#endif
}

static void printUsage() {
    std::cout << "Usage: rcpsp_driver [options]\n"
              << "Options:\n"
              << "  --precompute            Run in heuristic precomputation mode.\n"
              << "  --problem-type <type>   Problem type to solve (default: j30).\n"
              << "  --group-start <value>   First group to process (default: 1).\n"
              << "  --group-end <value>     Last group to process (default: 16).\n"
              << "  --exam-start <value>    First exam to process (default: 1).\n"
              << "  --exam-end <value>      Last exam to process (default: 10).\n"
              << "  --use-cache             Enable heuristic cache (default when available).\n"
              << "  --no-cache              Disable heuristic cache even if compiled in.\n"
              << "  -h, --help              Show this help message.\n";
}

int main(int argc, char* argv[]) {
    std::string problemType = "j30";
    int groupStart = 1;
    int groupEnd = 16;
    int examStart = 1;
    int examEnd = 10;
    bool precomputeMode = false;

#ifdef RCPSP_ENABLE_HCACHE
    bool cacheRequested = true;
#else
    bool cacheRequested = false;
#endif
    bool runtimeCacheEnabled = false;

    bool parseError = false;
    std::string parseErrorMessage;

    auto requireValue = [&](int index, const std::string& option) -> const char* {
        if (index + 1 >= argc) {
            parseError = true;
            parseErrorMessage = "Missing value for " + option;
            return nullptr;
        }
        return argv[index + 1];
    };

    auto parseIntValue = [&](const std::string& option, const char* value) -> int {
        try {
            return std::stoi(value);
        } catch (...) {
            parseError = true;
            parseErrorMessage = "Invalid integer for " + option + ": " + value;
            return 0;
        }
    };

    for (int i = 1; i < argc && !parseError; ++i) {
        std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            printUsage();
            return 0;
        } else if (arg == "--precompute" || arg == "--dry-run") {
            precomputeMode = true;
        } else if (arg == "--problem-type") {
            const char* value = requireValue(i, arg);
            if (parseError) break;
            problemType = value;
            ++i;
        } else if (arg == "--group-start") {
            const char* value = requireValue(i, arg);
            if (parseError) break;
            groupStart = parseIntValue(arg, value);
            ++i;
        } else if (arg == "--group-end") {
            const char* value = requireValue(i, arg);
            if (parseError) break;
            groupEnd = parseIntValue(arg, value);
            ++i;
        } else if (arg == "--exam-start") {
            const char* value = requireValue(i, arg);
            if (parseError) break;
            examStart = parseIntValue(arg, value);
            ++i;
        } else if (arg == "--exam-end") {
            const char* value = requireValue(i, arg);
            if (parseError) break;
            examEnd = parseIntValue(arg, value);
            ++i;
        } else if (arg == "--use-cache") {
            cacheRequested = true;
        } else if (arg == "--no-cache") {
            cacheRequested = false;
        } else {
            parseError = true;
            parseErrorMessage = "Unknown argument: " + arg;
        }
    }

    if (parseError) {
        std::cerr << parseErrorMessage << std::endl;
        printUsage();
        return 1;
    }

#ifdef RCPSP_ENABLE_HCACHE
    SetHCostCacheEnabled(cacheRequested);
    runtimeCacheEnabled = IsHCostCacheEnabled();
#else
    if (cacheRequested) {
        std::cerr << "[HCache] Cache support not built; ignoring --use-cache." << std::endl;
    }
    runtimeCacheEnabled = false;
#endif

    if (groupEnd < groupStart) {
        std::cerr << "group-end must be greater than or equal to group-start" << std::endl;
        return 1;
    }

    if (examEnd < examStart) {
        std::cerr << "exam-end must be greater than or equal to exam-start" << std::endl;
        return 1;
    }

    if (precomputeMode) {
#ifdef RCPSP_ENABLE_HCACHE
        if (!runtimeCacheEnabled) {
            std::cerr << "Error: heuristic cache disabled; re-run with --use-cache." << std::endl;
            return 1;
        }
        precomputeHCosts(problemType, groupStart, groupEnd, examStart, examEnd);
        return 0;
#else
        std::cerr << "Error: heuristic cache not available; rebuild with -DRCPSP_ENABLE_HCACHE=ON." << std::endl;
        return 1;
#endif
    }

    runBenchmark(problemType, groupStart, groupEnd, examStart, examEnd, runtimeCacheEnabled);
    return 0;
}


void runBenchmark(const std::string& problemType, int groupStart, int groupEnd, int examStart, int examEnd, bool useCache) {
    std::string folder = "results";

    std::string filename = buildResultsFilename(folder, problemType, groupStart, groupEnd, examStart, examEnd, useCache);

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
    file << "group,exam,time,finished,makespan,expand number,generated number,depth,PetriType,SetType,Use CS,Cache Enabled,Cache Hits,Cache Misses,Cache Size,generatedTime%,generatedTime(ave),avilableTime%,avilableTime(ave),hashTime%,hashTime(ave),HcostTime%,HcostTime(ave),comperTime%,comperTime(ave),succsesroTime%,sucssesorTime(ave)" << std::endl;
    //file << "group,exam,initialHcost" << std::endl;
// omp_set_num_threads(3);
//     //omp_set_num_threads(4); // 1. Set the core count.
// #pragma omp parallel for collapse(2) schedule(dynamic)

#ifdef RCPSP_ENABLE_HCACHE
    if (useCache) {
        std::cout << "[HCache] Heuristic cache with disk persistence enabled." << std::endl;
    } else {
        std::cout << "[HCache] Heuristic cache compiled but disabled for this run." << std::endl;
    }
#endif

    std::cout << "Running benchmark for " << problemType
              << " | groups " << groupStart << "-" << groupEnd
              << " | exams " << examStart << "-" << examEnd << std::endl;

    for (int i = groupStart; i <= groupEnd; ++i) {
        for (int j = examStart; j <= examEnd; ++j) {

            // 1. CLEAN THE SLATE (Crucial for thread_local variables)
            petri.reset();
            RCPSPex.reset();

#ifdef RCPSP_ENABLE_HCACHE
            if (useCache) {
                // Load cached h-costs from disk for this problem (if available)
                InitHCostCacheForProblem(problemType, i, j);
            } else {
                ClearHCostCache();
            }
#endif

            // 2. SOLVE
            solveRCPSP(i, j, filename, problemType);
            solveRCPSP_TT(i, j, filename, problemType);

#ifdef RCPSP_ENABLE_HCACHE
            if (useCache) {
                PrintCacheStats();
                FinalizeHCostCacheForProblem(problemType, i, j);
            }
#endif
        }
    }

    sortCSV(filename);
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
    initalHcost=getForwardHcost(first.unstartedTransitions,first.activeTransitionIndices);

    std::cout << "initalHcost\n";



    std::ofstream file(filename, std::ios::app);
    file << group << "," << exam << "," << initalHcost<<std::endl;

}

