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

// LBER configuration: mode ("root"|"pernode") and pipeline depth (1..6).
// Default depth 3 = CER+DFF+SHV (the efficient, best bound); 4=+RER, 5=+EER, 6=+GER.
std::string lberMode = "root";
int lberDepth = 3;

// Global optimal makespan map for validation
std::map<std::pair<int,int>, int> optimalMakespan;

// ============================================================================
// Optimal Makespan Loader
// ============================================================================
std::map<std::pair<int,int>, int> loadOptimalMakespan(const std::string& problemType) {
    std::map<std::pair<int,int>, int> result;
    std::string filepath = REPO_ROOT + "data/" + problemType + "opt.sm";
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Warning: Could not open optimal makespan file: " << filepath << std::endl;
        return result;
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
    std::string lberMode = "root";  // LBER mode: "root" (full pipeline) or "pernode"
    int lberDepth = 3;              // LBER pipeline depth 1..6 (higher = tighter, costlier)
    bool useDP = true;           // Use DP preprocessing for heuristic (default: true)
    bool sortResults = true;
    bool writeHeader = true;
    int timeLimit = 300;  // Time limit per problem in seconds (default: 300 = 5 minutes)

    // Display help
    bool showHelp = false;
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
        } else if (arg == "--lber-mode" && i + 1 < argc) {
            config.lberMode = argv[++i];
        } else if (arg == "--lber-depth" && i + 1 < argc) {
            config.lberDepth = std::stoi(argv[++i]);
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

        // Validate against optimal makespan
        auto it = optimalMakespan.find({group, exam});
        if (it != optimalMakespan.end()) {
            if (makespan != it->second) {
                std::cerr << "\nERROR: Makespan mismatch for group " << group << " exam " << exam
                          << "! Got " << makespan << ", expected optimal " << it->second << std::endl;
                exit(1);
            }
            std::cout << "Validated: makespan " << makespan << " matches optimal." << std::endl;
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

        // Validate against optimal makespan
        auto it = optimalMakespan.find({group, exam});
        if (it != optimalMakespan.end()) {
            if (makespan != it->second) {
                std::cerr << "\nERROR: Makespan mismatch for group " << group << " exam " << exam
                          << "! Got " << makespan << ", expected optimal " << it->second << std::endl;
                exit(1);
            }
            std::cout << "Validated: makespan " << makespan << " matches optimal." << std::endl;
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

// Forward TT2 search on the consistent Class-1 LBER bound. Mirrors
// solveRCPSP_TT but uses the RCPSPState_TT2 / RCPSP_TT2 environment.
int solveRCPSP_TT2(int group, int exam, const std::string& filename, const std::string& problemType = "j30") {
    std::cout << "started solving (TT2): " << group << ":" << exam << std::endl;
    count = 0;
    getPetri(petri, group, exam, problemType);
    getRCPSP(RCPSPex, group, exam, problemType);

    RCPSPState_TT2 first;
    RCPSPState_TT2 last = first;

    RCPSP_TT2 as1;
    TemplateAStar<RCPSPState_TT2, int, RCPSP_TT2> astar;
    // The LBER floor bound is admissible but not a consistent state heuristic
    // (it embeds lberRootBound - g), so node re-opening is required to keep A*
    // optimal: a cheaper path to an already-closed state lowers its g and
    // re-opens it (duplicate states matched via GetStateHash).
    astar.SetReopenNodes(true);
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

        // Validate against optimal makespan (a consistent admissible heuristic
        // must return the optimum without node re-opening).
        auto it = optimalMakespan.find({group, exam});
        if (it != optimalMakespan.end()) {
            if (makespan != it->second) {
                std::cerr << "\nERROR: Makespan mismatch for group " << group << " exam " << exam
                          << "! Got " << makespan << ", expected optimal " << it->second << std::endl;
                exit(1);
            }
            std::cout << "Validated: makespan " << makespan << " matches optimal." << std::endl;
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
    lberMode = config.lberMode;
    lberDepth = config.lberDepth;

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
            energeticInitialized = false;    // Reset LBER cache for new problem
            lberRootComputed = false;        // Reset LBER root-bound cache
            
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

            // Run forward TT2 (consistent Class-1 LBER) if selected
            if (config.method == "tt2") {
                setSearchTimeout(config.timeLimit);  // Reset timeout for this problem
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
