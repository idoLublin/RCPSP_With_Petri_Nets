#!/bin/bash
# ─────────────────────────────────────────────────────────────────────────────
# Server run: MEMORY of the TT2 search with and without RCPSP_TT2_COMPACT. Prepared 2026-10-09 from
# run_server_petritb.sh (gate, build, job loop) with the TT2 settings of run_tt2_max.sh (DR5 + DR4 + batch).
# No root stage, no upper bound.
#
# RCPSP_TT2_COMPACT=1: stored search nodes do not keep the token lists (resource_nodes, activity_nodes); the lists are
# rebuilt from the finished set and the active list when a node is expanded. Same search, less memory per stored node.
#
#   arm      RCPSP_TT2_COMPACT   what it is
#   ctl      0                   today's search (control)
#   compact  1                   stored nodes without token lists
#   chk      2                   lists kept, rebuilt as well and compared at every node, exit 7 on a difference
#                                (not in the default ARMS: memory as ctl, slower)
#
# ONE PROCESS PER INSTANCE (tt2one), so maxMem in a row is that instance's own peak, not the peak of a whole sweep.
# Instances: tt2_compact_j60_j90.txt = per size, three that the forward control run solved after 80-160 s (same
# search => the two arms must report the same expansions) and two that no model has proven (300 s: the peak case).
#
# Jobs = (instance, arm), the arms of an instance one after the other. MAX_PAR at once (default 2), one core each.
# MEMORY - these jobs are big: measured peak 50 GB (J60) / 63 GB (J90) per TT2 job (J30: 1 GB).
# The start gate is MAX_PAR x 63 + 50 GB, and a job only starts while MemAvailable >= JOB_AVAIL_GB (default 120).
# Do NOT run this next to another big run: launch with WAIT_FOR=<pattern> to queue behind it.
# Time: at most 300 s per job; 20 jobs admitted 240 s apart ~ 1.5 h.
# ARMS="..." picks arms, MAX_PAR; TIMEOUT_S / ONLY=<file> for a spot check.
#
# FULL RUN (all 480 instances of J30, J60, J90, both arms): ONLY=$PWD/tt2_full_j30_j60_j90.txt TAG=full MAX_PAR=auto
# STAGGER_S=0 JOB_AVAIL_GB=113. MAX_PAR=auto = as many 63 GB jobs as fit beside 50 GB free when the run starts, at
# most MAX_PAR_CAP (4); with at most MAX_PAR jobs the run never holds more than MAX_PAR x 63 GB, so no stagger is
# needed. Work: 24 h of search per arm (earlier full runs: J60 10.1 h, J90 13.7 h) => both arms take about
# 48 h / MAX_PAR. TAG goes into the folder and file names (RUN_tt2compact_<TAG>_<arm>.csv), so runs do not overwrite
# each other. With more than TABLE_MAX (40) instances the per-instance table is left out and only totals are printed.
# UPLOAD  HOG2/RCPSP: *.h *.cpp run_server_tt2_compact.sh tt2_compact_j60_j90.txt tt2_full_j30_j60_j90.txt ;
#         repo root: known_optima.csv (already there)
# ─────────────────────────────────────────────────────────────────────────────
set -u
REPO=${REPO:-$HOME/RCPSP_With_Petri_nets}
BIN_NAME=${BIN_NAME:-Driver_tt2compact}
cd "$REPO/HOG2/RCPSP" || { echo "cd failed"; exit 1; }

# Optional delayed start: WAIT_FOR = pgrep -f pattern of processes that must be gone first,
# then wait until MemAvailable >= MIN_AVAIL_GB. Sources are built only after the wait.
WAIT_FOR=${WAIT_FOR:-}
MAX_PAR=${MAX_PAR:-2}
MAX_PAR_CAP=${MAX_PAR_CAP:-4}
JOB_PEAK_GB=${JOB_PEAK_GB:-63}     # measured peak of one job (J90)
FREE_GB=${FREE_GB:-50}             # what must stay free with every job at its peak
avail(){ awk '/MemAvailable/ {printf "%d", $2/1048576}' /proc/meminfo; }
if [ -n "$WAIT_FOR" ]; then
  echo "[$(date +%F_%H:%M:%S)] waiting for processes to finish: $WAIT_FOR"
  while pgrep -f "$WAIT_FOR" > /dev/null; do sleep 300; done
