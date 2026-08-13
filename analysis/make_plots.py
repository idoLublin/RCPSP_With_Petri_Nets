"""Generate the book's figures from the master CSV.

Reads analysis/output/master_results.csv and writes, per benchmark set, into
output/figures/ (each figure as 300-dpi PNG and SVG):

    cactus_<set>.{png,svg}        instances solved within a time budget,
                                  one line per configuration
    solved_by_RS_<set>.{png,svg}  solved count grouped by Resource Strength
    solved_by_RF_<set>.{png,svg}  solved count grouped by Resource Factor

Styling is print-first: white background, thin black axes, no grid junk, and
series identity carried by line style / hatching (not color alone), so the
figures survive grayscale printing and colorblind readers alike.

Usage:
    python3 analysis/make_plots.py [--master PATH] [--sets j30 j60 j90]
                                   [--configs "TT2 LBCS" "TT2 LBCS +dom" ...]
"""

import argparse
import csv
from collections import defaultdict
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

OUTPUT_DIR = Path(__file__).resolve().parent / "output"
FIGURES_DIR = OUTPUT_DIR / "figures"

TIMEOUT = 300.0
INSTANCES_PER_SET = 480

# print-safe series styling: grey level + line style + marker, assigned in
# fixed order to configurations sorted by final solved count (best first)
SERIES_STYLES = [
    dict(color="0.0",  linestyle="-",  marker="o"),
    dict(color="0.35", linestyle="--", marker="s"),
    dict(color="0.0",  linestyle=":",  marker="^"),
    dict(color="0.5",  linestyle="-.", marker="D"),
    dict(color="0.35", linestyle="-",  marker="v"),
    dict(color="0.0",  linestyle="--", marker="x"),
    dict(color="0.5",  linestyle=":",  marker="P"),
    dict(color="0.35", linestyle="-.", marker="*"),
    dict(color="0.0",  linestyle=(0, (5, 1, 1, 1)), marker="<"),
    dict(color="0.5",  linestyle="--", marker=">"),
    dict(color="0.35", linestyle=":",  marker="h"),
    dict(color="0.0",  linestyle="-",  marker="s"),
]
BAR_FILLS = [
    dict(color="0.15", hatch=""),
    dict(color="0.45", hatch=""),
    dict(color="0.75", hatch=""),
    dict(color="1.0",  hatch="///"),
    dict(color="0.9",  hatch="..."),
    dict(color="0.6",  hatch="\\\\\\"),
    dict(color="1.0",  hatch="xxx"),
    dict(color="0.8",  hatch="||"),
    dict(color="0.3",  hatch="//"),
    dict(color="0.95", hatch="---"),
    dict(color="0.55", hatch="++"),
    dict(color="1.0",  hatch="\\\\"),
]

STYLE = {
    "figure.facecolor": "white",
    "axes.facecolor": "white",
    "axes.edgecolor": "black",
    "axes.linewidth": 0.8,
    "axes.spines.top": False,
    "axes.spines.right": False,
    "xtick.color": "black",
    "ytick.color": "black",
    "xtick.direction": "out",
    "ytick.direction": "out",
    "font.family": "serif",
    "font.size": 10,
    "axes.labelsize": 10,
    "axes.titlesize": 11,
    "legend.fontsize": 8.5,
    "legend.frameon": False,
    "savefig.dpi": 300,
    "savefig.bbox": "tight",
}


def config_label(row):
    label = f"{row['model']} {row['heuristic']}"
    if row["config"] == "dom":
        label += " +dom"
    elif row["config"] == "dom_dr4":
        label += " +dom+dr4"
    elif row["config"] == "ttdr":
        label += " +dr"
    elif row["config"].startswith("lber_"):
        label += f" ({row['config'].split('_')[1]})"
    return label


def load(path):
    with open(path, newline="") as fh:
        return list(csv.DictReader(fh))


