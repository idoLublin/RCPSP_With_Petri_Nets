//
// Created by idol on 29/12/2024.
//
// RCPSP Solver Driver - Configurable via command-line flags

#include <iostream>
#include <fstream>
#include <vector>
#include <thread>
#include <chrono>
#include <atomic>
#include <climits>
#include <cstdlib>
#include <sstream>
#include <algorithm>
#include <filesystem>
#include <iomanip>
#include <ctime>

#include "HeuristicTypes.h"
#include "RCPSPState.cpp"
#include "../../HOG2/generic/TemplateAStar.h"
#include "../../HOG2/generic/BAE.h"
#include "RCPSP.h"

namespace fs = std::filesystem;

// ============================================================================
// Configuration Structure
// ============================================================================
// Path to repository root (relative to executable location when running from repo root)
const std::string REPO_ROOT = "";

// Global heuristic selection
P_RCPSP::HeuristicType activeHeuristic = P_RCPSP::HeuristicType::CRITICAL_PATH;

// Global DP heuristic toggle
bool useDPHeuristic = true;

// Global optimal makespan map for validation
std::map<std::pair<int,int>, int> optimalMakespan;

// ============================================================================
// Optimal Makespan Loader
// ============================================================================
std::map<std::pair<int,int>, int> loadOptimalMakespan(const std::string& problemType) {
    std::map<std::pair<int,int>, int> result;
    // Try {problemType}opt.sm first (j30 case: published optimums).
    // Fall back to {problemType}lb.sm (j60 case: lower bounds only).
    // If neither exists, silently return empty — no validation, no warning.
    std::string filepath = REPO_ROOT + "data/" + problemType + "opt.sm";
    std::ifstream file(filepath);
    if (!file.is_open()) {
        filepath = REPO_ROOT + "data/" + problemType + "lb.sm";
        file.open(filepath);
    }
    if (!file.is_open()) {
        return result;  // no bounds file — caller will skip validation
    }

    std::string line;
    // Skip header lines (everything before the data, lines starting with non-digits)
    bool inData = false;
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        // Data lines start with a digit
        if (!inData) {
            // Check if line starts with a digit (data section)
            size_t firstNonSpace = line.find_first_not_of(" \t");
            if (firstNonSpace != std::string::npos && std::isdigit(line[firstNonSpace])) {
                inData = true;
            } else {
                continue;
            }
        }
        if (!inData) continue;

        std::istringstream iss(line);
        int group, exam, makespan;
        double cpuTime;
        if (iss >> group >> exam >> makespan >> cpuTime) {
            result[{group, exam}] = makespan;
        }
    }
    std::cout << "Loaded " << result.size() << " optimal makespans from " << filepath << std::endl;
    return result;
}

struct Config {
    // Problem range
    int groupStart = 1;
    int groupEnd = 1;
    int examStart = 1;
    int examEnd = 10;
    
    // Problem set type: j30, j60, j90, j120
    std::string problemType = "j30";
    
    // Solving methods: "tp", "tt", "all"
    std::string method = "all";
    
    // Output configuration (relative to repo root)
    std::string outputFolder = "data";
    std::string outputFile = "";  // If empty, auto-generate
    std::string tag = "";          // Optional tag to append to filename (e.g., "dp_test_1")
    
    // Algorithm options
    P_RCPSP::HeuristicType heuristic = P_RCPSP::HeuristicType::CRITICAL_PATH;  // Default: CP
    bool useDP = true;           // Use DP preprocessing for heuristic (default: true)
    bool sortResults = true;
    bool writeHeader = true;
    int timeLimit = 300;  // Time limit per problem in seconds (default: 300 = 5 minutes)

    // Display help
    bool showHelp = false;

    // Diagnostic mode: solve one instance with CP, replay the optimal path
    // through every heuristic and report admissibility/consistency violations
    bool diagnose = false;
};

// Forward declarations
void runSolver(const Config& config);
void sortCSV(const std::string& filename);
int solveRCPSP(int group, int exam, const std::string& filename, const std::string& problemType);
int solveRCPSP_TT(int group, int exam, const std::string& filename, const std::string& problemType);
int solveRCPSP_TT2(int group, int exam, const std::string& filename, const std::string& problemType);

