"""
Root-node LP lower bound for RCPSP vs the TT2 "max heuristic".

Bound-quality experiment (NOT a per-node search heuristic -- proven too costly).
Compares, at the ROOT state of each PSPLIB instance:
  * maxHeur   = max(CPM critical-path bound, volume/resource-load bound)
                -- exactly RCPSP.h:383 max(getForwardHcost, getforwardResource)
  * DT LP     = time-indexed relaxation, objective = start time of the sink
  * DT LP+cut = DT LP tightened by minimal-forbidden-set (MDA) cuts via separation

Backends: 'gurobi' (needs a real license; warm-started separation) or
          'scipy'  (HiGHS, free; rebuilds each round). Auto-detects.

See DESIGN / memory project_hseq_dt_lp_result for the why. Sound reframing:
the firing-count state-equation form is inadmissible for makespan (objective =
sum of durations); this is the admissible time-indexed relaxation instead.
"""
import re, time

# ----------------------------------------------------------------------------- parsing
def parse_sm(path):
    txt = open(path).read()
    n = int(re.search(r'jobs \(incl\.[^:]*:\s*(\d+)', txt).group(1))
    horizon = int(re.search(r'horizon\s*:\s*(\d+)', txt).group(1))
    nres = int(re.search(r'renewable\s*:\s*(\d+)\s*R', txt).group(1))

    # capacities: the integer line under RESOURCEAVAILABILITIES header row
    cap_block = txt.split('RESOURCEAVAILABILITIES:')[1].strip().splitlines()
    caps = [int(x) for x in cap_block[1].split()][:nres]

    succ = {i: [] for i in range(1, n + 1)}
    prec_block = txt.split('PRECEDENCE RELATIONS:')[1].split('***')[0]
    for line in prec_block.strip().splitlines()[1:]:
        p = line.split()
        if len(p) < 3 or not p[0].isdigit():
            continue
        job, nsucc = int(p[0]), int(p[2])
        succ[job] = [int(x) for x in p[3:3 + nsucc]]

    dur, dem = {}, {}
    req_block = txt.split('REQUESTS/DURATIONS:')[1].split('***')[0]
    for line in req_block.strip().splitlines():
        p = line.split()
        if len(p) < 3 + nres or not p[0].isdigit():
            continue
        job = int(p[0])
        dur[job] = int(p[2])
        dem[job] = [int(x) for x in p[3:3 + nres]]
    return dict(n=n, horizon=horizon, caps=caps, nres=nres, succ=succ, dur=dur, dem=dem)

# ----------------------------------------------------------------------------- max heuristic
def cpm(inst):
    """critical-path forward pass = getForwardHcost at the root."""
    n, succ, dur = inst['n'], inst['succ'], inst['dur']
    pred = {i: [] for i in range(1, n + 1)}
    for p in succ:
        for s in succ[p]:
            pred[s].append(p)
    ES = {}
    for a in range(1, n + 1):                      # ids are topologically sorted in PSPLIB
        ES[a] = max([ES[p] + dur[p] for p in pred[a]], default=0)
    cp = max(ES[a] + dur[a] for a in range(1, n + 1))
    return cp, ES, pred

def latest(inst, deadline, ES):
    n, succ, dur = inst['n'], inst['succ'], inst['dur']
    LS = {}
    for a in range(n, 0, -1):
        LF = min([LS[s] for s in succ[a]], default=deadline)
        LS[a] = LF - dur[a]
    return LS

def volume_bound(inst):
    """max_r sum_a dur_a*dem_ar / cap_r = getforwardResource at the root."""
    best = 0.0
    for r in range(inst['nres']):
        tot = sum(inst['dur'][a] * inst['dem'][a][r] for a in range(1, inst['n'] + 1))
        best = max(best, tot / inst['caps'][r])
    return best

# ----------------------------------------------------------------------------- MDA cuts
def trans_closure(inst):
    n, succ = inst['n'], inst['succ']
    reach = {a: set() for a in range(1, n + 1)}
    for a in range(n, 0, -1):
        for s in succ[a]:
            reach[a].add(s); reach[a] |= reach[s]
    return reach

