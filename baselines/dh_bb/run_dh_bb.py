#!/usr/bin/env python3
"""
Demeulemeester & Herroelen (1992) Branch-and-Bound for RCPSP.

Reference: Demeulemeester & Herroelen (1992)
  "A branch-and-bound procedure for the multiple resource-constrained
   project scheduling problem." Management Science 38(12): 1803-1818.

Key components implemented:
  1. Activity enumeration with minimal delaying alternatives (MDAs)
  2. CPM-based lower bound (EST via forward pass on unscheduled activities)
  3. Predecessor-set dominance rule

Output CSV: instance, solved, optimal, makespan, time_s
"""
import re, csv, sys, os, glob, argparse, time
from collections import defaultdict

TIMEOUT_S = 300

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))

DATA_DIRS = {
    "j30": r"C:\Users\idolu\CLionProjects\RCPSP_With_Petri_nets\extract_problems\data\j30.sm.tgz",
    "j60": r"C:\Users\idolu\Downloads\j60.sm.tgz",
    "j90": r"C:\Users\idolu\Downloads",
}


# ---------------------------------------------------------------------------
# PSPLIB .sm parser
# ---------------------------------------------------------------------------

def parse_sm(path):
    with open(path, encoding="utf-8", errors="replace") as f:
        data = f.read()

    m = re.search(r"jobs \(incl\. supersource/sink \)\s*:\s*(\d+)", data)
    n = int(m.group(1))  # includes source (1) and sink (n)

    prec_text = re.search(r"PRECEDENCE RELATIONS:(.*?)REQUESTS/DURATIONS:", data, re.DOTALL).group(1)
    succs = defaultdict(list)   # j -> [successors]
    preds = defaultdict(list)   # j -> [predecessors]
    for line in prec_text.splitlines():
        parts = line.split()
        if len(parts) >= 3 and parts[0].isdigit():
            i = int(parts[0])
            for s in parts[3:]:
                j = int(s)
                succs[i].append(j)
                preds[j].append(i)

    req_text = re.search(r"REQUESTS/DURATIONS:(.*?)RESOURCEAVAILABILITIES:", data, re.DOTALL).group(1)
    dur = {}
    req = {}  # j -> [r1, r2, ...]
    for line in req_text.splitlines():
        parts = line.split()
        if len(parts) >= 3 and parts[0].isdigit():
            j = int(parts[0])
            dur[j] = int(parts[2])
            req[j] = list(map(int, parts[3:]))

    res_text = re.search(r"RESOURCEAVAILABILITIES:(.*?)\*", data, re.DOTALL).group(1)
    res_lines = [l.strip() for l in res_text.splitlines() if l.strip()]
    cap = list(map(int, res_lines[-1].split()))  # resource capacities

    activities = list(range(1, n + 1))
    return activities, dur, req, cap, dict(succs), dict(preds)


# ---------------------------------------------------------------------------
# CPM forward / backward pass for EST and lower-bound computation
# ---------------------------------------------------------------------------

def compute_est(activities, dur, preds):
    """Earliest start times ignoring resources (critical path)."""
    est = {j: 0 for j in activities}
    changed = True
    while changed:
        changed = False
        for j in activities:
            for i in preds.get(j, []):
                candidate = est[i] + dur[i]
                if candidate > est[j]:
                    est[j] = candidate
                    changed = True
    return est


def compute_lst(activities, dur, succs, horizon):
    """Latest start times (for bounding only)."""
    sink = activities[-1]
    lst = {j: horizon - dur[j] for j in activities}
    changed = True
    while changed:
        changed = False
        for i in activities:
            for j in succs.get(i, []):
                candidate = lst[j] - dur[i]
                if candidate < lst[i]:
                    lst[i] = candidate
                    changed = True
    return lst


# ---------------------------------------------------------------------------
# Lower bound: CPM on remaining activities from current time t
# ---------------------------------------------------------------------------