fi
if [ "$MAX_PAR" = auto ]; then
  while [ "$(avail)" -lt $(( JOB_PEAK_GB + FREE_GB )) ]; do echo "[$(date +%F_%H:%M:%S)] MemAvailable $(avail) GB: not enough for one job"; sleep 300; done
  MAX_PAR=$(( ($(avail) - FREE_GB) / JOB_PEAK_GB )); [ "$MAX_PAR" -gt "$MAX_PAR_CAP" ] && MAX_PAR=$MAX_PAR_CAP
  echo "[$(date +%F_%H:%M:%S)] MAX_PAR=auto -> $MAX_PAR (MemAvailable $(avail) GB, $JOB_PEAK_GB GB per job, $FREE_GB GB kept free)"
fi
MIN_AVAIL_GB=${MIN_AVAIL_GB:-$(( MAX_PAR * JOB_PEAK_GB + FREE_GB ))}
while [ "$(avail)" -lt "$MIN_AVAIL_GB" ]; do echo "[$(date +%F_%H:%M:%S)] MemAvailable $(avail) GB < $MIN_AVAIL_GB"; sleep 300; done
echo "[$(date +%F_%H:%M:%S)] starting, MemAvailable $(avail) GB"

echo "=== BUILD ($(date)) ==="
g++ -std=c++20 -fopenmp -O3 -I../.. -I../gui/STUB Driver.cpp -o "$BIN_NAME" 2> build_tt2compact_err.txt \
  && echo "BUILD OK" || { echo "BUILD FAILED"; tail -40 build_tt2compact_err.txt; exit 1; }

cd "$REPO" || { echo "cd repo failed"; exit 1; }
RES=${RES:-$REPO/new_results}
LOGDIR=${LOGDIR:-$REPO/logs}
OPT=${OPT:-$REPO/known_optima.csv}
mkdir -p "$RES" "$LOGDIR"
ONLY=${ONLY:-$REPO/HOG2/RCPSP/tt2_compact_j60_j90.txt}
[ -f "$ONLY" ] || { echo "missing $ONLY"; exit 1; }
[ -f "$OPT" ] || { echo "missing $OPT"; exit 1; }
CAP=${TIMEOUT_S:-300}
JOB_AVAIL_GB=${JOB_AVAIL_GB:-120}
STAGGER_S=${STAGGER_S:-240}
TAG=${TAG:-}
SUB=tt2compact${TAG:+_$TAG}        # folder under new_results, prefix of the logs and of the merged files
TABLE_MAX=${TABLE_MAX:-40}
ARMS=${ARMS:-"ctl compact"}
# solver settings exactly as in run_tt2_max.sh (the standing TT2 configuration)
TT2="RCPSP_TT2_DR5=1 RCPSP_TT2_DR4=1 RCPSP_TT2_BATCH=1 RCPSP_TIMEOUT_S=$CAP"

log(){ echo "[$(date +%F_%H:%M:%S)] $*"; }
# value of RCPSP_TT2_COMPACT
arm_def(){ case $1 in
  ctl)     echo 0 ;;
  compact) echo 1 ;;
  chk)     echo 2 ;;
  *)       return 1 ;;
esac; }
for A in $ARMS; do arm_def "$A" > /dev/null || { log "unknown arm $A"; exit 1; }; done

mapfile -t INST < <(tr -d '\r' < "$ONLY" | grep -v '^#' | awk -F, 'NF>=3')
[ "${#INST[@]}" -gt 0 ] || { echo "no instances in $ONLY"; exit 1; }
JOBS=""
for I in "${INST[@]}"; do for A in $ARMS; do JOBS="$JOBS $A:$I"; done; done

