"""Generate the book's summary and ablation tables from the master CSV.

Reads analysis/output/master_results.csv and writes, per benchmark set:

    output/tables/summary_<set>.csv / .md
        rows = configurations (model + heuristic + dominance),
        columns = solved (of 480), avg time on solved, avg expanded on solved
    output/tables/ablation_<set>.csv / .md
        heuristics that exist both with and without dominance pruning;
        value with dominance, base value and delta in parentheses

Averages are computed over the SOLVED subset of each configuration only, so
configurations with different solved sets are not directly comparable on
time/nodes; the markdown output carries that footnote.

Usage:
    python3 analysis/make_tables.py [--master PATH]
"""

import argparse
import csv
from collections import defaultdict
from pathlib import Path

OUTPUT_DIR = Path(__file__).resolve().parent / "output"
TABLES_DIR = OUTPUT_DIR / "tables"

FOOTNOTE = ("Averages are over each configuration's solved instances only; "
            "configurations solving different subsets are not directly "
            "comparable on time/nodes.")

HEUR_ORDER = ["CP", "LBCC", "LBIP0", "LBMAX", "LBCS", "LBER"]


def load_master(path):
    with open(path, newline="") as fh:
        return list(csv.DictReader(fh))


def config_label(row):
    label = f"{row['model']} {row['heuristic']}"
    if row["config"] == "dom":
        label += " +dom"
    elif row["config"] == "ttdr":
        label += " +dr"
    elif row["config"].startswith("lber_"):
        label += f" ({row['config'].split('_')[1]})"
    return label


def aggregate(rows):
    """-> {set: {label: {n, solved, sum_time_solved, sum_expand_solved}}}"""
    agg = defaultdict(lambda: defaultdict(lambda: {
        "n": 0, "solved": 0, "time": 0.0, "expand": 0}))
    for r in rows:
        a = agg[r["set"]][config_label(r)]
        a["n"] += 1
        if r["solved"] == "True":
            a["solved"] += 1
            a["time"] += float(r["time"])
            a["expand"] += int(r["expand_number"])
        a.setdefault("order", (r["model"],
                               HEUR_ORDER.index(r["heuristic"])
                               if r["heuristic"] in HEUR_ORDER else 99,
                               r["config"] != "base"))
    return agg


def fmt_time(t):
    return f"{t:.2f}" if t < 100 else f"{t:.1f}"


def write_summary(set_name, configs):
    rows_out = []
    for label in sorted(configs, key=lambda l: configs[l]["order"]):
        a = configs[label]
        solved = a["solved"]
        rows_out.append({
            "configuration": label,
            "instances": a["n"],
            "solved": solved,
            "avg_time_solved_s": fmt_time(a["time"] / solved) if solved else "",
            "avg_expanded_solved": f"{a['expand'] / solved:.0f}" if solved else "",
        })

    csv_path = TABLES_DIR / f"summary_{set_name}.csv"
    with open(csv_path, "w", newline="") as fh:
        w = csv.DictWriter(fh, fieldnames=rows_out[0].keys())
        w.writeheader()
        w.writerows(rows_out)

    md = [f"### {set_name.upper()} summary — solved of {rows_out[0]['instances']}, "
          "300 s timeout", "",
          "| Configuration | Solved | Avg time on solved (s) | Avg expanded on solved |",
          "|---|---:|---:|---:|"]
    for r in rows_out:
        n_note = f" of {r['instances']}" if r["instances"] != 480 else ""
        md.append(f"| {r['configuration']} | {r['solved']}{n_note} "
                  f"| {r['avg_time_solved_s']} | {int(r['avg_expanded_solved']):,} |"
                  if r["avg_expanded_solved"] else
                  f"| {r['configuration']} | {r['solved']}{n_note} | — | — |")
    md += ["", f"_{FOOTNOTE}_", ""]
    (TABLES_DIR / f"summary_{set_name}.md").write_text("\n".join(md))
    return csv_path


def write_ablation(set_name, configs):
    """Heuristics present both as 'MODEL H' and 'MODEL H +dom'."""
    pairs = []
    for label in sorted(configs, key=lambda l: configs[l]["order"]):
        for suffix in (" +dom", " +dr"):
            if label.endswith(suffix) and label[:-len(suffix)] in configs:
                pairs.append((label[:-len(suffix)], label))
    if not pairs:
        return None

    def stats(a):
        s = a["solved"]
        return (s,
                a["time"] / s if s else float("nan"),
                a["expand"] / s if s else float("nan"))

    rows_out = []
    md = [f"### {set_name.upper()} ablation — dominance pruning on vs off "
          "(base value and delta in parentheses)", "",
          "| Configuration | Solved | Avg time on solved (s) | Avg expanded on solved |",
          "|---|---:|---:|---:|"]
    for base_label, dom_label in pairs:
        bs, bt, be = stats(configs[base_label])
        ds, dt, de = stats(configs[dom_label])
        rows_out.append({
            "configuration": dom_label,
            "solved_dom": ds, "solved_base": bs, "solved_delta": ds - bs,
            "avg_time_dom": f"{dt:.2f}", "avg_time_base": f"{bt:.2f}",
            "avg_expand_dom": f"{de:.0f}", "avg_expand_base": f"{be:.0f}",
        })
        md.append(
            f"| {dom_label} | {ds} ({bs}, {ds - bs:+d}) "
            f"| {fmt_time(dt)} ({fmt_time(bt)}) "
            f"| {de:,.0f} ({be:,.0f}) |")
    md += ["", f"_{FOOTNOTE} Time/node columns show the with-dominance value "
           "and the no-dominance value in parentheses._", ""]

    csv_path = TABLES_DIR / f"ablation_{set_name}.csv"
    with open(csv_path, "w", newline="") as fh:
        w = csv.DictWriter(fh, fieldnames=rows_out[0].keys())
        w.writeheader()
        w.writerows(rows_out)
    (TABLES_DIR / f"ablation_{set_name}.md").write_text("\n".join(md))
    return csv_path


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--master", default=str(OUTPUT_DIR / "master_results.csv"))
    ap.add_argument("--machine", default="university",
                    help="only rows from this machine ('all' disables the "
                         "filter); default university, so book tables never "
                         "mix timings from different machines")
    args = ap.parse_args()

    TABLES_DIR.mkdir(parents=True, exist_ok=True)
    rows = load_master(args.master)
    if args.machine != "all":
        rows = [r for r in rows if r.get("machine", "university") == args.machine]
    agg = aggregate(rows)
    for set_name in sorted(agg):
        write_summary(set_name, agg[set_name])
        write_ablation(set_name, agg[set_name])
        print(f"{set_name}: summary + ablation written")
    print(f"tables in {TABLES_DIR}")


if __name__ == "__main__":
    main()
