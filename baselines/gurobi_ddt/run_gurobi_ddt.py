#!/usr/bin/env python3
"""
Gurobi + DDT (Disaggregated Precedence) RCPSP baseline.
Implements the time-indexed MIP with DDT constraints from Christofides et al. (1987).

DDT precedence for (i -> j) with duration p_i:
    sum_{t=0}^{tau} x[j][t] <= sum_{t=0}^{tau - p_i} x[i][t]   for all tau

This is tighter than the aggregated constraint (s_i + p_i <= s_j).

Env flags (parsed by value, all default off; every flag is written to the CSV header and each row):
  RCPSP_GRB_NOPRESOLVE=1       Presolve=0 (the MIP counterpart of the root stage CBS runs without)
  RCPSP_GRB_NOCUTS=1           Cuts=0
  RCPSP_GRB_NOHEUR=1           Heuristics=0, RINS=0, PumpPasses=0, MinRelNodes=0, ZeroObjNodes=0
  RCPSP_GRB_NOSYMMETRY=1       Symmetry=0
  RCPSP_GRB_BUILD_IN_BUDGET=1  model build time counts inside the budget (TimeLimit = budget - build)
  RCPSP_GRB_SOFTMEM=<GB>       SoftMemLimit (0 = none); an instance over it ends with status MEM_LIMIT
  RCPSP_GRB_TIMEOUT=<s>        per-instance budget (default 300)
  RCPSP_ONLY=<file>            only the instances listed as size,group,exam (one per line)
  RCPSP_RESULTS_DIR=<dir>      output directory (default <repo>/new_results)

Output: <results dir>/gurobi_<size>_ddt[-<flags>]_vNN.csv (or --out), one row per instance: solver status,
makespan, proven lower bound, gap, nodes, build/solve time, peak memory, and an independent check of the
returned schedule (verified). Prints 'Done -> <csv>' at the end.
"""
import time, re, csv, sys, os, glob, argparse, math
from collections import defaultdict

try:
    import gurobipy as gp
    from gurobipy import GRB
except ImportError:
    print("gurobipy not found. Install with: pip install gurobipy")
    sys.exit(1)

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_DIR = os.path.dirname(os.path.dirname(SCRIPT_DIR))

DATA_DIRS = {
    "j30": r"C:\Users\idolu\CLionProjects\RCPSP_With_Petri_nets\extract_problems\data\j30.sm.tgz",
    "j60": r"C:\Users\idolu\Downloads\j60.sm.tgz",
    "j90": r"C:\Users\idolu\Downloads",
}


def env_num(name, default, kind=int):
    """Parse an env flag by value (RCPSP_X=0 is off); unset or empty means the default."""
    v = os.environ.get(name, "").strip()
    if v == "":
        return default
    try:
        return kind(v)
    except ValueError:
        sys.exit(f"{name}={v!r}: expected a number")


NOPRESOLVE = env_num("RCPSP_GRB_NOPRESOLVE", 0) != 0
NOCUTS = env_num("RCPSP_GRB_NOCUTS", 0) != 0
NOHEUR = env_num("RCPSP_GRB_NOHEUR", 0) != 0
NOSYMMETRY = env_num("RCPSP_GRB_NOSYMMETRY", 0) != 0
BUILD_IN_BUDGET = env_num("RCPSP_GRB_BUILD_IN_BUDGET", 0) != 0
SOFTMEM_GB = env_num("RCPSP_GRB_SOFTMEM", 0.0, float)
TIMEOUT_S = env_num("RCPSP_GRB_TIMEOUT", 300.0, float)
ONLY_FILE = os.environ.get("RCPSP_ONLY", "").strip()
THREADS = 1

FLAGS = {
    "grbNoPresolve": int(NOPRESOLVE),
    "grbNoCuts": int(NOCUTS),
    "grbNoHeur": int(NOHEUR),
    "grbNoSymmetry": int(NOSYMMETRY),
    "grbBuildInBudget": int(BUILD_IN_BUDGET),
    "grbSoftMemGB": SOFTMEM_GB,
    "timeoutS": TIMEOUT_S,
    "threads": THREADS,
    "onlyFile": os.path.basename(ONLY_FILE),
}
RESULT_COLS = ["instance", "size", "group", "exam", "status", "solved", "optimal", "makespan", "provenLB", "gap",
               "time_s", "build_s", "total_s", "nodes", "simplexIters", "horizon", "sgsUB", "nVars", "nConstrs",
               "maxMemGB", "verified"]