// ============================================================================
// Helper Functions
// ============================================================================
void printUsage(const char* programName) {
    std::cout << "Usage: " << programName << " [OPTIONS]\n\n"
              << "RCPSP Solver - Configurable Problem Solver\n\n"
              << "Options:\n"
              << "  --group-start N    Starting group number (default: 1)\n"
              << "  --group-end N      Ending group number (default: 1)\n"
              << "  --exam-start N     Starting exam number (default: 1)\n"
              << "  --exam-end N       Ending exam number (default: 10)\n"
              << "  --problem-type T   Problem type: j30, j60, j90, j120 (default: j30)\n"
              << "  --method M         Solving method: tp, tt, all (default: all)\n"
              << "  --output-folder F  Output folder path (default: data)\n"
              << "  --output-file F    Output filename (default: auto-generated)\n"
              << "                     Auto format: based on date, problem type, group/exam range, method, and optional tag\n"
              << "  --tag TAG          Optional tag to append to filename (e.g., dp_test_1)\n"
              << "  --time-limit N     Time limit per problem in seconds (default: 300)\n"
              << "  --heuristic H      Heuristic type: cp (Critical Path), lbcs (Lower Bound CS)\n"
              << "                     (default: cp)\n"
              << "  --dp               Use DP preprocessing for heuristic (default)\n"
              << "  --no-dp            Disable DP preprocessing (use original heuristic)\n"
              << "  --use-cs           [DEPRECATED] Same as --heuristic lbcs\n"
              << "  --no-cs            [DEPRECATED] Same as --heuristic cp\n"
              << "  --no-sort          Disable result sorting\n"
              << "  --no-header        Don't write CSV header\n"
              << "  --help, -h         Show this help message\n\n"
              << "Environment Variables (used as defaults if flags not provided):\n"
              << "  RCPSP_GROUP_START, RCPSP_GROUP_END\n"
              << "  RCPSP_EXAM_START, RCPSP_EXAM_END\n"
              << "  RCPSP_PROBLEM_TYPE, RCPSP_METHOD\n"
              << "  RCPSP_OUTPUT_FOLDER, RCPSP_OUTPUT_FILE, RCPSP_TAG\n"
              << "  RCPSP_HEURISTIC (cp or lbcs), RCPSP_TIME_LIMIT\n\n"
              << "Examples:\n"
              << "  " << programName << " --group-start 1 --group-end 5 --method tp\n"
              << "  " << programName << " --problem-type j60 --heuristic lbcs\n"
              << "  " << programName << " --tag dp_test_1 --group-start 16 --exam-end 10\n"
              << "  RCPSP_HEURISTIC=lbcs " << programName << "\n";
}

std::string getEnvOrDefault(const char* envVar, const std::string& defaultVal) {
    const char* val = std::getenv(envVar);
    return val ? std::string(val) : defaultVal;
}

int getEnvOrDefault(const char* envVar, int defaultVal) {
    const char* val = std::getenv(envVar);
    if (val) {
        try { return std::stoi(val); } catch (...) {}
    }
    return defaultVal;
}

bool getEnvOrDefault(const char* envVar, bool defaultVal) {
    const char* val = std::getenv(envVar);
    if (val) {
        std::string s(val);
        return s == "1" || s == "true" || s == "yes";
    }
    return defaultVal;
}