# each job writes into its own folder (the only CSV there is its one row)
run_job(){ # <arm> <size> <group> <exam>
  local d="$RES/$SUB/$1/$2_$3_$4" lg="$LOGDIR/${SUB}_$1_$2_$3_$4.log" val csv wd="$REPO"
  [ "$2" = j30 ] && wd="$REPO/HOG2/RCPSP"            # j30 data sits under HOG2/RCPSP, j60/j90 at the repo root
  val=$(arm_def "$1")
  rm -rf "$d"; mkdir -p "$d"
  log "  start $1 $2 $3:$4 [RCPSP_TT2_COMPACT=$val]"
  ( cd "$wd" && env $TT2 RCPSP_TT2_COMPACT="$val" RCPSP_RESULTS_DIR="$d" "$REPO/HOG2/RCPSP/$BIN_NAME" tt2one "$2" "$3" "$4" ) > "$lg" 2>&1
  local rc=$?
  csv=$(ls "$d"/*.csv 2>/dev/null | tail -1)
  log "  end   $1 $2 $3:$4 exit=$rc rows=$( [ -f "$csv" ] && awk 'NR>1' "$csv" | wc -l ) check_failures=$(grep -c 'TT2-COMPACT' "$lg")"
}

log "starting: MAX_PAR=$MAX_PAR list=$(basename "$ONLY") arms: $ARMS jobs: $(echo $JOBS | wc -w)"
for J in $JOBS; do A=${J%%:*}; IFS=, read -r sz g e <<< "${J#*:}"
  while [ "$(jobs -rp | wc -l)" -ge "$MAX_PAR" ]; do wait -n; done
  while [ "$(avail)" -lt "$JOB_AVAIL_GB" ]; do log "MemAvailable $(avail) GB < $JOB_AVAIL_GB, holding $A $sz $g:$e"; sleep 300; done
  run_job "$A" "$sz" "$g" "$e" &
  sleep "$STAGGER_S"   # let the job reach its working memory before the next one is admitted
done
wait
log "=== all jobs done ==="

# one file per arm, rows in list order: new_results/RUN_tt2compact[_<TAG>]_<arm>.csv
for A in $ARMS; do out="$RES/RUN_${SUB}_$A.csv"; : > "$out"
  for I in "${INST[@]}"; do IFS=, read -r sz g e <<< "$I"
    csv=$(ls "$RES/$SUB/$A/${sz}_${g}_${e}"/*.csv 2>/dev/null | tail -1); [ -f "$csv" ] || continue
    [ -s "$out" ] || head -1 "$csv" > "$out"
    awk 'NR>1' "$csv" >> "$out"
  done
done

echo "=== RESULT (solved = proven optimal; WRONG must be 0; peak = the largest single instance) ==="
for A in $ARMS; do f="$RES/RUN_${SUB}_$A.csv"; [ -s "$f" ] || { echo "RUN_${SUB}_$A: MISSING"; continue; }
  awk -F, -v N="RUN_${SUB}_$A" 'NR==FNR{ o[$2","$3","$4]=$5; next }
    FNR==1{ for (i=1;i<=NF;i++) c[$i]=i; next }
    { rows++; k=$(c["problemType"])","$(c["group"])","$(c["exam"])
      if ($(c["solved"])=="True") { s++; if ((k in o) && $(c["makespan"])+0 != o[k]+0) w++ }
      if ($(c["maxMem"])>m) m=$(c["maxMem"])
      fl="dr5=" $(c["useTT2DR5"]) " dr4=" $(c["useTT2DR4"]) " batch=" $(c["useTT2Batch"]) " compact=" $(c["tt2Compact"]) " cap=" $(c["timeoutS"])
      if (!(fl in seen)) { seen[fl]=1; all = all (all=="" ? "" : " | ") fl } }
    END{ printf "%s: rows=%d solved=%d WRONG=%d peak=%.1fGB   flags as run: %s\n", N, rows, s, w, m/1048576, all }' "$OPT" "$f"
done

C="$RES/RUN_${SUB}_ctl.csv"; K="$RES/RUN_${SUB}_compact.csv"
if [ -s "$C" ] && [ -s "$K" ]; then
  echo "=== ctl vs compact (each pair: ctl, then compact) ==="
  awk -F, -v TMAX="$TABLE_MAX" 'FNR==1{ fi++; delete c; for (i=1;i<=NF;i++) c[$i]=i; next }
    { k=$(c["problemType"])" "$(c["group"])":"$(c["exam"]); if (fi==1) ord[++n]=k; have[fi,k]=1
      S[fi,k]=$(c["solved"]); M[fi,k]=$(c["makespan"]); E[fi,k]=$(c["expandNumber"]); G[fi,k]=$(c["generatedNumber"])
      I[fi,k]=$(c["itemsAtEnd"]); P[fi,k]=$(c["maxMem"]); T[fi,k]=$(c["time"]) }
    END{ if (n <= TMAX) printf "  %-10s %-11s %23s %-5s | %15s %6s | %13s | %13s\n", "instance", "solved", "expanded", "same", "peak GB", "ratio", "bytes/node", "time s"
      for (j=1;j<=n;j++){ k=ord[j]; if (!((2,k) in have)) { miss++; if (n <= TMAX) printf "  %-10s no compact row\n", k; continue }
        both=(S[1,k]=="True" && S[2,k]=="True")
        same= both ? ((E[1,k]==E[2,k] && G[1,k]==G[2,k] && M[1,k]==M[2,k]) ? "yes" : "NO") : "-"
        if (same=="NO") { bad++; badl = badl " " k }
        split(k, a, " "); z=a[1]; np[z]++
        if (S[1,k]=="True") sc[z]++
        if (S[2,k]=="True") sk[z]++
        if (both) { nb[z]++; tc[z]+=T[1,k]; tk[z]+=T[2,k] }
        else if (S[1,k]=="True") only1 = only1 " " k
        else if (S[2,k]=="True") only2 = only2 " " k
        else { nu[z]++; mc[z]+=P[1,k]; mk[z]+=P[2,k]; ec[z]+=E[1,k]; ek[z]+=E[2,k] }
        if (P[1,k]>pc[z]) pc[z]=P[1,k]
        if (P[2,k]>pk[z]) pk[z]=P[2,k]
        if (n <= TMAX) printf "  %-10s %-5s %-5s %11d %11d %-5s | %7.2f %7.2f %5.2fx | %6.0f %6.0f | %6.0f %6.0f\n", k, S[1,k], S[2,k], E[1,k], E[2,k], same,
               P[1,k]/1048576, P[2,k]/1048576, P[1,k]/(P[2,k]>0?P[2,k]:1), P[1,k]*1024/(I[1,k]>0?I[1,k]:1), P[2,k]*1024/(I[2,k]>0?I[2,k]:1), T[1,k], T[2,k] }
      if (n <= TMAX) {
        printf "  same = both arms solved it with equal expansions, generated nodes and makespan; bytes/node = peak memory / stored nodes\n"
        printf "  unsolved pairs ran the same 300 s: more expansions there means faster per node, not a different search\n" }
      printf "  TOTALS per set (pairs = instances with a row in both arms)\n"
      nz = split("j30 j60 j90", zs, " ")
      for (q=1;q<=nz;q++){ z=zs[q]; if (!(z in np)) continue
        printf "  %s: pairs %d | solved ctl %d = %.1f%%, compact %d = %.1f%% | solved by both %d, time compact/ctl x%.2f | largest job ctl %.1f GB, compact %.1f GB (x%.2f lower)", z, np[z], sc[z], 100*sc[z]/np[z], sk[z], 100*sk[z]/np[z], nb[z], (tc[z]>0 ? tk[z]/tc[z] : 0), pc[z]/1048576, pk[z]/1048576, pc[z]/(pk[z]>0?pk[z]:1)
        if (nu[z] > 0) printf " | unsolved by both %d: memory x%.2f lower, expansions in 300 s x%.2f", nu[z], mc[z]/(mk[z]>0?mk[z]:1), ek[z]/(ec[z]>0?ec[z]:1)
        printf "\n" }
      printf "  solved only by ctl:%s\n", (only1=="" ? " none" : only1)
      printf "  solved only by compact:%s\n", (only2=="" ? " none" : only2)
      printf "  instances without a compact row: %d\n", miss+0
      printf "  solved by both with a DIFFERENT search: %d (must be 0)%s\n", bad+0, badl }' "$C" "$K"
fi
grep -l "TT2-COMPACT" "$LOGDIR"/${SUB}_*.log 2>/dev/null | sed 's/^/CHECK FAILURE in /'
log "=== DONE ==="