def minimal_forbidden_sets(inst, reach, maxsize=3):
    """minimal activity sets whose joint demand exceeds a resource cap (MDAs)."""
    n, dem, caps, nres, dur = (inst['n'], inst['dem'], inst['caps'], inst['nres'], inst['dur'])
    acts = [a for a in range(1, n + 1) if dur[a] > 0]
    ordered = lambda a, b: b in reach[a] or a in reach[b]
    fpairs, fs = set(), set()
    for i in range(len(acts)):
        for j in range(i + 1, len(acts)):
            a, b = acts[i], acts[j]
            if ordered(a, b):
                continue
            for r in range(nres):
                if dem[a][r] + dem[b][r] > caps[r]:
                    fp = frozenset((a, b)); fs.add(fp); fpairs.add(fp); break
    if maxsize >= 3:
        for i in range(len(acts)):
            for j in range(i + 1, len(acts)):
                for k in range(j + 1, len(acts)):
                    a, b, c = acts[i], acts[j], acts[k]
                    if ordered(a, b) or ordered(a, c) or ordered(b, c):
                        continue
                    if any(frozenset(p) in fpairs for p in ((a, b), (a, c), (b, c))):
                        continue
                    for r in range(nres):
                        if dem[a][r] + dem[b][r] + dem[c][r] > caps[r]:
                            fs.add(frozenset((a, b, c))); break
    return fs

# ----------------------------------------------------------------------------- model scaffold
def _windows(inst):
    n, dur = inst['n'], inst['dur']
    H = inst['horizon']
    cp, ES, _ = cpm(inst)
    LS = latest(inst, H, ES)
    win = {a: range(ES[a], LS[a] + 1) for a in range(1, n + 1)}
    ai = {a: (ES[a], LS[a] + dur[a] - 1) for a in range(1, n + 1)}   # active interval
    return win, ai, cp

def _sep_round(inst, win, ai, fsets, xval, added):
    """return list of new violated forbidden-set cuts as (frozenset F, t)."""
    dur = inst['dur']
    new = []
    for F in fsets:
        lo = max(ai[a][0] for a in F); hi = min(ai[a][1] for a in F)
        for t in range(lo, hi + 1):
            if (F, t) in added:
                continue
            s = 0.0
            for a in F:
                for tau in win[a]:
                    if tau <= t <= tau + dur[a] - 1:
                        s += xval[(a, tau)]
            if s > len(F) - 1 + 1e-6:
                new.append((F, t)); added.add((F, t))
    return new

# ----------------------------------------------------------------------------- gurobi backend
def _solve_gurobi(inst, cuts, maxsize, max_rounds, timelimit):
    import gurobipy as gp
    from gurobipy import GRB
    n, succ, dur, dem, caps, nres = (inst['n'], inst['succ'], inst['dur'],
                                     inst['dem'], inst['caps'], inst['nres'])
    H = inst['horizon']
    win, ai, cp = _windows(inst)
    m = gp.Model(); m.Params.OutputFlag = 0; m.Params.Threads = 1
    if timelimit: m.Params.TimeLimit = timelimit
    x = {(a, t): m.addVar(lb=0.0, ub=1.0, obj=(t if a == n else 0.0))
         for a in range(1, n + 1) for t in win[a]}
    m.ModelSense = GRB.MINIMIZE
    for a in range(1, n + 1):
        m.addConstr(gp.quicksum(x[(a, t)] for t in win[a]) == 1)
    for p in range(1, n + 1):
        for s in succ[p]:
            m.addConstr(gp.quicksum(t * x[(s, t)] for t in win[s])
                        - gp.quicksum(t * x[(p, t)] for t in win[p]) >= dur[p])
    for r in range(nres):
        for t in range(0, H + 1):
            terms = [dem[a][r] * x[(a, tau)]
                     for a in range(1, n + 1) if dem[a][r] and dur[a]
                     for tau in win[a] if tau <= t <= tau + dur[a] - 1]
            if terms:
                m.addConstr(gp.quicksum(terms) <= caps[r])
    m.optimize()
    if m.Status not in (GRB.OPTIMAL, GRB.TIME_LIMIT) or m.SolCount == 0:
        return dict(lp=None, ncuts=0, nfs=0, status='infeasible')
    obj = m.ObjVal
    ncuts = nfs = 0
    if cuts:
        fsets = minimal_forbidden_sets(inst, trans_closure(inst), maxsize); nfs = len(fsets)
        added = set()
        for _ in range(max_rounds):
            xval = {k: v.X for k, v in x.items()}
            new = _sep_round(inst, win, ai, fsets, xval, added)
            if not new:
                break
            for F, t in new:
                m.addConstr(gp.quicksum(x[(a, tau)] for a in F for tau in win[a]
                                        if tau <= t <= tau + dur[a] - 1) <= len(F) - 1)
                ncuts += 1
            m.optimize()
            if m.SolCount == 0:
                break
            obj = m.ObjVal
    return dict(lp=obj, ncuts=ncuts, nfs=nfs, status='ok')