# parameter values read back from the model after they are set (what Gurobi actually used)
PARAM_COLS = ["pPresolve", "pCuts", "pHeuristics", "pRINS", "pPumpPasses", "pSymmetry", "pThreads",
              "pTimeLimit", "pMIPGap", "pSoftMemLimit", "gurobiVersion"]
FIELDNAMES = RESULT_COLS + list(FLAGS) + PARAM_COLS

STATUS_NAMES = {GRB.OPTIMAL: "OPTIMAL", GRB.TIME_LIMIT: "TIME_LIMIT", GRB.MEM_LIMIT: "MEM_LIMIT",
                GRB.INFEASIBLE: "INFEASIBLE", GRB.INF_OR_UNBD: "INF_OR_UNBD", GRB.INTERRUPTED: "INTERRUPTED",
                GRB.SUBOPTIMAL: "SUBOPTIMAL", GRB.NUMERIC: "NUMERIC"}


def parse_sm(path):
    with open(path, encoding="utf-8") as f:
        data = f.read()

    m = re.search(r"jobs \(incl\. supersource/sink \)\s*:\s*(\d+)", data)
    n = int(m.group(1))

    prec_text = re.search(
        r"PRECEDENCE RELATIONS:(.*?)REQUESTS/DURATIONS:", data, re.DOTALL
    ).group(1)
    succs = []
    for line in prec_text.splitlines():
        parts = line.split()
        if len(parts) >= 3 and parts[0].isdigit():
            i = int(parts[0])
            for j in parts[3:]:
                succs.append((i, int(j)))

    req_text = re.search(
        r"REQUESTS/DURATIONS:(.*?)RESOURCEAVAILABILITIES:", data, re.DOTALL
    ).group(1)
    durations, rreq = [], []
    for line in req_text.splitlines():
        parts = line.split()
        if len(parts) >= 3 and parts[0].isdigit():
            durations.append(int(parts[2]))
            rreq.append(list(map(int, parts[3:])))

    res_text = re.search(r"RESOURCEAVAILABILITIES:(.*?)\*", data, re.DOTALL).group(1)
    res_lines = [l.strip() for l in res_text.splitlines() if l.strip()]
    rc = list(map(int, res_lines[-1].split()))

    return n, durations, rreq, rc, succs


def _serial_sgs_makespan(n, durations, rreq, rc, predecessors, est):
    """Serial schedule generation scheme -> a resource-feasible makespan.

    This is a valid UPPER bound on the optimal makespan, used to size the
    time-indexed horizon. Without a resource-feasible UB the model can be
    infeasible (the critical-path length is only a LOWER bound).
    """
    n_res = len(rc)
    cap = sum(durations) + 1                 # serial worst case bounds the arrays
    usage = [[0] * n_res for _ in range(cap + 1)]
    finish = [0] * (n + 1)
    scheduled = set()
    remaining = set(range(1, n + 1))

    while remaining:
        eligible = [j for j in remaining
                    if all(p in scheduled for p in predecessors.get(j, []))]
        eligible.sort(key=lambda j: (est[j], j))   # earliest EST first (deterministic)
        j = eligible[0]
        d = durations[j - 1]
        t = max((finish[p] for p in predecessors.get(j, [])), default=0)
        while not all(usage[tt][k] + rreq[j - 1][k] <= rc[k]
                      for tt in range(t, t + d) for k in range(n_res)):
            t += 1
        finish[j] = t + d
        for tt in range(t, t + d):
            for k in range(n_res):
                usage[tt][k] += rreq[j - 1][k]
        scheduled.add(j)
        remaining.discard(j)

    return finish[n]