Config parseArgs(int argc, char* argv[]) {
    Config config;
    
    // First, apply environment variable defaults
    config.groupStart = getEnvOrDefault("RCPSP_GROUP_START", config.groupStart);
    config.groupEnd = getEnvOrDefault("RCPSP_GROUP_END", config.groupEnd);
    config.examStart = getEnvOrDefault("RCPSP_EXAM_START", config.examStart);
    config.examEnd = getEnvOrDefault("RCPSP_EXAM_END", config.examEnd);
    config.problemType = getEnvOrDefault("RCPSP_PROBLEM_TYPE", config.problemType);
    config.method = getEnvOrDefault("RCPSP_METHOD", config.method);
    config.outputFolder = getEnvOrDefault("RCPSP_OUTPUT_FOLDER", config.outputFolder);
    config.outputFile = getEnvOrDefault("RCPSP_OUTPUT_FILE", config.outputFile);
    config.tag = getEnvOrDefault("RCPSP_TAG", config.tag);
    config.timeLimit = getEnvOrDefault("RCPSP_TIME_LIMIT", config.timeLimit);

    // Heuristic selection from environment
    const char* heuristicEnv = std::getenv("RCPSP_HEURISTIC");
    if (heuristicEnv) {
        config.heuristic = P_RCPSP::stringToHeuristicType(heuristicEnv);
    }
    
    // Parse command-line arguments (override env vars)
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "--help" || arg == "-h") {
            config.showHelp = true;
        } else if (arg == "--group-start" && i + 1 < argc) {
            config.groupStart = std::stoi(argv[++i]);
        } else if (arg == "--group-end" && i + 1 < argc) {
            config.groupEnd = std::stoi(argv[++i]);
        } else if (arg == "--exam-start" && i + 1 < argc) {
            config.examStart = std::stoi(argv[++i]);
        } else if (arg == "--exam-end" && i + 1 < argc) {
            config.examEnd = std::stoi(argv[++i]);
        } else if (arg == "--problem-type" && i + 1 < argc) {
            config.problemType = argv[++i];
        } else if (arg == "--method" && i + 1 < argc) {
            config.method = argv[++i];
        } else if (arg == "--output-folder" && i + 1 < argc) {
            config.outputFolder = argv[++i];
        } else if (arg == "--output-file" && i + 1 < argc) {
            config.outputFile = argv[++i];
        } else if (arg == "--tag" && i + 1 < argc) {
            config.tag = argv[++i];
        } else if (arg == "--time-limit" && i + 1 < argc) {
            config.timeLimit = std::stoi(argv[++i]);
        } else if (arg == "--heuristic" && i + 1 < argc) {
            config.heuristic = P_RCPSP::stringToHeuristicType(argv[++i]);
        } else if (arg == "--use-cs") {
            // Deprecated: backward compatibility
            config.heuristic = P_RCPSP::HeuristicType::LBCS;
        } else if (arg == "--no-cs") {
            // Deprecated: backward compatibility
            config.heuristic = P_RCPSP::HeuristicType::CRITICAL_PATH;
        } else if (arg == "--dp") {
            config.useDP = true;
        } else if (arg == "--no-dp") {
            config.useDP = false;
        } else if (arg == "--no-sort") {
            config.sortResults = false;
        } else if (arg == "--diagnose") {
            config.diagnose = true;
        } else if (arg == "--no-header") {
            config.writeHeader = false;
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            std::cerr << "Use --help for usage information.\n";
        }
    }
    
    return config;
}

std::string generateOutputFilename(const Config& config) {
    // Build full path relative to repo root
    std::string fullOutputFolder = REPO_ROOT + config.outputFolder;
    
    // Ensure folder exists
    if (!fs::exists(fullOutputFolder)) {
        fs::create_directories(fullOutputFolder);
    }
    
    if (!config.outputFile.empty()) {
        return fullOutputFolder + "/" + config.outputFile;
    }
    
    // Generate filename with date and ranges
    // Format: YYYY-MM-DD_j30_g1-48_e1-10_all.csv
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm = *std::localtime(&time);
    
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d") << "_"
        << config.problemType << "_"
        << "g" << config.groupStart;
    
    if (config.groupStart != config.groupEnd) {
        oss << "-" << config.groupEnd;
    }
    
    oss << "_e" << config.examStart;
    
    if (config.examStart != config.examEnd) {
        oss << "-" << config.examEnd;
    }
    
    oss << "_" << config.method;
    
    // Add tag if provided
    if (!config.tag.empty()) {
        oss << "_" << config.tag;
    }
    
    std::string baseName = oss.str();
    std::string filename = fullOutputFolder + "/" + baseName + ".csv";
    
    // If file exists, add a counter
    if (fs::exists(filename)) {
        int count = 1;
        do {
            filename = fullOutputFolder + "/" + baseName + "_" + std::to_string(count) + ".csv";
            count++;
        } while (fs::exists(filename));
    }
    
    return filename;
}

