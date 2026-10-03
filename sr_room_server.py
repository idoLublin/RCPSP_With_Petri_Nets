#!/usr/bin/env python3
"""SR room at ROOT vs maxLB, on the maxLB-unsolved sets (embedded below).
Runs j60 + j90 in parallel via a process pool; exact single-res per resource,
300 s/solve, 1 worker/solve (pool gives the parallelism). Resumable: appends to
OUT_CSV and skips instances already there. Prints a summary at the end.

USAGE (server):
  python3 sr_room_server.py --j60dir ~/lber_run/json_outputs_j60 \
                            --j90dir ~/lber_run/json_outputs_j90 \
                            [--out sr_room.csv] [--procs N]
Reads  <dir>/<size><g>_<x>/rcpsp.json  (PSPLIB-as-JSON: activities + edges + caps).
Needs: pip install ortools
"""
import os, csv, math, time, json, argparse, multiprocessing as mp
from collections import deque
from ortools.sat.python import cp_model

J60_UNSOLVED = [(1,2),(1,4),(1,5),(1,6),(1,7),(1,9),(1,10),(2,3),(2,5),(2,6),(2,7),(3,3),(3,7),(3,8),(3,9),(5,1),(5,2),(5,3),(5,4),(5,5),(5,6),(5,7),(5,8),(5,9),(5,10),(6,1),(6,2),(6,3),(6,6),(6,7),(9,1),(9,2),(9,3),(9,4),(9,5),(9,6),(9,7),(9,8),(9,9),(9,10),(10,7),(10,8),(10,9),(13,1),(13,2),(13,3),(13,4),(13,5),(13,6),(13,7),(13,8),(13,9),(13,10),(14,1),(14,3),(14,4),(14,9),(14,10),(17,3),(17,8),(21,3),(21,5),(21,9),(22,4),(22,10),(25,1),(25,2),(25,3),(25,4),(25,5),(25,6),(25,7),(25,8),(25,9),(25,10),(26,3),(26,4),(26,6),(26,9),(29,1),(29,2),(29,3),(29,4),(29,5),(29,6),(29,7),(29,8),(29,9),(29,10),(30,2),(30,3),(30,5),(30,7),(30,10),(37,6),(38,2),(41,2),(41,3),(41,5),(41,9),(41,10),(45,1),(45,2),(45,3),(45,4),(45,5),(45,6),(45,7),(45,8),(45,9),(45,10),(46,3),(46,4),(46,5),(46,7),(46,9)]
J90_UNSOLVED = [(1,1),(1,2),(1,3),(1,4),(1,5),(1,6),(1,7),(1,8),(1,9),(1,10),(2,9),(3,10),(5,1),(5,2),(5,3),(5,4),(5,5),(5,6),(5,7),(5,8),(5,9),(5,10),(6,3),(6,5),(6,8),(6,10),(9,1),(9,2),(9,3),(9,4),(9,5),(9,6),(9,7),(9,8),(9,9),(9,10),(10,8),(10,10),(13,1),(13,2),(13,3),(13,4),(13,5),(13,6),(13,7),(13,8),(13,9),(13,10),(14,6),(17,1),(17,2),(17,3),(17,4),(17,6),(17,7),(17,8),(17,9),(17,10),(19,4),(21,1),(21,2),(21,3),(21,4),(21,5),(21,6),(21,7),(21,8),(21,9),(21,10),(22,3),(22,9),(22,10),(25,1),(25,2),(25,3),(25,4),(25,5),(25,6),(25,7),(25,8),(25,9),(25,10),(26,2),(26,4),(26,5),(26,7),(26,8),(29,1),(29,2),(29,3),(29,4),(29,5),(29,6),(29,7),(29,8),(29,9),(29,10),(30,3),(30,5),(30,7),(30,9),(30,10),(33,1),(33,2),(33,3),(33,4),(33,5),(33,7),(33,9),(33,10),(34,3),(34,4),(34,5),(34,8),(37,1),(37,2),(37,3),(37,4),(37,5),(37,6),(37,7),(37,8),(37,9),(37,10),(38,1),(38,2),(38,3),(38,5),(38,6),(38,9),(41,1),(41,2),(41,3),(41,4),(41,5),(41,6),(41,7),(41,8),(41,9),(41,10),(42,2),(42,3),(42,6),(42,7),(42,9),(42,10),(45,1),(45,2),(45,3),(45,4),(45,5),(45,6),(45,7),(45,8),(45,9),(45,10),(46,1),(46,3),(46,4),(46,5),(46,6),(46,7),(46,8),(46,9)]

