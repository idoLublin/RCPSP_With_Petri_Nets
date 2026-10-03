#!/usr/bin/env python3
"""
CBC + DDT (Disaggregated Precedence) RCPSP baseline.

Same model, same instance parsing, same time windows and the same independent schedule check as the Gurobi and
SCIP baselines -- those helpers are imported from baselines/gurobi_ddt/run_gurobi_ddt.py, so the three MIP
baselines cannot drift apart in how they read an instance or verify an answer. Only the solver layer differs.

WHY THIS EXISTS: the earlier CBC numbers came through PuLP (baselines/mip_ddt/run_ddt_mip.py), where
    optimal = (status == pulp.LpStatusOptimal) or (sol_status == LpSolutionOptimal)
and PuLP exposes NO DUAL BOUND at all, so an "optimal" row could never be checked. Here the verdict and the
bound are read from CBC's own log:
    Result - Optimal solution found      -> optimal
    Result - Stopped on time limit       -> timelimit
    Lower bound:  <x>                    -> provenLB   (absent when CBC closed the gap: bound = objective)
PuLP is still used to build the model and read the solution back; only the verdict comes from the log.

Env flags (parsed by value, all default off; every flag is written to the CSV header and each row):
  RCPSP_CBC_NOPRESOLVE=1      presolve off     the MIP counterpart of the root stage CBS runs without
  RCPSP_CBC_NOCUTS=1          cut generation off
  RCPSP_CBC_NOHEUR=1          primal heuristics off
  RCPSP_CBC_BUILD_IN_BUDGET=1 model build time counts inside the budget (limit = budget - build)
  RCPSP_CBC_TIMEOUT=<s>       per-instance budget (default 300)
  RCPSP_ONLY=<file>           only the instances listed as size,group,exam (one per line)
  RCPSP_RESULTS_DIR=<dir>     output directory (default <repo>/new_results)

CBC exposes no switch for node propagation or conflict analysis, so it has no counterpart to the SCIP
noprop / noconf arms -- use the SCIP baseline for those.

Output: <results dir>/cbc_<size>_ddt[-<flags>]_vNN.csv (or --out). Prints 'Done -> <csv>'.

Usage:
  python run_cbc_ddt.py j30 [--data-dir DIR] [--out CSV] [--limit N]
"""
import time, csv, sys, os, re, math, argparse, types, tempfile, shutil

try:
    import pulp
except ImportError:
    sys.exit("pulp not found. Install with: pip install pulp")

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
BASELINES_DIR = os.path.dirname(SCRIPT_DIR)
REPO_DIR = os.path.dirname(BASELINES_DIR)

# Reuse the Gurobi baseline's parsing / windows / verification (see run_scip_ddt.py for the same shim).
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
import run_gurobi_ddt as G  # noqa: E402

parse_sm = G.parse_sm
compute_time_windows = G.compute_time_windows
verify_schedule = G.verify_schedule
collect_instances = G.collect_instances
load_only = G.load_only
env_num = G.env_num
DATA_DIRS = G.DATA_DIRS

NOPRESOLVE = env_num("RCPSP_CBC_NOPRESOLVE", 0) != 0
NOCUTS = env_num("RCPSP_CBC_NOCUTS", 0) != 0
NOHEUR = env_num("RCPSP_CBC_NOHEUR", 0) != 0
BUILD_IN_BUDGET = env_num("RCPSP_CBC_BUILD_IN_BUDGET", 0) != 0
TIMEOUT_S = env_num("RCPSP_CBC_TIMEOUT", 300.0, float)
ONLY_FILE = os.environ.get("RCPSP_ONLY", "").strip()
THREADS = 1