int solveRCPSP(int group, int exam, const std::string& filename, const std::string& problemType = "j30") {
    std::cout << "started solving (TP): " << group << ":" << exam << std::endl;

    getPetri(petri, group, exam, problemType);
    getRCPSP(RCPSPex, group, exam, problemType);
    RCPSPex.computeAndStoreDeepDependencies();

    RCPSPState first;
    RCPSPState last = first;
    last.h = 0;

    for (int i = 0; i < last.marking.size(); ++i) {
        if (last.marking[i] == 1) {
            last.marking[i] = 0;
        }
    }

    // Set the Goal State using place name to ID mapping
    int finalID = petri.place_name_to_id.at(finalstatename);
    last.marking[finalID] = 1;

    RCPSP as1;
    TemplateAStar<RCPSPState, int, RCPSP> astar;
    // The chain-based LBCS is admissible but not proven consistent; with an
    // inconsistent heuristic A* needs node re-opening to stay optimal.
    if (activeHeuristic == P_RCPSP::HeuristicType::LBCS) astar.SetReopenNodes(true);
    std::vector<RCPSPState> path;
    std::chrono::duration<double> elapsed;

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

            makespan = state.g;
        }

        // Validate against bounds. For j30 the loaded value is a published
        // optimum (exact-match required). For LB-only datasets (e.g. j60lb.sm)
        // the value is a lower bound — only makespan < LB is an error.
        // Instances missing from the file are silently skipped.
        auto it = optimalMakespan.find({group, exam});
        if (it != optimalMakespan.end()) {
            int boundValue = it->second;
            bool isExactOptimum = (problemType == "j30");
            if (isExactOptimum) {
                if (makespan != boundValue) {
                    std::cerr << "\nERROR: Makespan mismatch for group " << group << " exam " << exam
                              << "! Got " << makespan << ", expected optimal " << boundValue << std::endl;
                    exit(1);
                }
                std::cout << "Validated: makespan " << makespan << " matches published optimum." << std::endl;
            } else {
                if (makespan < boundValue) {
                    std::cerr << "\nERROR: makespan " << makespan << " below published LB " << boundValue
                              << " for group " << group << " exam " << exam
                              << " — heuristic is INADMISSIBLE." << std::endl;
                    exit(1);
                }
                std::cout << "Validated: makespan " << makespan << " >= LB " << boundValue
                          << " (no UB published)." << std::endl;
            }
        }
    } else {
        std::cout << "Path not found or timeout occurred.\n";
    }

    std::cout << "Nodes Expanded: " << astar.GetNodesExpanded() << std::endl;
    std::cout << "Nodes Touched: " << astar.GetNodesTouched() << std::endl;

    std::string dpTag = useDPHeuristic ? "_DP" : "_NoDP";
    std::ofstream file(filename, std::ios::app);
    file << group << "," << exam << "," << elapsed.count() << ","
         << (!path.empty() ? "True" : "False") << ","
         << makespan << ","
         << astar.GetNodesExpanded() << ","
         << astar.GetNodesTouched() << ","
         << path.size() << ","
         << "TP" << ","
         << problemType << ","
         << P_RCPSP::heuristicTypeToString(activeHeuristic) << dpTag
         << "\n";

    return 0;
}
int solveRCPSP_TT(int group, int exam, const std::string& filename, const std::string& problemType = "j30") {
    std::cout << "started solving (TT): " << group << ":" << exam << std::endl;
    count = 0;
    getPetri(petri, group, exam, problemType);
    getRCPSP(RCPSPex, group, exam, problemType);

    RCPSPState_TT first;
    RCPSPState_TT last = first;

    RCPSP_TT as1;
    TemplateAStar<RCPSPState_TT, int, RCPSP_TT> astar;
    if (activeHeuristic == P_RCPSP::HeuristicType::LBCS) astar.SetReopenNodes(true);
    std::vector<RCPSPState_TT> path;

    std::chrono::duration<double> elapsed;

    auto start = std::chrono::high_resolution_clock::now();
    astar.GetPath(&as1, first, last, path);
    auto end = std::chrono::high_resolution_clock::now();
    elapsed = end - start;

    int makespan = 0;

    if (!path.empty()) {
        std::cout << "Path found!" << std::endl;
        RCPSPState_TT state = path.back();

        std::cout << std::endl;
        makespan = state.g;
        std::cout << "\nFinal makespan: " << makespan << std::endl;

        // Validate against bounds. For j30 the loaded value is a published
        // optimum (exact-match required). For LB-only datasets (e.g. j60lb.sm)
        // the value is a lower bound — only makespan < LB is an error.
        // Instances missing from the file are silently skipped.
        auto it = optimalMakespan.find({group, exam});
        if (it != optimalMakespan.end()) {
            int boundValue = it->second;
            bool isExactOptimum = (problemType == "j30");
            if (isExactOptimum) {
                if (makespan != boundValue) {
                    std::cerr << "\nERROR: Makespan mismatch for group " << group << " exam " << exam
                              << "! Got " << makespan << ", expected optimal " << boundValue << std::endl;
                    exit(1);
                }
                std::cout << "Validated: makespan " << makespan << " matches published optimum." << std::endl;
            } else {
                if (makespan < boundValue) {
                    std::cerr << "\nERROR: makespan " << makespan << " below published LB " << boundValue
                              << " for group " << group << " exam " << exam
                              << " — heuristic is INADMISSIBLE." << std::endl;
                    exit(1);
                }
                std::cout << "Validated: makespan " << makespan << " >= LB " << boundValue
                          << " (no UB published)." << std::endl;
            }
        }
    } else {
        std::cout << "Path not found or timeout occurred.\n";
    }

    std::cout << "Nodes Expanded: " << astar.GetNodesExpanded() << std::endl;
    std::cout << "Nodes Touched: " << astar.GetNodesTouched() << std::endl;

    std::string dpTag = useDPHeuristic ? "_DP" : "_NoDP";
    std::ofstream file(filename, std::ios::app);
    file << group << "," << exam << "," << elapsed.count() << ","
         << (!path.empty() ? "True" : "False") << ","
         << makespan << ","
         << astar.GetNodesExpanded() << ","
         << astar.GetNodesTouched() << ","
         << path.size() << ","
         << "TT" << ","
         << problemType << ","
         << P_RCPSP::heuristicTypeToString(activeHeuristic) << dpTag
         << "\n";

    return 0;
}
// ============================================================================
// TT2 solver — TTPNR formulation (Lublin/Atzmon/Cohen, SoCS 2026)
// ============================================================================
int solveRCPSP_TT2(int group, int exam, const std::string& filename, const std::string& problemType = "j30") {
    std::cout << "started solving (TT2): " << group << ":" << exam << std::endl;
    count = 0;
    getPetri(petri, group, exam, problemType);
    getRCPSP(RCPSPex, group, exam, problemType);

    RCPSPState_TT2 first;
    RCPSPState_TT2 last = first;

    RCPSP_TT2 as1;
    TemplateAStar<RCPSPState_TT2, int, RCPSP_TT2> astar;
    if (activeHeuristic == P_RCPSP::HeuristicType::LBCS) astar.SetReopenNodes(true);
    std::vector<RCPSPState_TT2> path;

    std::chrono::duration<double> elapsed;

    auto start = std::chrono::high_resolution_clock::now();
    astar.GetPath(&as1, first, last, path);
    auto end = std::chrono::high_resolution_clock::now();
    elapsed = end - start;

    int makespan = 0;

    if (!path.empty()) {
        std::cout << "Path found!" << std::endl;
        RCPSPState_TT2 state = path.back();
        std::cout << std::endl;
        makespan = state.g;
        std::cout << "\nFinal makespan: " << makespan << std::endl;

        // Validate against bounds. For j30 the loaded value is a published
        // optimum (exact-match required). For LB-only datasets (e.g. j60lb.sm)
        // the value is a lower bound — only makespan < LB is an error.
        // Instances missing from the file are silently skipped.
        auto it = optimalMakespan.find({group, exam});
        if (it != optimalMakespan.end()) {
            int boundValue = it->second;
            bool isExactOptimum = (problemType == "j30");
            if (isExactOptimum) {
                if (makespan != boundValue) {
                    std::cerr << "\nERROR: Makespan mismatch for group " << group << " exam " << exam
                              << "! Got " << makespan << ", expected optimal " << boundValue << std::endl;
                    exit(1);
                }
                std::cout << "Validated: makespan " << makespan << " matches published optimum." << std::endl;
            } else {
                if (makespan < boundValue) {
                    std::cerr << "\nERROR: makespan " << makespan << " below published LB " << boundValue
                              << " for group " << group << " exam " << exam
                              << " — heuristic is INADMISSIBLE." << std::endl;
                    exit(1);
                }
                std::cout << "Validated: makespan " << makespan << " >= LB " << boundValue
                          << " (no UB published)." << std::endl;
            }
        }
    } else {
        std::cout << "Path not found or timeout occurred.\n";
    }

    std::cout << "Nodes Expanded: " << astar.GetNodesExpanded() << std::endl;
    std::cout << "Nodes Touched: " << astar.GetNodesTouched() << std::endl;

    std::string dpTag = useDPHeuristic ? "_DP" : "_NoDP";
    std::ofstream file(filename, std::ios::app);
    file << group << "," << exam << "," << elapsed.count() << ","
         << (!path.empty() ? "True" : "False") << ","
         << makespan << ","
         << astar.GetNodesExpanded() << ","
         << astar.GetNodesTouched() << ","
         << path.size() << ","
         << "TT2" << ","
         << problemType << ","
         << P_RCPSP::heuristicTypeToString(activeHeuristic) << dpTag
         << "\n";

    return 0;
}

