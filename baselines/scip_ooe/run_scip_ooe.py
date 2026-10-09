#!/usr/bin/env python3
"""
SCIP + OOE (On/Off Event-based) RCPSP baseline.

The event-based MILP of Kone, Artigues, Lopez & Mongeau (2011), "Event-based MILP models for resource-constrained
project scheduling problems", Computers & Operations Research 38(1):3-13 - their On/Off Event-based model, the
stronger of their two event-based models. It has no time index: z[i,e] = 1 if activity i starts at event e or is
still in process right after it, t[e] = date of event e, one event per (non-dummy) activity.

    min Cmax
    sum_e z[i,e] >= 1                                                    every activity is processed
    Cmax >= t[e] + (z[i,e] - z[i,e-1]) p_i                               an activity that starts at e ends by Cmax
    t[0] = 0,  t[e+1] >= t[e]
    t[f] >= t[e] + ((z[i,e] - z[i,e-1]) - (z[i,f] - z[i,f-1]) - 1) p_i   starts at e, ends at f  =>  p_i apart   (e < f)
    sum_{e'<e}  z[i,e'] <= e     (1 - (z[i,e] - z[i,e-1]))               nothing before the start      (e > 0)
    sum_{e'>=e} z[i,e'] <= (n-e) (1 + (z[i,e] - z[i,e-1]))               nothing after the end         (e > 0)
    z[i,e] + sum_{e'<=e} z[j,e'] <= 1 + (1 - z[i,e]) e                   precedence i -> j
    sum_i b[i,k] z[i,e] <= B_k                                           capacity at every event
with z[i,-1] = 0. Dummy source / sink are left out (zero duration, no demand); n = number of real activities.

Same instance parsing, same independent schedule check and the same solver layer as the time-indexed SCIP baseline
(baselines/scip_ddt/run_scip_ddt.py): helpers are imported from baselines/gurobi_ddt/run_gurobi_ddt.py. The serial
SGS makespan that the time-indexed model uses as its horizon is given to this model too, as an upper bound on Cmax
and on every event date, so both formulations start from the same information.

Checking a solution: event dates are continuous, so the schedule that is verified is rebuilt from the z pattern
alone - every event at the earliest date the pattern allows (integral) - and must be feasible with a makespan not
above the reported objective.

Env flags (parsed by value, all default off; every flag is written to the CSV header and each row):
  RCPSP_SCIP_NOPRESOLVE / NOCUTS / NOHEUR / NOPROP / NOCONF = 1     as in run_scip_ddt.py
  RCPSP_SCIP_BUILD_IN_BUDGET=1 model build time counts inside the budget (limit = budget - build)
  RCPSP_SCIP_SOFTMEM=<GB>      limits/memory (0 = none)
  RCPSP_SCIP_TIMEOUT=<s>       per-instance budget (default 300)
  RCPSP_ONLY=<file>            only the instances listed as size,group,exam (one per line)
  RCPSP_RESULTS_DIR=<dir>      output directory (default <repo>/new_results)

Output: <results dir>/scip_<size>_ooe[-<flags>]_vNN.csv (or --out), one row per instance. Prints 'Done -> <csv>'.

Usage:
  python run_scip_ooe.py j30 [--data-dir DIR] [--out CSV] [--limit N]
"""
import time, csv, sys, os, math, argparse, types

try:
    from pyscipopt import Model, quicksum, SCIP_PARAMSETTING
except ImportError:
    sys.exit("pyscipopt not found. Install with: pip install pyscipopt")

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
BASELINES_DIR = os.path.dirname(SCRIPT_DIR)
REPO_DIR = os.path.dirname(BASELINES_DIR)

# Reuse the Gurobi baseline's parsing / SGS bound / verification, exactly as run_scip_ddt.py does.
sys.path.insert(0, os.path.join(BASELINES_DIR, "gurobi_ddt"))
try:
    import gurobipy  # noqa: F401
except ImportError:
    stub = types.ModuleType("gurobipy")
    stub.GRB = types.SimpleNamespace(BINARY=0, MINIMIZE=1, OPTIMAL=2, TIME_LIMIT=9, MEM_LIMIT=17,
                                     INFEASIBLE=3, INF_OR_UNBD=4, INTERRUPTED=11, SUBOPTIMAL=13, NUMERIC=12)
    stub.Model = object
    stub.GurobiError = Exception
    stub.quicksum = sum
    stub.gurobi = types.SimpleNamespace(version=lambda: (0, 0, 0))
    sys.modules["gurobipy"] = stub
    sys.modules["gurobipy.GRB"] = stub.GRB