FLAGS = {
    "cbcNoPresolve": int(NOPRESOLVE),
    "cbcNoCuts": int(NOCUTS),
    "cbcNoHeur": int(NOHEUR),
    "cbcBuildInBudget": int(BUILD_IN_BUDGET),
    "timeoutS": TIMEOUT_S,
    "threads": THREADS,
    "onlyFile": os.path.basename(ONLY_FILE),
}
RESULT_COLS = ["instance", "size", "group", "exam", "status", "solved", "optimal", "makespan", "provenLB", "gap",
               "time_s", "build_s", "total_s", "nodes", "lpIters", "horizon", "sgsUB", "nVars", "nConstrs",
               "maxMemGB", "verified"]
PARAM_COLS = ["pOptions", "pThreads", "pTimeLimit", "pTimeMode", "cbcVersion"]
FIELDNAMES = RESULT_COLS + list(FLAGS) + PARAM_COLS

# CBC's own verdict lines, in the order it prints them.
STATUS_PATTERNS = [
    ("optimal",    re.compile(r"^Result - Optimal solution found", re.M)),
    ("timelimit",  re.compile(r"^Result - Stopped on time limit", re.M)),
    ("infeasible", re.compile(r"^Result - Problem proven infeasible", re.M)),
    ("unbounded",  re.compile(r"^Result - Problem unbounded", re.M)),
    ("nodelimit",  re.compile(r"^Result - Stopped on maximum nodes", re.M)),
    ("interrupt",  re.compile(r"^Result - Stopped on ctrl-c", re.M)),
    ("solutionlimit", re.compile(r"^Result - Stopped on maximum solutions", re.M)),
]
RE_OBJ = re.compile(r"^Objective value:\s+([-\d.eE+]+)", re.M)
RE_LB = re.compile(r"^Lower bound:\s+([-\d.eE+]+)", re.M)
RE_GAP = re.compile(r"^Gap:\s+([-\d.eE+]+)", re.M)
RE_NODES = re.compile(r"^Enumerated nodes:\s+(\d+)", re.M)
RE_ITERS = re.compile(r"^Total iterations:\s+(\d+)", re.M)


def read_cbc_log(path):
    """CBC's own verdict, bound and counters. Returns a dict; status 'unknown' if the log says nothing."""
    try:
        with open(path, encoding="utf-8", errors="replace") as f:
            txt = f.read()
    except OSError:
        return {"status": "NOLOG", "objective": None, "lb": None, "gap": None, "nodes": -1, "iters": -1}
    status = "unknown"
    for name, pat in STATUS_PATTERNS:
        if pat.search(txt):
            status = name
            break

    def num(pat, cast=float):
        m = pat.search(txt)
        return cast(m.group(1)) if m else None
    return {"status": status, "objective": num(RE_OBJ), "lb": num(RE_LB), "gap": num(RE_GAP),
            "nodes": num(RE_NODES, int) if RE_NODES.search(txt) else -1,
            "iters": num(RE_ITERS, int) if RE_ITERS.search(txt) else -1}


def cbc_options():
    """Extra CLI arguments handed to the cbc binary, in the order CBC expects (setting before -solve)."""
    opts = []
    if NOPRESOLVE:
        opts += ["presolve", "off"]
    if NOCUTS:
        opts += ["cuts", "off"]
    if NOHEUR:
        opts += ["heuristics", "off"]
    return opts