// ============================================================================
// Heuristic diagnosis (--diagnose)
// ============================================================================
// Solves one instance with the admissible CP heuristic (TT2), then replays the
// optimal path computing every heuristic component at each state. A state with
// g + h > optimum proves that heuristic inadmissible; h(parent) > edge + h(child)
// (with g+h <= opt everywhere) points at inconsistency + no-reopening instead.
int diagnoseHeuristics(int group, int exam, const std::string& problemType) {
    petri.reset();
    RCPSPex.reset();
    heuristicDPInitialized = false;
    lbccInitialized = false;
    lbip0Initialized = false;
    lbrcInitialized = false;
    demandMatrixInitialized = false;

    getPetri(petri, group, exam, problemType);
    getRCPSP(RCPSPex, group, exam, problemType);

    activeHeuristic = P_RCPSP::HeuristicType::CRITICAL_PATH;
    setSearchTimeout(600);

    RCPSPState_TT2 first, last;
    RCPSP_TT2 env;
    TemplateAStar<RCPSPState_TT2, int, RCPSP_TT2> astar;
    std::vector<RCPSPState_TT2> path;
    astar.GetPath(&env, first, last, path);

    if (path.empty()) {
        std::cout << "diagnose: no path found (timeout?)" << std::endl;
        return 1;
    }
    const int opt = path.back().g;
    std::cout << "diagnose g" << group << " e" << exam << " (" << problemType
              << "): optimal makespan via CP = " << opt << "\n"
              << "step |   g | cpH | hres | lbcs | lbcc | lbip0 | lbmax | f(lbcs) flags\n";

    std::vector<double> lbcsH(path.size()), fullH(path.size());
    bool anyInadmissible = false;

    for (size_t i = 0; i < path.size(); ++i) {
        const auto& st = path[i];
        std::vector<short> unf;
        for (int t = 1; t <= (int)petri.Transitions.size(); ++t)
            if (!st.finishedActivitiys.test(t)) unf.push_back(t);

        double cpH   = unf.empty() ? 0 : getForwardHcostDP(unf, st.activeTransitionIndices);
        double hres  = unf.empty() ? 0 : getforwardResource(unf, st.activeTransitionIndices);
        double lbcs  = unf.empty() ? 0 : computeLBCS(unf, st.activeTransitionIndices);
        double lbcc  = unf.empty() ? 0 : computeCriticalCapacityLB(unf, st.activeTransitionIndices, st.g);
        double lbip0 = unf.empty() ? 0 : computeLBIP0(st.g);
        double lbmax = unf.empty() ? 0 : computeLBRC(st.g);

        lbcsH[i] = lbcs;
        fullH[i] = std::max({cpH, hres, lbcs});
        double f = st.g + fullH[i];

        std::string flags;
        if (st.g + cpH   > opt + 1e-9) flags += " CP>OPT!";
        if (st.g + hres  > opt + 1e-9) flags += " HRES>OPT!";
        if (st.g + lbcs  > opt + 1e-9) flags += " LBCS>OPT!";
        if (st.g + lbcc  > opt + 1e-9) flags += " LBCC>OPT!";
        if (st.g + lbip0 > opt + 1e-9) flags += " LBIP0>OPT!";
        if (st.g + lbmax > opt + 1e-9) flags += " LBMAX>OPT!";
        if (!flags.empty()) anyInadmissible = true;

        printf("%4zu | %3d | %3.0f | %4.1f | %4.0f | %4.0f | %5.0f | %5.0f | %5.1f%s\n",
               i, (int)st.g, cpH, hres, lbcs, lbcc, lbip0, lbmax, f, flags.c_str());

        if (!flags.empty()) {
            printf("      actives:");
            for (const auto& a : st.activeTransitionIndices) printf(" (%d,rem=%d)", a.first, a.second);
            printf("\n      unfinished:");
            for (short t : unf) printf(" %d", t);
            printf("\n");
        }
    }

    // Consistency along the path for the lbcs-dispatched h = max(cp,hres,lbcs)
    for (size_t i = 0; i + 1 < path.size(); ++i) {
        double edge = path[i+1].g - path[i].g;
        if (fullH[i] > edge + fullH[i+1] + 1e-9)
            printf("INCONSISTENT h(lbcs-dispatch) at step %zu: h=%.1f > edge %.1f + h(next)=%.1f\n",
                   i, fullH[i], edge, fullH[i+1]);
    }

    std::cout << (anyInadmissible
        ? "VERDICT: heuristic value(s) above optimum on an optimal path -> INADMISSIBLE.\n"
        : "VERDICT: no overestimate on this optimal path -> suspect inconsistency + reopenNodes=false.\n");
    return 0;
}