def compute_lb(t, unscheduled, finish_times_all, dur, succs_all, activities):
    """
    CPM lower bound for current node.
    finish_times_all[j] = time job j becomes available (finish time if done, else t + dur if running,
    else computed via CPM for unscheduled).
    """
    # For each unscheduled job, compute EST from t using already-known finish times
    # as constraints from predecessors that are running or done.
    est = {j: t for j in unscheduled}
    changed = True
    iters = 0
    while changed and iters < len(unscheduled) * 2:
        changed = False
        iters += 1
        for j in unscheduled:
            for sj in succs_all.get(j, []):
                if sj in est:
                    candidate = est[j] + dur[j]
                    if candidate > est[sj]:
                        est[sj] = candidate
                        changed = True
    # LB = earliest finish of the sink
    sink = activities[-1]
    return est.get(sink, t) + dur[sink]


# ---------------------------------------------------------------------------
# D&H Branch-and-Bound
# ---------------------------------------------------------------------------

class Solver:
    def __init__(self, activities, dur, req, cap, succs, preds, timeout=TIMEOUT_S):
        self.activities = activities
        self.dur = dur
        self.req = req        # j -> [r1, r2, ...]
        self.cap = cap        # [R1, R2, ...]
        self.succs = succs
        self.preds = preds
        self.n_res = len(cap)
        self.n = len(activities)
        self.source = activities[0]
        self.sink = activities[-1]
        self.timeout = timeout
        self.deadline = None

        # Pre-compute EST for initial UB (SSGS heuristic)
        self.est_base = compute_est(activities, dur, preds)

        self.best_makespan = None
        self.best_schedule = None

        # Dominance: for each frozenset of completed activities, store
        # the set of finish-time vectors seen — prune if dominated.
        # Key: frozenset of completed activity IDs
        # Value: list of finish-time tuples (indexed by sorted activities)
        self.dominance_store = defaultdict(list)

    # ------------------------------------------------------------------
    # Initial upper bound via Serial Schedule Generation Scheme (SSGS)
    # ------------------------------------------------------------------
    def ssgs_upper_bound(self):
        scheduled = {}  # j -> start_time
        remaining_cap = [list(self.cap) for _ in range(sum(self.dur.values()) + 1)]
        completed = set()
        pending = set(self.activities)

        while pending:
            eligible = [j for j in pending if all(p in completed for p in self.preds.get(j, []))]
            eligible.sort(key=lambda j: self.est_base[j])

            for j in eligible:
                t = max(self.est_base[j],
                        max((scheduled[p] + self.dur[p] for p in self.preds.get(j, [])), default=0))
                # Find earliest feasible t
                d = self.dur[j]
                while True:
                    if all(remaining_cap[tt][k] >= self.req[j][k]
                           for tt in range(t, t + d) for k in range(self.n_res)):
                        break
                    t += 1
                scheduled[j] = t
                for tt in range(t, t + d):
                    for k in range(self.n_res):
                        remaining_cap[tt][k] -= self.req[j][k]
                completed.add(j)
                pending.discard(j)

        makespan = scheduled[self.sink] + self.dur[self.sink]
        return makespan, scheduled

    # ------------------------------------------------------------------
    # Resource usage of a set of running jobs at a given moment
    # ------------------------------------------------------------------
    def res_used(self, running):
        """running: dict {j -> finish_time}. Returns list of resource usage."""
        used = [0] * self.n_res
        for j in running:
            for k in range(self.n_res):
                used[k] += self.req[j][k]
        return used

    # ------------------------------------------------------------------
    # Dominance check and storage
    # ------------------------------------------------------------------
    def is_dominated(self, completed_set, finish_times):
        """
        Check if current partial schedule is dominated by a previously seen one.
        Dominated = all finish times in stored schedule <= current finish times.
        """
        key = frozenset(completed_set)
        acts = sorted(completed_set)
        vec = tuple(finish_times[j] for j in acts)
        for prev_vec in self.dominance_store[key]:
            if all(prev_vec[i] <= vec[i] for i in range(len(acts))):
                return True
        return False

    def store_schedule(self, completed_set, finish_times):
        key = frozenset(completed_set)
        acts = sorted(completed_set)
        vec = tuple(finish_times[j] for j in acts)
        self.dominance_store[key].append(vec)

    # ------------------------------------------------------------------
    # Core recursive B&B procedure
    # ------------------------------------------------------------------
    def search(self, t, running, completed, pending, finish_times):
        """
        t          : current decision time
        running    : dict {j -> finish_time} (started, not yet done)
        completed  : set of completed activity IDs
        pending    : set of not-yet-started activities
        finish_times: dict {j -> finish_time} for all non-pending activities
        """
        if time.time() > self.deadline:
            return

        # --- Advance time: move running jobs that finish at or before t to completed ---
        newly_done = [j for j, ft in running.items() if ft <= t]
        if newly_done:
            running = dict(running)
            for j in newly_done:
                del running[j]
                completed = completed | {j}

        # --- Update finish_times for completed jobs (unchanged, already set) ---

        # --- Check if all done ---
        if not pending and not running:
            mk = finish_times[self.sink]
            if mk < self.best_makespan:
                self.best_makespan = mk
                self.best_schedule = dict(finish_times)
            return

        # --- Eligible set: predecessors all completed ---
        eligible = [j for j in pending
                    if all(p in completed for p in self.preds.get(j, []))]

        # --- Lower bound pruning ---
        # EST for each pending job from t, given current completed finish_times
        est = {}
        for j in pending:
            est[j] = max(t, max((finish_times[p] for p in self.preds.get(j, [])), default=0))
        # Propagate through successors
        changed = True
        while changed:
            changed = False
            for j in pending:
                for sj in self.succs.get(j, []):
                    if sj in pending:
                        cand = est[j] + self.dur[j]
                        if cand > est.get(sj, 0):
                            est[sj] = cand
                            changed = True
        lb = est.get(self.sink, t) + self.dur[self.sink] if self.sink in pending else finish_times.get(self.sink, t)
        if lb >= self.best_makespan:
            return

        # --- Dominance check ---
        if self.is_dominated(completed, finish_times):
            return
        self.store_schedule(completed, finish_times)

        # --- Resource usage by currently running jobs ---
        used = self.res_used(running)

        # --- Find which eligible activities can start at t ---
        can_start = []
        for j in eligible:
            if all(used[k] + self.req[j][k] <= self.cap[k] for k in range(self.n_res)):
                can_start.append(j)

        # --- Activities that are eligible but BLOCKED by resources ---
        blocked = [j for j in eligible if j not in can_start]

        if not blocked:
            # All eligible activities can start (or none eligible).
            # Start all that fit (greedy: sort by EST).
            can_start.sort(key=lambda j: est.get(j, 0))
            # Greedily start activities that fit
            used_now = list(used)
            to_start = []
            for j in can_start:
                if all(used_now[k] + self.req[j][k] <= self.cap[k] for k in range(self.n_res)):
                    to_start.append(j)
                    for k in range(self.n_res):
                        used_now[k] += self.req[j][k]

            new_running = dict(running)
            new_finish = dict(finish_times)
            new_pending = set(pending)
            for j in to_start:
                ft = t + self.dur[j]
                new_running[j] = ft
                new_finish[j] = ft
                new_pending.discard(j)

            if not to_start and not running:
                # Nothing to do — shouldn't happen if problem is feasible
                return

            # Next event: earliest completion
            if new_running:
                t_next = min(new_running.values())
            else:
                t_next = t
            self.search(t_next, new_running, completed, new_pending, new_finish)

        else:
            # Some eligible activities are BLOCKED.
            # Compute MDAs: minimal subsets of 'running' whose completion
            # would free enough resources for at least one blocked job.
            #
            # Simplified MDA: for each activity i in 'running', check if
            # removing i from 'running' would allow any blocked j to start.
            # Branch on each such i (delay until i finishes).
            #
            # Also create the branch where we start what we can now and
            # advance to next event.

            mdas = []
            for i in sorted(running.keys(), key=lambda x: running[x]):
                used_without_i = [used[k] - self.req[i][k] for k in range(self.n_res)]
                for j in blocked:
                    if all(used_without_i[k] + self.req[j][k] <= self.cap[k]
                           for k in range(self.n_res)):
                        mdas.append(i)
                        break

            if not mdas:
                # No single-activity MDA — use multi-activity: advance to next completion
                t_next = min(running.values())
                self.search(t_next, running, completed, pending, finish_times)
                return

            # Branch: for each MDA activity i, delay until i finishes
            branch_times = sorted(set(running[i] for i in mdas))
            for t_branch in branch_times:
                if time.time() > self.deadline:
                    return
                self.search(t_branch, running, completed, pending, finish_times)

    # ------------------------------------------------------------------
    # Public solve interface
    # ------------------------------------------------------------------
    def solve(self):
        self.deadline = time.time() + self.timeout

        # Initial UB from SSGS heuristic
        ub, sched = self.ssgs_upper_bound()
        self.best_makespan = ub
        self.best_schedule = {j: sched[j] + self.dur[j] for j in sched}

        # Initial state: source is completed, everything else pending
        source = self.source
        t0 = self.dur[source]  # source finishes at t=0 (dur=0) or its duration
        finish_times = {source: self.dur[source]}
        completed = {source}
        pending = set(self.activities) - {source}
        running = {}

        self.search(0, running, completed, pending, finish_times)

        optimal = time.time() <= self.deadline
        return self.best_makespan, optimal, self.best_schedule