def compute_time_windows(n, durations, succs, rreq, rc):
    """EST via CPM, a resource-feasible makespan UB via serial SGS, then LST.

    Returns (est, lst, horizon) — all consistent with a horizon that can
    actually hold a feasible schedule. The horizon is the SGS makespan (a
    valid UB), NOT the critical-path length (which is only a lower bound).
    """
    est = [0] * (n + 1)
    predecessors = defaultdict(list)
    for i, j in succs:
        predecessors[j].append(i)

    # Forward pass: EST
    changed = True
    while changed:
        changed = False
        for i, j in succs:
            new_est = est[i] + durations[i - 1]
            if new_est > est[j]:
                est[j] = new_est
                changed = True

    # Valid UB on the makespan (resource-feasible) -> the time-indexed horizon
    ub = _serial_sgs_makespan(n, durations, rreq, rc, predecessors, est)

    # Backward pass: LST so every activity finishes by the horizon
    lst = [ub - durations[i - 1] for i in range(n + 1)]
    lst[0] = 0  # unused (1-indexed)

    changed = True
    while changed:
        changed = False
        for i, j in reversed(succs):
            new_lst_i = lst[j] - durations[i - 1]
            if new_lst_i < lst[i]:
                lst[i] = new_lst_i
                changed = True

    return est[1:], lst[1:], ub  # 0-indexed: est[k] = EST for activity k+1


def verify_schedule(n, durations, rreq, rc, succs, start):
    """Independent check of a returned schedule: every activity started once, precedences, capacities."""
    if sorted(start) != list(range(1, n + 1)):
        return False
    for i, j in succs:
        if start[i] + durations[i - 1] > start[j]:
            return False
    usage = defaultdict(lambda: [0] * len(rc))
    for j in range(1, n + 1):
        for t in range(start[j], start[j] + durations[j - 1]):
            u = usage[t]
            for k in range(len(rc)):
                u[k] += rreq[j - 1][k]
                if u[k] > rc[k]:
                    return False
    return start[n] >= max(start[j] + durations[j - 1] for j in range(1, n + 1))


def set_params(model, limit):
    model.Params.TimeLimit = limit
    model.Params.Threads = THREADS
    model.Params.MIPGap = 0.0
    if SOFTMEM_GB > 0:
        model.Params.SoftMemLimit = SOFTMEM_GB
    if NOPRESOLVE:
        model.Params.Presolve = 0
    if NOCUTS:
        model.Params.Cuts = 0
    if NOHEUR:
        model.Params.Heuristics = 0.0
        model.Params.RINS = 0
        model.Params.PumpPasses = 0
        model.Params.MinRelNodes = 0
        model.Params.ZeroObjNodes = 0
    if NOSYMMETRY:
        model.Params.Symmetry = 0


def param_readback(model):
    p = model.Params
    return {"pPresolve": p.Presolve, "pCuts": p.Cuts, "pHeuristics": p.Heuristics, "pRINS": p.RINS,
            "pPumpPasses": p.PumpPasses, "pSymmetry": p.Symmetry, "pThreads": p.Threads,
            "pTimeLimit": round(p.TimeLimit, 3), "pMIPGap": p.MIPGap, "pSoftMemLimit": p.SoftMemLimit,
            "gurobiVersion": ".".join(map(str, gp.gurobi.version()))}


