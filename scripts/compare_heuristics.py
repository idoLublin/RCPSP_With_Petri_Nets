#!/usr/bin/env python3
"""
Compare heuristics (CP, LBCS, LBCC) for RCPSP solver.
FOCUS: Success/Failure is the PRIMARY metric. Node comparison is SECONDARY.

Features:
- Compare against optimal makespans from j30opt.sm
- Analyze performance by Resource Strength (RS)

Usage:
    python scripts/compare_heuristics.py
    python scripts/compare_heuristics.py --optimal /path/to/j30opt.sm
"""

import pandas as pd
import numpy as np
import argparse
from pathlib import Path
import sys

def load_csv(path):
    """Load CSV file, return None if not found or empty."""
    try:
        df = pd.read_csv(path)
        if len(df) == 0:
            return None
        return df
    except Exception as e:
        print(f"Warning: Could not load {path}: {e}")
        return None

def load_optimal_makespans(path):
    """Parse j30opt.sm file to get optimal makespans."""
    optimal = {}
    try:
        with open(path) as f:
            for line in f:
                # Format: "1       1         43       0.30"
                parts = line.split()
                if len(parts) >= 3 and parts[0].isdigit():
                    group = int(parts[0])
                    exam = int(parts[1])
                    makespan = int(parts[2])
                    optimal[(group, exam)] = makespan
    except Exception as e:
        print(f"Warning: Could not load optimal makespans from {path}: {e}")
    return optimal

def load_rs_data(path):
    """Load RS (Resource Strength) values from TT_ido.csv."""
    rs_map = {}
    try:
        df = pd.read_csv(path)
        for _, row in df.iterrows():
            rs_map[(row['group'], row['exam'])] = row['RS']
    except Exception as e:
        print(f"Warning: Could not load RS data from {path}: {e}")
    return rs_map