import run_gurobi_ddt as G  # noqa: E402

parse_sm = G.parse_sm
compute_time_windows = G.compute_time_windows
verify_schedule = G.verify_schedule
collect_instances = G.collect_instances
load_only = G.load_only
env_num = G.env_num
DATA_DIRS = G.DATA_DIRS

NOPRESOLVE = env_num("RCPSP_SCIP_NOPRESOLVE", 0) != 0
NOCUTS = env_num("RCPSP_SCIP_NOCUTS", 0) != 0
NOHEUR = env_num("RCPSP_SCIP_NOHEUR", 0) != 0
NOPROP = env_num("RCPSP_SCIP_NOPROP", 0) != 0
NOCONF = env_num("RCPSP_SCIP_NOCONF", 0) != 0
BUILD_IN_BUDGET = env_num("RCPSP_SCIP_BUILD_IN_BUDGET", 0) != 0
SOFTMEM_GB = env_num("RCPSP_SCIP_SOFTMEM", 0.0, float)
TIMEOUT_S = env_num("RCPSP_SCIP_TIMEOUT", 300.0, float)
ONLY_FILE = os.environ.get("RCPSP_ONLY", "").strip()
THREADS = 1

FLAGS = {
    "formulation": "ooe",
    "scipNoPresolve": int(NOPRESOLVE),
    "scipNoCuts": int(NOCUTS),
    "scipNoHeur": int(NOHEUR),
    "scipNoProp": int(NOPROP),
    "scipNoConf": int(NOCONF),
    "scipBuildInBudget": int(BUILD_IN_BUDGET),
    "scipSoftMemGB": SOFTMEM_GB,
    "timeoutS": TIMEOUT_S,
    "threads": THREADS,
    "onlyFile": os.path.basename(ONLY_FILE),
}
RESULT_COLS = ["instance", "size", "group", "exam", "status", "solved", "optimal", "makespan", "provenLB", "gap",
               "time_s", "build_s", "total_s", "nodes", "lpIters", "nEvents", "sgsUB", "nVars", "nConstrs",
               "maxMemGB", "verified"]
PARAM_COLS = ["pPresolveRounds", "pSepaRounds", "pSepaRoundsRoot", "pPropRounds", "pPropRoundsRoot",
              "pConflict", "pThreads", "pTimeLimit", "pMemLimit", "scipVersion"]
FIELDNAMES = RESULT_COLS + list(FLAGS) + PARAM_COLS

try:
    SCIP_VERSION = str(Model().version())
except Exception:
    SCIP_VERSION = "unknown"


def set_params(m, limit):
    m.hideOutput()
    m.setParam("limits/time", max(0.0, limit))
    if SOFTMEM_GB > 0:
        m.setParam("limits/memory", SOFTMEM_GB * 1024.0)   # SCIP takes MB
    m.setParam("lp/threads", THREADS)
    m.setParam("parallel/maxnthreads", THREADS)
    m.setParam("randomization/randomseedshift", 0)
    if NOPRESOLVE:
        m.setPresolve(SCIP_PARAMSETTING.OFF)
    if NOCUTS:
        m.setSeparating(SCIP_PARAMSETTING.OFF)
    if NOHEUR:
        m.setHeuristics(SCIP_PARAMSETTING.OFF)
    if NOPROP:
        m.setParam("propagating/maxrounds", 0)
        m.setParam("propagating/maxroundsroot", 0)
    if NOCONF:
        m.setParam("conflict/enable", False)


def param_readback(m):
    def g(name, default=-1):
        try:
            return m.getParam(name)
        except Exception:
            return default
    return {"pPresolveRounds": g("presolving/maxrounds"),
            "pSepaRounds": g("separating/maxrounds"),
            "pSepaRoundsRoot": g("separating/maxroundsroot"),
            "pPropRounds": g("propagating/maxrounds"),
            "pPropRoundsRoot": g("propagating/maxroundsroot"),
            "pConflict": g("conflict/enable"),
            "pThreads": g("lp/threads"),
            "pTimeLimit": g("limits/time"),
            "pMemLimit": g("limits/memory"),
            "scipVersion": SCIP_VERSION}