def solve_instance(env, sm_path, inst_name, size, group, exam):
    t0 = time.perf_counter()
    n, durations, rreq, rc, succs = parse_sm(sm_path)
    n_res = len(rc)
    est, lst, H = compute_time_windows(n, durations, succs, rreq, rc)

    with gp.Model(env=env) as model:
        # x[i][t] = 1 if activity i (1-indexed) starts at time t
        x = {}
        for i in range(1, n + 1):
            for t in range(est[i - 1], lst[i - 1] + 1):
                x[i, t] = model.addVar(vtype=GRB.BINARY, obj=0.0, name=f"x_{i}_{t}")
        model.update()

        # Each activity scheduled exactly once
        for i in range(1, n + 1):
            model.addConstr(
                gp.quicksum(x[i, t] for t in range(est[i - 1], lst[i - 1] + 1)) == 1,
                name=f"once_{i}",
            )

        # DDT precedence constraints: for (i->j), for all tau:
        #   sum_{t=0}^{tau} x[j][t] <= sum_{t=0}^{tau - d_i} x[i][t]
        for pred, succ in succs:
            d = durations[pred - 1]
            # Only add for tau where the constraint can be binding
            for tau in range(est[succ - 1], lst[succ - 1] + d + 1):
                lhs_terms = [
                    x[succ, t]
                    for t in range(est[succ - 1], min(tau, lst[succ - 1]) + 1)
                ]
                rhs_terms = [
                    x[pred, t]
                    for t in range(est[pred - 1], min(tau - d, lst[pred - 1]) + 1)
                    if tau - d >= est[pred - 1]
                ]
                if lhs_terms:
                    model.addConstr(
                        gp.quicksum(lhs_terms) <= gp.quicksum(rhs_terms),
                        name=f"ddt_{pred}_{succ}_{tau}",
                    )

        # Resource constraints
        for t in range(H):
            for r in range(n_res):
                terms = []
                for i in range(1, n + 1):
                    if rreq[i - 1][r] == 0:
                        continue
                    d = durations[i - 1]
                    for q in range(
                        max(est[i - 1], t - d + 1), min(t, lst[i - 1]) + 1
                    ):
                        terms.append(rreq[i - 1][r] * x[i, q])
                if terms:
                    model.addConstr(
                        gp.quicksum(terms) <= rc[r], name=f"res_{r}_{t}"
                    )

        # Objective: minimize finish time of sink (activity n, duration 0)
        model.setObjective(
            gp.quicksum(t * x[n, t] for t in range(est[n - 1], lst[n - 1] + 1)),
            GRB.MINIMIZE,
        )
        model.update()
        build_s = time.perf_counter() - t0

        limit = TIMEOUT_S - build_s if BUILD_IN_BUDGET else TIMEOUT_S
        row = {"instance": inst_name, "size": size, "group": group, "exam": exam, "horizon": H, "sgsUB": H,
               "nVars": model.NumVars, "nConstrs": model.NumConstrs, "build_s": round(build_s, 3)}
        row.update(FLAGS)
        if limit <= 0:
            row.update({"status": "BUILD_TIMEOUT", "solved": False, "optimal": False, "makespan": -1,
                        "provenLB": -1, "gap": "", "time_s": 0.0, "total_s": round(build_s, 3), "nodes": 0,
                        "simplexIters": 0, "maxMemGB": -1, "verified": ""})
            return row

        set_params(model, limit)
        row.update(param_readback(model))

        t1 = time.perf_counter()
        model.optimize()
        solve_s = time.perf_counter() - t1

        status = STATUS_NAMES.get(model.Status, str(model.Status))
        solved = model.SolCount > 0
        makespan = int(round(model.ObjVal)) if solved else -1
        try:
            proven_lb = math.ceil(model.ObjBound - 1e-6)
        except (gp.GurobiError, AttributeError):
            proven_lb = -1
        try:
            max_mem = round(model.MaxMemUsed, 3)
        except (gp.GurobiError, AttributeError):
            max_mem = -1

        verified = ""
        if solved:
            start = {}
            for (i, t), var in x.items():
                if var.X > 0.5:
                    if i in start:
                        start = None
                        break
                    start[i] = t
            verified = bool(start) and verify_schedule(n, durations, rreq, rc, succs, start) \
                and start[n] == makespan

        row.update({"status": status, "solved": solved, "optimal": model.Status == GRB.OPTIMAL,
                    "makespan": makespan, "provenLB": proven_lb,
                    "gap": round(model.MIPGap, 6) if solved else "",
                    "time_s": round(solve_s, 3), "total_s": round(build_s + solve_s, 3),
                    "nodes": int(model.NodeCount), "simplexIters": int(model.IterCount),
                    "maxMemGB": max_mem, "verified": verified})
        return row


def _natural_key(path, prefix):
    # Sort by (param, instance) numerically so order is 1..48 x 1..10,
    # not lexicographic (which puts j3010_1 before j301_1).
    name = os.path.basename(path).replace(".sm", "")
    after = name[len(prefix):]            # e.g. "10_1"
    parts = after.split("_")
    try:
        return (0, int(parts[0]), int(parts[1]))
    except (ValueError, IndexError):
        return (1, 0, name)


def collect_instances(size, data_dir=None):
    data_dir = data_dir or DATA_DIRS[size]
    # Use *_* to match only instance files (e.g. j901_1.sm), not j90hrs.sm etc.
    pattern = os.path.join(data_dir, f"{size}*_*.sm")
    files = sorted(glob.glob(pattern), key=lambda f: _natural_key(f, size))
    return [(f, os.path.basename(f).replace(".sm", "")) for f in files]


def load_only(path):
    keep = set()
    with open(path, encoding="utf-8") as f:
        for line in f:
            line = line.split("#")[0].strip()
            if line:
                s, g, e = (x.strip() for x in line.split(","))
                keep.add((s, int(g), int(e)))
    return keep