# ----------------------------------------------------------------------------- scipy backend
def _solve_scipy(inst, cuts, maxsize, max_rounds, timelimit):
    import numpy as np
    from scipy.optimize import linprog
    from scipy.sparse import lil_matrix, csr_matrix
    n, succ, dur, dem, caps, nres = (inst['n'], inst['succ'], inst['dur'],
                                     inst['dem'], inst['caps'], inst['nres'])
    H = inst['horizon']
    win, ai, cp = _windows(inst)
    idx = {}; col = 0
    for a in range(1, n + 1):
        for t in win[a]:
            idx[(a, t)] = col; col += 1
    nvar = col
    eq = [{idx[(a, t)]: 1.0 for t in win[a]} for a in range(1, n + 1)]
    b_eq = [1.0] * n
    ub, b_ub = [], []
    for p in range(1, n + 1):
        for s in succ[p]:
            row = {}
            for t in win[s]: row[idx[(s, t)]] = row.get(idx[(s, t)], 0) - t
            for t in win[p]: row[idx[(p, t)]] = row.get(idx[(p, t)], 0) + t
            ub.append(row); b_ub.append(-dur[p])
    for r in range(nres):
        for t in range(0, H + 1):
            row = {}
            for a in range(1, n + 1):
                w = dem[a][r]
                if not w or not dur[a]: continue
                for tau in win[a]:
                    if tau <= t <= tau + dur[a] - 1:
                        row[idx[(a, tau)]] = row.get(idx[(a, tau)], 0) + w
            if row: ub.append(row); b_ub.append(caps[r])
    c = np.zeros(nvar)
    for t in win[n]: c[idx[(n, t)]] = t

    def mat(rows):
        M = lil_matrix((len(rows), nvar))
        for i, rr in enumerate(rows):
            for j, v in rr.items(): M[i, j] = v
        return csr_matrix(M)

    A_eq = mat(eq); beq = np.array(b_eq, float)
    ncuts = nfs = 0; obj = None
    fsets = minimal_forbidden_sets(inst, trans_closure(inst), maxsize) if cuts else set()
    nfs = len(fsets); added = set()
    for _ in range(max_rounds + 1):
        res = linprog(c, A_ub=mat(ub), b_ub=np.array(b_ub, float),
                      A_eq=A_eq, b_eq=beq, bounds=[(0, 1)] * nvar, method='highs')
        if not res.success:
            return dict(lp=obj, ncuts=ncuts, nfs=nfs,
                        status='ok' if obj is not None else 'infeasible')
        obj = res.fun
        if not cuts:
            break
        xval = {k: res.x[v] for k, v in idx.items()}
        new = _sep_round(inst, win, ai, fsets, xval, added)
        if not new:
            break
        for F, t in new:
            row = {}
            for a in F:
                for tau in win[a]:
                    if tau <= t <= tau + dur[a] - 1:
                        row[idx[(a, tau)]] = row.get(idx[(a, tau)], 0) + 1.0
            ub.append(row); b_ub.append(len(F) - 1); ncuts += 1
    return dict(lp=obj, ncuts=ncuts, nfs=nfs, status='ok')

# ----------------------------------------------------------------------------- public
def available_backend(prefer='auto'):
    if prefer in ('gurobi', 'auto'):
        try:
            import gurobipy  # noqa
            # verify a license actually acquires (token server may be down)
            import gurobipy as gp
            e = gp.Env(empty=True); e.setParam('OutputFlag', 0); e.start(); e.dispose()
            return 'gurobi'
        except Exception:
            if prefer == 'gurobi':
                raise
    return 'scipy'

def solve_dt(inst, backend='scipy', cuts=True, maxsize=3, max_rounds=8, timelimit=0):
    t0 = time.perf_counter()
    fn = _solve_gurobi if backend == 'gurobi' else _solve_scipy
    out = fn(inst, cuts, maxsize, max_rounds, timelimit)
    out['time'] = time.perf_counter() - t0
    out['backend'] = backend
    return out
