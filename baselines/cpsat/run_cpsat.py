#!/usr/bin/env python3
"""
OR-Tools CP-SAT RCPSP baseline with COMPONENT SWITCHES.

Purpose: find out WHICH part of CP-SAT makes it strong on the low-resource-strength instances our searches do not
prove - its LP relaxation, the cumulative propagators, clause learning / restarts, the learned branching, or the
direction of the proof (from an incumbent down vs. from a bound up) - so we know what is worth mimicking.

Model (the standard CP-SAT one): one start variable per activity in [0, sum of durations], a fixed-size interval
per activity, one cumulative constraint per resource, precedence as start_j >= start_i + d_i, minimise the sink.
Instance parsing and the independent schedule check are imported from baselines/gurobi_ddt/run_gurobi_ddt.py,
so every baseline reads and verifies instances the same way.

Env flags (parsed by value; all default to CP-SAT's own default; every flag is in the CSV header and each row):
  RCPSP_CPSAT_WORKERS=<n>      workers (default 1 = single core)
  RCPSP_CPSAT_TIMEOUT=<s>      per-instance budget (default 300), model build inside the budget
  RCPSP_CPSAT_NOPRESOLVE=1     presolve off
  RCPSP_CPSAT_LIN=<0|1|2>      linearization level; 0 = NO LP relaxation at all (default: CP-SAT's, 1)
  RCPSP_CPSAT_NODISJ=1         no pairwise disjunctive reasoning inside cumulative
  RCPSP_CPSAT_TTEF=1           + time-table edge finding in cumulative (off by default in CP-SAT)
  RCPSP_CPSAT_OVERLOAD=1       + energetic overload checker in cumulative (off by default in CP-SAT)
  RCPSP_CPSAT_SEARCH=<auto|fixed|portfolio|lp|pseudo>   branching; fixed = chronological: the activity with the
                               smallest earliest start, at its earliest start (clause learning stays on)
  RCPSP_CPSAT_NORESTART=1      no restarts
  RCPSP_CPSAT_FORGET=1         learned clauses are deleted almost immediately (CP-SAT has no switch for "no
                               learning"; this is the closest: conflict analysis still backjumps, nothing is kept)
  RCPSP_CPSAT_OPT=<default|core|lbtree>   how the objective is closed: default = improve incumbents and tighten
                               from above; core / lbtree = CP-SAT's two "prove from below" modes
  RCPSP_CPSAT_MODE=<opt|prove|find>   opt = minimise. With the KNOWN optimum C* (known_optima.csv):
                               prove = makespan <= C*-1 must be INFEASIBLE (the pure optimality proof),
                               find  = find any schedule with makespan <= C* (the pure primal problem)
  RCPSP_ONLY=<file>            only the instances listed as size,group,exam
  RCPSP_RESULTS_DIR=<dir>      output directory (default <repo>/new_results)

Per instance, besides the usual columns: conflicts / branches / restarts / LP iterations, and for MODE=opt the
times at which the incumbent first equalled the final makespan (tIncumbentBest) and at which the proven lower
bound first equalled it (tBoundBest) - the split between "finding" and "proving" inside one run.

Output: <results dir>/cpsat_<size>_<flags>_vNN.csv, one row per instance. Prints 'Done -> <csv>'.
Usage:  python run_cpsat.py j60 [--data-dir DIR] [--out CSV] [--limit N]
"""
import time, csv, sys, os, argparse, types

try:
    from ortools.sat.python import cp_model
    import ortools
except ImportError:
    sys.exit("ortools not found. Install with: pip install --user ortools")

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
BASELINES_DIR = os.path.dirname(SCRIPT_DIR)
REPO_DIR = os.path.dirname(BASELINES_DIR)

# shared parser / verifier (see run_scip_ddt.py for why gurobipy is stubbed when missing)
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

parse_sm, verify_schedule, collect_instances, load_only, env_num, DATA_DIRS = \
    G.parse_sm, G.verify_schedule, G.collect_instances, G.load_only, G.env_num, G.DATA_DIRS

