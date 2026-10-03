#!/bin/bash
# ─────────────────────────────────────────────────────────────────────────────
# FORWARD TT2 CONTROL RUN — j30 + j60 + j90, CURRENT code, DR5 + DR4 + batch, 300s/instance,
# with per-phase profiling on (RCPSP_TT2_PROF=1).
#
# WHY: the forward j60/j90 results on disk come from OLD code (08-23 / 08-24: their CSVs lack 22
# columns the fixed backward run records) and the j90 one had batch OFF. The fixed backward run
# (output_tt2rev_j{60,90}_tt2dr5-batch_20260909-140811.csv) used DR5 + DR4 + batch, so this is its
# like-for-like forward control. It also fixes the HBAE target set: RS <= 0.5 instances that
# NEITHER direction solves.
#
# ARMS:  tt2_j60, tt2_j90 (forward); tt2_j30 + tt2rev_j30 always (minutes; profiled j30 fwd/bwd pair).
#        WITH_BWD=1 also reruns tt2rev_j60 / tt2rev_j90 with this same binary (strictly same code,
#        profiled too; +2 processes).
# WALL TIME: roughly 12-16 h. The old j90 forward averaged ~120 s per instance and the benchmark
#        loop cannot be split across processes yet.
# PROFILING COST: ~6-12% (measured on j30; expansions identical with it on and off).
#
# RECORDED PER INSTANCE (appended TT2 CSV columns, see writeTT2ProfCols in Driver.cpp):
#   profAstarSec profSuccSec profAvailSec profGenSec profDR5Sec profHashSec profHeurCPSec profHeurResSec
#   (+ existing heurTimeSec); call counts; childrenRaw/Kept (DR5 cut); hCallsAll/hKnownHits/hDeltaZeroHits;
#   domBuckets domComparisons; openAtEnd itemsAtEnd uniqueExpanded; maxFExpanded (proven lower bound)
#   and fAt1s/10s/60s/120s/240s; timedOut.   Trace: logs/ctl_trace_<arm>.csv, a snapshot every 5 s.
#
# UPLOAD FIRST: HOG2/RCPSP: Driver.cpp RCPSP.h Globals.h (profiling) + DominanceTT2.h RCPSPState.cpp
#               RCPSPState.h SubsetSolver.h BAE_RCPSP.h run_server_tt2_control.sh
#               repo root: known_optima.csv (already there from the HBAE run)
#
# OUTPUT: all CSVs in $ROOT/new_results, named <solver>_<size>_<info>_vNN.csv, e.g.
#         TT2fwd_j60_dr5-dr4-batch-prof_v01.csv, TT2bwd_j30_dr5-dr4-batch-prof_v01.csv;
#         targets_j60_rs_le_0.5.txt / targets_j90_rs_le_0.5.txt ("group exam" per line).
#
# RE-RUN ONLY THE ANALYSIS:
#   ANALYZE_ONLY=1 [F30=<fwd30> B30=<bwd30>] F60=<fwd60> F90=<fwd90> [B60=<bwd60> B90=<bwd90>] [OLD60=<old fwd60> OLD90=<old fwd90>] \
#     bash run_server_tt2_control.sh
# ─────────────────────────────────────────────────────────────────────────────
set -u
ROOT=${ROOT:-~/RCPSP_With_Petri_nets}
RD="$ROOT/HOG2/RCPSP"
OPT=${OPT:-"$ROOT/known_optima.csv"}
NR=${NR:-"$ROOT/new_results"}   # every CSV of every size lands here (RCPSP_RESULTS_DIR, set below)
L="$RD/logs"
F30=${F30:-}; B30=${B30:-}; F60=${F60:-}; F90=${F90:-}; B60=${B60:-}; B90=${B90:-}; OLD60=${OLD60:-}; OLD90=${OLD90:-}

