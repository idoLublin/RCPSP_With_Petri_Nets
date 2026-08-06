"""Build the master results CSV from the university-compute result files.

Scans data/university_compute_real_final_resluts/ recursively, classifies
every CSV, dedupes byte-identical files, joins the PSPLIB NC/RF/RS
parameters, and writes:

    analysis/output/master_results.csv   one row per (instance x model x
                                         heuristic x config)
    analysis/output/coverage.md          coverage matrix + inconsistency report

The schema is model-agnostic: future TT (TTPN) runs produced by
analysis/run_tt_sweep.sh drop into the same results tree (or are passed via
--extra-dir) and merge with model=TT.

Solver CSV quirks handled here:
  * base/lber files declare 23 header columns but rows carry only 11 fields
    (the trailing timing-profile block is legacy cruft) -> we parse by
    position, never by header;
  * dom_* files carry 14 fully-populated columns (adds dominance_pruned,
    dominance_pop_pruned, ub_pruned; newer builds add dr4_pruned as a 15th);
  * unsolved rows have finished=False, makespan=0 -> makespan left empty.

Usage:
    python3 analysis/build_master.py [--results-dir DIR] [--extra-dir DIR ...]
"""

import argparse
import csv
import hashlib
import re
import sys
from collections import defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from psplib_params import group_params, validate_against_tt_ido

REPO_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_RESULTS_DIR = REPO_ROOT / "data" / "university_compute_real_final_resluts"
OUTPUT_DIR = Path(__file__).resolve().parent / "output"

TIMEOUT_SECONDS = 300.0

MASTER_COLUMNS = [
    "set", "group", "exam", "instance", "NC", "RF", "RS",
    "model", "heuristic", "config", "dominance",
    "solved", "makespan", "time", "expand_number", "generated_number", "depth",
    "dominance_pruned", "dominance_pop_pruned", "ub_pruned", "dr4_pruned",
    "run_date", "machine", "source_file",
]

# filename families ---------------------------------------------------------

# 2026-07-07_j30_g1-48_e1-10_tt2_cp_j30.csv   (optionally " (1)" before .csv)
BASE_RE = re.compile(
    r"^(?P<date>\d{4}-\d{2}-\d{2})_(?P<set>j\d+)_g[\d-]+_e[\d-]+_"
    r"(?P<model>tt2?|tp)_(?P<tag>.+?)(?: \(\d+\))?\.csv$"
)
# dom_j30_lbcs.csv / dom_j90_max.csv
DOM_RE = re.compile(r"^dom_(?P<set>j\d+)_(?P<heur>\w+)\.csv$")
# lber_j30_d4.csv / lber_j30_d3.csv — LBER at delta setting dN
LBER_RE = re.compile(r"^lber_(?P<set>j\d+)_d(?P<d>\d+)\.csv$")

MODEL_NAMES = {"tp": "TP", "tt": "TT", "tt2": "TT2"}


def classify(path: Path):
    """Return (config, run_date) for a result file, or None if unrecognized."""
    name = path.name
    if DOM_RE.match(name):
        return "dom", ""
    m = LBER_RE.match(name)
    if m:
        return f"lber_d{m.group('d')}", ""
    m = BASE_RE.match(name)
    if m:
        # TT runs from the university machine encode the flavor in the tag:
        # "ttdr" = Liu dominance rules DR1/DR2/DR5 on, "nodom" = plain TT
        tag = m.group("tag")
        if "ttdr" in tag:
            return "ttdr", m.group("date")
        return "base", m.group("date")
    return None


def parse_file(path: Path, config: str, run_date: str, machine: str):
    """Yield master-schema dict rows from one solver CSV."""
    with open(path, newline="") as fh:
        reader = csv.reader(fh)
        header = next(reader, None)
        if not header or header[0] != "group":
            raise ValueError(f"{path}: unexpected header {header[:3]}")
        for lineno, row in enumerate(reader, start=2):
            if not row or not row[0].strip():
                continue
            if len(row) < 11:
                raise ValueError(f"{path}:{lineno}: only {len(row)} fields")
            (group, exam, time_s, finished, makespan, expand, generated,
             depth, petri_type, set_type, heuristic_raw) = row[:11]
            group, exam = int(group), int(exam)
            solved = finished.strip() == "True"
            nc, rf, rs = group_params(set_type, group)
            heuristic = heuristic_raw.replace("_DP", "").replace("_NoDP", "")
            out = {
                "set": set_type,
                "group": group,
                "exam": exam,
                "instance": f"{set_type}{group}_{exam}",
                "NC": nc, "RF": rf, "RS": rs,
                "model": petri_type,
                "heuristic": heuristic,
                "config": config,
                "dominance": config in ("dom", "ttdr"),
                "solved": solved,
                "makespan": makespan if solved else "",
                "time": float(time_s),
                "expand_number": int(expand),
                "generated_number": int(generated),
                "depth": depth if solved else "",
                "dominance_pruned": "",
                "dominance_pop_pruned": "",
                "ub_pruned": "",
                "dr4_pruned": "",
                "run_date": run_date,
                "machine": machine,
                "source_file": str(path.relative_to(REPO_ROOT)),
            }
            # dom files: 14 populated columns (15 with dr4_pruned in newer builds)
            if len(row) >= 14 and row[11].strip():
                out["dominance_pruned"] = int(row[11])
                out["dominance_pop_pruned"] = int(row[12])
                out["ub_pruned"] = int(row[13])
                if len(row) >= 15 and row[14].strip():
                    out["dr4_pruned"] = int(row[14])
            yield out


