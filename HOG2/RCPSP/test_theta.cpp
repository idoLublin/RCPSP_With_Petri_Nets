// Standalone unit test for the Θ-tree ECT construction (ThetaTree.h).
// Build:  g++ -std=c++20 -O2 test_theta.cpp -o test_theta && ./test_theta
#include "ThetaTree.h"
#include <cstdio>
#include <vector>
#include <utility>

static int failures = 0;

static void check(const char* name, long got, long want) {
    bool ok = (got == want);
    if (!ok) ++failures;
    std::printf("[%s] %-28s got=%ld want=%ld\n", ok ? "PASS" : "FAIL", name, got, want);
}

int main() {
    using L = std::vector<std::pair<long, long>>;  // (est, energy)

    // 1. Empty set -> 0.
    check("empty", thetaTreeECT(L{}, 2), 0);

    // 2. Single task: est=0, p=2, dem=1 (energy=2), C=2 -> ceil((0*2+2)/2)=1.
    check("single_est0", thetaTreeECT(L{{0, 2}}, 2), 1);

    // 3. Single task with est: est=5, p=2, dem=2 (energy=4), C=2
    //    -> ceil((5*2+4)/2)=ceil(14/2)=7  (start >=5, run 2 -> finish >=7).
    check("single_est5", thetaTreeECT(L{{5, 4}}, 2), 7);

    // 4. Classic 3-task, C=2: A(est0,e2) B(est0,e2) C(est0,e4).
    //    All est 0 -> Env = 0 + 8 = 8 -> ceil(8/2)=4.
    //    (A,B run parallel [0,2); C alone [2,4).)
    check("three_tasks_est0", thetaTreeECT(L{{0, 2}, {0, 2}, {0, 4}}, 2), 4);

    // 5. est-anchor dominates: A(est0,e4) B(est3,e4), C=2.
    //    {B} -> 3*2+4=10 -> 5 ; {A,B} -> 0+8=8 -> 4 ; max picks 5.
    check("est_anchor_wins", thetaTreeECT(L{{0, 4}, {3, 4}}, 2), 5);

    // 6. Same set, but the aggregate (all together) wins over any anchor:
    //    A(est0,e6) B(est1,e6) C(est2,e6), C=3.
    //    {all}: min est 0 -> 0+18=18 -> ceil(18/3)=6.
    //    {B,C}: 1*3+12=15 -> 5 ; {C}: 2*3+6=12 -> 4. Max=6.
    check("aggregate_wins", thetaTreeECT(L{{0, 6}, {1, 6}, {2, 6}}, 3), 6);

    // 7. Reduces to the energy bound when est all 0: Σenergy/C.
    //    energies 3,3,3,3 (=12), C=4 -> ceil(12/4)=3.
    check("energy_bound_special", thetaTreeECT(L{{0, 3}, {0, 3}, {0, 3}, {0, 3}}, 4), 3);

    // 8. Capacity 1 (unary): three unit tasks est 0,1,2 each energy 1.
    //    {all} min est 0 -> 0+3=3 ; {est2} -> 2*1+1=3. Max 3 -> ceil(3/1)=3.
    check("unary_chain", thetaTreeECT(L{{0, 1}, {1, 1}, {2, 1}}, 1), 3);

    // 9. Non-power-of-two count (5 leaves) exercises padding.
    //    est all 0, energies 1,1,1,1,1 (=5), C=2 -> ceil(5/2)=3.
    check("five_leaves_pad", thetaTreeECT(L{{0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}}, 2), 3);

    // 10. Later high-est heavy task with earlier light tasks, C=2:
    //     A(est0,e2) B(est0,e2) D(est10,e6).
    //     {D}: 10*2+6=26 -> 13 ; {all}: 0+10=10 -> 5. Max 13.
    check("late_heavy", thetaTreeECT(L{{0, 2}, {0, 2}, {10, 6}}, 2), 13);

    std::printf("\n%s (%d failure%s)\n", failures ? "TESTS FAILED" : "ALL TESTS PASSED",
                failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