WORKERS = env_num("RCPSP_CPSAT_WORKERS", 1)
TIMEOUT_S = env_num("RCPSP_CPSAT_TIMEOUT", 300.0, float)
NOPRESOLVE = env_num("RCPSP_CPSAT_NOPRESOLVE", 0) != 0
LIN = env_num("RCPSP_CPSAT_LIN", -1)                      # -1 = leave CP-SAT's default
NODISJ = env_num("RCPSP_CPSAT_NODISJ", 0) != 0
TTEF = env_num("RCPSP_CPSAT_TTEF", 0) != 0
OVERLOAD = env_num("RCPSP_CPSAT_OVERLOAD", 0) != 0
SEARCH = (os.environ.get("RCPSP_CPSAT_SEARCH", "") or "auto").strip().lower()
NORESTART = env_num("RCPSP_CPSAT_NORESTART", 0) != 0
FORGET = env_num("RCPSP_CPSAT_FORGET", 0) != 0
OPT = (os.environ.get("RCPSP_CPSAT_OPT", "") or "default").strip().lower()
MODE = (os.environ.get("RCPSP_CPSAT_MODE", "") or "opt").strip().lower()
ONLY_FILE = os.environ.get("RCPSP_ONLY", "").strip()
NO_RESTART = 0   # SatParameters.RestartAlgorithm.NO_RESTART (the enum is not exposed by every OR-Tools build)
SEARCHES = {"auto": "AUTOMATIC_SEARCH", "fixed": "FIXED_SEARCH", "portfolio": "PORTFOLIO_SEARCH",
            "lp": "LP_SEARCH", "pseudo": "PSEUDO_COST_SEARCH"}
if SEARCH not in SEARCHES: sys.exit(f"RCPSP_CPSAT_SEARCH={SEARCH}: want one of {sorted(SEARCHES)}")
if OPT not in ("default", "core", "lbtree"): sys.exit(f"RCPSP_CPSAT_OPT={OPT}: want default|core|lbtree")
if MODE not in ("opt", "prove", "find"): sys.exit(f"RCPSP_CPSAT_MODE={MODE}: want opt|prove|find")

FLAGS = {"cpWorkers": WORKERS, "cpNoPresolve": int(NOPRESOLVE), "cpLin": LIN, "cpNoDisj": int(NODISJ),
         "cpTtef": int(TTEF), "cpOverload": int(OVERLOAD), "cpSearch": SEARCH, "cpNoRestart": int(NORESTART),
         "cpForget": int(FORGET), "cpOpt": OPT, "cpMode": MODE, "timeoutS": TIMEOUT_S,
         "onlyFile": os.path.basename(ONLY_FILE)}
RESULT_COLS = ["instance", "size", "group", "exam", "status", "solved", "optimal", "makespan", "provenLB",
               "knownOpt", "time_s", "build_s", "total_s", "conflicts", "branches", "restarts", "lpIters",
               "booleans", "tIncumbentBest", "tBoundBest", "nSolutions", "verified"]
PARAM_COLS = ["pWorkers", "pPresolve", "pLin", "pDisjInCumul", "pTtef", "pOverload", "pSearch", "pRestarts",
              "pCore", "pLbTree", "pTimeLimit", "ortoolsVersion"]
FIELDNAMES = RESULT_COLS + list(FLAGS) + PARAM_COLS


def load_known_optima():
    out = {}
    p = os.path.join(REPO_DIR, "known_optima.csv")
    if os.path.exists(p):
        for r in csv.DictReader(open(p, encoding="utf-8")):
            out[(r["size"], int(r["group"]), int(r["exam"]))] = int(r["optimum"])
    return out


class Timeline(cp_model.CpSolverSolutionCallback):
    """Wall time and makespan of every improving schedule."""
    def __init__(self, obj_var, t0):
        super().__init__(); self.obj, self.t0, self.sols = obj_var, t0, []
    def on_solution_callback(self):
        self.sols.append((time.perf_counter() - self.t0, int(self.value(self.obj))))