def compare_heuristics(cp_path, lbcs_path, lbcc_path, optimal_path, rs_path):
    """Compare three heuristics and print statistics."""

    # Load data
    cp_df = load_csv(cp_path)
    lbcs_df = load_csv(lbcs_path)
    lbcc_df = load_csv(lbcc_path)
    optimal = load_optimal_makespans(optimal_path) if optimal_path else {}
    rs_map = load_rs_data(rs_path) if rs_path else {}

    print("=" * 80)
    print("HEURISTIC COMPARISON REPORT")
    print("PRIMARY METRIC: Success (Solved or Not)")
    print("SECONDARY METRIC: Node Expansion (only for solved problems)")
    print("=" * 80)

    # Check what's loaded
    print("\n### DATA LOADED ###")
    print(f"CP:   {len(cp_df) if cp_df is not None else 0} problems tested")
    print(f"LBCS: {len(lbcs_df) if lbcs_df is not None else 0} problems tested")
    print(f"LBCC: {len(lbcc_df) if lbcc_df is not None else 0} problems tested")
    print(f"Optimal makespans: {len(optimal)} problems")
    print(f"RS data: {len(rs_map)} problems")

    if cp_df is None:
        print("ERROR: CP results required as baseline!")
        return

    # Get all problems from all files
    all_problems = set()
    if cp_df is not None:
        all_problems.update(zip(cp_df['group'], cp_df['exam']))
    if lbcs_df is not None:
        all_problems.update(zip(lbcs_df['group'], lbcs_df['exam']))
    if lbcc_df is not None:
        all_problems.update(zip(lbcc_df['group'], lbcc_df['exam']))

    all_problems = sorted(all_problems)

    # Build result dictionaries
    def get_result(df, group, exam):
        """Get result for a problem. Returns (finished, makespan, expanded, time) or None."""
        if df is None:
            return None
        row = df[(df['group'] == group) & (df['exam'] == exam)]
        if len(row) == 0:
            return None
        row = row.iloc[0]
        return (row['finished'], row['makespan'], row['expand_number'], row['time'])

    # =========================================================================
    # SECTION 0: OPTIMAL MAKESPAN VERIFICATION
    # =========================================================================
    if optimal:
        print("\n" + "=" * 80)
        print("### OPTIMAL MAKESPAN VERIFICATION ###")
        print("Comparing found makespans against known optimal values from j30opt.sm")
        print("=" * 80)

        cp_optimal = cp_suboptimal = cp_better = 0
        lbcs_optimal = lbcs_suboptimal = lbcs_better = 0
        lbcc_optimal = lbcc_suboptimal = lbcc_better = 0

        problems_with_issues = []

        for group, exam in all_problems:
            opt = optimal.get((group, exam))
            if opt is None:
                continue

            cp_res = get_result(cp_df, group, exam)
            lbcs_res = get_result(lbcs_df, group, exam)
            lbcc_res = get_result(lbcc_df, group, exam)

            # Check CP
            if cp_res and cp_res[0]:  # solved
                if cp_res[1] == opt:
                    cp_optimal += 1
                elif cp_res[1] > opt:
                    cp_suboptimal += 1
                    problems_with_issues.append((group, exam, "CP", cp_res[1], opt))
                else:
                    cp_better += 1
                    problems_with_issues.append((group, exam, "CP BETTER?", cp_res[1], opt))

            # Check LBCS
            if lbcs_res and lbcs_res[0]:
                if lbcs_res[1] == opt:
                    lbcs_optimal += 1
                elif lbcs_res[1] > opt:
                    lbcs_suboptimal += 1
                    problems_with_issues.append((group, exam, "LBCS", lbcs_res[1], opt))
                else:
                    lbcs_better += 1
                    problems_with_issues.append((group, exam, "LBCS BETTER?", lbcs_res[1], opt))

            # Check LBCC
            if lbcc_res and lbcc_res[0]:
                if lbcc_res[1] == opt:
                    lbcc_optimal += 1
                elif lbcc_res[1] > opt:
                    lbcc_suboptimal += 1
                    problems_with_issues.append((group, exam, "LBCC", lbcc_res[1], opt))
                else:
                    lbcc_better += 1
                    problems_with_issues.append((group, exam, "LBCC BETTER?", lbcc_res[1], opt))

        cp_total_solved = cp_optimal + cp_suboptimal + cp_better
        lbcs_total_solved = lbcs_optimal + lbcs_suboptimal + lbcs_better
        lbcc_total_solved = lbcc_optimal + lbcc_suboptimal + lbcc_better

        print(f"\n{'Heuristic':<10} {'Optimal':<10} {'Suboptimal':<12} {'Total Solved':<15}")
        print("-" * 50)
        print(f"{'CP':<10} {cp_optimal:<10} {cp_suboptimal:<12} {cp_total_solved:<15}" if cp_total_solved > 0 else "CP: N/A")
        print(f"{'LBCS':<10} {lbcs_optimal:<10} {lbcs_suboptimal:<12} {lbcs_total_solved:<15}" if lbcs_total_solved > 0 else "LBCS: N/A")
        print(f"{'LBCC':<10} {lbcc_optimal:<10} {lbcc_suboptimal:<12} {lbcc_total_solved:<15}" if lbcc_total_solved > 0 else "LBCC: N/A")

        if problems_with_issues:
            print(f"\n*** PROBLEMS WITH MAKESPAN ISSUES ({len(problems_with_issues)} total) ***")
            print(f"{'Problem':<10} {'Heuristic':<15} {'Found':<10} {'Optimal':<10} {'Diff':<10}")
            print("-" * 60)
            for g, e, heuristic, found, opt in problems_with_issues:
                diff = int(found) - opt
                print(f"{g},{e:<8} {heuristic:<15} {int(found):<10} {opt:<10} {diff:+d}")
        else:
            print("\nAll solved problems found OPTIMAL makespan. Admissibility VERIFIED.")

    # =========================================================================
    # SECTION 1: SUCCESS RATE SUMMARY (MOST IMPORTANT)
    # =========================================================================
    print("\n" + "=" * 80)
    print("### SECTION 1: SUCCESS RATE (PRIMARY METRIC) ###")
    print("=" * 80)

    cp_solved = lbcs_solved = lbcc_solved = 0
    cp_tested = lbcs_tested = lbcc_tested = 0

    for group, exam in all_problems:
        cp_res = get_result(cp_df, group, exam)
        lbcs_res = get_result(lbcs_df, group, exam)
        lbcc_res = get_result(lbcc_df, group, exam)

        if cp_res is not None:
            cp_tested += 1
            if cp_res[0]:
                cp_solved += 1
        if lbcs_res is not None:
            lbcs_tested += 1
            if lbcs_res[0]:
                lbcs_solved += 1
        if lbcc_res is not None:
            lbcc_tested += 1
            if lbcc_res[0]:
                lbcc_solved += 1

    print(f"\n{'Heuristic':<12} {'Solved':<10} {'Tested':<10} {'Success Rate':<15}")
    print("-" * 50)
    print(f"{'CP':<12} {cp_solved:<10} {cp_tested:<10} {100*cp_solved/cp_tested:.1f}%" if cp_tested > 0 else "CP: N/A")
    print(f"{'LBCS':<12} {lbcs_solved:<10} {lbcs_tested:<10} {100*lbcs_solved/lbcs_tested:.1f}%" if lbcs_tested > 0 else "LBCS: N/A")
    print(f"{'LBCC':<12} {lbcc_solved:<10} {lbcc_tested:<10} {100*lbcc_solved/lbcc_tested:.1f}%" if lbcc_tested > 0 else "LBCC: N/A")

    # =========================================================================
    # SECTION 2: PERFORMANCE BY RESOURCE STRENGTH (RS)
    # =========================================================================
    if rs_map:
        print("\n" + "=" * 80)
        print("### SECTION 2: PERFORMANCE BY RESOURCE STRENGTH (RS) ###")
        print("RS=0.2 (Low - tight resources) to RS=1.0 (High - loose resources)")
        print("=" * 80)

        rs_values = [0.2, 0.5, 0.7, 1.0]
        rs_labels = {0.2: "Low (tight)", 0.5: "Medium-low", 0.7: "Medium-high", 1.0: "High (loose)"}

        for rs in rs_values:
            # Get problems with this RS where ALL 3 heuristics were tested
            rs_problems_all3 = []
            for (g, e) in all_problems:
                if rs_map.get((g, e)) != rs:
                    continue
                cp_res = get_result(cp_df, g, e)
                lbcs_res = get_result(lbcs_df, g, e)
                lbcc_res = get_result(lbcc_df, g, e)
                # Only include if all 3 were tested
                if cp_res is not None and lbcs_res is not None and lbcc_res is not None:
                    rs_problems_all3.append((g, e, cp_res, lbcs_res, lbcc_res))

            if not rs_problems_all3:
                continue

            print(f"\n--- RS={rs} ({rs_labels[rs]}) - {len(rs_problems_all3)} problems (tested by all 3) ---")

            # Success counts (only problems tested by all 3)
            cp_s = sum(1 for _, _, cp, _, _ in rs_problems_all3 if cp[0])
            lbcs_s = sum(1 for _, _, _, lbcs, _ in rs_problems_all3 if lbcs[0])
            lbcc_s = sum(1 for _, _, _, _, lbcc in rs_problems_all3 if lbcc[0])
            total = len(rs_problems_all3)

            # Print success rates
            print(f"  Success Rate (on {total} common problems):")
            print(f"    CP:   {cp_s}/{total} ({100*cp_s/total:.0f}%)" if total > 0 else "    CP:   N/A")
            print(f"    LBCS: {lbcs_s}/{total} ({100*lbcs_s/total:.0f}%)" if total > 0 else "    LBCS: N/A")
            print(f"    LBCC: {lbcc_s}/{total} ({100*lbcc_s/total:.0f}%)" if total > 0 else "    LBCC: N/A")

            # Node reduction for commonly solved (all 3 solved)
            common_solved = [(g, e, cp, lbcs, lbcc) for g, e, cp, lbcs, lbcc in rs_problems_all3
                             if cp[0] and lbcs[0] and lbcc[0]]
            if common_solved:
                cp_nodes = sum(cp[2] for _, _, cp, _, _ in common_solved)
                lbcs_nodes = sum(lbcs[2] for _, _, _, lbcs, _ in common_solved)
                lbcc_nodes = sum(lbcc[2] for _, _, _, _, lbcc in common_solved)
                if cp_nodes > 0:
                    lbcs_reduction = (cp_nodes - lbcs_nodes) / cp_nodes * 100
                    lbcc_reduction = (cp_nodes - lbcc_nodes) / cp_nodes * 100
                    print(f"  Node Reduction (on {len(common_solved)} all-solved):")
                    print(f"    LBCS vs CP: {lbcs_reduction:+.1f}%")
                    print(f"    LBCC vs CP: {lbcc_reduction:+.1f}%")

    # =========================================================================
    # SECTION 3: INTERESTING CASES (Different outcomes)
    # =========================================================================
    print("\n" + "=" * 80)
    print("### SECTION 3: INTERESTING CASES (Different Success/Failure) ###")
    print("=" * 80)

    interesting_cases = []
    for group, exam in all_problems:
        cp_res = get_result(cp_df, group, exam)
        lbcs_res = get_result(lbcs_df, group, exam)
        lbcc_res = get_result(lbcc_df, group, exam)

        cp_status = "YES" if (cp_res and cp_res[0]) else ("NO" if cp_res else None)
        lbcs_status = "YES" if (lbcs_res and lbcs_res[0]) else ("NO" if lbcs_res else None)
        lbcc_status = "YES" if (lbcc_res and lbcc_res[0]) else ("NO" if lbcc_res else None)

        statuses = [s for s in [cp_status, lbcs_status, lbcc_status] if s is not None]
        if "YES" in statuses and "NO" in statuses:
            interesting_cases.append((group, exam, cp_res, lbcs_res, lbcc_res))

    if interesting_cases:
        for group, exam, cp_res, lbcs_res, lbcc_res in interesting_cases:
            prob = f"{group},{exam}"
            rs = rs_map.get((group, exam), "?")
            print(f"\nProblem {prob} (RS={rs}):")

            if cp_res:
                if cp_res[0]:
                    print(f"  CP:   SOLVED in {cp_res[3]:.1f}s, makespan={int(cp_res[1])}, nodes={int(cp_res[2]):,}")
                else:
                    print(f"  CP:   TIMEOUT after {cp_res[3]:.1f}s, nodes={int(cp_res[2]):,}")
            else:
                print(f"  CP:   Not tested")

            if lbcs_res:
                if lbcs_res[0]:
                    print(f"  LBCS: SOLVED in {lbcs_res[3]:.1f}s, makespan={int(lbcs_res[1])}, nodes={int(lbcs_res[2]):,}")
                else:
                    print(f"  LBCS: TIMEOUT after {lbcs_res[3]:.1f}s, nodes={int(lbcs_res[2]):,}")
            else:
                print(f"  LBCS: Not tested")

            if lbcc_res:
                if lbcc_res[0]:
                    print(f"  LBCC: SOLVED in {lbcc_res[3]:.1f}s, makespan={int(lbcc_res[1])}, nodes={int(lbcc_res[2]):,}")
                else:
                    print(f"  LBCC: TIMEOUT after {lbcc_res[3]:.1f}s, nodes={int(lbcc_res[2]):,}")
            else:
                print(f"  LBCC: Not tested")
    else:
        print("\nNo interesting cases found (all heuristics have same success/failure status)")

    # =========================================================================
    # SECTION 4: NODE EXPANSION (ONLY FOR COMMONLY SOLVED PROBLEMS)
    # =========================================================================
    print("\n" + "=" * 80)
    print("### SECTION 4: NODE EXPANSION (SECONDARY METRIC) ###")
    print("Only comparing problems that ALL tested heuristics SOLVED")
    print("=" * 80)

    # Find problems solved by all
    commonly_solved_cp_lbcs = []
    commonly_solved_cp_lbcc = []

    for group, exam in all_problems:
        cp_res = get_result(cp_df, group, exam)
        lbcs_res = get_result(lbcs_df, group, exam)
        lbcc_res = get_result(lbcc_df, group, exam)

        if cp_res and cp_res[0]:
            if lbcs_res and lbcs_res[0]:
                commonly_solved_cp_lbcs.append((group, exam, cp_res, lbcs_res))
            if lbcc_res and lbcc_res[0]:
                commonly_solved_cp_lbcc.append((group, exam, cp_res, lbcc_res))

    print(f"\nProblems solved by both CP and LBCS: {len(commonly_solved_cp_lbcs)}")
    print(f"Problems solved by both CP and LBCC: {len(commonly_solved_cp_lbcc)}")

    # --- CP vs LBCS ---
    if commonly_solved_cp_lbcs:
        print(f"\n--- LBCS vs CP (on {len(commonly_solved_cp_lbcs)} commonly solved problems) ---")

        cp_total = sum(r[2][2] for r in commonly_solved_cp_lbcs)
        lbcs_total = sum(r[3][2] for r in commonly_solved_cp_lbcs)

        lbcs_better = sum(1 for r in commonly_solved_cp_lbcs if r[3][2] < r[2][2])
        lbcs_same = sum(1 for r in commonly_solved_cp_lbcs if r[3][2] == r[2][2])
        lbcs_worse = sum(1 for r in commonly_solved_cp_lbcs if r[3][2] > r[2][2])

        reduction = (cp_total - lbcs_total) / cp_total * 100 if cp_total > 0 else 0

        print(f"  Total nodes - CP: {cp_total:,}, LBCS: {lbcs_total:,}")
        print(f"  Overall reduction: {reduction:+.1f}%")
        print(f"  LBCS expands fewer nodes: {lbcs_better} problems")
        print(f"  Same nodes: {lbcs_same} problems")
        print(f"  LBCS expands more nodes: {lbcs_worse} problems")

    # --- CP vs LBCC ---
    if commonly_solved_cp_lbcc:
        print(f"\n--- LBCC vs CP (on {len(commonly_solved_cp_lbcc)} commonly solved problems) ---")

        cp_total = sum(r[2][2] for r in commonly_solved_cp_lbcc)
        lbcc_total = sum(r[3][2] for r in commonly_solved_cp_lbcc)

        lbcc_better = sum(1 for r in commonly_solved_cp_lbcc if r[3][2] < r[2][2])
        lbcc_same = sum(1 for r in commonly_solved_cp_lbcc if r[3][2] == r[2][2])
        lbcc_worse = sum(1 for r in commonly_solved_cp_lbcc if r[3][2] > r[2][2])

        reduction = (cp_total - lbcc_total) / cp_total * 100 if cp_total > 0 else 0

        print(f"  Total nodes - CP: {cp_total:,}, LBCC: {lbcc_total:,}")
        print(f"  Overall reduction: {reduction:+.1f}%")
        print(f"  LBCC expands fewer nodes: {lbcc_better} problems")
        print(f"  Same nodes: {lbcc_same} problems")
        print(f"  LBCC expands more nodes: {lbcc_worse} problems")

    # =========================================================================
    # FINAL SUMMARY
    # =========================================================================
    print("\n" + "=" * 80)
    print("### FINAL SUMMARY ###")
    print("=" * 80)

    print(f"\nSuccess Rate:")
    print(f"  CP:   {cp_solved}/{cp_tested} ({100*cp_solved/cp_tested:.1f}%)" if cp_tested > 0 else "  CP: N/A")
    print(f"  LBCS: {lbcs_solved}/{lbcs_tested} ({100*lbcs_solved/lbcs_tested:.1f}%)" if lbcs_tested > 0 else "  LBCS: N/A")
    print(f"  LBCC: {lbcc_solved}/{lbcc_tested} ({100*lbcc_solved/lbcc_tested:.1f}%)" if lbcc_tested > 0 else "  LBCC: N/A")

    print(f"\nInteresting cases (different success/failure): {len(interesting_cases)}")

    print("\n" + "=" * 80)
    print("END OF REPORT")
    print("=" * 80)

def main():
    parser = argparse.ArgumentParser(description='Compare RCPSP heuristics')
    parser.add_argument('--cp', default='data/results_cp_tt.csv', help='CP results CSV')
    parser.add_argument('--lbcs', default='data/results_lbcs_correct.csv', help='LBCS results CSV')
    parser.add_argument('--lbcc', default='data/results_lbcc_tt.csv', help='LBCC results CSV')
    parser.add_argument('--optimal', default=None, help='Path to j30opt.sm file with optimal makespans')
    parser.add_argument('--rs', default='data/TT_ido.csv', help='Path to CSV with RS values')

    args = parser.parse_args()

    # Get project root (script is in scripts/)
    script_dir = Path(__file__).parent
    project_root = script_dir.parent

    cp_path = project_root / args.cp
    lbcs_path = project_root / args.lbcs
    lbcc_path = project_root / args.lbcc
    rs_path = project_root / args.rs if args.rs else None
    optimal_path = args.optimal  # Use as-is if absolute path

    compare_heuristics(cp_path, lbcs_path, lbcc_path, optimal_path, rs_path)

if __name__ == '__main__':
    main()
