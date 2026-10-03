#!/bin/bash
# ─────────────────────────────────────────────────────────────────────────────
# Server run: TIE-BREAK variants for the Petri-net searches. Prepared 2026-10-03 from run_server_cpcbs.sh
# (gate, build) with the solver settings of run_server_bi_sweep.sh (TT2 = DR5 + DR4 + batch; HBAE = f order,
# max stop). No root stage, no upper bound.
#
# RCPSP_TT2_TBSPEC sets the order among nodes of equal priority (f in TT2, b in HBAE): one letter per key, most
# significant first, UPPER = larger first, lower = smaller first.  G elapsed time, F finished activities,
# A activities in progress, P started (= F + A), R total remaining time of those in progress, Z zero-delay child.
# Today's orders are GFA (TT2) and ZGFA (HBAE). Only equal-priority nodes are reordered, so every arm is optimal.
#
# Instances: petri_tb_j60_j90.txt = where an order can matter and where it can hurt -
#   plateau: TT2 forward or backward already expanded f = optimum and still timed out   (10 on J60, 32 on J90)
#   slow:    solved by forward or backward TT2 but only after >= 60 s                   (41 on J60, 11 on J90)
#
#   arm        search    spec   what it is
#   pt-f-ctl   forward   GFA    today's order through the new code path (control)
#   pt-f-fga   forward   FGA    most finished first, before elapsed time (dive by progress)
#   pt-f-gaf   forward   GAF    in-progress count before finished count
#   pt-f-gp    forward   GP     most started first
#   pt-f-gfar  forward   GFAr   today's order, then least remaining running time
#   pt-b-ctl   backward  GFA    control
#   pt-b-fga   backward  FGA
#   pt-h-ctl   HBAE      ZGFA   control (= RCPSP_BI_ACTIVETB=1 with zero-delay children first)
#   pt-h-zfga  HBAE      ZFGA   most finished before elapsed time
#   pt-h-zgp   HBAE      ZGP    most started first
# Local code check (26 J30 instances): the three controls reproduce today's searches expansion for expansion,
# every variant returns the known optima; on J30 the variants are within 3% of the control.
#
# Jobs = (arm, size); every arm's J90 first, then J60. MAX_PAR at once (default 4), one core each.
# MEMORY - these jobs are big: measured peak 50 GB (J60) / 63 GB (J90) per TT2 job, 45 / 56 GB per HBAE job.
# The start gate is MAX_PAR x 63 + 50 GB, and a job only starts while MemAvailable >= JOB_AVAIL_GB (default 120).
# Do NOT run this next to another big run: launch with WAIT_FOR=<pattern> to queue behind it.
# Time: 300 s per unsolved instance: at most 3.3 h for a J90 job and 4.2 h for a J60 job; 20 jobs at 4 ~ 12 h.
# Each job's CSV is copied to new_results/RUN_<arm>_<size>.csv AS SOON AS THAT JOB ENDS.
# ARMS="..." picks arms, SIZES, MAX_PAR; TIMEOUT_S / ONLY=<file> for a spot check.
# UPLOAD  HOG2/RCPSP: *.h *.cpp run_server_petritb.sh petri_tb_j60_j90.txt ;  repo root: known_optima.csv (already there)
# ─────────────────────────────────────────────────────────────────────────────
set -u
REPO=${REPO:-$HOME/RCPSP_With_Petri_nets}
BIN_NAME=${BIN_NAME:-Driver_petritb}
cd "$REPO/HOG2/RCPSP" || { echo "cd failed"; exit 1; }

# Optional delayed start: WAIT_FOR = pgrep -f pattern of processes that must be gone first (e.g. the probe2 sweep),
# then wait until MemAvailable >= MIN_AVAIL_GB. Sources are built only after the wait.
WAIT_FOR=${WAIT_FOR:-}
MAX_PAR=${MAX_PAR:-4}
MIN_AVAIL_GB=${MIN_AVAIL_GB:-$(( MAX_PAR * 63 + 50 ))}
avail(){ awk '/MemAvailable/ {printf "%d", $2/1048576}' /proc/meminfo; }
if [ -n "$WAIT_FOR" ]; then
  echo "[$(date +%F_%H:%M:%S)] waiting for processes to finish: $WAIT_FOR"
  while pgrep -f "$WAIT_FOR" > /dev/null; do sleep 300; done
fi
while [ "$(avail)" -lt "$MIN_AVAIL_GB" ]; do echo "[$(date +%F_%H:%M:%S)] MemAvailable $(avail) GB < $MIN_AVAIL_GB"; sleep 300; done
echo "[$(date +%F_%H:%M:%S)] starting, MemAvailable $(avail) GB"

echo "=== BUILD ($(date)) ==="
g++ -std=c++20 -fopenmp -O3 -I../.. -I../gui/STUB Driver.cpp -o "$BIN_NAME" 2> build_petritb_err.txt \
  && echo "BUILD OK" || { echo "BUILD FAILED"; tail -40 build_petritb_err.txt; exit 1; }