def solve_instance(sm_path, inst_name, size, group, exam, known):
    t0 = time.perf_counter()
    n, durations, rreq, rc, succs = parse_sm(sm_path)
    H = sum(durations)
    kopt = known.get((size, group, exam))
    row = {"instance": inst_name, "size": size, "group": group, "exam": exam, "knownOpt": kopt if kopt is not None else ""}
    row.update(FLAGS)
    if MODE != "opt" and kopt is None:
        row.update({"status": "NO_KNOWN_OPT", "solved": False, "optimal": False, "makespan": -1})
        return row

    m = cp_model.CpModel()
    s = [None] + [m.new_int_var(0, H, f"s{i}") for i in range(1, n + 1)]
    iv = [None] + [m.new_fixed_size_interval_var(s[i], durations[i - 1], f"i{i}") for i in range(1, n + 1)]
    m.add(s[1] == 0)
    for a, b in succs:
        m.add(s[b] >= s[a] + durations[a - 1])
    for r in range(len(rc)):
        use = [i for i in range(1, n + 1) if rreq[i - 1][r] > 0 and durations[i - 1] > 0]
        if use:
            m.add_cumulative([iv[i] for i in use], [rreq[i - 1][r] for i in use], rc[r])
    if MODE == "opt":
        m.minimize(s[n])
    elif MODE == "prove":
        m.add(s[n] <= kopt - 1)
    else:
        m.add(s[n] <= kopt)
    if SEARCH == "fixed":
        m.add_decision_strategy(s[1:], cp_model.CHOOSE_LOWEST_MIN, cp_model.SELECT_MIN_VALUE)

    solver = cp_model.CpSolver()
    p = solver.parameters
    p.num_workers = WORKERS
    p.random_seed = 0
    if NOPRESOLVE: p.cp_model_presolve = False
    if LIN >= 0: p.linearization_level = LIN
    if NODISJ: p.use_disjunctive_constraint_in_cumulative = False
    if TTEF: p.use_timetable_edge_finding_in_cumulative = True
    if OVERLOAD: p.use_overload_checker_in_cumulative = True
    if SEARCH != "auto":                    # the enum lives on the parameter class (new builds) or on cp_model (old)
        p.search_branching = getattr(type(p), SEARCHES[SEARCH], None) or getattr(cp_model, SEARCHES[SEARCH])
    if NORESTART:
        while len(p.restart_algorithms): p.restart_algorithms.pop()
        p.restart_algorithms.append(NO_RESTART)
    if FORGET:
        p.clause_cleanup_period = 100       # clean up every 100 learned clauses (default 10000) ...
        p.clause_cleanup_target = 0
        p.clause_cleanup_ratio = 1.0        # ... delete all that are not protected ...
        p.clause_cleanup_lbd_bound = 0      # ... and protect none by quality (default keeps LBD <= 5 forever)
    if OPT == "core": p.optimize_with_core = True
    if OPT == "lbtree": p.optimize_with_lb_tree_search = True

    build_s = time.perf_counter() - t0
    p.max_time_in_seconds = max(0.1, TIMEOUT_S - build_s)
    row.update({"pWorkers": p.num_workers, "pPresolve": int(p.cp_model_presolve), "pLin": p.linearization_level,
                "pDisjInCumul": int(p.use_disjunctive_constraint_in_cumulative),
                "pTtef": int(p.use_timetable_edge_finding_in_cumulative),
                "pOverload": int(p.use_overload_checker_in_cumulative),
                "pSearch": str(p.search_branching).split(".")[-1],
                "pRestarts": "+".join(str(int(a)) for a in p.restart_algorithms) or "default",
                "pCore": int(p.optimize_with_core), "pLbTree": int(p.optimize_with_lb_tree_search),
                "pTimeLimit": round(p.max_time_in_seconds, 3), "ortoolsVersion": ortools.__version__})

    t1 = time.perf_counter()
    tl = Timeline(s[n], t1)
    bounds = []
    try:
        solver.best_bound_callback = lambda b: bounds.append((time.perf_counter() - t1, b))
    except Exception:
        pass
    status = solver.solve(m, tl)
    solve_s = time.perf_counter() - t1
    name = solver.status_name(status)                      # OPTIMAL | FEASIBLE | INFEASIBLE | UNKNOWN | MODEL_INVALID
    has_sol = status in (cp_model.OPTIMAL, cp_model.FEASIBLE)

    makespan, verified, lb = -1, "", -1
    if has_sol:
        start = {i: int(solver.value(s[i])) for i in range(1, n + 1)}
        makespan = start[n]
        verified = verify_schedule(n, durations, rreq, rc, succs, start)
    if MODE == "opt":
        lb = int(round(solver.best_objective_bound)) if name in ("OPTIMAL", "FEASIBLE") else -1
        optimal = name == "OPTIMAL"
        t_inc = next((round(t, 3) for t, v in tl.sols if v == makespan), "") if has_sol else ""
        t_bnd = next((round(t, 3) for t, b in bounds if int(round(b)) >= makespan), "") if optimal else ""
        if optimal and t_bnd == "": t_bnd = round(solve_s, 3)
    elif MODE == "prove":                                   # success = the model is proven infeasible
        optimal = name == "INFEASIBLE"; lb = kopt if optimal else -1; t_inc, t_bnd = "", (round(solve_s, 3) if optimal else "")
        if has_sol: name = "WRONG_KNOWN_OPT"                # a schedule shorter than the "optimum": must never happen
    else:                                                   # find: success = a schedule of makespan <= C*
        optimal = has_sol and makespan <= kopt; t_inc, t_bnd = (round(solve_s, 3) if optimal else ""), ""
    rs = solver.response_proto
    stat = lambda f: int(getattr(rs, f, getattr(solver, f, -1)))
    row.update({"status": name, "solved": has_sol, "optimal": optimal, "makespan": makespan, "provenLB": lb,
                "time_s": round(solve_s, 3), "build_s": round(build_s, 3), "total_s": round(build_s + solve_s, 3),
                "conflicts": stat("num_conflicts"), "branches": stat("num_branches"), "restarts": stat("num_restarts"),
                "lpIters": stat("num_lp_iterations"), "booleans": stat("num_booleans"),
                "tIncumbentBest": t_inc, "tBoundBest": t_bnd, "nSolutions": len(tl.sols), "verified": verified})
    return row


