#!/usr/bin/env python3
"""Compare CP_DP vs LBCS vs LBCC heuristic results, correlated with RS (Resource Strength)."""

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

def get_rs_bucket(rs):
    if rs <= 0.2:
        return "0.00-0.20 (very low)"
    elif rs <= 0.4:
        return "0.21-0.40 (low)"
    elif rs <= 0.6:
        return "0.41-0.60 (medium)"
    elif rs <= 0.8:
        return "0.61-0.80 (high)"
    else:
        return "0.81-1.00 (very high)"

def main():
    base = "data/"
    base_date = base + "real_data/"
    cp_file = base_date + "2026-02-01_j30_g1-48_e1-10_tt_cp_dp.csv"
    lbcs_file = base_date + "2026-02-02_j30_g1-48_e1-10_tt_lbcs.csv"
    lbcc_file = base + "2026-02-09_j30_g1-48_e1-10_tt_lbcc.csv"
    rs_file = base + "TT_ido.csv"

    cp = load_results(cp_file)
    lbcs = load_results(lbcs_file)
    lbcc = load_results(lbcc_file)
    rs_map = load_rs(rs_file)

    common = sorted(set(cp.keys()) & set(lbcs.keys()) & set(lbcc.keys()))
    print(f"Common problems: {len(common)}")
    print(f"CP total: {len(cp)}, LBCS total: {len(lbcs)}, LBCC total: {len(lbcc)}")
    print()

    # Solve counts
    cp_solved = sum(1 for k in common if cp[k]['finished'])
    lbcs_solved = sum(1 for k in common if lbcs[k]['finished'])
    lbcc_solved = sum(1 for k in common if lbcc[k]['finished'])

    print("=" * 90)
    print("SOLVE SUMMARY")
    print("=" * 90)
    print(f"CP   solved: {cp_solved}/{len(common)}")
    print(f"LBCS solved: {lbcs_solved}/{len(common)}")
    print(f"LBCC solved: {lbcc_solved}/{len(common)}")
    print()

    # Unique solves
    for name, data in [("CP", cp), ("LBCS", lbcs), ("LBCC", lbcc)]:
        others = [d for n, d in [("CP", cp), ("LBCS", lbcs), ("LBCC", lbcc)] if n != name]
        unique = [k for k in common if data[k]['finished'] and not others[0][k]['finished'] and not others[1][k]['finished']]
        if unique:
            print(f"--- {name} uniquely solved ({len(unique)}) ---")
            for key in unique:
                rs = rs_map.get(key, -1)
                print(f"  g{key[0]:2d} e{key[1]:2d}  RS={rs:.2f}  time={data[key]['time']:.1f}s  expanded={data[key]['expanded']}")
            print()

    # LBCC vs CP pairwise
    all_three_solved = [k for k in common if cp[k]['finished'] and lbcs[k]['finished'] and lbcc[k]['finished']]

    print("=" * 90)
    print(f"ALL THREE SOLVED: {len(all_three_solved)} problems")
    print("=" * 90)

    if all_three_solved:
        # Pairwise comparisons
        for name_a, data_a, name_b, data_b in [
            ("LBCS", lbcs, "CP", cp),
            ("LBCC", lbcc, "CP", cp),
            ("LBCC", lbcc, "LBCS", lbcs),
        ]:
            a_better = sum(1 for k in all_three_solved if data_a[k]['expanded'] < data_b[k]['expanded'])
            b_better = sum(1 for k in all_three_solved if data_a[k]['expanded'] > data_b[k]['expanded'])
            equal = sum(1 for k in all_three_solved if data_a[k]['expanded'] == data_b[k]['expanded'])
            ratios = [data_a[k]['expanded'] / data_b[k]['expanded'] for k in all_three_solved if data_b[k]['expanded'] > 0]
            avg_ratio = sum(ratios) / len(ratios) if ratios else 0

            print(f"\n  {name_a} vs {name_b}:")
            print(f"    {name_a} fewer nodes: {a_better} ({100*a_better/len(all_three_solved):.1f}%)")
            print(f"    {name_b} fewer nodes: {b_better} ({100*b_better/len(all_three_solved):.1f}%)")
            print(f"    Equal:              {equal}")
            print(f"    Avg {name_a}/{name_b} expansion ratio: {avg_ratio:.4f} (lower = {name_a} better)")

    # Breakdown by RS
    print()
    print("=" * 90)
    print("BREAKDOWN BY RESOURCE STRENGTH (RS)")
    print("=" * 90)

    rs_buckets = defaultdict(list)
    for key in all_three_solved:
        rs = rs_map.get(key, -1)
        rs_buckets[get_rs_bucket(rs)].append(key)

    for bucket in sorted(rs_buckets.keys()):
        keys = rs_buckets[bucket]
        n = len(keys)

        avg_cp_exp = sum(cp[k]['expanded'] for k in keys) / n
        avg_lbcs_exp = sum(lbcs[k]['expanded'] for k in keys) / n
        avg_lbcc_exp = sum(lbcc[k]['expanded'] for k in keys) / n

        avg_cp_time = sum(cp[k]['time'] for k in keys) / n
        avg_lbcs_time = sum(lbcs[k]['time'] for k in keys) / n
        avg_lbcc_time = sum(lbcc[k]['time'] for k in keys) / n

        lbcs_vs_cp = sum(1 for k in keys if lbcs[k]['expanded'] < cp[k]['expanded'])
        lbcc_vs_cp = sum(1 for k in keys if lbcc[k]['expanded'] < cp[k]['expanded'])
        lbcc_vs_lbcs = sum(1 for k in keys if lbcc[k]['expanded'] < lbcs[k]['expanded'])

        print(f"\n  RS {bucket}: {n} problems")
        print(f"    Avg expanded  - CP: {avg_cp_exp:>12,.0f}  LBCS: {avg_lbcs_exp:>12,.0f}  LBCC: {avg_lbcc_exp:>12,.0f}")
        print(f"    Avg time (s)  - CP: {avg_cp_time:>12.2f}  LBCS: {avg_lbcs_time:>12.2f}  LBCC: {avg_lbcc_time:>12.2f}")
        print(f"    Fewer nodes   - LBCS<CP: {lbcs_vs_cp}/{n}  LBCC<CP: {lbcc_vs_cp}/{n}  LBCC<LBCS: {lbcc_vs_lbcs}/{n}")

    # Top 10 biggest LBCC improvements vs CP
    print()
    print("=" * 90)
    print("TOP 10 BIGGEST IMPROVEMENTS (LBCC vs CP by expansion ratio)")
    print("=" * 90)
    items = []
    for k in all_three_solved:
        if cp[k]['expanded'] > 0:
            items.append({
                'key': k,
                'rs': rs_map.get(k, -1),
                'cp_exp': cp[k]['expanded'],
                'lbcs_exp': lbcs[k]['expanded'],
                'lbcc_exp': lbcc[k]['expanded'],
                'ratio': lbcc[k]['expanded'] / cp[k]['expanded'],
                'cp_time': cp[k]['time'],
                'lbcc_time': lbcc[k]['time'],
            })
    items.sort(key=lambda x: x['ratio'])

    print(f"  {'Problem':>10}  {'RS':>5}  {'CP expanded':>12}  {'LBCS expanded':>13}  {'LBCC expanded':>13}  {'LBCC/CP':>7}  {'CP time':>8}  {'LBCC time':>9}")
    print(f"  {'-'*10}  {'-'*5}  {'-'*12}  {'-'*13}  {'-'*13}  {'-'*7}  {'-'*8}  {'-'*9}")
    for item in items[:10]:
        k = item['key']
        print(f"  g{k[0]:2d} e{k[1]:2d}   {item['rs']:5.2f}  {item['cp_exp']:>12,}  {item['lbcs_exp']:>13,}  {item['lbcc_exp']:>13,}  {item['ratio']:>7.4f}  {item['cp_time']:>7.2f}s  {item['lbcc_time']:>8.2f}s")

if __name__ == '__main__':
    main()