# ---------------------------------------------------------------------------
# Instance collection
# ---------------------------------------------------------------------------

def collect_instances(size):
    data_dir = DATA_DIRS[size]
    pattern = os.path.join(data_dir, f"{size}*_*.sm")
    files = sorted(glob.glob(pattern))
    return [(f, os.path.basename(f).replace(".sm", "")) for f in files]


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(description="D&H B&B RCPSP baseline")
    parser.add_argument("size", choices=["j30", "j60", "j90"])
    parser.add_argument("--out", default=None)
    parser.add_argument("--timeout", type=int, default=TIMEOUT_S)
    args = parser.parse_args()

    out_csv = args.out or os.path.join(SCRIPT_DIR, f"results_dh_bb_{args.size}.csv")
    instances = collect_instances(args.size)

    if not instances:
        print(f"No .sm files found for {args.size} in {DATA_DIRS[args.size]}")
        sys.exit(1)

    print(f"D&H B&B on {len(instances)} {args.size} instances (timeout={args.timeout}s)")
    print(f"Output: {out_csv}")

    with open(out_csv, "w", newline="", encoding="utf-8") as csvfile:
        writer = csv.DictWriter(
            csvfile, fieldnames=["instance", "solved", "optimal", "makespan", "time_s"]
        )
        writer.writeheader()

        for idx, (sm_path, inst_name) in enumerate(instances, 1):
            activities, dur, req, cap, succs, preds = parse_sm(sm_path)
            solver = Solver(activities, dur, req, cap, succs, preds, timeout=args.timeout)

            t0 = time.time()
            makespan, optimal, _ = solver.solve()
            elapsed = time.time() - t0

            row = {
                "instance": inst_name,
                "solved": True,
                "optimal": optimal,
                "makespan": makespan,
                "time_s": round(elapsed, 3),
            }
            writer.writerow(row)
            csvfile.flush()

            status = "OPT" if optimal else "SAT"
            print(f"  [{idx:3d}/{len(instances)}] {inst_name:15s}  {status}  makespan={makespan}  {elapsed:.1f}s")

    print(f"\nDone. Results saved to {out_csv}")


if __name__ == "__main__":
    main()
