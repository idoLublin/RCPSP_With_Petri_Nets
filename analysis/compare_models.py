"""TT (TTPN) vs TT2 (TTPNR) head-to-head comparison from the master CSV.

For each set and each heuristic present in BOTH models (config=base), emits:

    output/tables/tt_vs_tt2_<set>.csv / .md
        solved counts, commonly-solved count, geometric-mean node ratio
        (TT / TT2) and geometric-mean time ratio on commonly-solved instances
    output/figures/tt_vs_tt2_solved_<set>.{png,svg}
        grouped bars: instances solved per heuristic, TT vs TT2
    output/figures/tt_vs_tt2_nodes_<set>.{png,svg}
        log-log scatter of expanded nodes per commonly-solved instance

Node counts are machine-independent, so the node comparison is valid even
when the two models were run on different machines; the TIME ratio is only
meaningful when both ran on the same machine — the table footnotes it
otherwise.

Usage:
    python3 analysis/compare_models.py [--sets j30 ...] [--master PATH]
"""

import argparse
import csv
import math
from collections import defaultdict
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

OUTPUT_DIR = Path(__file__).resolve().parent / "output"
TABLES_DIR = OUTPUT_DIR / "tables"
FIGURES_DIR = OUTPUT_DIR / "figures"

STYLE = {
    "figure.facecolor": "white",
    "axes.facecolor": "white",
    "axes.edgecolor": "black",
    "axes.linewidth": 0.8,
    "axes.spines.top": False,
    "axes.spines.right": False,
    "font.family": "serif",
    "font.size": 10,
    "legend.fontsize": 8.5,
    "legend.frameon": False,
    "savefig.dpi": 300,
    "savefig.bbox": "tight",
}

HEUR_ORDER = ["CP", "LBCC", "LBIP0", "LBMAX", "LBCS", "LBER"]


def geomean(values):
    return math.exp(sum(math.log(v) for v in values) / len(values)) if values else float("nan")


def load(path):
    with open(path, newline="") as fh:
        return [r for r in csv.DictReader(fh) if r["config"] == "base"]


def save(fig, stem):
    FIGURES_DIR.mkdir(parents=True, exist_ok=True)
    for ext in ("png", "svg"):
        fig.savefig(FIGURES_DIR / f"{stem}.{ext}")
    plt.close(fig)
    print(f"  wrote figures/{stem}.png + .svg")


