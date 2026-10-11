#!/bin/bash
# ─────────────────────────────────────────────────────────────────────────────
# Server run: SPEED of the TT2 search with RCPSP_TT2_PRECHECK, and with jemalloc. Prepared 2026-10-11 from
# run_server_tt2_compact.sh (gate, build, one process per instance, merge, summary), standing TT2 settings
# (DR5 + DR4 + batch, 300 s). No root stage, no upper bound.
#
# RCPSP_TT2_PRECHECK=1: DR5 tests a child on its DR5 view (scheduled set, g, active releases) BEFORE the child is built,
# so the ~72% of children DR5 prunes are never constructed. Same tests in the same order => the SAME search: equal
# expansions, generated nodes, makespans and DR5 counters (checked on all 480 J30 for five TT2 configurations).
# Locally: 29% fewer instructions per instance (J30 13:2), wall time about -9%. Memory is unchanged.
# jemalloc changes only the allocator (no code change): locally another -9% time and -7% peak memory.
#
#   arm    RCPSP_TT2_PRECHECK  allocator   what it is
#   ctl    0                   glibc       today's search (control)
#   pre    1                   glibc       pre-check
#   je     1                   jemalloc    pre-check + jemalloc (LD_PRELOAD); dropped if libjemalloc is not found
#   ctlje  0                   jemalloc    jemalloc alone (not in the default ARMS)
#   chk    2                   glibc       check mode: also builds every child and compares, exit 7 on a difference
#                                          (not in the default ARMS: slower than ctl)
#
# What to read: all arms run the same search, so (1) "same" must be yes on every instance solved by both, (2) the
# time ratio on those is the speed-up, (3) on instances nobody solves the expansion ratio in 300 s is the speed-up,
# and (4) instances solved only by the faster arm are the payoff.
#
# ONE PROCESS PER INSTANCE (tt2one), one core per job. Instances: tt2_compact_j60_j90.txt = per size, three that the
# forward control run solved after 80-160 s and two that no model has proven.
# Jobs = (instance, arm), the arms of an instance one after the other. MAX_PAR at once (default 2).
# MEMORY - these jobs are big: measured peak 50 GB (J60) / 63 GB (J90) per TT2 job (J30: 1 GB).
# The start gate is MAX_PAR x 63 + 50 GB, and a job only starts while MemAvailable >= JOB_AVAIL_GB (default 120).
# Do NOT run this next to another big run: launch with WAIT_FOR=<pattern> to queue behind it
# (e.g. WAIT_FOR=run_server_tt2_compact.sh).
# Time: at most 300 s per job; 30 jobs admitted 240 s apart ~ 2 h.
# ARMS="..." picks arms, MAX_PAR; TIMEOUT_S / ONLY=<file> for a spot check; COMPACT=1 runs every arm with
# RCPSP_TT2_COMPACT=1 (then JOB_PEAK_GB=35 is enough).
#
# FULL RUN (all 480 instances of J30, J60, J90): ONLY=$PWD/tt2_full_j30_j60_j90.txt TAG=full MAX_PAR=auto STAGGER_S=0
# JOB_AVAIL_GB=113 ARMS="ctl je". Work: ~24 h of search per arm => 2 arms ~ 48 h / MAX_PAR. TAG goes into the folder
# and file names (RUN_tt2pre_<TAG>_<arm>.csv), so runs do not overwrite each other. With more than TABLE_MAX (40)
# instances the per-instance table is left out and only totals are printed.
# UPLOAD  HOG2/RCPSP: *.h *.cpp run_server_tt2_precheck.sh tt2_compact_j60_j90.txt tt2_full_j30_j60_j90.txt ;
#         repo root: known_optima.csv (already there)
# ─────────────────────────────────────────────────────────────────────────────
set -u
REPO=${REPO:-$HOME/RCPSP_With_Petri_nets}
BIN_NAME=${BIN_NAME:-Driver_tt2pre}
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
g++ -std=c++20 -fopenmp -O3 -I../.. -I../gui/STUB Driver.cpp -o "$BIN_NAME" 2> build_tt2pre_err.txt \
  && echo "BUILD OK" || { echo "BUILD FAILED"; tail -40 build_tt2pre_err.txt; exit 1; }

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
SUB=tt2pre${TAG:+_$TAG}            # folder under new_results, prefix of the logs and of the merged files
TABLE_MAX=${TABLE_MAX:-40}
COMPACT=${COMPACT:-0}
# jemalloc: JEMALLOC=<path> overrides; otherwise the first libjemalloc.so.2 the loader knows
JEMALLOC=${JEMALLOC:-$(ldconfig -p 2>/dev/null | awk '/libjemalloc\.so\.2/ {print $NF; exit}')}
for p in /usr/lib/x86_64-linux-gnu/libjemalloc.so.2 /lib/x86_64-linux-gnu/libjemalloc.so.2 /usr/lib64/libjemalloc.so.2; do
  [ -z "$JEMALLOC" ] && [ -f "$p" ] && JEMALLOC=$p
