#!/usr/bin/env python3
"""
DDT (disaggregated-precedence) time-indexed MIP for RCPSP, solved with
open-source solvers via PuLP: CBC, SCIP (pyscipopt), or HiGHS (highspy).

Same formulation as the Gurobi baseline — used to cross-check its results.
Output CSV: instance, solved, optimal, makespan, time_s

Usage:
  python run_ddt_mip.py j30 --solver cbc  [--data-dir DIR] [--out CSV]
  python run_ddt_mip.py j60 --solver scip
  python run_ddt_mip.py j90 --solver highs
"""
import re, csv, sys, os, glob, argparse, time
from collections import defaultdict
import pulp

TIMEOUT_S = 300
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
DATA_DIRS = {
    "j30": r"C:\Users\idolu\CLionProjects\RCPSP_With_Petri_nets\extract_problems\data\j30.sm.tgz",
    "j60": r"C:\Users\idolu\Downloads\j60.sm.tgz",
    "j90": r"C:\Users\idolu\Downloads",
}


def parse_sm(path):
    with open(path, encoding="utf-8", errors="replace") as f:
        data = f.read()
    n = int(re.search(r"jobs \(incl\. supersource/sink \)\s*:\s*(\d+)", data).group(1))
    prec = re.search(r"PRECEDENCE RELATIONS:(.*?)REQUESTS/DURATIONS:", data, re.DOTALL).group(1)
    succs = []
    for line in prec.splitlines():
        p = line.split()
        if len(p) >= 3 and p[0].isdigit():
            for j in p[3:]:
                succs.append((int(p[0]), int(j)))
    req = re.search(r"REQUESTS/DURATIONS:(.*?)RESOURCEAVAILABILITIES:", data, re.DOTALL).group(1)
    durations, rreq = [], []
    for line in req.splitlines():
        p = line.split()
        if len(p) >= 3 and p[0].isdigit():
            durations.append(int(p[2])); rreq.append(list(map(int, p[3:])))
    res = re.search(r"RESOURCEAVAILABILITIES:(.*?)\*", data, re.DOTALL).group(1)
    rc = list(map(int, [l for l in res.splitlines() if l.strip()][-1].split()))
    return n, durations, rreq, rc, succs


def _serial_sgs_makespan(n, durations, rreq, rc, predecessors, est):
    n_res = len(rc); cap = sum(durations) + 1
    usage = [[0]*n_res for _ in range(cap+1)]
    finish = [0]*(n+1); scheduled=set(); remaining=set(range(1, n+1))
    while remaining:
        elig = [j for j in remaining if all(p in scheduled for p in predecessors.get(j, []))]
        elig.sort(key=lambda j: (est[j], j)); j = elig[0]; d = durations[j-1]
        t = max((finish[p] for p in predecessors.get(j, [])), default=0)
        while not all(usage[tt][k] + rreq[j-1][k] <= rc[k] for tt in range(t, t+d) for k in range(n_res)):
            t += 1
        finish[j] = t + d
        for tt in range(t, t+d):
            for k in range(n_res): usage[tt][k] += rreq[j-1][k]
        scheduled.add(j); remaining.discard(j)
    return finish[n]


def compute_time_windows(n, durations, succs, rreq, rc):
    est = [0]*(n+1); predecessors = defaultdict(list)
    for i, j in succs: predecessors[j].append(i)
    changed = True
    while changed:
        changed = False
        for i, j in succs:
            if est[i] + durations[i-1] > est[j]:
                est[j] = est[i] + durations[i-1]; changed = True
    ub = _serial_sgs_makespan(n, durations, rreq, rc, predecessors, est)
    lst = [ub - durations[i-1] for i in range(n+1)]; lst[0] = 0
    changed = True
    while changed:
        changed = False
        for i, j in reversed(succs):
            if lst[j] - durations[i-1] < lst[i]:
                lst[i] = lst[j] - durations[i-1]; changed = True
    return est[1:], lst[1:], ub


def make_solver(name):
    if name == "cbc":
        return pulp.PULP_CBC_CMD(timeLimit=TIMEOUT_S, threads=1, msg=False)
    if name == "scip":
        return pulp.SCIP_PY(timeLimit=TIMEOUT_S, msg=False)
    if name == "highs":
        return pulp.HiGHS(timeLimit=TIMEOUT_S, threads=1, msg=False)
    raise ValueError(name)