def open_next_csv(out_dir, size, info):
    """<solver>_<size>_<info>_vNN.csv, first free NN; opened exclusively so parallel runs never share a file."""
    os.makedirs(out_dir, exist_ok=True)
    for v in range(1, 1000):
        path = os.path.join(out_dir, f"gurobi_{size}_{info}_v{v:02d}.csv")
        try:
            return path, open(path, "x", newline="", encoding="utf-8")
        except FileExistsError:
            continue
    sys.exit(f"no free file name for gurobi_{size}_{info} in {out_dir}")


def main():
    parser = argparse.ArgumentParser(description="Gurobi DDT RCPSP baseline")
    parser.add_argument("size", choices=["j30", "j60", "j90"])
    parser.add_argument("--out", default=None, help="explicit output CSV (default: results dir, versioned name)")
    parser.add_argument("--data-dir", default=None,
                        help="Directory containing the .sm files (overrides built-in default)")
    args = parser.parse_args()

    instances = collect_instances(args.size, args.data_dir)
    if not instances:
        data_dir = args.data_dir or DATA_DIRS[args.size]
        print(f"No .sm files found in {data_dir}")
        sys.exit(1)
    if ONLY_FILE:
        keep = load_only(ONLY_FILE)
        instances = [(p, nm) for p, nm in instances
                     if (args.size, *map(int, nm[len(args.size):].split("_"))) in keep]
        if not instances:
            print(f"RCPSP_ONLY={ONLY_FILE} selects no {args.size} instance")
            sys.exit(1)

    info = "ddt" + "".join(f"-{tag}" for tag, on in (("nopresolve", NOPRESOLVE), ("nocuts", NOCUTS),
                                                    ("noheur", NOHEUR), ("nosym", NOSYMMETRY),
                                                    ("budget", BUILD_IN_BUDGET), ("only", bool(ONLY_FILE))) if on)
    if args.out:
        out_csv, fh = args.out, open(args.out, "w", newline="", encoding="utf-8")
    else:
        out_dir = os.environ.get("RCPSP_RESULTS_DIR", "").strip() or os.path.join(REPO_DIR, "new_results")
        out_csv, fh = open_next_csv(out_dir, args.size, info)

    try:
        env = gp.Env(empty=True)
        env.setParam("OutputFlag", 0)
        env.start()
    except gp.GurobiError as e:
        print(f"Gurobi environment failed to start (license / token server?): {e}")
        fh.close()
        os.remove(out_csv)
        sys.exit(2)

    print(f"Gurobi {'.'.join(map(str, gp.gurobi.version()))} DDT on {len(instances)} {args.size} instances")
    print("effective flags: " + " ".join(f"{k}={v}" for k, v in FLAGS.items()))
    print(f"output: {out_csv}")

    n_opt = n_err = 0
    with fh:
        writer = csv.DictWriter(fh, fieldnames=FIELDNAMES, lineterminator="\n")
        writer.writeheader()
        for i, (sm_path, inst_name) in enumerate(instances, 1):
            group, exam = map(int, inst_name[len(args.size):].split("_"))
            try:
                row = solve_instance(env, sm_path, inst_name, args.size, group, exam)
            except gp.GurobiError as e:
                n_err += 1
                row = {"instance": inst_name, "size": args.size, "group": group, "exam": exam,
                       "status": f"ERROR {e.errno}", "solved": False, "optimal": False, "makespan": -1}
                row.update(FLAGS)
            writer.writerow(row)
            fh.flush()
            n_opt += bool(row.get("optimal"))
            mk = row["makespan"] if row.get("solved") else "n/a"
            print(f"  [{i:3d}/{len(instances)}] {inst_name:15s}  {row['status']:12s} makespan={mk} "
                  f"lb={row.get('provenLB', '')} {row.get('total_s', 0.0)}s verified={row.get('verified', '')}",
                  flush=True)
            if row.get("solved") and row.get("verified") is not True:
                print(f"  !! {inst_name}: returned schedule FAILED verification", flush=True)

    env.dispose()
    print(f"\nproven optimal {n_opt}/{len(instances)}, errors {n_err}")
    print(f"Done -> {out_csv}")


if __name__ == "__main__":
    main()