# Real base heuristic (rootH = getForwardHcost/getforwardResource) our solver used at the
# root, per unsolved instance — the DYNAMIC baseline to compare exact-SR against (not a
# static Python max(CP,RC)). Extracted from VAR_tt2_sr_tt2_{j60,j90}.csv.
ROOTH60 = {"1_2":65,"1_4":79,"1_5":68,"1_6":52,"1_7":60,"1_9":75,"1_10":76,"2_3":77,"2_5":53,"2_6":61,"2_7":49,"3_3":102,"3_7":58,"3_8":52,"3_9":63,"5_1":59,"5_2":77.25,"5_3":59,"5_4":56.4286,"5_5":82.6,"5_6":64,"5_7":59.1333,"5_8":65,"5_9":82,"5_10":75.7368,"6_1":60,"6_2":66,"6_3":72,"6_6":53,"6_7":60,"9_1":78.3684,"9_2":71,"9_3":83.1176,"9_4":68.7647,"9_5":75.2222,"9_6":90,"9_7":89.4737,"9_8":80.0588,"9_9":85.2778,"9_10":80.4,"10_7":69,"10_8":64,"10_9":73,"13_1":101.5,"13_2":99.6842,"13_3":80.3333,"13_4":88.7083,"13_5":88.3,"13_6":84.6,"13_7":76.3462,"13_8":102.353,"13_9":90.4762,"13_10":108.55,"14_1":59,"14_3":61,"14_4":65,"14_9":61,"14_10":67,"17_3":79,"17_8":66,"21_3":66.2857,"21_5":75,"21_9":69,"22_4":64,"22_10":69,"25_1":92.2,"25_2":76.0556,"25_3":94.6875,"25_4":84.5294,"25_5":77.4118,"25_6":91.9474,"25_7":75.25,"25_8":82.5625,"25_9":77.3529,"25_10":85.9412,"26_3":71,"26_4":65,"26_6":73,"26_9":62,"29_1":90.0476,"29_2":103.188,"29_3":105.75,"29_4":108.947,"29_5":93.3158,"29_6":139.188,"29_7":107.167,"29_8":84.2273,"29_9":93.4444,"29_10":106.136,"30_2":65,"30_3":80,"30_5":72,"30_7":77,"30_10":81,"37_6":78.6667,"38_2":71,"41_2":85,"41_3":78.6471,"41_5":92.8125,"41_9":102.6,"41_10":97.1765,"45_1":83.9474,"45_2":117.611,"45_3":118.125,"45_4":92.2941,"45_5":92.8333,"45_6":115.412,"45_7":101.895,"45_8":109.278,"45_9":100.778,"45_10":96.6111,"46_3":79,"46_4":71,"46_5":82,"46_7":75,"46_9":60}
ROOTH90 = {"1_1":67,"1_2":88,"1_3":59,"1_4":76,"1_5":84,"1_6":61,"1_7":83,"1_8":85,"1_9":66,"1_10":87,"2_9":79,"3_10":65,"5_1":70.5238,"5_2":90,"5_3":76.1429,"5_4":90,"5_5":98.7647,"5_6":75,"5_7":96.5,"5_8":82.8889,"5_9":95.8421,"5_10":87.65,"6_3":75,"6_5":71,"6_8":67,"6_10":94,"9_1":90,"9_2":112.947,"9_3":94.4762,"9_4":101.85,"9_5":121.579,"9_6":103.85,"9_7":100.636,"9_8":107.278,"9_9":101.773,"9_10":99.4091,"10_8":81,"10_10":75,"13_1":120.55,"13_2":110.958,"13_3":101.692,"13_4":104.667,"13_5":106.217,"13_6":112.769,"13_7":110.043,"13_8":108.571,"13_9":113.545,"13_10":109.84,"14_6":76,"17_1":78,"17_2":87,"17_3":86,"17_4":92,"17_6":91,"17_7":73,"17_8":101,"17_9":80,"17_10":81,"19_4":77,"21_1":90.625,"21_2":93,"21_3":101,"21_4":90,"21_5":90.4706,"21_6":87,"21_7":91,"21_8":91.5,"21_9":91,"21_10":87.625,"22_3":83,"22_9":97,"22_10":75,"25_1":104.429,"25_2":112.043,"25_3":104.667,"25_4":121.263,"25_5":104.667,"25_6":105.619,"25_7":113.3,"25_8":127,"25_9":89.15,"25_10":109.579,"26_2":85,"26_4":96,"26_5":82,"26_7":82,"26_8":82,"29_1":118.944,"29_2":115.826,"29_3":128.895,"29_4":133.762,"29_5":109.296,"29_6":109.154,"29_7":153.444,"29_8":142.35,"29_9":111.792,"29_10":108.917,"30_3":102,"30_5":83,"30_7":84,"30_9":82,"30_10":90,"33_1":82,"33_2":107,"33_3":105,"33_4":85,"33_5":106,"33_7":94,"33_9":86,"33_10":113,"34_3":82,"34_4":76,"34_5":80,"34_8":78,"37_1":87.1111,"37_2":96.375,"37_3":115,"37_4":94.8333,"37_5":94.8,"37_6":98.1667,"37_7":103.438,"37_8":94.6,"37_9":106,"37_10":97.9375,"38_1":83,"38_2":77,"38_3":88,"38_5":84,"38_6":88,"38_9":95,"41_1":122.045,"41_2":133.75,"41_3":131.353,"41_4":132,"41_5":112.5,"41_6":111.222,"41_7":133.556,"41_8":136.75,"41_9":102.682,"41_10":135.75,"42_2":92,"42_3":94,"42_6":89,"42_7":83,"42_9":82,"42_10":85,"45_1":137.19,"45_2":130.905,"45_3":135.167,"45_4":116.261,"45_5":160.55,"45_6":153.316,"45_7":123.391,"45_8":143.2,"45_9":133.6,"45_10":145.35,"46_1":103,"46_3":112,"46_4":88,"46_5":91,"46_6":83,"46_7":89,"46_8":87,"46_9":74.1351}

