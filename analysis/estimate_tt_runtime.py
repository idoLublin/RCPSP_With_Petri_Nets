"""Estimate wall time for the pending TT (TTPN) sweeps — WITHOUT running them.

Two bounds per (set, heuristic) configuration, sequential single-core:

  worst case   480 instances x 300 s = 40.0 h per configuration (hard ceiling)
  realistic    j30: measured totals of the full TT j30 sweeps that exist in
               data/real_data (laptop machine, same 300 s protocol) — timeouts
               dominate the total, so machine differences barely move it.
               j60/j90: lower-bounded by the measured TT2 total on the same
               set/heuristic (TT solves fewer instances than TT2 and pays 300 s
               for each extra failure), so the realistic figure sits between
               that bound and the 40 h cap — plan with the cap.

Scopes reported:
  (i)   full matrix:  5 heuristics x {j30, j60, j90}
  (ii)  recommended:  5 heuristics x j30, plus {cp, lbcs} x j60
        (cp = baseline, lbcs = best TT2 heuristic)
  (iii) minimal:      {cp, lbcs} x j30

Usage:
    python3 analysis/estimate_tt_runtime.py
"""

import csv
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
MASTER = Path(__file__).resolve().parent / "output" / "master_results.csv"
TIMEOUT = 300.0

# full 480-instance TT j30 sweeps from the laptop (same protocol, 300 s)
LAPTOP_TT_J30 = {
    "cp": "data/real_data/2026-02-01_j30_g1-48_e1-10_tt_cp_dp.csv",
    "lbcs": "data/real_data/2026-02-02_j30_g1-48_e1-10_tt_lbcs.csv",
    "lbcc": "data/real_data/2026-02-09_j30_g1-48_e1-10_tt_lbcs.csv".replace("lbcs", "lbcc"),
    "lbip0": "data/real_data/2026-05-30_j30_g1-48_e1-10_tt_lbip0.csv",
    # no laptop TT lbmax sweep exists; LBMAX = max(CP, hres) behaves like CP
    "lbmax": None,
}

HEURISTICS = ["cp", "lbcs", "lbcc", "lbip0", "lbmax"]
WORST_PER_CONFIG_H = 480 * TIMEOUT / 3600  # 40 h


def solver_csv_total_hours(path):
    """(total wall hours, solved, timeouts) of a full solver CSV, capping
    unsolved rows at the 300 s budget."""
    total, solved, timeouts = 0.0, 0, 0
    with open(REPO_ROOT / path, newline="") as fh:
        reader = csv.reader(fh)
        next(reader)
        for row in reader:
            if not row or not row[0].strip():
                continue
            if row[3] == "True":
                solved += 1
                total += float(row[2])
            else:
                timeouts += 1
                total += TIMEOUT
    return total / 3600, solved, timeouts


def tt2_total_hours(master_rows, set_name, heur):
    total = 0.0
    for r in master_rows:
        if (r["set"] == set_name and r["config"] == "base"
                and r["heuristic"].lower() == heur):
            total += float(r["time"]) if r["solved"] == "True" else TIMEOUT
    return total / 3600


def estimate(set_name, heur, laptop_cache, master_rows):
    """-> (realistic_low_h, realistic_high_h, note)"""
    if set_name == "j30":
        key = heur if LAPTOP_TT_J30.get(heur) else "cp"
        hours, solved, timeouts = laptop_cache[key]
        note = f"measured laptop TT j30 ({solved} solved, {timeouts} timeouts)"
        if key != heur:
            note += " [proxied by cp; no TT lbmax run exists]"
        return hours, hours, note
    low = tt2_total_hours(master_rows, set_name, heur)
    return low, WORST_PER_CONFIG_H, f"TT2 {set_name} total {low:.1f} h is a lower bound"


def report_scope(title, configs, laptop_cache, master_rows):
    print(f"\n{title}")
    print(f"  {'set':4} {'heuristic':9} {'worst (h)':>9} {'realistic (h)':>14}  basis")
    worst_sum, low_sum, high_sum = 0.0, 0.0, 0.0
    for set_name, heur in configs:
        low, high, note = estimate(set_name, heur, laptop_cache, master_rows)
        worst_sum += WORST_PER_CONFIG_H
        low_sum += low
        high_sum += high
        realistic = f"{low:.1f}" if low == high else f"{low:.1f}-{high:.1f}"
        print(f"  {set_name:4} {heur:9} {WORST_PER_CONFIG_H:9.1f} {realistic:>14}  {note}")
    days = worst_sum / 24
    if abs(high_sum - low_sum) < 0.5:
        realistic_total = f"~{low_sum:.0f} h ({low_sum / 24:.1f} days)"
    else:
        realistic_total = (f"{low_sum:.0f}-{high_sum:.0f} h "
                           f"({low_sum / 24:.1f}-{high_sum / 24:.1f} days)")
    print(f"  TOTAL: worst case {worst_sum:.0f} h ({days:.1f} days); "
          f"realistic {realistic_total}, sequential single-core")


def main():
    laptop_cache = {}
    for heur, path in LAPTOP_TT_J30.items():
        if path:
            laptop_cache[heur] = solver_csv_total_hours(path)
    with open(MASTER, newline="") as fh:
        master_rows = [r for r in csv.DictReader(fh)
                       if r["set"] in ("j60", "j90") and r["config"] == "base"]

    print("TT (TTPN) sweep wall-time estimates — 300 s timeout, 480 instances "
          "per set, one core, sequential")
    print(f"per-configuration hard ceiling: {WORST_PER_CONFIG_H:.0f} h")

    full = [(s, h) for s in ("j30", "j60", "j90") for h in HEURISTICS]
    recommended = [("j30", h) for h in HEURISTICS] + [("j60", "cp"), ("j60", "lbcs")]
    minimal = [("j30", "cp"), ("j30", "lbcs")]

    report_scope("(i) FULL MATRIX: 5 heuristics x j30+j60+j90", full,
                 laptop_cache, master_rows)
    report_scope("(ii) RECOMMENDED: 5 x j30 + baseline cp & best lbcs on j60",
                 recommended, laptop_cache, master_rows)
    report_scope("(iii) MINIMAL: cp + lbcs on j30 only", minimal,
                 laptop_cache, master_rows)
    print("\nNo sweep launched. Start one with e.g.:")
    print("  analysis/run_tt_sweep.sh j30 cp lbcs lbcc lbip0 lbmax")


if __name__ == "__main__":
    main()