done
ARMS=${ARMS:-"ctl pre je"}
# solver settings exactly as in run_tt2_max.sh (the standing TT2 configuration)
TT2="RCPSP_TT2_DR5=1 RCPSP_TT2_DR4=1 RCPSP_TT2_BATCH=1 RCPSP_TT2_COMPACT=$COMPACT RCPSP_TIMEOUT_S=$CAP"

log(){ echo "[$(date +%F_%H:%M:%S)] $*"; }
# "<RCPSP_TT2_PRECHECK> <allocator>"
arm_def(){ case $1 in
  ctl)   echo "0 glibc" ;;
  pre)   echo "1 glibc" ;;
  je)    echo "1 je" ;;
  ctlje) echo "0 je" ;;
  chk)   echo "2 glibc" ;;
  *)     return 1 ;;
esac; }
KEEP=""
for A in $ARMS; do
  d=$(arm_def "$A") || { log "unknown arm $A"; exit 1; }
  if [ "${d#* }" = je ] && [ ! -f "$JEMALLOC" ]; then log "arm $A dropped: libjemalloc.so.2 not found (install libjemalloc2 or set JEMALLOC=<path>)"; continue; fi
  KEEP="$KEEP $A"
done
ARMS=$KEEP
[ -n "$ARMS" ] || { log "no arms left"; exit 1; }
log "jemalloc: ${JEMALLOC:-not found}"

mapfile -t INST < <(tr -d '\r' < "$ONLY" | grep -v '^#' | awk -F, 'NF>=3')
[ "${#INST[@]}" -gt 0 ] || { echo "no instances in $ONLY"; exit 1; }
JOBS=""
for I in "${INST[@]}"; do for A in $ARMS; do JOBS="$JOBS $A:$I"; done; done