def flag_info():
    parts = [] if MODE == "opt" else [MODE]
    if WORKERS != 1: parts.append(f"w{WORKERS}")
    if NOPRESOLVE: parts.append("nopresolve")
    if LIN >= 0: parts.append(f"lin{LIN}")
    if NODISJ: parts.append("nodisj")
    if TTEF: parts.append("ttef")
    if OVERLOAD: parts.append("overload")
    if SEARCH != "auto": parts.append(SEARCH)
    if NORESTART: parts.append("norestart")
    if FORGET: parts.append("forget")
    if OPT != "default": parts.append(OPT)
    if ONLY_FILE: parts.append("only")
    return "-".join(parts) or "default"


def open_next_csv(out_dir, size, info):
    os.makedirs(out_dir, exist_ok=True)
    for v in range(1, 1000):
        path = os.path.join(out_dir, f"cpsat_{size}_{info}_v{v:02d}.csv")
        try:
            return path, open(path, "x", newline="", encoding="utf-8")
        except FileExistsError:
            continue
    sys.exit(f"no free file name for cpsat_{size}_{info} in {out_dir}")


def main():
    ap = argparse.ArgumentParser(description="OR-Tools CP-SAT RCPSP baseline with component switches")
    ap.add_argument("size", choices=["j30", "j60", "j90"])
    ap.add_argument("--data-dir", default=None)
    ap.add_argument("--out", default=None)
    ap.add_argument("--limit", type=int, default=None, help="only the first N instances (testing)")
    args = ap.parse_args()

    instances = collect_instances(args.size, args.data_dir)
    if not instances:
        sys.exit(f"no instances found for {args.size} in {args.data_dir or DATA_DIRS[args.size]}")
    keep = load_only(ONLY_FILE) if ONLY_FILE else None
    known = load_known_optima()
    wanted = []
    for path, name in instances:
        _, g, e = G._natural_key(path, args.size)
        if keep is not None and (args.size, g, e) not in keep:
            continue
        wanted.append((path, name, g, e))
    if args.limit:
        wanted = wanted[:args.limit]

    out_dir = os.environ.get("RCPSP_RESULTS_DIR", "").strip() or os.path.join(REPO_DIR, "new_results")
    if args.out:
        out_path, fh = args.out, open(args.out, "w", newline="", encoding="utf-8")
    else:
        out_path, fh = open_next_csv(out_dir, args.size, flag_info())

    print(f"CP-SAT baseline: {args.size}, {len(wanted)} instances, {TIMEOUT_S}s each, flags={flag_info()}, ortools {ortools.__version__}")
    with fh:
        w = csv.DictWriter(fh, fieldnames=FIELDNAMES, lineterminator="\n")
        w.writeheader()
        for i, (path, name, g, e) in enumerate(wanted, 1):
            try:
                row = solve_instance(path, name, args.size, g, e, known)
            except Exception as ex:
                row = {"instance": name, "size": args.size, "group": g, "exam": e,
                       "status": f"ERROR {type(ex).__name__}: {ex}"[:120], "solved": False, "optimal": False, "makespan": -1}
                row.update(FLAGS)
            w.writerow({c: row.get(c, "") for c in FIELDNAMES})
            fh.flush()
            print(f"  [{i:3d}/{len(wanted)}] {name:12s} {str(row.get('status')):16s} mk={row.get('makespan')} lb={row.get('provenLB', '')} "
                  f"{row.get('total_s', 0.0)}s conflicts={row.get('conflicts', '')} verified={row.get('verified', '')}", flush=True)
            if row.get("solved") and row.get("verified") is not True:
                print(f"      !! schedule failed verification on {name}", flush=True)
    print(f"Done -> {out_path}")


if __name__ == "__main__":
    main()
