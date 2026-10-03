#!/usr/bin/env python3
"""
Run the root-node LP bound-quality experiment over a PSPLIB instance set.

  python run.py --data DIR --size j30 [--optima known_optima.csv]
                [--backend auto|gurobi|scipy] [--jobs N] [--subset all|2|1]
                [--cuts 1] [--maxsize 3] [--rounds 8] [--timelimit 0] [--out FILE]

Writes one CSV row per instance (incrementally, crash-safe) plus a summary to
stdout. CSV name self-identifies the experiment arm if --out is omitted.
"""
import os, sys, csv, glob, math, argparse, datetime, multiprocessing as mp
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import rcpsp_lp as R

def load_optima(path):
    opt = {}
    if path and os.path.exists(path):
        for r in csv.DictReader(open(path)):
            if r.get('optimum'):
                opt[r['instance']] = int(r['optimum'])
    return opt

def instance_name(path):
    return os.path.basename(path)[:-3]  # strip .sm

def param_of(name, size):
    # name like j30<param>_<inst>  -> strip leading size digits then split
    core = name[len(size):] if name.startswith(size) else name
    return core.split('_')[0]

def pick_subset(paths, size, mode):
    if mode == 'all':
        return paths
    keep = 1 if mode == '1' else 2
    by_param = {}
    for p in paths:
        by_param.setdefault(param_of(instance_name(p), size), []).append(p)
    out = []
    for param, ps in by_param.items():
        out += sorted(ps)[:keep]
    return sorted(out)

# worker ---------------------------------------------------------------------
_CFG = {}
def _init(cfg): _CFG.update(cfg)

def _work(path):
    name = instance_name(path)
    try:
        inst = R.parse_sm(path)
        cp, _, _ = R.cpm(inst)
        vb_raw = R.volume_bound(inst)
        vb = math.ceil(vb_raw)
        plain = R.solve_dt(inst, _CFG['backend'], cuts=False,
                           timelimit=_CFG['timelimit'])
        cutr = R.solve_dt(inst, _CFG['backend'], cuts=bool(_CFG['cuts']),
                          maxsize=_CFG['maxsize'], max_rounds=_CFG['rounds'],
                          timelimit=_CFG['timelimit']) if _CFG['cuts'] else plain
        lp_p = math.ceil(plain['lp'] - 1e-6) if plain['lp'] is not None else None
        lp_c = math.ceil(cutr['lp'] - 1e-6) if cutr['lp'] is not None else None
        return dict(instance=name, n=inst['n'], horizon=inst['horizon'],
                    cpm=cp, volume=vb, volume_raw=round(vb_raw, 3),
                    maxHeur=max(cp, vb),
                    lp_plain=lp_p, lp_plain_val=round(plain['lp'], 3) if plain['lp'] is not None else None,
                    lp_cuts=lp_c, lp_cuts_val=round(cutr['lp'], 3) if cutr['lp'] is not None else None,
                    ncuts=cutr['ncuts'], nforbidden=cutr['nfs'],
                    solve_sec=round(plain['time'] + cutr['time'], 2),
                    status=cutr['status'], error='')
    except Exception as e:
        return dict(instance=name, error=repr(e), status='error')

