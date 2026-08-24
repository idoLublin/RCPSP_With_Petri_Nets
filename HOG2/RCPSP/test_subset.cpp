// Unit test for the mini subset-RCPSP lower-bound solver (SubsetSolver.h).
// Build:  g++ -std=c++20 -O2 test_subset.cpp -o test_subset && ./test_subset
#include "SubsetSolver.h"
#include <cstdio>

static int failures = 0;
static void check(const char* name, long got, long want) {
    bool ok = (got == want);
    if (!ok) ++failures;
    std::printf("[%s] %-30s got=%ld want=%ld\n", ok ? "PASS" : "FAIL", name, got, want);
}
static void checkb(const char* name, bool got, bool want) {
    bool ok = (got == want);
    if (!ok) ++failures;
    std::printf("[%s] %-30s got=%d want=%d\n", ok ? "PASS" : "FAIL", name, got, want);
}

static SubsetInstance make(int n, int R, std::vector<int> dur, std::vector<int> rel,
                           std::vector<int> cap, std::vector<std::vector<int>> dem,
                           std::vector<std::vector<int>> preds) {
    SubsetInstance P; P.n=n; P.R=R; P.dur=dur; P.release=rel; P.cap=cap; P.demand=dem; P.preds=preds;
    return P;
}
static long lbOf(const SubsetInstance& P, long cap) { return subsetRcpspLB(P, cap).lb; }
static const long BIG = 1000000;  // effectively-unlimited budget

int main() {
    // 1. Single activity: makespan = duration.
    check("single", lbOf(make(1,1,{5},{0},{10},{{0}},{{}}), BIG), 5);

    // 2. Two activities, each demand 2 of cap 3 -> cannot overlap -> sequential 3+4.
    check("seq_by_resource", lbOf(make(2,1,{3,4},{0,0},{3},{{2},{2}},{{},{}}), BIG), 7);

    // 3. Same but cap 4 -> both fit in parallel -> max(3,4)=4.
    check("parallel_fits", lbOf(make(2,1,{3,4},{0,0},{4},{{2},{2}},{{},{}}), BIG), 4);

    // 4. Precedence chain 0->1 (no resource conflict) -> 3+4.
    check("precedence_chain", lbOf(make(2,1,{3,4},{0,0},{9},{{1},{1}},{{},{0}}), BIG), 7);

    // 5. Release time shifts the whole thing.
    check("release_shift", lbOf(make(1,1,{2},{5},{9},{{1}},{{}}), BIG), 7);

    // 6. Unary chain: 3 tasks, each demand 1 of cap 1, dur 2 -> forced serial -> 6.
    check("unary_serialize", lbOf(make(3,1,{2,2,2},{0,0,0},{1},{{1},{1},{1}},{{},{},{}}), BIG), 6);

    // 7. Two resources: demands only collide on r0 -> serial on r0 -> 4.
    check("two_resources", lbOf(make(2,2,{2,2},{0,0},{2,5},{{2,0},{2,0}},{{},{}}), BIG), 4);

    // 8. Chain of three with mixed durations.
    check("chain_three", lbOf(make(3,1,{2,3,4},{0,0,0},{9},{{1},{1},{1}},{{},{0},{1}}), BIG), 9);

    // 9. expandCap=0 -> resource-BLIND CPM root bound (valid LB <= opt). Case 2 opt=7, CPM=4.
    {
        auto r = subsetRcpspLB(make(2,1,{3,4},{0,0},{3},{{2},{2}},{{},{}}), 0);
        check ("expandcap0_lb",     r.lb, 4);
        checkb("expandcap0_capped", r.capped, true);   // stopped on the cap, not solved
        check ("expandcap0_expands", r.expands, 0);
    }

    // 10. Telemetry on a fully-solved instance: capped=false, some expansions used.
    {
        auto r = subsetRcpspLB(make(3,1,{2,2,2},{0,0,0},{1},{{1},{1},{1}},{{},{},{}}), BIG);
        check ("solved_lb",     r.lb, 6);
        checkb("solved_capped", r.capped, false);
        checkb("solved_expands_pos", r.expands > 0, true);
    }

    std::printf("\n%s (%d failure%s)\n", failures ? "TESTS FAILED" : "ALL TESTS PASSED",
                failures, failures==1?"":"s");
    return failures ? 1 : 0;
}