def schedule_from_pattern(acts, E, durations, on):
    """Start times from the on/off pattern alone: every event at the earliest date the pattern allows.
    on[i] = sorted list of events at which activity i is on. Returns {activity: start} or None if the pattern is
    not one contiguous block per activity."""
    first, last = {}, {}
    for i in acts:
        ev = on[i]
        if not ev or ev != list(range(ev[0], ev[-1] + 1)):
            return None
        first[i], last[i] = ev[0], ev[-1]
    t = [0] * len(E)
    for f in E[1:]:
        t[f] = t[f - 1]
        for i in acts:                       # i ends at event f: it was on at f-1 and is off at f
            if last[i] == f - 1:
                t[f] = max(t[f], t[first[i]] + durations[i - 1])
    return {i: t[first[i]] for i in acts}


def solve_instance(sm_path, inst_name, size, group, exam):
    t0 = time.perf_counter()
    n, durations, rreq, rc, succs = parse_sm(sm_path)
    n_res = len(rc)
    _, _, ub = compute_time_windows(n, durations, succs, rreq, rc)      # serial SGS makespan: Cmax <= ub

    acts = list(range(2, n))                  # real activities; 1 and n are the dummy source / sink
    nE = len(acts)
    E = list(range(nE))
    prec = [(i, j) for i, j in succs if i != 1 and j != n]

    m = Model("rcpsp_ooe")
    z = {(i, e): m.addVar(vtype="B", name=f"z_{i}_{e}") for i in acts for e in E}
    t = {e: m.addVar(vtype="C", lb=0.0, ub=float(ub), name=f"t_{e}") for e in E}
    cmax = m.addVar(vtype="C", lb=0.0, ub=float(ub), name="cmax")

    def dz(i, e):                             # z[i,e] - z[i,e-1], with z[i,-1] = 0
        return z[i, e] - z[i, e - 1] if e > 0 else z[i, e]

    for i in acts:
        p = durations[i - 1]
        m.addCons(quicksum(z[i, e] for e in E) >= 1, name=f"proc_{i}")
        for e in E:
            m.addCons(cmax >= t[e] + dz(i, e) * p, name=f"cmax_{i}_{e}")
            for f in E[e + 1:]:
                m.addCons(t[f] >= t[e] + (dz(i, e) - dz(i, f) - 1) * p, name=f"dur_{i}_{e}_{f}")
            if e > 0:
                m.addCons(quicksum(z[i, e2] for e2 in E[:e]) <= e * (1 - dz(i, e)), name=f"before_{i}_{e}")
                m.addCons(quicksum(z[i, e2] for e2 in E[e:]) <= (nE - e) * (1 + dz(i, e)), name=f"after_{i}_{e}")
    m.addCons(t[0] == 0, name="t0")
    for e in E[:-1]:
        m.addCons(t[e + 1] >= t[e], name=f"order_{e}")
    for i, j in prec:
        for e in E:
            m.addCons(z[i, e] + quicksum(z[j, e2] for e2 in E[:e + 1]) <= 1 + (1 - z[i, e]) * e,
                      name=f"prec_{i}_{j}_{e}")
    for e in E:
        for r in range(n_res):
            terms = [rreq[i - 1][r] * z[i, e] for i in acts if rreq[i - 1][r] > 0]
            if terms:
                m.addCons(quicksum(terms) <= rc[r], name=f"res_{r}_{e}")
    m.setObjective(cmax, "minimize")

    build_s = time.perf_counter() - t0
    limit = TIMEOUT_S - build_s if BUILD_IN_BUDGET else TIMEOUT_S
    row = {"instance": inst_name, "size": size, "group": group, "exam": exam, "nEvents": nE, "sgsUB": ub,
           "nVars": m.getNVars(), "nConstrs": m.getNConss(), "build_s": round(build_s, 3)}
    row.update(FLAGS)
    if limit <= 0:
        row.update({"status": "BUILD_TIMEOUT", "solved": False, "optimal": False, "makespan": -1,
                    "provenLB": -1, "gap": "", "time_s": 0.0, "total_s": round(build_s, 3), "nodes": 0,
                    "lpIters": 0, "maxMemGB": -1, "verified": ""})
        row.update({c: -1 for c in PARAM_COLS})
        return row

    set_params(m, limit)
    row.update(param_readback(m))

    t1 = time.perf_counter()
    m.optimize()
    solve_s = time.perf_counter() - t1

    status = m.getStatus()                     # 'optimal' | 'timelimit' | 'memlimit' | 'infeasible' | ...
    solved = m.getNSols() > 0
    makespan = int(round(m.getObjVal())) if solved else -1
    try:
        proven_lb = int(math.ceil(m.getDualbound() - 1e-6))
    except Exception:
        proven_lb = -1
    try:
        gap = round(m.getGap(), 6) if solved else ""
    except Exception:
        gap = ""

    verified = ""
    if solved:
        sol = m.getBestSol()
        on = {i: [e for e in E if m.getSolVal(sol, z[i, e]) > 0.5] for i in acts}
        s = schedule_from_pattern(acts, E, durations, on)
        verified = False
        if s is not None:
            start = dict(s)
            start[1] = 0
            start[n] = max(start[i] + durations[i - 1] for i in acts)
            # the rebuilt schedule must be feasible and no longer than the objective; when SCIP proved
            # optimality it must be exactly as long
            verified = verify_schedule(n, durations, rreq, rc, succs, start) and start[n] <= makespan \
                and (status != "optimal" or start[n] == makespan)

    row.update({"status": status, "solved": solved, "optimal": status == "optimal",
                "makespan": makespan, "provenLB": proven_lb, "gap": gap,
                "time_s": round(solve_s, 3), "total_s": round(build_s + solve_s, 3),
                "nodes": int(m.getNTotalNodes()), "lpIters": int(m.getNLPIterations()),
                "maxMemGB": -1, "verified": verified})
    m.freeProb()
    return row