def solve_instance(sm_path, inst_name, solver_name, verbose=False):
    n, durations, rreq, rc, succs = parse_sm(sm_path)
    n_res = len(rc)
    est, lst, H = compute_time_windows(n, durations, succs, rreq, rc)

    prob = pulp.LpProblem("rcpsp_ddt", pulp.LpMinimize)
    x = {(i, t): pulp.LpVariable(f"x_{i}_{t}", cat="Binary")
         for i in range(1, n+1) for t in range(est[i-1], lst[i-1]+1)}

    # objective: sink start time (= makespan, sink duration 0)
    prob += pulp.lpSum(t * x[n, t] for t in range(est[n-1], lst[n-1]+1))

    # each activity scheduled once
    for i in range(1, n+1):
        prob += pulp.lpSum(x[i, t] for t in range(est[i-1], lst[i-1]+1)) == 1

    # DDT precedence
    for pred, succ in succs:
        d = durations[pred-1]
        for tau in range(est[succ-1], lst[succ-1]+d+1):
            lhs = [x[succ, t] for t in range(est[succ-1], min(tau, lst[succ-1])+1)]
            rhs = [x[pred, t] for t in range(est[pred-1], min(tau-d, lst[pred-1])+1) if tau-d >= est[pred-1]]
            if lhs:
                prob += pulp.lpSum(lhs) <= pulp.lpSum(rhs)

    # resource
    for t in range(H):
        for r in range(n_res):
            terms = []
            for i in range(1, n+1):
                if rreq[i-1][r] == 0:
                    continue
                d = durations[i-1]
                for q in range(max(est[i-1], t-d+1), min(t, lst[i-1])+1):
                    terms.append(rreq[i-1][r] * x[i, q])
            if terms:
                prob += pulp.lpSum(terms) <= rc[r]

    solver = make_solver(solver_name)
    t0 = time.time()
    prob.solve(solver)
    elapsed = time.time() - t0

    status = prob.status                       # 1 = LpStatusOptimal
    sol_status = getattr(prob, "sol_status", None)
    try:
        obj = pulp.value(prob.objective)
    except Exception:
        obj = None
    has_sol = obj is not None and sol_status not in (
        pulp.constants.LpSolutionNoSolutionFound, pulp.constants.LpSolutionInfeasible)
    optimal = (status == pulp.LpStatusOptimal) or (sol_status == pulp.constants.LpSolutionOptimal)
    makespan = int(round(obj)) if has_sol else -1

    if verbose:
        print(f"    [{solver_name}] status={pulp.LpStatus[status]}({status}) "
              f"sol_status={sol_status} obj={obj} -> makespan={makespan} optimal={optimal} {elapsed:.2f}s")

    return {"instance": inst_name, "solved": has_sol, "optimal": bool(optimal),
            "makespan": makespan if has_sol else -1, "time_s": round(elapsed, 3)}


def _key(path, prefix):
    name = os.path.basename(path).replace(".sm", ""); after = name[len(prefix):]
    p = after.split("_")
    try: return (0, int(p[0]), int(p[1]))
    except (ValueError, IndexError): return (1, 0, name)


def collect_instances(size, data_dir=None):
    data_dir = data_dir or DATA_DIRS[size]
    files = sorted(glob.glob(os.path.join(data_dir, f"{size}*_*.sm")), key=lambda f: _key(f, size))
    return [(f, os.path.basename(f).replace(".sm", "")) for f in files]


def main():
    ap = argparse.ArgumentParser(description="DDT MIP RCPSP baseline (CBC/SCIP/HiGHS)")
    ap.add_argument("size", choices=["j30", "j60", "j90"])
    ap.add_argument("--solver", required=True, choices=["cbc", "scip", "highs"])
    ap.add_argument("--data-dir", default=None)
    ap.add_argument("--out", default=None)
    ap.add_argument("--limit", type=int, default=None, help="only first N instances (testing)")
    ap.add_argument("--verbose", action="store_true")
    args = ap.parse_args()

    out_csv = args.out or os.path.join(SCRIPT_DIR, f"results_{args.solver}_ddt_{args.size}.csv")
    insts = collect_instances(args.size, args.data_dir)
    if not insts:
        print(f"No .sm files for {args.size} in {args.data_dir or DATA_DIRS[args.size]}"); sys.exit(1)
    if args.limit:
        insts = insts[:args.limit]

    print(f"DDT-{args.solver.upper()} on {len(insts)} {args.size} instances (timeout={TIMEOUT_S}s)")
    with open(out_csv, "w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=["instance", "solved", "optimal", "makespan", "time_s"])
        w.writeheader()
        for idx, (p, name) in enumerate(insts, 1):
            row = solve_instance(p, name, args.solver, verbose=args.verbose)
            w.writerow(row); f.flush()
            st = "OPT" if row["optimal"] else ("SAT" if row["solved"] else "---")
            print(f"  [{idx:3d}/{len(insts)}] {name:15s} {st} makespan={row['makespan']} {row['time_s']:.1f}s")
    print(f"Done -> {out_csv}")


if __name__ == "__main__":
    main()