def parse_json(path):
    dd=json.load(open(path,encoding='utf-8'))
    acts=[x for x in dd if isinstance(x,dict) and 'duration' in x]
    edges=[x for x in dd if isinstance(x,list)]
    caps=[x for x in dd if isinstance(x,dict) and 'R1' in x and 'duration' not in x]
    idx={a['name']:i for i,a in enumerate(acts)}
    p=[int(a['duration']) for a in acts]
    u=[[int(a.get('resource_demands',{}).get(f'R{k+1}',0)) for k in range(4)] for a in acts]
    c=[int(caps[-1][f'R{k+1}']) for k in range(4)]
    e=[[idx[a],idx[b]] for a,b in edges[-1]]
    return p,u,e,c

def cp_lb(p,e,n):
    succ=[[] for _ in range(n)]; ind=[0]*n
    for a,b in e: succ[a].append(b); ind[b]+=1
    ef=[p[i] for i in range(n)]; q=deque(i for i in range(n) if ind[i]==0)
    while q:
        x=q.popleft()
        for v in succ[x]:
            if ef[x]+p[v]>ef[v]: ef[v]=ef[x]+p[v]
            ind[v]-=1
            if ind[v]==0: q.append(v)
    return max(ef)
def rc_lb(p,u,c): return max((math.ceil(sum(p[i]*u[i][k] for i in range(len(p)))/c[k]) for k in range(len(c)) if c[k]>0),default=0)
def sgs(p,u,c,e,n):
    succ=[[] for _ in range(n)]; ind=[0]*n; pred=[[] for _ in range(n)]
    for a,b in e: succ[a].append(b); ind[b]+=1; pred[b].append(a)
    H=sum(p)+1; usg=[[0]*len(c) for _ in range(H+1)]; fin=[0]*n
    q=deque(i for i in range(n) if ind[i]==0); order=[]
    while q:
        x=q.popleft(); order.append(x)
        for v in succ[x]:
            ind[v]-=1
            if ind[v]==0: q.append(v)
    for i in order:
        t=max((fin[a] for a in pred[i]),default=0)
        while any(usg[tt][r]+u[i][r]>c[r] for tt in range(t,t+p[i]) for r in range(len(c))): t+=1
        fin[i]=t+p[i]
        for tt in range(t,t+p[i]):
            for r in range(len(c)): usg[tt][r]+=u[i][r]
    return max(fin)