def build(sources):
    """sources: list of (path, machine) where path is a directory or a file."""
    csv_files = []
    for src, machine in sources:
        src = Path(src).resolve()
        found = [src] if src.is_file() else sorted(src.rglob("*.csv"))
        if not found:
            sys.exit(f"no CSV files found under {src}")
        csv_files += [(p, machine) for p in found]

    # dedupe byte-identical files (browser "(1)"/"(2)" download copies)
    seen_hashes = {}
    duplicates = []
    kept_files = []
    for path, machine in csv_files:
        digest = hashlib.md5(path.read_bytes()).hexdigest()
        if digest in seen_hashes:
            duplicates.append((path, seen_hashes[digest]))
        else:
            seen_hashes[digest] = path
            kept_files.append((path, machine))

    rows = []
    skipped = []
    for path, machine in kept_files:
        kind = classify(path)
        if kind is None:
            skipped.append(path)
            continue
        config, run_date = kind
        rows.extend(parse_file(path, config, run_date, machine))

    rows.sort(key=lambda r: (r["set"], r["model"], r["config"], r["heuristic"],
                             r["group"], r["exam"]))
    return rows, duplicates, skipped, kept_files


def write_master(rows, out_path):
    out_path.parent.mkdir(parents=True, exist_ok=True)
    with open(out_path, "w", newline="") as fh:
        writer = csv.DictWriter(fh, fieldnames=MASTER_COLUMNS)
        writer.writeheader()
        writer.writerows(rows)


def write_coverage(rows, duplicates, skipped, out_path):
    """Coverage matrix + inconsistency report as markdown."""
    stats = defaultdict(lambda: {"n": 0, "solved": 0, "max_time": 0.0,
                                 "files": set(), "pairs": set()})
    for r in rows:
        key = (r["set"], r["model"], r["config"], r["heuristic"], r["machine"])
        s = stats[key]
        s["n"] += 1
        s["solved"] += r["solved"]
        s["max_time"] = max(s["max_time"], r["time"])
        s["files"].add(r["source_file"])
        s["pairs"].add((r["group"], r["exam"]))

    lines = ["# Results coverage report", "",
             f"Source rows: {len(rows)} across {len({f for s in stats.values() for f in s['files']})} files.",
             f"Per-instance timeout: {TIMEOUT_SECONDS:.0f} s (overshoot up to ~10 s occurs because the limit is checked between node expansions).",
             "",
             "## Coverage matrix",
             "",
             "| set | model | config | heuristic | machine | rows | solved | timeouts | max time (s) | status |",
             "|---|---|---|---|---|---:|---:|---:|---:|---|"]
    for key in sorted(stats):
        s = stats[key]
        set_name, model, config, heur, machine = key
        dup_pairs = s["n"] - len(s["pairs"])
        status = "complete" if s["n"] == 480 else f"PARTIAL ({s['n']}/480)"
        if dup_pairs:
            status += f", {dup_pairs} repeated instance rows"
        lines.append(
            f"| {set_name} | {model} | {config} | {heur} | {machine} | {s['n']} | {s['solved']} "
            f"| {s['n'] - s['solved']} | {s['max_time']:.1f} | {status} |")

    lines += ["", "## Duplicate files (byte-identical, one kept)", ""]
    if duplicates:
        for dup, kept in duplicates:
            lines.append(f"- `{dup.name}` == `{kept.name}` (dropped duplicate)")
    else:
        lines.append("- none")

    lines += ["", "## Unrecognized files (skipped)", ""]
    if skipped:
        lines += [f"- `{p.name}`" for p in skipped]
    else:
        lines.append("- none")

    lines += ["", "## Missing combinations", ""]
    sets = sorted({r["set"] for r in rows})
    heuristics = sorted({r["heuristic"] for r in rows})
    configs = sorted({r["config"] for r in rows})
    for config in configs:
        for set_name in sets:
            for heur in heuristics:
                key = (set_name, "TT2", config, heur, "university")
                if key not in stats and not (config.startswith("lber") and heur != "LBER") \
                        and not (config in ("base",) and heur == "LBER"):
                    lines.append(f"- {set_name} / {config} / {heur}: not run")
    lines.append("")

    out_path.write_text("\n".join(lines))
    return lines


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--results-dir", default=str(DEFAULT_RESULTS_DIR))
    ap.add_argument("--extra-dir", action="append", default=[],
                    help="additional result trees to merge (e.g. TT runs)")
    ap.add_argument("--extra-file", action="append", default=[],
                    help="individual result CSVs to merge")
    ap.add_argument("--extra-machine", default="laptop",
                    help="machine label for --extra-dir/--extra-file rows "
                         "(default laptop); the main results dir is 'university'")
    args = ap.parse_args()

    n = validate_against_tt_ido()
    print(f"parameter mapping validated against TT_ido.csv ({n} rows)")

    sources = [(args.results_dir, "university")]
    sources += [(d, args.extra_machine) for d in args.extra_dir]
    sources += [(f, args.extra_machine) for f in args.extra_file]
    rows, duplicates, skipped, files = build(sources)
    master_path = OUTPUT_DIR / "master_results.csv"
    coverage_path = OUTPUT_DIR / "coverage.md"
    write_master(rows, master_path)
    write_coverage(rows, duplicates, skipped, coverage_path)
    print(f"{len(files)} files parsed ({len(duplicates)} duplicates dropped, "
          f"{len(skipped)} unrecognized)")
    print(f"wrote {master_path.relative_to(REPO_ROOT)} ({len(rows)} rows)")
    print(f"wrote {coverage_path.relative_to(REPO_ROOT)}")


if __name__ == "__main__":
    main()
