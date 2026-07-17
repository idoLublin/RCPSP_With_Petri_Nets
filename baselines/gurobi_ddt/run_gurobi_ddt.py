#!/usr/bin/env python3
"""
Gurobi + DDT (Disaggregated Precedence) RCPSP baseline.
Implements the time-indexed MIP with DDT constraints from Christofides et al. (1987).

DDT precedence for (i -> j) with duration p_i:
    sum_{t=0}^{tau} x[j][t] <= sum_{t=0}^{tau - p_i} x[i][t]   for all tau

This is tighter than the aggregated constraint (s_i + p_i <= s_j).

Output CSV columns: instance, solved, optimal, makespan, time_s
"""
import time, re, csv, sys, os, glob, argparse
from collections import defaultdict

try:
    import gurobipy as gp
    from gurobipy import GRB
except ImportError:
    print("gurobipy not found. Install with: pip install gurobipy")
    sys.exit(1)

TIMEOUT_S = 300
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))

DATA_DIRS = {
    "j30": r"C:\Users\idolu\CLionProjects\RCPSP_With_Petri_nets\extract_problems\data\j30.sm.tgz",
    "j60": r"C:\Users\idolu\Downloads\j60.sm.tgz",
    "j90": r"C:\Users\idolu\Downloads",
}


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


def solve_instance(sm_path, inst_name):
    n, durations, rreq, rc, succs = parse_sm(sm_path)
    n_res = len(rc)
    est, lst, H = compute_time_windows(n, durations, succs, rreq, rc)

    env = gp.Env(empty=True)
    env.setParam("OutputFlag", 0)
    env.start()

    with gp.Model(env=env) as model:
        model.Params.TimeLimit = TIMEOUT_S
        model.Params.Threads = 1
        model.Params.MIPGap = 0.0

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

        t0 = time.time()
        model.optimize()
        elapsed = time.time() - t0

        if model.SolCount > 0:
            makespan = int(round(model.ObjVal))
            optimal = model.Status == GRB.OPTIMAL
        else:
            makespan = -1
            optimal = False

        return {
            "instance": inst_name,
            "solved": model.SolCount > 0,
            "optimal": optimal,
            "makespan": makespan,
            "time_s": round(elapsed, 3),
        }


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


def main():
    parser = argparse.ArgumentParser(description="Gurobi DDT RCPSP baseline")
    parser.add_argument("size", choices=["j30", "j60", "j90"])
    parser.add_argument("--out", default=None)
    parser.add_argument("--data-dir", default=None,
                        help="Directory containing the .sm files (overrides built-in default)")
    args = parser.parse_args()

    out_csv = args.out or os.path.join(SCRIPT_DIR, f"results_gurobi_ddt_{args.size}.csv")
    instances = collect_instances(args.size, args.data_dir)

    if not instances:
        data_dir = args.data_dir or DATA_DIRS[args.size]
        print(f"No .sm files found in {data_dir}")
        sys.exit(1)

    print(f"Running Gurobi DDT on {len(instances)} {args.size} instances (timeout={TIMEOUT_S}s)")

    with open(out_csv, "w", newline="", encoding="utf-8") as csvfile:
        writer = csv.DictWriter(
            csvfile, fieldnames=["instance", "solved", "optimal", "makespan", "time_s"]
        )
        writer.writeheader()

        for i, (sm_path, inst_name) in enumerate(instances, 1):
            row = solve_instance(sm_path, inst_name)
            writer.writerow(row)
            csvfile.flush()
            status = "OPT" if row["optimal"] else ("SAT" if row["solved"] else "---")
            mk = row["makespan"] if row["solved"] else "n/a"
            print(f"  [{i:3d}/{len(instances)}] {inst_name:15s}  {status}  makespan={mk}  {row['time_s']:.1f}s")

    print(f"\nDone. Results saved to {out_csv}")


if __name__ == "__main__":
    main()