FIELDS = ['instance', 'size', 'param', 'n', 'horizon', 'opt', 'cpm', 'volume',
          'volume_raw', 'maxHeur', 'lp_plain', 'lp_plain_val', 'lp_cuts',
          'lp_cuts_val', 'ncuts', 'nforbidden', 'lp_beats_max', 'volume_binding',
          'closed_root', 'gap_max', 'gap_lp', 'solve_sec', 'status',
          'backend', 'cuts', 'maxsize', 'rounds', 'timelimit', 'error']

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--data', required=True, help='dir of .sm files')
    ap.add_argument('--size', required=True, help='j30 / j60 / j90 (name prefix)')
    ap.add_argument('--optima', default='known_optima.csv')
    ap.add_argument('--backend', default='auto', choices=['auto', 'gurobi', 'scipy'])
    ap.add_argument('--jobs', type=int, default=max(1, mp.cpu_count() - 1))
    ap.add_argument('--subset', default='all', help="all | 2 (per param) | 1 (per param)")
    ap.add_argument('--cuts', type=int, default=1)
    ap.add_argument('--maxsize', type=int, default=3)
    ap.add_argument('--rounds', type=int, default=8)
    ap.add_argument('--timelimit', type=float, default=0)
    ap.add_argument('--out', default='')
    args = ap.parse_args()

    backend = R.available_backend(args.backend)
    if args.backend == 'gurobi' and backend != 'gurobi':
        sys.exit('gurobi requested but license did not acquire')
    paths = pick_subset(sorted(glob.glob(os.path.join(args.data, '*.sm'))),
                        args.size, args.subset)
    if not paths:
        sys.exit(f'no .sm files under {args.data}')
    opt = load_optima(args.optima)
    stamp = datetime.datetime.now().strftime('%Y%m%d-%H%M%S')
    out = args.out or (f'lp_rootbound_{args.size}_{backend}_cuts{args.cuts}'
                       f'_sz{args.maxsize}_{args.subset}_{stamp}.csv')
    print(f'# backend={backend} jobs={args.jobs} instances={len(paths)} '
          f'subset={args.subset} cuts={args.cuts} maxsize={args.maxsize} -> {out}',
          flush=True)

    cfg = dict(backend=backend, cuts=args.cuts, maxsize=args.maxsize,
               rounds=args.rounds, timelimit=args.timelimit)
    rows = []
    with open(out, 'w', newline='') as f:
        w = csv.DictWriter(f, fieldnames=FIELDS); w.writeheader()
        pool = mp.Pool(args.jobs, initializer=_init, initargs=(cfg,))
        done = 0
        for res in pool.imap_unordered(_work, paths):
            done += 1
            name = res['instance']
            o = opt.get(name)
            res['size'] = args.size
            res['param'] = param_of(name, args.size)
            res['opt'] = o if o is not None else ''
            res['backend'] = backend; res['cuts'] = args.cuts
            res['maxsize'] = args.maxsize; res['rounds'] = args.rounds
            res['timelimit'] = args.timelimit
            if res.get('status') != 'error' and res.get('lp_cuts') is not None:
                mh, lpc = res['maxHeur'], res['lp_cuts']
                res['lp_beats_max'] = int(lpc > mh)
                res['volume_binding'] = int(res['volume'] > res['cpm'])
                res['closed_root'] = int(o is not None and max(mh, lpc) == o and mh < o)
                res['gap_max'] = (o - mh) if o is not None else ''
                res['gap_lp'] = (o - max(mh, lpc)) if o is not None else ''
            w.writerow({k: res.get(k, '') for k in FIELDS}); f.flush()
            rows.append(res)
            if done % 25 == 0 or done == len(paths):
                print(f'  {done}/{len(paths)}', flush=True)
        pool.close(); pool.join()
    summarize(rows, opt, out)

def summarize(rows, opt, out):
    ok = [r for r in rows if r.get('status') != 'error' and r.get('lp_cuts') is not None]
    errs = [r for r in rows if r.get('status') == 'error']
    N = len(ok)
    print(f'\n=== summary ({out}) ===')
    print(f'solved-LP: {N}   errors: {len(errs)}')
    if not N:
        for e in errs[:5]: print('  ERR', e['instance'], e.get('error'))
        return
    winp = sum(r['lp_plain'] > r['maxHeur'] for r in ok if r['lp_plain'] is not None)
    winc = sum(r['lp_cuts'] > r['maxHeur'] for r in ok)
    tie2win = sum((r['lp_plain'] is not None and r['lp_plain'] <= r['maxHeur']
                   and r['lp_cuts'] > r['maxHeur']) for r in ok)
    withopt = [r for r in ok if opt.get(r['instance']) is not None]
    def avg(xs): return sum(xs) / len(xs) if xs else float('nan')
    print(f'LP+cuts > maxHeur : {winc}/{N} ({100*winc/N:.1f}%)   plain LP: {winp}/{N} ({100*winp/N:.1f}%)')
    print(f'ties turned to wins by cuts: {tie2win}')
    if withopt:
        gm = avg([opt[r['instance']] - r['maxHeur'] for r in withopt])
        gc = avg([opt[r['instance']] - max(r['maxHeur'], r['lp_cuts']) for r in withopt])
        closed = sum(r.get('closed_root') == 1 for r in withopt)
        print(f'(over {len(withopt)} with known opt) avg gap-to-opt: '
              f'maxHeur {gm:.3f} -> max(maxHeur,LP+cuts) {gc:.3f}   (-{gm-gc:.3f})')
        print(f'newly proven optimal at root by cuts: {closed}')
    # complementarity: where do wins land?
    wins = [r for r in ok if r['lp_cuts'] > r['maxHeur']]
    if wins:
        volweak = sum(r['volume'] <= r['cpm'] for r in wins)
        print(f'complementarity: {volweak}/{len(wins)} wins are volume-weak '
              f'(CPM-binding) -> LP adds resource-timing info orthogonal to volume')
        if withopt:
            gw = avg([opt[r['instance']] - r['maxHeur'] for r in wins if opt.get(r['instance'])])
            print(f'  maxHeur gap-to-opt on win instances: {gw:.2f} (vs fleet {gm:.2f}) '
                  f"-> fires where the bound is {'LOOSE' if gw>gm else 'tight'}")
    print(f'avg solve time/instance: {avg([r["solve_sec"] for r in ok]):.2f}s')

if __name__ == '__main__':
    main()