cd "$REPO" || { echo "cd repo failed"; exit 1; }
RES=${RES:-$REPO/new_results}
LOGDIR=${LOGDIR:-$REPO/logs}
OPT=${OPT:-$REPO/known_optima.csv}
mkdir -p "$RES" "$LOGDIR"
ONLY=${ONLY:-$REPO/HOG2/RCPSP/petri_tb_j60_j90.txt}
[ -f "$ONLY" ] || { echo "missing $ONLY"; exit 1; }
[ -f "$OPT" ] || { echo "missing $OPT"; exit 1; }
CAP=${TIMEOUT_S:-300}
JOB_AVAIL_GB=${JOB_AVAIL_GB:-120}
SIZES=${SIZES:-"j90 j60"}
ARMS=${ARMS:-"pt-f-ctl pt-f-fga pt-h-ctl pt-h-zfga pt-b-ctl pt-b-fga pt-f-gaf pt-f-gp pt-f-gfar pt-h-zgp"}
# solver settings exactly as in run_server_bi_sweep.sh (arms tt2fwd / tt2bwd / bi-fmaxtb)
TT2="RCPSP_TT2_DR5=1 RCPSP_TT2_DR4=1 RCPSP_TT2_BATCH=1 RCPSP_TT2_PROF=1 RCPSP_TIMEOUT_S=$CAP"
HB="RCPSP_MEET_NORM=1 RCPSP_BI_DR5=3 RCPSP_BI_DR4=1 RCPSP_BI_BATCH=0 RCPSP_BI_NIP=0 RCPSP_BI_DRAINCERT=1 RCPSP_BI_ASSERT=1 RCPSP_TT2_PROF=1 RCPSP_TIMEOUT_S=$CAP RCPSP_BI_PRIO=1 RCPSP_BI_STOP=max RCPSP_BI_ACTIVETB=1"

log(){ echo "[$(date +%F_%H:%M:%S)] $*"; }
# <mode> | <spec>
arm_def(){ case $1 in
  pt-f-ctl)  echo "tt2|GFA" ;;      pt-f-fga)  echo "tt2|FGA" ;;     pt-f-gaf) echo "tt2|GAF" ;;
  pt-f-gp)   echo "tt2|GP" ;;       pt-f-gfar) echo "tt2|GFAr" ;;    pt-f-fewact) echo "tt2|GFa" ;;
  pt-b-ctl)  echo "tt2rev|GFA" ;;   pt-b-fga)  echo "tt2rev|FGA" ;;  pt-b-gp) echo "tt2rev|GP" ;;
  pt-h-ctl)  echo "tt2hbae|ZGFA" ;; pt-h-zfga) echo "tt2hbae|ZFGA" ;; pt-h-zgp) echo "tt2hbae|ZGP" ;;
  pt-h-nodz) echo "tt2hbae|GFA" ;;
  *)         return 1 ;;
esac; }
for A in $ARMS; do arm_def "$A" > /dev/null || { log "unknown arm $A"; exit 1; }; done

JOBS=""
for sz in $SIZES; do for A in $ARMS; do JOBS="$JOBS $A:$sz"; done; done

# each job writes into its own folder (the only CSV there is its result), copied to $RES/RUN_<arm>_<size>.csv at the end
run_job(){ # <arm> <size>
  local d="$RES/petritb/$1/$2" lg="$LOGDIR/$1_$2.log" def mode spec base csv
  def=$(arm_def "$1"); mode=${def%%|*}; spec=${def#*|}
  base=$TT2; [ "$mode" = tt2hbae ] && base=$HB
  rm -rf "$d"; mkdir -p "$d"
  log "  start $1 $2 [$mode spec $spec]"
  env $base RCPSP_TT2_TBSPEC="$spec" RCPSP_RESULTS_DIR="$d" RCPSP_ONLY="$ONLY" ./HOG2/RCPSP/$BIN_NAME ${mode}_$2 > "$lg" 2>&1
  local rc=$?
  csv=$(ls "$d"/*.csv 2>/dev/null | tail -1)
  [ -f "$csv" ] && cp "$csv" "$RES/RUN_$1_$2.csv"
  log "  end   $1 $2 exit=$rc rows=$( [ -f "$csv" ] && awk 'NR>1' "$csv" | wc -l ) false_results=$(grep -c 'FALSE RESULT' "$lg")"
}

log "starting: MAX_PAR=$MAX_PAR list=$(basename "$ONLY") jobs:$JOBS"
for J in $JOBS; do A=${J%:*}; sz=${J#*:}
  while [ "$(jobs -rp | wc -l)" -ge "$MAX_PAR" ]; do wait -n; done
  while [ "$(avail)" -lt "$JOB_AVAIL_GB" ]; do log "MemAvailable $(avail) GB < $JOB_AVAIL_GB, holding $A $sz"; sleep 300; done
  run_job "$A" "$sz" &
  sleep 240          # let the job reach its working memory before the next one is admitted
done
wait
log "=== all jobs done ==="

echo "=== RESULT (solved = proven optimal; WRONG must be 0) ==="
for J in $JOBS; do A=${J%:*}; sz=${J#*:}; f="$RES/RUN_${A}_${sz}.csv"; [ -f "$f" ] || { echo "RUN_${A}_${sz}: MISSING"; continue; }
  awk -F, -v SZ="$sz" -v N="RUN_${A}_${sz}" 'NR==FNR{ if ($2==SZ) o[$3","$4]=$5; next }
    FNR==1{ for (i=1;i<=NF;i++) c[$i]=i; pv=("proven" in c) ? c["proven"] : c["solved"]; next }
    { rows++; if ($pv=="1" || $pv=="True" || $pv=="true") { s++; t+=$(c["time"]); k=$(c["group"])","$(c["exam"]); if ((k in o) && $(c["makespan"])+0 != o[k]+0) w++ }
      e+=$(c["expandNumber"]); if ($(c["maxMem"])>m) m=$(c["maxMem"]); spec=$(c["tt2TbSpec"]) }
    END{ printf "%s: spec=%s solved=%d/%d WRONG=%d time_on_solved=%.0fs expansions=%d peak=%.1fGB\n", N, spec, s, rows, w, t, e, m/1048576 }' "$OPT" "$f"
done
grep -l "FALSE RESULT" "$LOGDIR"/pt-*.log 2>/dev/null | sed 's/^/FALSE RESULT in /'
log "=== DONE ==="
