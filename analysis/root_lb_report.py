#!/usr/bin/env python3
"""Root-state lower-bound comparison report.

For every instance we evaluated each heuristic at the INITIAL state only
(no search) via `Driver --root-lb`. A run with `--heuristic H` starts A*
from h(root) = max(CP_dp, HRES, extra_H), so the effective root LB is
ceil of that max. CP's column is the baseline hmax = max(CP_dp, HRES).

LBER (enhanced energetic reasoning, depth 3 = CER+DFF+SHV) comes from the
lber-tt2-port branch; its destructive root bound is merged in from
data/root_lb/root_lber_{set}.csv when present, floored the same way.

Truth columns:
    j30 -> true optimal makespan (data/j30opt.sm)
    j60 -> best-known range LB..UB (data/j60lb.sm + data/j60hrs.sm)
    j90 -> none requested (bounds files kept for reference)
"""
import csv
import math
import os
import re

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
DATA = os.path.join(ROOT, "data")

BASE_HEURISTICS = ["CP", "LBCS", "LBCC", "LBIP0", "LBMAX"]


def read_root_csv(problem):
    rows = {}
    with open(os.path.join(DATA, "root_lb", f"root_lb_{problem}.csv")) as f:
        for r in csv.DictReader(f):
            g, e = int(r["Group"]), int(r["Exam"])
            rows[(g, e)] = {k: float(r[k]) for k in
                            ("CP", "HRES", "LBCS", "LBCC", "LBIP0", "LBRC")}
    return rows


def read_lber_csv(problem):
    path = os.path.join(DATA, "root_lb", f"root_lber_{problem}.csv")
    if not os.path.exists(path):
        return None
    out = {}
    with open(path) as f:
        for r in csv.DictReader(f):
            out[(int(r["Group"]), int(r["Exam"]))] = float(r["LBER"])
    return out or None


def read_bounds(path):
    """Parse PSPLIB-style bound files: first three ints on a data line."""
    out = {}
    if not os.path.exists(path):
        return out
    pat = re.compile(r"^\s*(\d+)\s+(\d+)\s+(\d+)")
    with open(path, errors="replace") as f:
        for line in f:
            m = pat.match(line)
            if m:
                g, e, v = map(int, m.groups())
                out[(g, e)] = v
    return out


def effective(raw, hs, lber_val):
    floor = max(raw["CP"], raw["HRES"])
    extras = {"CP": 0.0, "LBCS": raw["LBCS"], "LBCC": raw["LBCC"],
              "LBIP0": raw["LBIP0"], "LBMAX": raw["LBRC"]}
    if lber_val is not None:
        extras["LBER"] = lber_val
    return {h: math.ceil(max(floor, extras[h]) - 1e-9) for h in hs}


def analyze(problem, truth_kind, opt=None, lb=None, ub=None):
    raw_rows = read_root_csv(problem)
    lber = read_lber_csv(problem)
    hs = BASE_HEURISTICS + (["LBER"] if lber else [])
    keys = sorted(raw_rows)
    table = []
    stats = {h: {"win": 0, "strict": 0, "beats_cp": 0, "sum": 0.0,
                 "exact": 0, "gapsum": 0.0} for h in hs}
    best_exact = 0
    gap_best_sum = 0.0
    n_with_opt = 0

    for k in keys:
        eff = effective(raw_rows[k], hs, lber.get(k) if lber else None)
        best = max(eff.values())
        winners = [h for h in hs if eff[h] == best]
        for h in hs:
            stats[h]["sum"] += eff[h]
            if eff[h] == best:
                stats[h]["win"] += 1
            if eff[h] > eff["CP"]:
                stats[h]["beats_cp"] += 1
        if len(winners) == 1:
            stats[winners[0]]["strict"] += 1

        row = {"g": k[0], "e": k[1], "eff": eff, "best": best,
               "winners": winners, "raw": raw_rows[k]}
        if truth_kind == "opt":
            o = opt.get(k)
            row["opt"] = o
            if o is not None:
                n_with_opt += 1
                gap_best_sum += o - best
                if best == o:
                    best_exact += 1
                for h in hs:
                    stats[h]["gapsum"] += o - eff[h]
                    if eff[h] == o:
                        stats[h]["exact"] += 1
        elif truth_kind == "range":
            row["lb"] = lb.get(k)
            row["ub"] = ub.get(k)
            u = row["ub"]
            if u is not None:
                n_with_opt += 1
                gap_best_sum += u - best
                if best == u:  # root bound meets the best-known UB -> optimum proven at root
                    best_exact += 1
                for h in hs:
                    stats[h]["gapsum"] += u - eff[h]
        table.append(row)
    n = len(keys)
    return dict(problem=problem, n=n, hs=hs, table=table, stats=stats,
                best_exact=best_exact, gap_best_sum=gap_best_sum,
                n_with_opt=n_with_opt, truth_kind=truth_kind)


def winners_str(ws, hs):
    return "all (tie)" if len(ws) == len(hs) else ", ".join(ws)


def summary_block(res, gap_label):
    s = res["stats"]
    n = res["n"]
    head = "| Heuristic | Best-at-root (incl. ties) | Sole winner | Beats CP baseline | Mean root LB |"
    sep = "|---|---|---|---|---|"
    if res["truth_kind"] == "opt":
        head += f" Mean gap to {gap_label} | Exact (= optimum) |"
        sep += "---|---|"
    elif res["truth_kind"] == "range":
        head += f" Mean gap to {gap_label} |"
        sep += "---|"
    lines = [head, sep]
    for h in res["hs"]:
        row = (f"| {h} | {s[h]['win']} ({100*s[h]['win']/n:.1f}%) | {s[h]['strict']} "
               f"| {s[h]['beats_cp']} | {s[h]['sum']/n:.2f} |")
        if res["truth_kind"] == "opt":
            row += f" {s[h]['gapsum']/res['n_with_opt']:.2f} | {s[h]['exact']} ({100*s[h]['exact']/res['n_with_opt']:.1f}%) |"
        elif res["truth_kind"] == "range":
            row += f" {s[h]['gapsum']/res['n_with_opt']:.2f} |"
        lines.append(row)
    return "\n".join(lines)