# each job writes into its own folder (the only CSV there is its one row)
run_job(){ # <arm> <size> <group> <exam>
  local d="$RES/$SUB/$1/$2_$3_$4" lg="$LOGDIR/${SUB}_$1_$2_$3_$4.log" def pc al pre="" csv wd="$REPO"
  [ "$2" = j30 ] && wd="$REPO/HOG2/RCPSP"            # j30 data sits under HOG2/RCPSP, j60/j90 at the repo root
  def=$(arm_def "$1"); pc=${def% *}; al=${def#* }
  [ "$al" = je ] && pre="LD_PRELOAD=$JEMALLOC"
  rm -rf "$d"; mkdir -p "$d"
  log "  start $1 $2 $3:$4 [RCPSP_TT2_PRECHECK=$pc allocator=$al]"
  ( cd "$wd" && env $pre $TT2 RCPSP_TT2_PRECHECK="$pc" RCPSP_RESULTS_DIR="$d" "$REPO/HOG2/RCPSP/$BIN_NAME" tt2one "$2" "$3" "$4" ) > "$lg" 2>&1
  local rc=$?
  csv=$(ls "$d"/*.csv 2>/dev/null | tail -1)
  log "  end   $1 $2 $3:$4 exit=$rc rows=$( [ -f "$csv" ] && awk 'NR>1' "$csv" | wc -l ) check_failures=$(grep -c 'PRECHECK\] DR5 view differs' "$lg") preload_errors=$(grep -c 'cannot be preloaded' "$lg")"
}

log "starting: MAX_PAR=$MAX_PAR list=$(basename "$ONLY") arms:$ARMS compact=$COMPACT jobs: $(echo $JOBS | wc -w)"
for J in $JOBS; do A=${J%%:*}; IFS=, read -r sz g e <<< "${J#*:}"
  while [ "$(jobs -rp | wc -l)" -ge "$MAX_PAR" ]; do wait -n; done
  while [ "$(avail)" -lt "$JOB_AVAIL_GB" ]; do log "MemAvailable $(avail) GB < $JOB_AVAIL_GB, holding $A $sz $g:$e"; sleep 300; done
  run_job "$A" "$sz" "$g" "$e" &
  sleep "$STAGGER_S"   # let the job reach its working memory before the next one is admitted
done
wait
log "=== all jobs done ==="

# one file per arm, rows in list order: new_results/RUN_tt2pre[_<TAG>]_<arm>.csv
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
      fl="dr5=" $(c["useTT2DR5"]) " dr4=" $(c["useTT2DR4"]) " batch=" $(c["useTT2Batch"]) " compact=" $(c["tt2Compact"]) " precheck=" $(c["tt2Precheck"]) " cap=" $(c["timeoutS"])
      if (!(fl in seen)) { seen[fl]=1; all = all (all=="" ? "" : " | ") fl } }
    END{ printf "%s: rows=%d solved=%d WRONG=%d peak=%.1fGB   flags as run: %s\n", N, rows, s, w, m/1048576, all }' "$OPT" "$f"
done

# <base arm> <other arm>: same search? speed-up on the solved, expansions in the time cap on the unsolved
compare(){
  local B="$RES/RUN_${SUB}_$1.csv" X="$RES/RUN_${SUB}_$2.csv"
  [ -s "$B" ] && [ -s "$X" ] || return 0
  echo "=== $1 vs $2 (each pair: $1, then $2) ==="
  awk -F, -v TMAX="$TABLE_MAX" -v NB="$1" -v NX="$2" 'FNR==1{ fi++; delete c; for (i=1;i<=NF;i++) c[$i]=i; next }
    { k=$(c["problemType"])" "$(c["group"])":"$(c["exam"]); if (fi==1) ord[++n]=k; have[fi,k]=1
      S[fi,k]=$(c["solved"]); M[fi,k]=$(c["makespan"]); E[fi,k]=$(c["expandNumber"]); G[fi,k]=$(c["generatedNumber"])
      D[fi,k]=$(c["domPruned"]); P[fi,k]=$(c["maxMem"]); T[fi,k]=$(c["time"]) }
    END{ if (n <= TMAX) printf "  %-10s %-11s %23s %-5s | %13s | %15s %6s\n", "instance", "solved", "expanded", "same", "peak GB", "time s", "ratio"
      for (j=1;j<=n;j++){ k=ord[j]; if (!((2,k) in have)) { miss++; if (n <= TMAX) printf "  %-10s no %s row\n", k, NX; continue }
        both=(S[1,k]=="True" && S[2,k]=="True")
        same= both ? ((E[1,k]==E[2,k] && G[1,k]==G[2,k] && M[1,k]==M[2,k] && D[1,k]==D[2,k]) ? "yes" : "NO") : "-"
        if (same=="NO") { bad++; badl = badl " " k }
        split(k, a, " "); z=a[1]; np[z]++
        if (S[1,k]=="True") sb[z]++
        if (S[2,k]=="True") sx[z]++
        if (both) { nb[z]++; tb[z]+=T[1,k]; tx[z]+=T[2,k] }
        else if (S[1,k]=="True") only1 = only1 " " k
        else if (S[2,k]=="True") only2 = only2 " " k
        else { nu[z]++; eb[z]+=E[1,k]; ex[z]+=E[2,k] }
        if (P[1,k]>pb[z]) pb[z]=P[1,k]
        if (P[2,k]>px[z]) px[z]=P[2,k]
        if (n <= TMAX) printf "  %-10s %-5s %-5s %11d %11d %-5s | %6.2f %6.2f | %7.1f %7.1f %5.2fx\n", k, S[1,k], S[2,k], E[1,k], E[2,k], same,
               P[1,k]/1048576, P[2,k]/1048576, T[1,k], T[2,k], (T[2,k]>0 ? T[1,k]/T[2,k] : 0) }
      if (n <= TMAX) {
        printf "  same = both solved it with equal expansions, generated nodes, makespan and DR5 prunes; ratio = time %s / time %s\n", NB, NX
        printf "  unsolved pairs ran the same time cap: more expansions there means faster per node, not a different search\n" }
      printf "  TOTALS per set (pairs = instances with a row in both arms)\n"
      nz = split("j30 j60 j90", zs, " ")
      for (q=1;q<=nz;q++){ z=zs[q]; if (!(z in np)) continue
        printf "  %s: pairs %d | solved %s %d, %s %d | solved by both %d, speed-up x%.2f (total time %.0f s -> %.0f s) | largest job %.1f GB -> %.1f GB", z, np[z], NB, sb[z], NX, sx[z], nb[z], (tx[z]>0 ? tb[z]/tx[z] : 0), tb[z], tx[z], pb[z]/1048576, px[z]/1048576
        if (nu[z] > 0) printf " | unsolved by both %d: expansions in the cap x%.2f", nu[z], ex[z]/(eb[z]>0?eb[z]:1)
        printf "\n" }
      printf "  solved only by %s:%s\n", NB, (only1=="" ? " none" : only1)
      printf "  solved only by %s:%s\n", NX, (only2=="" ? " none" : only2)
      printf "  instances without a %s row: %d\n", NX, miss+0
      printf "  solved by both with a DIFFERENT search: %d (must be 0)%s\n", bad+0, badl }' "$B" "$X"
}
for A in $ARMS; do [ "$A" = ctl ] || compare ctl "$A"; done
case " $ARMS " in *" pre "*) case " $ARMS " in *" je "*) compare pre je ;; esac ;; esac
grep -l 'PRECHECK\] DR5 view differs' "$LOGDIR"/${SUB}_*.log 2>/dev/null | sed 's/^/CHECK FAILURE in /'
grep -l 'cannot be preloaded' "$LOGDIR"/${SUB}_*.log 2>/dev/null | sed 's/^/JEMALLOC NOT LOADED in /'
log "=== DONE ==="