// NOTE: Bidirectional search - currently not working
int solveRCPSP_Bi(int group, int exam, const std::string& filename) {
    std::cout << "started solving (Bi): " << group << ":" << exam << std::endl;
    count = 0;

    getPetri(petri, group, exam);
    getRCPSP(RCPSPex, group, exam);

    RCPSPState_bi first;
    first.direction = true;
    count = 2;
    RCPSPState_bi last = first;
    last.direction = false;
    last.h_b = first.h_f;
    last.h_f = 0;
    last.name = 1;

    for (auto& pair : last.marking) {
        // Skip resource places
        if (pair.first == "R1" || pair.first == "R2" || 
            pair.first == "R3" || pair.first == "R4") {
            continue;
        }
        if (pair.second == 1) { pair.second = 0; }
        if (pair.first == finalstatename) { pair.second = 1; }
    }

    last.avilableDeTransitionIndices = getAvilableDetransitionIndices(last.marking);
    for (int i = 1; i < petri.Transitions.size() + 1; i++) {
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

            max_f = std::max(max_f, state.g_f);
            max_b = std::max(max_b, state.g_b);
        }
    } else {
        std::cout << "Path not found or timeout occurred.\n";
    }

    std::cout << "Nodes Expanded: " << Bi_RCPSP.GetNodesExpanded() << std::endl;
    std::cout << "Nodes Touched: " << Bi_RCPSP.GetNodesTouched() << std::endl;

    std::ofstream file(filename, std::ios::app);
    file << group << "," << exam << "," << elapsed.count() << ","
         << (finished ? "True" : "False") << ","
         << max_f + max_b << ","
         << Bi_RCPSP.GetNodesExpanded()
         << "\n";

    return 0;
}