def compare_set(set_name, rows):
    # {model: {heuristic: {(g,e): row}}}
    data = defaultdict(lambda: defaultdict(dict))
    machines = defaultdict(set)
    for r in rows:
        if r["set"] != set_name or r["model"] not in ("TT", "TT2"):
            continue
        data[r["model"]][r["heuristic"]][(r["group"], r["exam"])] = r
        machines[r["model"]].add(r["machine"])
    common_heurs = [h for h in HEUR_ORDER
                    if h in data["TT"] and h in data["TT2"]]
    if not common_heurs:
        print(f"{set_name}: no heuristic present in both TT and TT2, skipped")
        return

    same_machine = machines["TT"] == machines["TT2"]
    time_caveat = ("" if same_machine else
                   f" TT ran on {'/'.join(sorted(machines['TT']))}, TT2 on "
                   f"{'/'.join(sorted(machines['TT2']))} — time ratios compare "
                   "different machines and are only indicative; node ratios are "
                   "machine-independent.")

    table_rows = []
    scatter = {}
    for heur in common_heurs:
        tt, tt2 = data["TT"][heur], data["TT2"][heur]
        tt_solved = {k for k, r in tt.items() if r["solved"] == "True"}
        tt2_solved = {k for k, r in tt2.items() if r["solved"] == "True"}
        common = tt_solved & tt2_solved
        node_ratios, time_ratios, pts = [], [], []
        for k in common:
            n_tt = int(tt[k]["expand_number"])
            n_tt2 = int(tt2[k]["expand_number"])
            if n_tt > 0 and n_tt2 > 0:
                node_ratios.append(n_tt / n_tt2)
                pts.append((n_tt, n_tt2))
            t_tt, t_tt2 = float(tt[k]["time"]), float(tt2[k]["time"])
            if t_tt > 0.01 and t_tt2 > 0.01:
                time_ratios.append(t_tt / t_tt2)
        scatter[heur] = pts
        table_rows.append({
            "heuristic": heur,
            "TT_solved": len(tt_solved),
            "TT2_solved": len(tt2_solved),
            "both_solved": len(common),
            "geomean_node_ratio_TT_over_TT2": f"{geomean(node_ratios):.2f}",
            "geomean_time_ratio_TT_over_TT2": f"{geomean(time_ratios):.2f}",
        })

    TABLES_DIR.mkdir(parents=True, exist_ok=True)
    with open(TABLES_DIR / f"tt_vs_tt2_{set_name}.csv", "w", newline="") as fh:
        w = csv.DictWriter(fh, fieldnames=table_rows[0].keys())
        w.writeheader()
        w.writerows(table_rows)

    md = [f"### {set_name.upper()}: TT (TTPN) vs TT2 (TTPNR), no dominance rules, "
          "300 s timeout", "",
          "| Heuristic | TT solved | TT2 solved | Both | Node ratio TT/TT2 (geo-mean) | Time ratio TT/TT2 (geo-mean) |",
          "|---|---:|---:|---:|---:|---:|"]
    for r in table_rows:
        md.append(f"| {r['heuristic']} | {r['TT_solved']} | {r['TT2_solved']} "
                  f"| {r['both_solved']} | {r['geomean_node_ratio_TT_over_TT2']} "
                  f"| {r['geomean_time_ratio_TT_over_TT2']} |")
    md += ["", f"_Ratios are geometric means over commonly-solved instances; "
           f"ratio > 1 means TT expands more / is slower.{time_caveat}_", ""]
    (TABLES_DIR / f"tt_vs_tt2_{set_name}.md").write_text("\n".join(md))
    print(f"  wrote tables/tt_vs_tt2_{set_name}.csv + .md")

    # grouped bars: solved per heuristic, TT vs TT2
    fig, ax = plt.subplots(figsize=(4.8, 3.0))
    xs = range(len(common_heurs))
    tt_counts = [r["TT_solved"] for r in table_rows]
    tt2_counts = [r["TT2_solved"] for r in table_rows]
    ax.bar([x - 0.2 for x in xs], tt_counts, width=0.38, label="TT (TTPN)",
           facecolor="1.0", edgecolor="black", linewidth=0.8, hatch="///")
    ax.bar([x + 0.2 for x in xs], tt2_counts, width=0.38, label="TT2 (TTPNR)",
           facecolor="0.3", edgecolor="black", linewidth=0.8)
    for x, (a, b) in zip(xs, zip(tt_counts, tt2_counts)):
        ax.text(x - 0.2, a + 4, str(a), ha="center", fontsize=8)
        ax.text(x + 0.2, b + 4, str(b), ha="center", fontsize=8)
    ax.set_xticks(list(xs))
    ax.set_xticklabels(common_heurs)
    ax.set_ylabel("instances solved (of 480)")
    ax.set_ylim(0, 500)
    ax.axhline(480, color="0.6", linewidth=0.8, linestyle=(0, (1, 2)))
    ax.set_title(f"{set_name.upper()}: solved, TT vs TT2 (300 s)")
    ax.legend(loc="upper left", bbox_to_anchor=(1.01, 1.0))
    save(fig, f"tt_vs_tt2_solved_{set_name}")

    # log-log scatter of expanded nodes on commonly-solved instances
    fig, ax = plt.subplots(figsize=(3.8, 3.8))
    markers = ["o", "s", "^", "D", "v", "x"]
    for heur, marker in zip(common_heurs, markers):
        if not scatter[heur]:
            continue
        xs_, ys_ = zip(*scatter[heur])
        ax.plot(xs_, ys_, marker, markersize=3, markerfacecolor="none",
                markeredgecolor="black", markeredgewidth=0.5, linestyle="none",
                label=heur, alpha=0.6)
    lims = ax.get_xlim() + ax.get_ylim()
    lo, hi = min(lims), max(lims)
    ax.plot([lo, hi], [lo, hi], color="0.5", linewidth=0.8, zorder=0)
    ax.set_xscale("log")
    ax.set_yscale("log")
    ax.set_xlabel("TT expanded nodes")
    ax.set_ylabel("TT2 expanded nodes")
    ax.set_title(f"{set_name.upper()}: nodes per instance\n(below diagonal = TT2 expands fewer)")
    ax.legend(loc="upper left", handletextpad=0.2)
    save(fig, f"tt_vs_tt2_nodes_{set_name}")


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--master", default=str(OUTPUT_DIR / "master_results.csv"))
    ap.add_argument("--sets", nargs="+", default=["j30", "j60", "j90"])
    args = ap.parse_args()

    plt.rcParams.update(STYLE)
    rows = load(args.master)
    for set_name in args.sets:
        print(f"{set_name}:")
        compare_set(set_name, rows)


if __name__ == "__main__":
    main()