def save(fig, stem):
    FIGURES_DIR.mkdir(parents=True, exist_ok=True)
    for ext in ("png", "svg"):
        fig.savefig(FIGURES_DIR / f"{stem}.{ext}")
    plt.close(fig)
    print(f"  wrote figures/{stem}.png + .svg")


def cactus(set_name, by_config):
    """Solved-within-budget curves. by_config: {label: [solved times]}."""
    order = sorted(by_config, key=lambda l: -len(by_config[l]))
    fig, ax = plt.subplots(figsize=(5.2, 3.4))
    for style, label in zip(SERIES_STYLES, order):
        times = sorted(by_config[label])
        xs, ys = [], []
        for i, t in enumerate(times, start=1):
            xs.append(max(t, 1e-2))
            ys.append(i)
        markevery = max(1, len(xs) // 9)
        ax.plot(xs, ys, linewidth=1.4, markersize=4.5, markevery=markevery,
                markerfacecolor="white", label=f"{label} ({len(times)})", **style)
    ax.set_xscale("log")
    ax.set_xlim(1e-2, TIMEOUT * 1.15)
    ax.set_ylim(0, INSTANCES_PER_SET * 1.02)
    ax.axvline(TIMEOUT, color="0.6", linewidth=0.8, linestyle=(0, (1, 2)))
    ax.text(TIMEOUT, ax.get_ylim()[1] * 0.02, " 300 s", color="0.4",
            fontsize=8, ha="left", va="bottom")
    ax.set_xlabel("time budget per instance (s)")
    ax.set_ylabel(f"instances solved (of {INSTANCES_PER_SET})")
    ax.set_title(f"{set_name.upper()}: instances solved within time budget")
    # curves that plateau high leave the lower-right corner free; curves that
    # plateau low (j90) leave the upper-left free instead
    best = max(len(t) for t in by_config.values())
    loc = "lower right" if best > 0.8 * INSTANCES_PER_SET else "upper left"
    ax.legend(loc=loc, handlelength=2.6)
    save(fig, f"cactus_{set_name}")


def grouped_bars(set_name, param, by_config_level, levels, per_level):
    """Solved counts grouped by a generator parameter (RS or RF)."""
    order = sorted(by_config_level,
                   key=lambda l: -sum(by_config_level[l].values()))
    n = len(order)
    width = 0.8 / n
    fig, ax = plt.subplots(figsize=(5.6, 3.2))
    for k, (fill, label) in enumerate(zip(BAR_FILLS, order)):
        xs = [i + (k - (n - 1) / 2) * width for i in range(len(levels))]
        ys = [by_config_level[label].get(lv, 0) for lv in levels]
        ax.bar(xs, ys, width=width * 0.94, label=label,
               facecolor=fill["color"], hatch=fill["hatch"],
               edgecolor="black", linewidth=0.6)
    ax.axhline(per_level, color="0.6", linewidth=0.8, linestyle=(0, (1, 2)))
    ax.text(len(levels) - 0.52, per_level, f"{per_level} instances ",
            color="0.4", fontsize=8, ha="right", va="bottom")
    ax.set_xticks(range(len(levels)))
    ax.set_xticklabels([f"{lv:g}" for lv in levels])
    ax.set_xlabel(f"{param} ({'Resource Strength' if param == 'RS' else 'Resource Factor'})")
    ax.set_ylabel(f"instances solved (of {per_level})")
    ax.set_ylim(0, per_level * 1.06)
    ax.set_title(f"{set_name.upper()}: solved by {param}")
    ax.legend(loc="upper left", bbox_to_anchor=(1.01, 1.0), handlelength=1.6)
    save(fig, f"solved_by_{param}_{set_name}")


def scaling_chart(best_by_set):
    """One-glance presentation chart: best configuration's solved count per
    set. best_by_set: {set: (label, solved)}."""
    sets = list(best_by_set)
    fig, ax = plt.subplots(figsize=(5.0, 3.2))
    solved = [best_by_set[s][1] for s in sets]
    ax.bar(range(len(sets)), solved, width=0.55,
           facecolor="0.35", edgecolor="black", linewidth=0.8)
    ax.axhline(INSTANCES_PER_SET, color="0.6", linewidth=0.8,
               linestyle=(0, (1, 2)))
    for x, s in enumerate(sets):
        label, n = best_by_set[s]
        pct = 100 * n / INSTANCES_PER_SET
        ax.text(x, n + 12, f"{n}/480 ({pct:.0f}%)", ha="center", fontsize=9)
        ax.text(x, 14, label.replace("TT2 ", "").replace(" ", "\n"),
                ha="center", fontsize=8, color="white")
    ax.set_xticks(range(len(sets)))
    ax.set_xticklabels([s.upper() for s in sets])
    ax.set_ylabel(f"instances solved (of {INSTANCES_PER_SET})")
    ax.set_ylim(0, INSTANCES_PER_SET * 1.12)
    ax.set_title("Best configuration per set (300 s)")
    save(fig, "scaling_summary")


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--master", default=str(OUTPUT_DIR / "master_results.csv"))
    ap.add_argument("--sets", nargs="+", default=["j30", "j60", "j90"])
    ap.add_argument("--configs", nargs="+", default=None,
                    help='labels to include, e.g. "TT2 LBCS" "TT2 LBCS +dom"; '
                         "default: every complete configuration")
    ap.add_argument("--machine", default="university",
                    help="only rows from this machine ('all' disables); "
                         "default university")
    ap.add_argument("--model", default="TT2",
                    help="only rows from this model ('all' disables); default "
                         "TT2 — TT-vs-TT2 figures come from compare_models.py")
    args = ap.parse_args()

    plt.rcParams.update(STYLE)
    rows = load(args.master)
    if args.machine != "all":
        rows = [r for r in rows if r.get("machine", "university") == args.machine]
    if args.model != "all":
        rows = [r for r in rows if r["model"] == args.model]

    best_by_set = {}

    for set_name in args.sets:
        set_rows = [r for r in rows if r["set"] == set_name]
        if not set_rows:
            print(f"{set_name}: no rows, skipped")
            continue

        counts = defaultdict(int)
        for r in set_rows:
            counts[config_label(r)] += 1
        labels = {l for l, c in counts.items() if c == INSTANCES_PER_SET}
        partial = set(counts) - labels
        if args.configs:
            labels = {l for l in args.configs if l in counts}
        elif partial:
            print(f"{set_name}: excluding partial configs {sorted(partial)}")
        if len(labels) > len(SERIES_STYLES):
            raise SystemExit(f"{set_name}: {len(labels)} configs > "
                             f"{len(SERIES_STYLES)} styles; pass --configs")

        solved_times = defaultdict(list)
        by_rs = defaultdict(lambda: defaultdict(int))
        by_rf = defaultdict(lambda: defaultdict(int))
        for r in set_rows:
            label = config_label(r)
            if label not in labels or r["solved"] != "True":
                continue
            solved_times[label].append(float(r["time"]))
            by_rs[label][float(r["RS"])] += 1
            by_rf[label][float(r["RF"])] += 1

        best_label = max(solved_times, key=lambda l: len(solved_times[l]))
        best_by_set[set_name] = (best_label, len(solved_times[best_label]))

        print(f"{set_name}:")
        cactus(set_name, solved_times)
        rs_levels = sorted({float(r["RS"]) for r in set_rows})
        rf_levels = sorted({float(r["RF"]) for r in set_rows})
        grouped_bars(set_name, "RS", by_rs, rs_levels,
                     INSTANCES_PER_SET // len(rs_levels))
        grouped_bars(set_name, "RF", by_rf, rf_levels,
                     INSTANCES_PER_SET // len(rf_levels))

    if len(best_by_set) > 1:
        print("scaling summary:")
        scaling_chart(best_by_set)


if __name__ == "__main__":
    main()
