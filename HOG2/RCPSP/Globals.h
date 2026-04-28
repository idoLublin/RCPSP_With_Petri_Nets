#ifndef GLOBALS_H
#define GLOBALS_H

#include <vector>
#include <string>
#include <cstdint>
#include "petriclasses.h"

using namespace P_RCPSP;

// ── Structs and Enums ─────────────────────────────────────────────
struct ResourceInfo {
    short capacity;
    std::vector<short> activity_indices;
    std::vector<short> demands;
};

struct Conflict {
    short t                  = 0;
    short resourceType       = 0;
    short num_activities     = 0;
    int   activity_start_index = 0;
};

enum class ConflictType : uint8_t {
    CARDINAL      = 0,
    SEMI_CARDINAL = 1,
    NON_CARDINAL  = 2
};

enum class HeuristicType : uint8_t {
    NONE = 0,
    CG   = 1,
    DG   = 2,
    HCBS = 3,
};

struct CBSConfig {
    bool use_conflict_prioritization = true;
    bool use_first_conflict          = false;
    bool use_ancestor_branching          = true;
    bool use_dominance          = true;
    bool use_pair_decomposition         = true;

    HeuristicType heuristic          = HeuristicType::CG;
};

// ── Problem instance ──────────────────────────────────────────────
inline PetriExample    petri;
inline RCPSP_example   RCPSPex;
inline short           g_num_activities = 0;
inline short           g_sink_id        = 0;
inline std::string     finalstatename;
inline std::string     initialstatename;

// ── Precomputed structures ────────────────────────────────────────
inline std::vector<std::vector<short>> downstream;
inline std::vector<std::vector<short>> upstream;
inline std::vector<ResourceInfo>       resource_info;

// ── CBS Configuration ─────────────────────────────────────────────
inline CBSConfig       setting;

// ── Debug ─────────────────────────────────────────────────────────
inline long            debug_cardinal_num = 0;
inline bool            allcorrect         = true;

#endif // GLOBALS_H