def solve_instance(sm_path, inst_name, size, group, exam, log_dir):
    t0 = time.perf_counter()
    n, durations, rreq, rc, succs = parse_sm(sm_path)
    n_res = len(rc)
    est, lst, H = compute_time_windows(n, durations, succs, rreq, rc)

    prob = pulp.LpProblem(f"rcpsp_{inst_name}", pulp.LpMinimize)
    x = {}
    for i in range(1, n + 1):
        for t in range(est[i - 1], lst[i - 1] + 1):
            x[i, t] = pulp.LpVariable(f"x_{i}_{t}", cat="Binary")

    for i in range(1, n + 1):
        prob += pulp.lpSum(x[i, t] for t in range(est[i - 1], lst[i - 1] + 1)) == 1

    for pred, succ in succs:
        d = durations[pred - 1]
        for tau in range(est[succ - 1], lst[succ - 1] + d + 1):
            lhs = [x[succ, t] for t in range(est[succ - 1], min(tau, lst[succ - 1]) + 1)]
            rhs = [x[pred, t] for t in range(est[pred - 1], min(tau - d, lst[pred - 1]) + 1)
                   if tau - d >= est[pred - 1]]
            if lhs:
                prob += pulp.lpSum(lhs) <= pulp.lpSum(rhs)

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
                prob += pulp.lpSum(terms) <= rc[r]

    prob += pulp.lpSum(t * x[n, t] for t in range(est[n - 1], lst[n - 1] + 1))

    build_s = time.perf_counter() - t0
    limit = TIMEOUT_S - build_s if BUILD_IN_BUDGET else TIMEOUT_S
    row = {"instance": inst_name, "size": size, "group": group, "exam": exam, "horizon": H, "sgsUB": H,
           "nVars": len(x), "nConstrs": len(prob.constraints), "build_s": round(build_s, 3),
           "maxMemGB": -1}
    row.update(FLAGS)
    opts = cbc_options()
    row.update({"pOptions": " ".join(opts), "pThreads": THREADS, "pTimeLimit": round(max(0.0, limit), 3), "pTimeMode": "elapsed",
                "cbcVersion": CBC_VERSION})
    if limit <= 0:
        row.update({"status": "BUILD_TIMEOUT", "solved": False, "optimal": False, "makespan": -1,
                    "provenLB": -1, "gap": "", "time_s": 0.0, "total_s": round(build_s, 3), "nodes": 0,
                    "lpIters": 0, "verified": ""})
        return row

    log_path = os.path.join(log_dir, f"cbc_{inst_name}.log")
    # timeMode="elapsed": CBC's -sec limit is CPU seconds by default. That gives CBC more than the budget in
    # wall clock whenever the box is loaded (unfair against every other solver, which runs on wall clock) and
    # never fires at all if the process stalls. Every baseline here is a 300 s WALL-CLOCK budget.
    solver = CBC_CLASS(timeLimit=limit, threads=THREADS, msg=False, timeMode="elapsed",
                       logPath=log_path, options=opts)
    t1 = time.perf_counter()
    try:
        prob.solve(solver)
    except Exception as ex:
        row.update({"status": f"ERROR {type(ex).__name__}", "solved": False, "optimal": False, "makespan": -1,
                    "provenLB": -1, "gap": "", "time_s": round(time.perf_counter() - t1, 3),
                    "total_s": round(time.perf_counter() - t0, 3), "nodes": -1, "lpIters": -1, "verified": ""})
        return row
    solve_s = time.perf_counter() - t1

    cbc = read_cbc_log(log_path)
    status = cbc["status"]
    optimal = status == "optimal"

    # An incumbent exists only when CBC printed an objective value AND the variables came back integral.
    start = {}
    integral = True
    for (i, t), var in x.items():
        v = var.value()
        if v is not None and v > 0.5:
            if i in start:
                integral = False
                break
            start[i] = t
    solved = cbc["objective"] is not None and integral and len(start) == n

    makespan = int(round(cbc["objective"])) if solved else -1
    if optimal and solved:
        proven_lb = makespan                       # CBC closed the gap; it prints no Lower bound line
    elif cbc["lb"] is not None:
        proven_lb = int(math.ceil(cbc["lb"] - 1e-6))
    else:
        proven_lb = -1
    gap = round(cbc["gap"], 6) if cbc["gap"] is not None else ("" if not solved else 0.0)

    verified = ""
    if solved:
        verified = verify_schedule(n, durations, rreq, rc, succs, start) and start[n] == makespan

    row.update({"status": status, "solved": solved, "optimal": optimal, "makespan": makespan,
                "provenLB": proven_lb, "gap": gap, "time_s": round(solve_s, 3),
                "total_s": round(build_s + solve_s, 3), "nodes": cbc["nodes"], "lpIters": cbc["iters"],
                "verified": verified})
    try:
        os.remove(log_path)
    except OSError:
        pass
    return row


