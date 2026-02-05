#!/usr/bin/env python3
"""Compare CP_DP vs LBCS heuristic results, correlated with RS (Resource Strength)."""

import csv
import sys
from collections import defaultdict

def load_results(filepath):
    """Load solver results into dict keyed by (group, exam)."""
    results = {}
    with open(filepath) as f:
        reader = csv.DictReader(f)
        for row in reader:
            key = (int(row['group']), int(row['exam']))
            results[key] = {
                'time': float(row['time']),
                'finished': row['finished'] == 'True',
                'makespan': int(row['makespan']),
                'expanded': int(row['expand_number']),
                'generated': int(row['generated_number']),
            }
    return results

def load_rs(filepath):
    """Load RS values from TT_ido.csv keyed by (group, exam)."""
    rs_map = {}
    with open(filepath) as f:
        reader = csv.DictReader(f)
        for row in reader:
            key = (int(row['group']), int(row['exam']))
            rs_map[key] = float(row['RS'])
    return rs_map

def main():
    base = "data/"
    cp_file = base + "2026-02-01_j30_g1-48_e1-10_tt_cp_dp.csv"
    lbcs_file = base + "2026-02-02_j30_g1-48_e1-10_tt_lbcs.csv"
    rs_file = base + "TT_ido.csv"

    cp = load_results(cp_file)
    lbcs = load_results(lbcs_file)
    rs_map = load_rs(rs_file)

    # Only compare problems that both attempted
    common = sorted(set(cp.keys()) & set(lbcs.keys()))
    print(f"Common problems: {len(common)}")
    print(f"CP total: {len(cp)}, LBCS total: {len(lbcs)}")
    print()

    # Categorize results
    both_solved = []
    lbcs_only = []
    cp_only = []
    neither = []

    for key in common:
        c, l = cp[key], lbcs[key]
        if c['finished'] and l['finished']:
            both_solved.append(key)
        elif l['finished'] and not c['finished']:
            lbcs_only.append(key)
        elif c['finished'] and not l['finished']:
            cp_only.append(key)
        else:
            neither.append(key)

    print("=" * 80)
    print("SUMMARY")
    print("=" * 80)
    print(f"Both solved:      {len(both_solved)}")
    print(f"LBCS only solved: {len(lbcs_only)}")
    print(f"CP only solved:   {len(cp_only)}")
    print(f"Neither solved:   {len(neither)}")
    print()

    if lbcs_only:
        print("--- LBCS solved but CP didn't ---")
        for key in lbcs_only:
            rs = rs_map.get(key, -1)
            print(f"  g{key[0]:2d} e{key[1]:2d}  RS={rs:.2f}  LBCS: {lbcs[key]['time']:.1f}s  expanded={lbcs[key]['expanded']}")
        print()

    if cp_only:
        print("--- CP solved but LBCS didn't ---")
        for key in cp_only:
            rs = rs_map.get(key, -1)
            print(f"  g{key[0]:2d} e{key[1]:2d}  RS={rs:.2f}  CP: {cp[key]['time']:.1f}s  expanded={cp[key]['expanded']}")
        print()

    if neither:
        print("--- Neither solved (both timed out) ---")
        for key in neither:
            rs = rs_map.get(key, -1)
            print(f"  g{key[0]:2d} e{key[1]:2d}  RS={rs:.2f}  CP expanded={cp[key]['expanded']:,}  LBCS expanded={lbcs[key]['expanded']:,}")
        print()

    # Also check: problems CP solved but LBCS hasn't attempted yet
    cp_only_not_attempted = sorted(set(cp.keys()) - set(lbcs.keys()))
    if cp_only_not_attempted:
        # Group by solved/unsolved in CP
        cp_solved_not_in_lbcs = [k for k in cp_only_not_attempted if cp[k]['finished']]
        cp_unsolved_not_in_lbcs = [k for k in cp_only_not_attempted if not cp[k]['finished']]
        print(f"--- CP attempted but LBCS hasn't reached yet: {len(cp_only_not_attempted)} problems ---")
        print(f"    Of which CP solved: {len(cp_solved_not_in_lbcs)}, CP unsolved: {len(cp_unsolved_not_in_lbcs)}")
        print()

    # For both_solved: compare node expansions
    if both_solved:
        print("=" * 80)
        print("BOTH SOLVED - Node Expansion Comparison")
        print("=" * 80)

        lbcs_better = 0
        cp_better = 0
        equal = 0
        total_ratio = 0

        # Group by RS buckets
        rs_buckets = defaultdict(list)

        for key in both_solved:
            c, l = cp[key], lbcs[key]
            rs = rs_map.get(key, -1)
            ratio = l['expanded'] / c['expanded'] if c['expanded'] > 0 else 0
            total_ratio += ratio

            if l['expanded'] < c['expanded']:
                lbcs_better += 1
            elif l['expanded'] > c['expanded']:
                cp_better += 1
            else:
                equal += 1

            # Bucket RS into ranges
            if rs <= 0.2:
                bucket = "0.00-0.20 (very low)"
            elif rs <= 0.4:
                bucket = "0.21-0.40 (low)"
            elif rs <= 0.6:
                bucket = "0.41-0.60 (medium)"
            elif rs <= 0.8:
                bucket = "0.61-0.80 (high)"
            else:
                bucket = "0.81-1.00 (very high)"

            rs_buckets[bucket].append({
                'key': key,
                'cp_expanded': c['expanded'],
                'lbcs_expanded': l['expanded'],
                'ratio': ratio,
                'cp_time': c['time'],
                'lbcs_time': l['time'],
                'rs': rs,
            })

        avg_ratio = total_ratio / len(both_solved)
        print(f"LBCS fewer nodes: {lbcs_better} ({100*lbcs_better/len(both_solved):.1f}%)")
        print(f"CP fewer nodes:   {cp_better} ({100*cp_better/len(both_solved):.1f}%)")
        print(f"Equal:            {equal}")
        print(f"Avg LBCS/CP expansion ratio: {avg_ratio:.4f} (lower = LBCS better)")
        print()

        print("=" * 80)
        print("BREAKDOWN BY RESOURCE STRENGTH (RS)")
        print("=" * 80)
        for bucket in sorted(rs_buckets.keys()):
            items = rs_buckets[bucket]
            n = len(items)
            avg_r = sum(x['ratio'] for x in items) / n
            lbcs_wins = sum(1 for x in items if x['lbcs_expanded'] < x['cp_expanded'])
            avg_cp_exp = sum(x['cp_expanded'] for x in items) / n
            avg_lbcs_exp = sum(x['lbcs_expanded'] for x in items) / n
            avg_cp_time = sum(x['cp_time'] for x in items) / n
            avg_lbcs_time = sum(x['lbcs_time'] for x in items) / n

            print(f"\n  RS {bucket}: {n} problems")
            print(f"    LBCS fewer nodes: {lbcs_wins}/{n} ({100*lbcs_wins/n:.0f}%)")
            print(f"    Avg expansion ratio (LBCS/CP): {avg_r:.4f}")
            print(f"    Avg CP expanded:   {avg_cp_exp:,.0f}")
            print(f"    Avg LBCS expanded: {avg_lbcs_exp:,.0f}")
            print(f"    Avg CP time:   {avg_cp_time:.2f}s")
            print(f"    Avg LBCS time: {avg_lbcs_time:.2f}s")

        # Print detailed per-problem comparison for problems with big differences
        print()
        print("=" * 80)
        print("TOP 10 BIGGEST IMPROVEMENTS (LBCS vs CP by expansion ratio)")
        print("=" * 80)
        all_items = []
        for items in rs_buckets.values():
            all_items.extend(items)
        all_items.sort(key=lambda x: x['ratio'])

        print(f"  {'Problem':>10}  {'RS':>5}  {'CP expanded':>12}  {'LBCS expanded':>14}  {'Ratio':>7}  {'CP time':>8}  {'LBCS time':>9}")
        print(f"  {'-'*10}  {'-'*5}  {'-'*12}  {'-'*14}  {'-'*7}  {'-'*8}  {'-'*9}")
        for item in all_items[:10]:
            k = item['key']
            print(f"  g{k[0]:2d} e{k[1]:2d}   {item['rs']:5.2f}  {item['cp_expanded']:>12,}  {item['lbcs_expanded']:>14,}  {item['ratio']:>7.4f}  {item['cp_time']:>7.2f}s  {item['lbcs_time']:>8.2f}s")

if __name__ == '__main__':
    main()