def main():
    j30 = analyze("j30", "opt", opt=read_bounds(os.path.join(DATA, "j30opt.sm")))
    j60 = analyze("j60", "range",
                  lb=read_bounds(os.path.join(DATA, "j60lb.sm")),
                  ub=read_bounds(os.path.join(DATA, "j60hrs.sm")))
    j90 = analyze("j90", "none")

    out = []
    A = out.append
    A("# Root-State Lower Bound Comparison (first state only, no search)\n")

    # ---------- headline ----------
    A("## Headline: who gives the best root lower bound\n")
    A("| Set | Instances | Best heuristic (incl. ties) | Sole-winner leader | Notes |")
    A("|---|---|---|---|---|")
    for res, note in ((j30, "true optimum known for all 480"),
                      (j60, "truth = best-known range LB–UB (PSPLIB)"),
                      (j90, "no optimum column (as requested)")):
        s = res["stats"]
        lead = max(res["hs"], key=lambda h: s[h]["win"])
        sole = max(res["hs"], key=lambda h: s[h]["strict"])
        A(f"| {res['problem']} | {res['n']} | **{lead}** ({s[lead]['win']}/{res['n']}) "
          f"| {sole} ({s[sole]['strict']} sole wins) | {note} |")
    A("")

    for res, title in ((j30, "j30"), (j60, "j60"), (j90, "j90")):
        gap_label = "optimum" if res["truth_kind"] == "opt" else "UB"
        A(f"## {title} — aggregate summary ({res['n']} instances)\n")
        A(summary_block(res, gap_label))
        if res["truth_kind"] == "opt":
            A(f"\nBest-of-all-heuristics root bound equals the true optimum on "
              f"**{res['best_exact']}/{res['n_with_opt']}** instances; its mean gap to the "
              f"optimum is **{res['gap_best_sum']/res['n_with_opt']:.2f}** time units.\n")
        elif res["truth_kind"] == "range":
            A(f"\nBest root bound already **equals the best-known UB on "
              f"{res['best_exact']}/{res['n_with_opt']}** instances — those optima are proven "
              f"at the root with no search. Mean gap of the best root bound to the UB is "
              f"**{res['gap_best_sum']/res['n_with_opt']:.2f}** time units "
              f"(the true optimum lies inside LB–UB, so the real gap is smaller on open instances).\n")
        A("")

    # ---------- full tables ----------
    A("---\n")
    A("## Full per-instance tables\n")
    A("Values are the effective root lower bounds per heuristic. "
      "`Best` = tightest root LB; `Winner` = heuristic(s) achieving it.\n")

    def table_head(res, extra_cols):
        cols = ["Group", "Exam"] + res["hs"] + ["Best", "Winner"] + extra_cols
        A("| " + " | ".join(cols) + " |")
        A("|" + "---|" * len(cols))

    def eff_cells(r, res):
        return "".join(f" {r['eff'][h]} |" for h in res["hs"])

    # j30
    A("### j30 (with true makespan)\n")
    table_head(j30, ["True makespan", "Gap"])
    for r in j30["table"]:
        gap = "" if r["opt"] is None else r["opt"] - r["best"]
        A(f"| {r['g']} | {r['e']} |{eff_cells(r, j30)} **{r['best']}** "
          f"| {winners_str(r['winners'], j30['hs'])} | {r['opt']} | {gap} |")
    A("")

    # j60
    A("### j60 (with best-known range LB–UB)\n")
    A("Range shows the PSPLIB best-known bounds; a single number means LB = UB "
      "(verified optimum). `?` = PSPLIB publishes no LB for groups 42–48.\n")
    table_head(j60, ["Known range"])
    for r in j60["table"]:
        lbv, ubv = r["lb"], r["ub"]
        if lbv is not None and ubv is not None:
            rng = f"{ubv}" if lbv == ubv else f"{lbv}–{ubv}"
        elif ubv is not None:
            rng = f"?–{ubv}"
        else:
            rng = "?"
        A(f"| {r['g']} | {r['e']} |{eff_cells(r, j60)} **{r['best']}** "
          f"| {winners_str(r['winners'], j60['hs'])} | {rng} |")
    A("")

    # j90
    A("### j90\n")
    table_head(j90, [])
    for r in j90["table"]:
        A(f"| {r['g']} | {r['e']} |{eff_cells(r, j90)} **{r['best']}** "
          f"| {winners_str(r['winners'], j90['hs'])} |")
    A("")

    A("---\n")
    A("Raw component values (CP_dp, HRES, LBCS, LBCC, LBIP0, LBRC before the max/ceil) "
      "are in `data/root_lb/root_lb_{j30,j60,j90}.csv`; LBER root bounds (depth 3, "
      "lber-tt2-port branch) in `data/root_lb/root_lber_{j30,j60,j90}.csv`.\n")

    dest = os.path.join(HERE, "output", "ROOT_LB_REPORT.md")
    os.makedirs(os.path.dirname(dest), exist_ok=True)
    with open(dest, "w") as f:
        f.write("\n".join(out))
    print(f"wrote {dest} ({len(out)} lines)")


if __name__ == "__main__":
    main()