// ============================================================================
// CSV Header
// ============================================================================
const std::string CSV_HEADER = "group,exam,time,finished,makespan,expand_number,generated_number,depth,PetriType,SetType,Heuristic,generatedTime%,generatedTime(ave),avilableTime%,avilableTime(ave),hashTime%,hashTime(ave),HcostTime%,HcostTime(ave),comperTime%,comperTime(ave),succsesroTime%,sucssesorTime(ave)";

// ============================================================================
// Main Solver Runner
// ============================================================================
void runSolver(const Config& config) {
    std::string filename = generateOutputFilename(config);
    
    std::cout << "============================================\n";
    std::cout << "RCPSP Solver Configuration:\n";
    std::cout << "  Groups: " << config.groupStart << " - " << config.groupEnd << "\n";
    std::cout << "  Exams: " << config.examStart << " - " << config.examEnd << "\n";
    std::cout << "  Problem Type: " << config.problemType << "\n";
    std::cout << "  Method: " << config.method << "\n";
    std::cout << "  Output: " << filename << "\n";
    std::cout << "  Heuristic: " << P_RCPSP::heuristicTypeToString(config.heuristic)
              << " (" << P_RCPSP::heuristicTypeDescription(config.heuristic) << ")\n";
    std::cout << "  DP Preprocessing: " << (config.useDP ? "Enabled" : "Disabled") << "\n";
    std::cout << "  Time Limit: " << config.timeLimit << " seconds\n";
    std::cout << "============================================\n\n";

    // Set global heuristic type and DP flag
    activeHeuristic = config.heuristic;
    useDPHeuristic = config.useDP;

    // Load optimal makespan for validation
    optimalMakespan = loadOptimalMakespan(config.problemType);

    // Open output file
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open output file: " << filename << std::endl;
        return;
    }
    
    // Write header if requested
    if (config.writeHeader) {
        file << CSV_HEADER << std::endl;
    }
    file.close();
    
    // Main solving loop
    for (int group = config.groupStart; group <= config.groupEnd; group++) {
        for (int exam = config.examStart; exam <= config.examEnd; exam++) {
            // Reset state before each problem
            petri.reset();
            RCPSPex.reset();
            heuristicDPInitialized = false;  // Reset DP cache for new problem
            lbccInitialized = false;         // Reset LBcc cache for new problem
            lbip0Initialized = false;        // Reset LBip0 cache for new problem
            lbrcInitialized = false;         // Reset LBrc cache for new problem
            demandMatrixInitialized = false; // Reset demand-matrix cache for new problem

            // Run TP method if selected
            if (config.method == "tp" || config.method == "all") {
                setSearchTimeout(config.timeLimit);  // Reset timeout for this problem
                solveRCPSP(group, exam, filename, config.problemType);
            }
            
            // Run TT method if selected
            if (config.method == "tt" || config.method == "all") {
                setSearchTimeout(config.timeLimit);  // Reset timeout for this problem
                solveRCPSP_TT(group, exam, filename, config.problemType);
            }

            // Run TT2 method if selected (TTPNR / relative-delay tokens)
            if (config.method == "tt2" || config.method == "all") {
                setSearchTimeout(config.timeLimit);
                solveRCPSP_TT2(group, exam, filename, config.problemType);
            }
        }
    }
    
    // Sort results if requested
    if (config.sortResults) {
        sortCSV(filename);
    }
    
    std::cout << "\n✅ Completed! Results saved to: " << filename << std::endl;
}

// ============================================================================
// Main Entry Point
// ============================================================================
int main(int argc, char *argv[]) {
    Config config = parseArgs(argc, argv);
    
    if (config.showHelp) {
        printUsage(argv[0]);
        return 0;
    }

    if (config.diagnose) {
        return diagnoseHeuristics(config.groupStart, config.examStart, config.problemType);
    }

    runSolver(config);
    return 0;
}

// ============================================================================
// Result Sorting
// ============================================================================
struct ResultRow {
    std::string fullLine;
    std::string petriType;
    std::string setType;
    int setSize;
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
