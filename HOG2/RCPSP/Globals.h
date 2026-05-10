#ifndef GLOBALS_H
#define GLOBALS_H

#include <vector>
#include <string>
#include <cstdint>
#include "petriclasses.h"

using namespace P_RCPSP;
short LB=0;
// ── Structs and Enums ─────────────────────────────────────────────
struct ResourceInfo {
    std::string resource_nume;
    short capacity;
    std::vector<short> activity_indices;
    std::vector<short> demands;
    std::unordered_map<short, short> demand_lookup; // activity_idx -> demand

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
struct MDA {
    std::vector<short> activities;
    short cost; // max delta over members, with new_start computed from non-set members
};

template<short N>
struct ConflictKey;

template<>
struct ConflictKey<32> {
    uint32_t mask;
    short res_idx;

    bool operator==(const ConflictKey& o) const {
        return mask == o.mask && res_idx == o.res_idx;
    }
};

template<>
struct ConflictKey<62> {
    uint64_t mask;
    short res_idx;

    bool operator==(const ConflictKey& o) const {
        return mask == o.mask && res_idx == o.res_idx;
    }
};

template<>
struct ConflictKey<92> {
    uint64_t low;
    uint32_t high;
    short res_idx;

    bool operator==(const ConflictKey& o) const {
        return low == o.low && high == o.high && res_idx == o.res_idx;
    }
};

template<>
struct ConflictKey<122> {
    uint64_t low;
    uint64_t high;
    short res_idx;

    bool operator==(const ConflictKey& o) const {
        return low == o.low && high == o.high && res_idx == o.res_idx;
    }
};

template<short N>
struct ConflictKeyHash;

template<>
struct ConflictKeyHash<32> {
    size_t operator()(const ConflictKey<32>& k) const {
        return std::hash<uint32_t>()(k.mask) ^
               (std::hash<short>()(k.res_idx) << 1);
    }
};

template<>
struct ConflictKeyHash<62> {
    size_t operator()(const ConflictKey<62>& k) const {
        return std::hash<uint64_t>()(k.mask) ^
               (std::hash<short>()(k.res_idx) << 1);
    }
};

template<>
struct ConflictKeyHash<92> {
    size_t operator()(const ConflictKey<92>& k) const {
        return std::hash<uint64_t>()(k.low) ^
               (std::hash<uint32_t>()(k.high) << 1) ^
               (std::hash<short>()(k.res_idx) << 2);
    }
};

template<>
struct ConflictKeyHash<122> {
    size_t operator()(const ConflictKey<122>& k) const {
        return std::hash<uint64_t>()(k.low) ^
               (std::hash<uint64_t>()(k.high) << 1) ^
               (std::hash<short>()(k.res_idx) << 2);
    }
};



// cache type alias per N
template<short N>
using MDACache = std::unordered_map<ConflictKey<N>, std::vector<std::vector<short>>, ConflictKeyHash<N>>;

// one global cache per N
template<short N>
MDACache<N>& get_mda_cache() {
    static MDACache<N> cache;
    return cache;
}

// Cache: conflict bitmask -> minimal sets (no cost)
// std::unordered_map<ConflictKey, std::vector<std::vector<short>>, ConflictKeyHash> g_mda_cache;

struct CBSConfig {
    bool use_conflict_prioritization = true;
    bool use_first_conflict          = false;
    bool use_ancestor_branching          = false;
    bool use_dominance          = false;
    bool use_pair_decomposition         = false;
    bool use_greed_conflic_resultion_asstimation=false;
    bool use_MDA_sets=true;
    bool use_MDA_cache=true;
    HeuristicType heuristic          = HeuristicType::HCBS;
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