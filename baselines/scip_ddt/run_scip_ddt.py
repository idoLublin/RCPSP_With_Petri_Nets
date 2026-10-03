#!/usr/bin/env python3
"""
SCIP + DDT (Disaggregated Precedence) RCPSP baseline.

Same model, same instance parsing, same time windows and the same independent schedule check as the Gurobi
baseline (baselines/gurobi_ddt/run_gurobi_ddt.py) -- those helpers are imported from it, so the two baselines
can never drift apart in how they read an instance or verify an answer. Only the solver layer differs.

WHY THIS EXISTS: the earlier SCIP numbers came through PuLP (baselines/mip_ddt/run_ddt_mip.py), where
    optimal = (status == pulp.LpStatusOptimal) or (sol_status == LpSolutionOptimal)
is True for a run that hit the time limit with a feasible solution in hand. Those runs report TIMEOUTS AS
PROVEN OPTIMA. Here `optimal` is SCIP's own status string being exactly "optimal", and the dual bound is
written to the CSV as provenLB so the claim can be checked against the makespan.

Env flags (parsed by value, all default off; every flag is written to the CSV header and each row):
  RCPSP_SCIP_NOPRESOLVE=1      presolving off        the MIP counterpart of the root stage CBS runs without
  RCPSP_SCIP_NOCUTS=1          separating off
  RCPSP_SCIP_NOHEUR=1          primal heuristics off
  RCPSP_SCIP_NOPROP=1          node propagation off  SCIP-only: the counterpart of RCPSP_CBS_TT
  RCPSP_SCIP_NOCONF=1          conflict analysis off SCIP-only: the counterpart of RCPSP_CBS_NG
  RCPSP_SCIP_BUILD_IN_BUDGET=1 model build time counts inside the budget (limit = budget - build)
  RCPSP_SCIP_SOFTMEM=<GB>      limits/memory (0 = none); an instance over it ends with status memlimit
  RCPSP_SCIP_TIMEOUT=<s>       per-instance budget (default 300)
  RCPSP_ONLY=<file>            only the instances listed as size,group,exam (one per line)
  RCPSP_RESULTS_DIR=<dir>      output directory (default <repo>/new_results)

Output: <results dir>/scip_<size>_ddt[-<flags>]_vNN.csv (or --out), one row per instance. Prints 'Done -> <csv>'.

Usage:
  python run_scip_ddt.py j30 [--data-dir DIR] [--out CSV] [--limit N]
"""
import time, csv, sys, os, math, argparse, types

try:
    from pyscipopt import Model, quicksum, SCIP_PARAMSETTING
except ImportError:
    sys.exit("pyscipopt not found. Install with: pip install pyscipopt")

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
BASELINES_DIR = os.path.dirname(SCRIPT_DIR)
REPO_DIR = os.path.dirname(BASELINES_DIR)

# Reuse the Gurobi baseline's parsing / windows / verification so the two cannot diverge. That module imports
# gurobipy at load time and exits if it is missing; the solver object is never touched by the helpers we want,
# so stub it when it is absent rather than duplicating 120 lines of parser here.
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
               "time_s", "build_s", "total_s", "nodes", "lpIters", "horizon", "sgsUB", "nVars", "nConstrs",
               "maxMemGB", "verified"]
# parameter values read back from the model after they are set (what SCIP actually used)
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


def solve_instance(sm_path, inst_name, size, group, exam):
    t0 = time.perf_counter()
    n, durations, rreq, rc, succs = parse_sm(sm_path)
    n_res = len(rc)
    est, lst, H = compute_time_windows(n, durations, succs, rreq, rc)

    m = Model("rcpsp_ddt")
    # x[i, t] = 1 if activity i (1-indexed) starts at time t
    x = {}
    for i in range(1, n + 1):
        for t in range(est[i - 1], lst[i - 1] + 1):
            x[i, t] = m.addVar(vtype="B", name=f"x_{i}_{t}")

    # Each activity scheduled exactly once
    for i in range(1, n + 1):
        m.addCons(quicksum(x[i, t] for t in range(est[i - 1], lst[i - 1] + 1)) == 1, name=f"once_{i}")

    # DDT precedence: for (i->j), for all tau: sum_{t<=tau} x[j][t] <= sum_{t<=tau-d_i} x[i][t]
    for pred, succ in succs:
        d = durations[pred - 1]
        for tau in range(est[succ - 1], lst[succ - 1] + d + 1):
            lhs = [x[succ, t] for t in range(est[succ - 1], min(tau, lst[succ - 1]) + 1)]
            rhs = [x[pred, t] for t in range(est[pred - 1], min(tau - d, lst[pred - 1]) + 1)
                   if tau - d >= est[pred - 1]]
            if lhs:
                m.addCons(quicksum(lhs) <= quicksum(rhs), name=f"ddt_{pred}_{succ}_{tau}")

    # Resource capacity at each time point
    for t in range(H):
        for r in range(n_res):
            terms = []
            for i in range(1, n + 1):
                if rreq[i - 1][r] == 0:
                    continue
                d = durations[i - 1]
                for q in range(max(est[i - 1], t - d + 1), min(t, lst[i - 1]) + 1):
                    terms.append(rreq[i - 1][r] * x[i, q])
            if terms:
                m.addCons(quicksum(terms) <= rc[r], name=f"res_{r}_{t}")

    # Objective: minimise the start time of the sink (duration 0)
    m.setObjective(quicksum(t * x[n, t] for t in range(est[n - 1], lst[n - 1] + 1)), "minimize")

    build_s = time.perf_counter() - t0
    limit = TIMEOUT_S - build_s if BUILD_IN_BUDGET else TIMEOUT_S
    row = {"instance": inst_name, "size": size, "group": group, "exam": exam, "horizon": H, "sgsUB": H,
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
        start = {}
        for (i, t), var in x.items():
            if m.getSolVal(sol, var) > 0.5:
                if i in start:
                    start = None
                    break
                start[i] = t
        verified = bool(start) and verify_schedule(n, durations, rreq, rc, succs, start) \
            and start[n] == makespan

    row.update({"status": status, "solved": solved, "optimal": status == "optimal",
                "makespan": makespan, "provenLB": proven_lb, "gap": gap,
                "time_s": round(solve_s, 3), "total_s": round(build_s + solve_s, 3),
                "nodes": int(m.getNTotalNodes()), "lpIters": int(m.getNLPIterations()),
                "maxMemGB": -1, "verified": verified})
    m.freeProb()
    return row


def flag_info():
    parts = ["ddt"]
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
    ap = argparse.ArgumentParser(description="SCIP DDT RCPSP baseline")
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

    print(f"SCIP DDT baseline: {args.size}, {len(rows_wanted)} instances, {TIMEOUT_S}s each, flags={flag_info()}")
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