def sr_exact(p,u,c,e,H):
    best=0; proven=True
    for k in range(len(c)):
        n=len(p); m=cp_model.CpModel(); s=[m.NewIntVar(0,H,f"s{i}") for i in range(n)]; iv=[]; d=[]
        for i in range(n):
            if p[i]>0 and u[i][k]>0:
                iv.append(m.NewIntervalVar(s[i],p[i],m.NewIntVar(0,H,f"e{i}"),f"v{i}")); d.append(u[i][k])
        if iv: m.AddCumulative(iv,d,c[k])
        for a,b in e: m.Add(s[b]>=s[a]+p[a])
        ms=m.NewIntVar(0,H,"ms")
        for i in range(n): m.Add(ms>=s[i]+p[i])
        m.Minimize(ms)
        so=cp_model.CpSolver(); so.parameters.max_time_in_seconds=300.0; so.parameters.num_search_workers=1
        st=so.Solve(m)
        val=int(round(so.ObjectiveValue())) if st==cp_model.OPTIMAL else int(math.floor(so.BestObjectiveBound()))
        best=max(best,val); proven=proven and (st==cp_model.OPTIMAL)
    return best,proven

def work(job):
    size,g,x,d=job
    path=os.path.join(d,f"{size}{g}_{x}","rcpsp.json")
    if not os.path.exists(path): return (size,g,x,-1,-1,-1,"missing")
    base=(ROOTH60 if size=="j60" else ROOTH90).get(f"{g}_{x}")   # dynamic baseline = real rootH we use
    if base is None: return (size,g,x,-1,-1,-1,"no_rootH")
    try:
        p,u,e,c=parse_json(path)
        H=min(sgs(p,u,c,e,len(p)),sum(p))
        t0=time.time(); srv,pr=sr_exact(p,u,c,e,H)
        return (size,g,x,f"{base:g}",srv,int(pr),f"{time.time()-t0:.1f}")
    except Exception as ex:
        return (size,g,x,-1,-1,-1,str(ex)[:40])

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("--j60dir",required=True); ap.add_argument("--j90dir",required=True)
    ap.add_argument("--out",default="sr_room.csv"); ap.add_argument("--procs",type=int,default=max(1,mp.cpu_count()-1))
    a=ap.parse_args()
    done=set()
    if os.path.exists(a.out):
        for r in csv.DictReader(open(a.out)): done.add((r['size'],int(r['group']),int(r['exam'])))
    jobs=[("j60",g,x,a.j60dir) for (g,x) in J60_UNSOLVED if ("j60",g,x) not in done] \
        +[("j90",g,x,a.j90dir) for (g,x) in J90_UNSOLVED if ("j90",g,x) not in done]
    print(f"to do: {len(jobs)}  (procs={a.procs}, already done {len(done)})",flush=True)
    new=not os.path.exists(a.out)
    fh=open(a.out,"a",newline=""); w=csv.writer(fh)
    if new: w.writerow(["size","group","exam","base_rootH","SR","proven","secs"]); fh.flush()
    with mp.Pool(a.procs) as pool:
        for i,res in enumerate(pool.imap_unordered(work,jobs),1):
            w.writerow(res); fh.flush()
            if i%10==0: print(f"  {i}/{len(jobs)}",flush=True)
    fh.close()
    rows=[r for r in csv.DictReader(open(a.out))]
    for size in ("j60","j90"):
        rs=[r for r in rows if r['size']==size and r['SR']!='-1']
        imp=[r for r in rs if int(r['SR'])>float(r['base_rootH'])+1e-6]
        dz=sorted(int(r['SR'])-float(r['base_rootH']) for r in imp)
        print(f"\n{size}: unsolved={len(rs)}  SR>rootH(real base): {len(imp)} ({100*len(imp)/max(1,len(rs)):.0f}%)")
        if dz: print(f"   delta when better: mean {sum(dz)/len(dz):.1f}  median {dz[len(dz)//2]:.1f}  max {max(dz):.1f}")

if __name__=="__main__":
    main()
