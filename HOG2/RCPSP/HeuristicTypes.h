//
// HeuristicTypes.h
// Configurable heuristic selection for RCPSP solver
//

#ifndef HEURISTIC_TYPES_H
#define HEURISTIC_TYPES_H

#include <string>
#include <unordered_map>

namespace P_RCPSP {

/**
 * Available heuristic types for RCPSP solving.
 *
 * CRITICAL_PATH: Basic critical path heuristic (fast, DP-based)
 * LBCS: Lower Bound Critical Sequence - considers resource constraints
 *       Based on Coelho, J., & Vanhoucke, M. (2018)
 */
enum class HeuristicType {
    CRITICAL_PATH,  // Default - fast DP-based critical path
    LBCS,           // Lower Bound Critical Sequence (workload-based)
    LBCC,           // Lower Bound Critical Capacity (work arrival simulation)
    LBER,           // Enhanced Energetic Reasoning (Haouari, Kooli, Neron 2012)
    LBRC,           // max(critical path, resource work/capacity) - consistent, cheap
};

/**
 * Convert string to HeuristicType.
 * Supports both short and long forms for CLI flexibility.
 */
inline HeuristicType stringToHeuristicType(const std::string& str) {
    static const std::unordered_map<std::string, HeuristicType> mapping = {
        {"cp", HeuristicType::CRITICAL_PATH},
        {"critical-path", HeuristicType::CRITICAL_PATH},
        {"criticalpath", HeuristicType::CRITICAL_PATH},
        {"lbcs", HeuristicType::LBCS},
        {"lower-bound-cs", HeuristicType::LBCS},
        {"lowerboundcs", HeuristicType::LBCS},
        {"lbcc", HeuristicType::LBCC},
        {"critical-capacity", HeuristicType::LBCC},
        {"criticalcapacity", HeuristicType::LBCC},
        {"lber", HeuristicType::LBER},
        {"energetic", HeuristicType::LBER},
        {"energetic-reasoning", HeuristicType::LBER},
        {"lbrc", HeuristicType::LBRC},
        {"resource-capacity", HeuristicType::LBRC},
        {"resourcecapacity", HeuristicType::LBRC},
    };

    // Convert to lowercase for case-insensitive matching
    std::string lower = str;
    for (char& c : lower) {
        c = std::tolower(c);
    }

    auto it = mapping.find(lower);
    return (it != mapping.end()) ? it->second : HeuristicType::CRITICAL_PATH;
}

/**
 * Convert HeuristicType to string for display/CSV output.
 */
inline std::string heuristicTypeToString(HeuristicType type) {
    switch (type) {
        case HeuristicType::CRITICAL_PATH: return "CP";
        case HeuristicType::LBCS: return "LBCS";
        case HeuristicType::LBCC: return "LBCC";
        case HeuristicType::LBER: return "LBER";
        case HeuristicType::LBRC: return "LBRC";
        default: return "UNKNOWN";
    }
}

/**
 * Get full description of a heuristic type.
 */
inline std::string heuristicTypeDescription(HeuristicType type) {
    switch (type) {
        case HeuristicType::CRITICAL_PATH:
            return "Critical Path (precedence-based, ignores resources)";
        case HeuristicType::LBCS:
            return "Lower Bound Critical Sequence (workload-based)";
        case HeuristicType::LBCC:
            return "Lower Bound Critical Capacity (work arrival simulation)";
        case HeuristicType::LBER:
            return "Enhanced Energetic Reasoning (destructive; CER per-node, "
                   "DFF/RER/EER/GER/SHV in root mode)";
        case HeuristicType::LBRC:
            return "max(critical path, resource work/capacity) - consistent state "
                   "bound, no re-opening";
        default:
            return "Unknown heuristic";
    }
}

} // namespace P_RCPSP

#endif // HEURISTIC_TYPES_H