def flag_info():
    parts = ["ddt"]
    if NOPRESOLVE: parts.append("nopresolve")
    if NOCUTS:     parts.append("nocuts")
    if NOHEUR:     parts.append("noheur")
    if BUILD_IN_BUDGET: parts.append("budget")
    if ONLY_FILE:  parts.append("only")
    return "-".join(parts)


def open_next_csv(out_dir, size, info):
    os.makedirs(out_dir, exist_ok=True)
    for v in range(1, 1000):
        path = os.path.join(out_dir, f"cbc_{size}_{info}_v{v:02d}.csv")
        try:
            return path, open(path, "x", newline="", encoding="utf-8")
        except FileExistsError:
            continue
    sys.exit(f"no free file name for cbc_{size}_{info} in {out_dir}")


def cbc_solver_class():
    """PuLP <= 3 ships a CBC binary behind PULP_CBC_CMD; PuLP 4 dropped it and COIN_CMD calls the `cbc` found
    on PATH. Take the bundled one when it exists, else COIN_CMD with a real binary; refuse anything else."""
    cls = getattr(pulp, "PULP_CBC_CMD", None)
    if cls is not None:
        try:
            if os.path.exists(cls().path):
                return cls
        except Exception:
            pass
    cls = getattr(pulp, "COIN_CMD", None)
    if cls is not None:
        path = shutil.which(cls().path or "cbc")
        if path and not path.startswith("/mnt/"):     # a Windows shim seen through WSL interop is not a binary
            return cls
    return None


CBC_CLASS = cbc_solver_class()
try:
    CBC_VERSION = f"{CBC_CLASS.__name__}:{os.path.basename(CBC_CLASS().path)}" if CBC_CLASS else "none"
except Exception:
    CBC_VERSION = "unknown"


def main():
    ap = argparse.ArgumentParser(description="CBC DDT RCPSP baseline")
    ap.add_argument("size", choices=["j30", "j60", "j90"])
    ap.add_argument("--data-dir", default=None)
    ap.add_argument("--out", default=None)
    ap.add_argument("--limit", type=int, default=None, help="only the first N instances (testing)")
    args = ap.parse_args()

    if CBC_CLASS is None:
        sys.exit("no usable CBC: neither PuLP's bundled PULP_CBC_CMD nor a `cbc` binary on PATH for COIN_CMD")
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

    log_dir = tempfile.mkdtemp(prefix="cbclogs_")
    print(f"CBC DDT baseline: {args.size}, {len(rows_wanted)} instances, {TIMEOUT_S}s each, flags={flag_info()}")
    print("effective flags: " + " ".join(f"{k}={v}" for k, v in FLAGS.items()))
    try:
        with fh:
            writer = csv.DictWriter(fh, fieldnames=FIELDNAMES, lineterminator="\n")
            writer.writeheader()
            for i, (path, name, g, e) in enumerate(rows_wanted, 1):
                try:
                    row = solve_instance(path, name, args.size, g, e, log_dir)
                except Exception as ex:
                    row = {c: "" for c in FIELDNAMES}
                    row.update({"instance": name, "size": args.size, "group": g, "exam": e,
                                "status": f"ERROR {type(ex).__name__}", "solved": False, "optimal": False,
                                "makespan": -1})
                    row.update(FLAGS)
                writer.writerow({c: row.get(c, "") for c in FIELDNAMES})
                fh.flush()
                print(f"  [{i:3d}/{len(rows_wanted)}] {name:15s}  {str(row.get('status')):12s} "
                      f"makespan={row.get('makespan', -1)} lb={row.get('provenLB', '')} "
                      f"{row.get('total_s', 0.0)}s verified={row.get('verified', '')}", flush=True)
                if row.get("solved") and row.get("verified") is not True:
                    print(f"      !! schedule failed verification on {name}", flush=True)
    finally:
        shutil.rmtree(log_dir, ignore_errors=True)
    print(f"Done -> {out_path}")


if __name__ == "__main__":
    main()