gate(){ # label size csv — every solved row against the known optimum
  [ -f "$3" ] || { echo "  $1: no CSV"; return; }
  awk -F, -v LBL="$1" -v SZ="$2" 'NR==FNR{ if($2==SZ) o[$3","$4]=$5; next }
    FNR==1{ for(i=1;i<=NF;i++) c[$i]=i; if(!("makespan" in c) || !("solved" in c)){ bad=1; exit }; next }
    { n++; k=$1","$2; if(!($(c["solved"])=="True" || $(c["solved"])=="1")) next; p++
      if(!(k in o)){ unv++; next }
      if($(c["makespan"])+0 != o[k]+0){ w++; if(w<=5) printf "    WRONG %s g%s e%s got=%s opt=%s\n", LBL, $1, $2, $(c["makespan"]), o[k] } }
    END{ if(bad){ printf "  %-12s missing solved/makespan column -- CANNOT GATE\n", LBL; exit }
         printf "  %-12s rows=%d solved=%d WRONG=%d unverifiable(no known optimum)=%d\n", LBL, n, p, w+0, unv+0 }' "$OPT" "$3"; }

cov(){ # label csv — solved by RS
  [ -f "$2" ] || return
  awk -F, -v LBL="$1" 'NR==1{ for(i=1;i<=NF;i++) c[$i]=i; next }
    { n++; r=($1-1)%4; if($(c["solved"])=="True" || $(c["solved"])=="1"){ S[r]++; tot++ }; t+=$(c["time"]) }
    END{ if(n) printf "  %-12s rows=%d solved=%d   RS 0.2/0.5/0.7/1.0 = %d/%d/%d/%d   mean time=%.1fs\n", LBL, n, tot, S[0], S[1], S[2], S[3], t/n }' "$2"; }

comp(){ # size fwd.csv bwd.csv — complementarity by RS, and the HBAE target list
  [ -f "$2" ] && [ -f "$3" ] || { echo "  $1: need both forward and backward CSVs"; return; }
  TGT="$NR/targets_${1}_rs_le_0.5.txt"; : > "$TGT"
  awk -F, -v SZ="$1" -v TGT="$TGT" '
    NR==FNR{ if($2==SZ) o[$3","$4]=1; next }
    FNR==1{ fi++; delete c; for(i=1;i<=NF;i++) c[$i]=i; next }
    { k=$1","$2; seen[k]=1; s=($(c["solved"])=="True" || $(c["solved"])=="1"); t=$(c["time"])+0
      if(fi==1){ F[k]=s; FT[k]=t } else { B[k]=s; BT[k]=t } }
    END{ split("0.2 0.5 0.7 1.0", R, " ")
         printf "    RS    fwd  bwd  both  fwd-only  bwd-only  neither  union | neither w/ known opt | both solved: mean time fwd / bwd, bwd faster\n"
         for(r=0;r<4;r++){ f=b=bo=fo=ob=ne=kn=tf=tb=bf=0
           for(k in seen){ split(k, a, ","); if((a[1]-1)%4 != r) continue
             if(F[k]) f++; if(B[k]) b++
             if(F[k] && B[k]){ bo++; tf+=FT[k]; tb+=BT[k]; if(BT[k]<FT[k]) bf++ }
             if(F[k] && !B[k]) fo++; if(!F[k] && B[k]) ob++
             if(!F[k] && !B[k]){ ne++; if(k in o) kn++; if(r<=1) print a[1], a[2] >> TGT } }
           tm = bo ? sprintf("%.1fs / %.1fs, %d/%d", tf/bo, tb/bo, bf, bo) : "-"
           printf "    %-4s %4d %4d %5d %9d %9d %8d %6d | %20d | %s\n", R[r+1], f, b, bo, fo, ob, ne, f+ob, kn, tm } }' "$OPT" "$2" "$3"
  sort -n -k1,1 -k2,2 -o "$TGT" "$TGT"
  echo "    target list: $(wc -l < "$TGT") instances (RS <= 0.5, unsolved by both) -> $TGT"; }

drift(){ # label old.csv new.csv — did the forward search itself change since August?
  awk -F, -v LBL="$1" '
    function v(k){ return (k in c) ? $(c[k]) : "?" }
    FNR==1{ fi++; delete c; for(i=1;i<=NF;i++) c[$i]=i; next }
    FNR==2{ fl[fi]="dr5=" v("useTT2DR5") " dr4=" v("useTT2DR4") " batch=" v("useTT2Batch") }
    { k=$1","$2; s=(v("solved")=="True" || v("solved")=="1")
      if(fi==1){ OS[k]=s; OE[k]=v("expandNumber"); OT[k]=v("time") } else { NS[k]=s; NE[k]=v("expandNumber"); NTM[k]=v("time") } }
    END{ for(k in NS){ if(NS[k]) ns++; if(OS[k]) os++; if(NS[k]&&!OS[k]) gain++; if(!NS[k]&&OS[k]) loss++
                       if(NS[k]&&OS[k]){ n++; if(NE[k]==OE[k]) same++; oe+=OE[k]; nex+=NE[k]; ot+=OT[k]; nt+=NTM[k] } }
         printf "  %s  old[%s] solved=%d   new[%s] solved=%d   new-only=%d old-only=%d\n", LBL, fl[1], os, fl[2], ns, gain+0, loss+0
         if(fl[1]!=fl[2]) printf "    flags differ -> NOT a clean code-drift test\n"
         if(n) printf "    both solved %d: identical expansions on %d (%.0f%%); total expanded old=%.0f new=%.0f; total time old=%.0fs new=%.0fs\n", n, same+0, 100*same/n, oe, nex, ot, nt }' "$2" "$3"; }

prof(){ # label csv — where the A* time goes, by solved/unsolved and RS
  [ -f "$2" ] || return
  head -1 "$2" | grep -q profAstarSec || { echo "  $1: no profiling columns (older binary)"; return; }
  awk -F, -v LBL="$1" '
    function v(k){ return (k in c) ? $(c[k])+0 : 0 }
    function add(k){ n[k]++; A[k]+=v("profAstarSec"); S[k]+=v("profSuccSec"); AV[k]+=v("profAvailSec"); G[k]+=v("profGenSec")
      D[k]+=v("profDR5Sec"); H[k]+=v("heurTimeSec"); CP[k]+=v("profHeurCPSec"); HR[k]+=v("profHeurResSec"); HS[k]+=v("profHashSec")
      E[k]+=v("expandNumber"); CR[k]+=v("childrenRaw"); CK[k]+=v("childrenKept"); HA[k]+=v("hCallsAll"); DZ[k]+=v("hDeltaZeroHits")
      OP[k]+=v("openAtEnd"); if(v("maxMem")>MX[k]) MX[k]=v("maxMem") }
    NR==1{ for(i=1;i<=NF;i++) c[$i]=i; next }
    v("expandNumber")==0 { next }
    { r=($1-1)%4; st=($(c["solved"])=="True") ? "solved" : "unsolved"; add(st " RS=" r); add(st " all"); add("all") }
    END{ split("0.2 0.5 0.7 1.0", R, " ")
      printf "  %s — %% of A* wall time (rows where A* ran)\n", LBL
      printf "    %-18s %4s %8s | %5s %5s %5s | %5s %5s %5s | %5s %6s | %6s %6s %5s %9s %8s\n", "", "n", "A* sec", "avail", "gen", "DR5", "heur", "h-CP", "h-res", "hash", "open+", "us/exp", "DR5cut", "hDZ", "open@end", "RSS MB"
      for(q=0;q<3;q++){ st = (q==0) ? "solved" : (q==1) ? "unsolved" : ""
        for(r=-1;r<4;r++){
          if(q==2){ if(r!=-1) continue; k="all"; lab="ALL" } else { k = (r==-1) ? st " all" : st " RS=" r; lab = (r==-1) ? st " (all RS)" : st " RS=" R[r+1] }
          if(!n[k]) continue; a=(A[k]>0)?A[k]:1e-9
          printf "    %-18s %4d %8.0f | %5.1f %5.1f %5.1f | %5.1f %5.1f %5.1f | %5.1f %6.1f | %6.1f %5.0f%% %4.0f%% %9.0f %8.0f\n", lab, n[k], A[k],
                 100*AV[k]/a, 100*G[k]/a, 100*D[k]/a, 100*H[k]/a, 100*CP[k]/a, 100*HR[k]/a, 100*HS[k]/a, 100*(a-S[k]-H[k]-HS[k])/a,
                 1e6*A[k]/((E[k]>0)?E[k]:1), 100*(1-CK[k]/((CR[k]>0)?CR[k]:1)), 100*DZ[k]/((HA[k]>0)?HA[k]:1), OP[k]/n[k], MX[k]/1024 } }
      printf "    columns: avail/gen/DR5 are inside GetSuccessors; h-CP + h-res = the two heuristic parts (heur = their total)\n"
      printf "             open+ = A* minus successors, heuristic and hashing (open list, bookkeeping)\n"
      printf "             DR5cut = children removed by DR5; hDZ = heuristic calls answered by the zero-time shortcut\n"
      printf "             RSS MB = peak process RSS so far (cumulative over the benchmark process, not per instance)\n" }' "$2"; }

gap(){ # label size csv — how close did forward get on the instances it did NOT solve?
  [ -f "$3" ] || return
  head -1 "$3" | grep -q maxFExpanded || return
  awk -F, -v LBL="$1" -v SZ="$2" 'BEGIN{ split("fAt1s fAt10s fAt60s fAt120s fAt240s", FN, " ") }
    NR==FNR{ if($2==SZ) o[$3","$4]=$5; next }
    FNR==1{ for(i=1;i<=NF;i++) c[$i]=i; next }
    $(c["solved"])!="True" { k=$1","$2; r=($1-1)%4; u[r]++; if(!(k in o)){ nk[r]++; next }
      g=o[k]-$(c["maxFExpanded"]); if(g<=0) b0[r]++; else if(g==1) b1[r]++; else if(g==2) b2[r]++; else if(g<=5) b5[r]++; else bb[r]++
      for(j=3;j<=5;j++){ f=$(c[FN[j]])+0; if(f>=0){ sg[r,j]+=o[k]-f; ng[r,j]++ } }
      sgE[r]+=g; nE[r]++ }
    END{ split("0.2 0.5 0.7 1.0", R, " ")
      printf "  %s — unsolved instances: gap = known optimum - largest f expanded\n", LBL
      printf "    RS    unsolved  no-opt | gap 0    1    2  3-5   >5 | mean gap at 60s  120s  240s   end\n"
      for(r=0;r<4;r++){ if(!u[r]) continue
        printf "    %-4s %9d %7d | %5d %4d %4d %4d %4d |", R[r+1], u[r], nk[r]+0, b0[r]+0, b1[r]+0, b2[r]+0, b5[r]+0, bb[r]+0
        for(j=3;j<=5;j++) printf " %7s", (ng[r,j] ? sprintf("%.1f", sg[r,j]/ng[r,j]) : "-")
        printf " %6s\n", (nE[r] ? sprintf("%.1f", sgE[r]/nE[r]) : "-") } }' "$OPT" "$3"; }

flags(){ # label csv
  [ -f "$2" ] && awk -F, -v LBL="$1" 'function v(k){ return (k in c) ? $(c[k]) : "?" }
    NR==1{ for(i=1;i<=NF;i++) c[$i]=i; next }
    NR==2{ printf "  %-12s dr5=%s dr4=%s batch=%s prof=%s direction=%s problemType=%s cap=%s\n", LBL, v("useTT2DR5"), v("useTT2DR4"), v("useTT2Batch"), v("useTT2Prof"), v("direction"), v("problemType"), v("timeoutS"); exit }' "$2"; }

analyze(){
  echo ""; echo "=== 0. CORRECTNESS GATE (solved rows vs known optima; any WRONG invalidates that CSV) ==="
  gate "fwd j30" j30 "$F30"; gate "bwd j30" j30 "$B30"; gate "fwd j60" j60 "$F60"; gate "fwd j90" j90 "$F90"; gate "bwd j60" j60 "$B60"; gate "bwd j90" j90 "$B90"
  echo ""; echo "=== 1. COVERAGE BY RS ==="
  cov "fwd j30" "$F30"; cov "bwd j30" "$B30"
  cov "fwd j60" "$F60"; cov "bwd j60" "$B60"; cov "old fwd j60" "$OLD60"
  cov "fwd j90" "$F90"; cov "bwd j90" "$B90"; cov "old fwd j90" "$OLD90"
  echo ""; echo "=== 2. FORWARD vs BACKWARD, by RS -> HBAE target set ==="
  echo "  j30:"; comp j30 "$F30" "$B30"
  echo "  j60:"; comp j60 "$F60" "$B60"
  echo "  j90:"; comp j90 "$F90" "$B90"
  echo ""; echo "=== 3. DID THE FORWARD SEARCH CHANGE SINCE AUGUST? (old vs new forward, same instances) ==="
  if [ -f "$OLD60" ] && [ -f "$F60" ]; then drift "j60" "$OLD60" "$F60"; fi
  if [ -f "$OLD90" ] && [ -f "$F90" ]; then drift "j90" "$OLD90" "$F90"; fi
  echo ""; echo "=== 4. WHERE THE TIME GOES ==="
  prof "fwd j30" "$F30"; prof "bwd j30" "$B30"; prof "fwd j60" "$F60"; prof "fwd j90" "$F90"; prof "bwd j60" "$B60"; prof "bwd j90" "$B90"
  echo ""; echo "=== 5. HOW CLOSE ON UNSOLVED INSTANCES ==="
  gap "fwd j30" j30 "$F30"; gap "bwd j30" j30 "$B30"; gap "fwd j60" j60 "$F60"; gap "fwd j90" j90 "$F90"; gap "bwd j60" j60 "$B60"; gap "bwd j90" j90 "$B90"
  echo ""; echo "=== 6. FLAGS AS ACTUALLY RUN ==="
  flags "fwd j30" "$F30"; flags "bwd j30" "$B30"; flags "fwd j60" "$F60"; flags "fwd j90" "$F90"; flags "bwd j60" "$B60"; flags "bwd j90" "$B90"; flags "old fwd j60" "$OLD60"; flags "old fwd j90" "$OLD90"
}

if [ -n "${ANALYZE_ONLY:-}" ]; then analyze; exit 0; fi

cd "$RD" || { echo "cd failed"; exit 1; }
[ -f "$OPT" ] || { echo "ABORT: $OPT missing -- scp known_optima.csv to the repo root"; exit 1; }
mkdir -p "$L" "$NR"
export RCPSP_RESULTS_DIR="$NR"   # the binary writes every CSV here, whatever directory an arm starts in

echo "=== BUILD ($(date)) ==="
g++ -std=c++20 -fopenmp -O3 -I../.. -I../gui/STUB Driver.cpp -o Driver_tt2ctl 2> build_tt2ctl_err.txt \
  && echo "BUILD OK" || { echo "BUILD FAILED"; tail -40 build_tt2ctl_err.txt; exit 1; }
BIN="$RD/Driver_tt2ctl"
TT2="RCPSP_TT2_DR5=1 RCPSP_TT2_DR4=1 RCPSP_TT2_BATCH=1 RCPSP_TIMEOUT_S=300 RCPSP_TT2_PROF=1"

# PREFLIGHT (seconds): j60 data path, the build, and that the uploaded sources carry the profiling columns.
echo "=== PREFLIGHT ==="
snap(){ ls "$NR"/*.csv 2>/dev/null | sort; }
PRE=$(snap)
EXP=$(awk -F, '$2=="j60" && $3==4 && $4==1 {print $5}' "$OPT")
PF=$(cd "$ROOT" && env $TT2 RCPSP_TIMEOUT_S=60 "$BIN" tt2one j60 4 1 2>&1)
GOT=$(echo "$PF" | grep -oE 'optimal makespan=[0-9]+|Final makespan: [0-9]+' | grep -oE '[0-9]+$' | tail -1)
PCSV=$(comm -13 <(echo "$PRE") <(snap) | tail -1)
echo "  j60 4:1 -> makespan=${GOT:-none} (known optimum ${EXP:-?})   csv=${PCSV:-none}"
if [ -z "$GOT" ] || [ "$GOT" != "$EXP" ]; then echo "  ABORT: preflight instance did not reproduce the optimum (data path or build)"; exit 1; fi
if [ -z "$PCSV" ] || ! head -1 "$PCSV" | grep -q profAstarSec; then echo "  ABORT: CSV has no profiling columns -- upload Driver.cpp RCPSP.h Globals.h"; exit 1; fi
echo "  OK"

BEFORE=$(snap)
echo "=== ARMS ($(date)) ==="
( cd "$ROOT" && env $TT2 RCPSP_TT2_PROF_TRACE="$L/ctl_trace_fwd_j60.csv" "$BIN" tt2_j60 > "$L/ctl_tt2fwd_j60.log" 2>&1 ) &
sleep 3   # CSV names are timestamped to the second -- start every arm in its own second
( cd "$ROOT" && env $TT2 RCPSP_TT2_PROF_TRACE="$L/ctl_trace_fwd_j90.csv" "$BIN" tt2_j90 > "$L/ctl_tt2fwd_j90.log" 2>&1 ) &
sleep 3   # j30 data lives under HOG2/RCPSP, so these two run from $RD
( cd "$RD" && env $TT2 RCPSP_TT2_PROF_TRACE="$L/ctl_trace_fwd_j30.csv" "$BIN" tt2_j30 > "$L/ctl_tt2fwd_j30.log" 2>&1 ) &
sleep 3
( cd "$RD" && env $TT2 RCPSP_TT2_PROF_TRACE="$L/ctl_trace_bwd_j30.csv" "$BIN" tt2rev_j30 > "$L/ctl_tt2bwd_j30.log" 2>&1 ) &
if [ -n "${WITH_BWD:-}" ]; then
  sleep 3; ( cd "$ROOT" && env $TT2 RCPSP_TT2_PROF_TRACE="$L/ctl_trace_bwd_j60.csv" "$BIN" tt2rev_j60 > "$L/ctl_tt2bwd_j60.log" 2>&1 ) &
  sleep 3; ( cd "$ROOT" && env $TT2 RCPSP_TT2_PROF_TRACE="$L/ctl_trace_bwd_j90.csv" "$BIN" tt2rev_j90 > "$L/ctl_tt2bwd_j90.log" 2>&1 ) &
fi
wait
echo "=== ARMS DONE ($(date)) ==="

for f in $(comm -13 <(echo "$BEFORE") <(snap)); do
  read -r d p < <(awk -F, 'NR==1{for(i=1;i<=NF;i++) c[$i]=i; next} NR==2{print (("direction" in c)?$(c["direction"]):"?"), (("problemType" in c)?$(c["problemType"]):"?"); exit}' "$f")
  case "$d:$p" in
    fwd:j30) F30="$f" ;; bwd:j30) B30="$f" ;;
    fwd:j60) F60="$f" ;; fwd:j90) F90="$f" ;;
    bwd:j60) B60="$f" ;; bwd:j90) B90="$f" ;;
  esac
done
findone(){ for n in "$@"; do x=$(find "$ROOT" -name "$n" 2>/dev/null | head -1); [ -n "$x" ] && { echo "$x"; return; }; done; }
[ -z "$B60" ] && B60=$(findone output_tt2rev_j60_tt2dr5-batch_20260909-140811.csv BWD_tt2rev_j60.csv)
[ -z "$B90" ] && B90=$(findone output_tt2rev_j90_tt2dr5-batch_20260909-140811.csv BWD_tt2rev_j90.csv)
[ -z "$OLD60" ] && OLD60=$(findone output_tt2_j60_tt2dr5-batch_20260823-134523.csv)
[ -z "$OLD90" ] && OLD90=$(findone output_tt2_j90_tt2dr5_20260824-211704.csv)
echo "  fwd j30=$F30"; echo "  bwd j30=$B30"; echo "  fwd j60=$F60"; echo "  fwd j90=$F90"; echo "  bwd j60=$B60"; echo "  bwd j90=$B90"; echo "  old fwd j60=$OLD60"; echo "  old fwd j90=$OLD90"
analyze
echo ""; echo "=== ALL DONE ($(date)) ==="