def flag_info():
    parts = ["ooe"]
    if NOPRESOLVE: parts.append("nopresolve")
    if NOCUTS:     parts.append("nocuts")
    if NOHEUR:     parts.append("noheur")
    if NOPROP:     parts.append("noprop")
    if NOCONF:     parts.append("noconf")
    if BUILD_IN_BUDGET: parts.append("budget")
    if ONLY_FILE:  parts.append("only")
    return "-".join(parts)


def open_next_csv(out_dir, size, info):
    os.makedirs(out_dir, exist_ok=True)
    for v in range(1, 1000):
        path = os.path.join(out_dir, f"scip_{size}_{info}_v{v:02d}.csv")
        try:
            return path, open(path, "x", newline="", encoding="utf-8")
        except FileExistsError:
            continue
    sys.exit(f"no free file name for scip_{size}_{info} in {out_dir}")


def main():
    ap = argparse.ArgumentParser(description="SCIP OOE (event-based) RCPSP baseline")
    ap.add_argument("size", choices=["j30", "j60", "j90"])
    ap.add_argument("--data-dir", default=None)
    ap.add_argument("--out", default=None)
    ap.add_argument("--limit", type=int, default=None, help="only the first N instances (testing)")
    args = ap.parse_args()

    instances = collect_instances(args.size, args.data_dir)
    if not instances:
        sys.exit(f"no instances found for {args.size} in {args.data_dir or DATA_DIRS[args.size]}")
    keep = load_only(ONLY_FILE) if ONLY_FILE else None

    rows_wanted = []
    for path, name in instances:
        _, g, e = G._natural_key(path, args.size)   # (0, group, exam)
        if keep is not None and (args.size, g, e) not in keep:
            continue
        rows_wanted.append((path, name, g, e))
    if args.limit:
        rows_wanted = rows_wanted[:args.limit]

    out_dir = os.environ.get("RCPSP_RESULTS_DIR", "").strip() or os.path.join(REPO_DIR, "new_results")
    if args.out:
        out_path, fh = args.out, open(args.out, "w", newline="", encoding="utf-8")
    else:
        out_path, fh = open_next_csv(out_dir, args.size, flag_info())

    print(f"SCIP OOE baseline: {args.size}, {len(rows_wanted)} instances, {TIMEOUT_S}s each, flags={flag_info()}")
    with fh:
        writer = csv.DictWriter(fh, fieldnames=FIELDNAMES, lineterminator="\n")
        writer.writeheader()
        for i, (path, name, g, e) in enumerate(rows_wanted, 1):
            try:
                row = solve_instance(path, name, args.size, g, e)
            except Exception as ex:
                row = {c: "" for c in FIELDNAMES}
                row.update({"instance": name, "size": args.size, "group": g, "exam": e,
                            "status": f"ERROR {type(ex).__name__}", "solved": False, "optimal": False,
                            "makespan": -1})
                row.update(FLAGS)
            writer.writerow({c: row.get(c, "") for c in FIELDNAMES})
            fh.flush()
            mk = row.get("makespan", -1)
            print(f"  [{i:3d}/{len(rows_wanted)}] {name:15s}  {str(row.get('status')):12s} makespan={mk} "
                  f"lb={row.get('provenLB', '')} {row.get('total_s', 0.0)}s verified={row.get('verified', '')}",
                  flush=True)
            if row.get("solved") and row.get("verified") is not True:
                print(f"      !! schedule failed verification on {name}", flush=True)
    print(f"Done -> {out_path}")


if __name__ == "__main__":
    main()
