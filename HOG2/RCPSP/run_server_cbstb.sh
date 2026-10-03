#!/bin/bash
# ─────────────────────────────────────────────────────────────────────────────
# Server run: PLATEAU TIE-BREAK variants on the valid CBS. Prepared 2026-10-02 from run_server_cpcbs.sh.
#
# Where the gap is: on the plateau instances CBS has provenLB = optimum at the root, so every node on the way to a
# goal has the same f and only the ORDER among equal-f nodes decides whether the schedule is found. RCPSP_CBS_TBSPEC
# sets that order explicitly: one letter per key, most significant first, UPPER = larger first, lower = smaller:
#   G makespan   D depth   T earliest conflict time   S total delay   C conflict events   O total overload
#   N creation order (N = newest first = depth-first among ties)
# Any order among equal-f nodes keeps A* optimal, so every arm is sound; "GTD" is today's RCPSP_CBS_TB=3.
#
# REF = cfg8, DR5 both, skyline, lazy, deadline propagation (16 steps), pathmax, hybrid T=0.3, DR4 off.
# Unvalidated local hint (26 J60 plateau instances, 10 s, laptop): GTD 16, depth-first 17, fewest-conflicts-first
# 22 with every solve under 1 s, most-conflicts-first 0. This run is the real measurement.
#
#   arm          spec   list     what it is
#   tb-ref       GTD    plateau  today's tie-break through the new code path (control)
#   tb-c         Gc     plateau  fewest conflict events first
#   tb-cd        GcD    plateau  ... then deeper                              <- local best
#   tb-cd-nog    cD     plateau  the same without the makespan key in front
#   tb-ct        GcT    plateau  fewest conflicts, then feasible furthest out
#   tb-ctd       GcTD   plateau  ... then deeper
#   tb-od        GoD    plateau  least total overload first, then deeper
#   tb-ocd       GocD   plateau  overload, then conflict count, then deeper
#   tb-cod       GcoD   plateau  conflict count, then overload, then deeper
#   tb-lifo      GDN    plateau  deeper, then newest (plain depth-first)
#   tb-fifo      GDn    plateau  deeper, then oldest
#   tb-slack     GDs    plateau  deeper, then least total delay
#   tb-c-slack   Gcs    plateau  fewest conflicts, then least total delay
#   tb-cd-sel1   GcD    plateau  + conflict-selection mode 1   (best partner of the old tie-break locally)
#   tb-cd-adapt  GcD    plateau  + adaptive branching          (same)
#   tbs-cd       GcD    screen   the local best on everything within reach: shows regressions and the small-gap set
#   tbf-cd       GcD    j30 full soundness and J30 coverage of the local best (480 instances)
#   tb-none      G      plateau  no plateau tie-break at all (not in the default list; for STACK=base it is the old base)
# plateau = plateau_j60_j90.txt (26 J60 + 110 J90).  screen = screen_j60_j90.txt (391 + 384).
#
# STACK=base runs the same arms on the OLD BASE instead of REF (DR4 on, T=0.5, no propagation, no pathmax): what
# each tie-break rule is worth ON ITS OWN. Use another BIN_NAME if the first run is still going, e.g.
#   STACK=base BIN_NAME=Driver_cbstbb ARMS="tb-ref tb-cd tb-od tb-lifo tb-c-slack" nohup bash run_server_cbstb.sh > cbstb_base_log.txt 2>&1 &
#
# Jobs = (arm, size). The three long jobs (tbf-cd j30, tbs-cd j90, tbs-cd j60) start first; then every plateau arm
# on J60 (26 instances: minutes to 2 h), then on J90. MAX_PAR at once (default 8), one core each.
# Memory: 20.4 GB per job by the measured CBS J90 peak => 163 GB worst case; a job also waits for
# MemAvailable >= JOB_AVAIL_GB (default 100). Time: a plateau arm costs 300 s per instance it does NOT solve.
# Each job's CSV is copied to new_results/RUN_<arm>_<size>.csv AS SOON AS THAT JOB ENDS.
# ARMS="..." picks arms, MAX_PAR; TIMEOUT_S / ONLY_ALL=<file> for a spot check.
# UPLOAD  HOG2/RCPSP: *.h *.cpp run_server_cbstb.sh plateau_j60_j90.txt screen_j60_j90.txt
# ─────────────────────────────────────────────────────────────────────────────
set -u
REPO=${REPO:-$HOME/RCPSP_With_Petri_nets}
BIN_NAME=${BIN_NAME:-Driver_cbstb}
cd "$REPO/HOG2/RCPSP" || { echo "cd failed"; exit 1; }

# Optional delayed start: WAIT_FOR = pgrep -f pattern of processes that must be gone first (e.g. the probe2 sweep),
# then wait until MemAvailable >= MIN_AVAIL_GB. Sources are built only after the wait.
WAIT_FOR=${WAIT_FOR:-}
MAX_PAR=${MAX_PAR:-8}
MIN_AVAIL_GB=${MIN_AVAIL_GB:-$(( MAX_PAR * 21 + 50 ))}
avail(){ awk '/MemAvailable/ {printf "%d", $2/1048576}' /proc/meminfo; }
if [ -n "$WAIT_FOR" ]; then
  echo "[$(date +%F_%H:%M:%S)] waiting for processes to finish: $WAIT_FOR"
  while pgrep -f "$WAIT_FOR" > /dev/null; do sleep 300; done
fi
while [ "$(avail)" -lt "$MIN_AVAIL_GB" ]; do echo "[$(date +%F_%H:%M:%S)] MemAvailable $(avail) GB < $MIN_AVAIL_GB"; sleep 300; done
echo "[$(date +%F_%H:%M:%S)] starting, MemAvailable $(avail) GB"

echo "=== BUILD ($(date)) ==="
g++ -std=c++20 -fopenmp -O3 -I../.. -I../gui/STUB Driver.cpp -o "$BIN_NAME" 2> build_cbstb_err.txt \
  && echo "BUILD OK" || { echo "BUILD FAILED"; tail -40 build_cbstb_err.txt; exit 1; }

cd "$REPO" || { echo "cd repo failed"; exit 1; }
RES=${RES:-$REPO/new_results}
LOGDIR=${LOGDIR:-$REPO/logs}
mkdir -p "$RES" "$LOGDIR"
PLATEAU="$REPO/HOG2/RCPSP/plateau_j60_j90.txt"
SCREEN="$REPO/HOG2/RCPSP/screen_j60_j90.txt"
ONLY_ALL=${ONLY_ALL:-}
for f in "$PLATEAU" "$SCREEN" $ONLY_ALL; do [ -f "$f" ] || { echo "missing $f"; exit 1; }; done
TIMEOUT_S=${TIMEOUT_S:-}
JOB_AVAIL_GB=${JOB_AVAIL_GB:-100}
ARMS=${ARMS:-"tbf-cd tbs-cd tb-cd tb-ref tb-c tb-cd-nog tb-ct tb-ctd tb-od tb-ocd tb-cod tb-lifo tb-fifo tb-slack tb-c-slack tb-cd-sel1 tb-cd-adapt"}
# RCPSP_CBS_TB=0: the spec replaces the whole chain, so the numeric mode is switched off and the CSV shows the spec
REF="RCPSP_DR5=1 RCPSP_DOM_RULE=both RCPSP_HYBRID=1 RCPSP_SKYLINE=1 RCPSP_LAZY=1 RCPSP_CBS_TB=0 RCPSP_CBS_TT=1 RCPSP_CBS_TT_STEPS=16 RCPSP_CBS_PATHMAX=1 RCPSP_HYBRID_T=0.3 RCPSP_DR4=0"
# STACK=base: the same orders on the OLD BASE (DR4 on, T=0.5, no propagation, no pathmax) = the tie-break ALONE.
# Results then go to new_results/tb-base/<arm>/ and RUN_<arm>-base_<size>.csv.
STACK=${STACK:-ref}; SFX=""
if [ "$STACK" = base ]; then
  REF="RCPSP_DR5=1 RCPSP_DOM_RULE=both RCPSP_DR4=1 RCPSP_HYBRID=1 RCPSP_HYBRID_T=0.5 RCPSP_SKYLINE=1 RCPSP_LAZY=1 RCPSP_CBS_TB=0"; SFX="-base"
elif [ "$STACK" != ref ]; then echo "STACK must be ref or base"; exit 1; fi

log(){ echo "[$(date +%F_%H:%M:%S)] $*"; }
arm_spec(){ case $1 in
  tb-none) echo G ;;
  tb-ref) echo GTD ;;   tb-c) echo Gc ;;      tb-cd|tbs-cd|tbf-cd|tb-cd-sel1|tb-cd-adapt) echo GcD ;;
  tb-cd-nog) echo cD ;; tb-ct) echo GcT ;;    tb-ctd) echo GcTD ;;   tb-od) echo GoD ;;    tb-ocd) echo GocD ;;
  tb-cod) echo GcoD ;;  tb-lifo) echo GDN ;;  tb-fifo) echo GDn ;;   tb-slack) echo GDs ;; tb-c-slack) echo Gcs ;;
  *) return 1 ;;
esac; }
arm_extra(){ case $1 in tb-cd-sel1) echo "RCPSP_CBS_SEL=1" ;; tb-cd-adapt) echo "RCPSP_CBS_ADAPT_BRANCH=1" ;; *) echo "" ;; esac; }
arm_list(){ case $1 in tbf-*) echo "" ;; tbs-*) echo "$SCREEN" ;; *) echo "$PLATEAU" ;; esac; }
for A in $ARMS; do arm_spec "$A" > /dev/null || { log "unknown arm $A"; exit 1; }; done

# long jobs first (full J30, screen list), then every plateau arm on J60, then on J90
JOBS=""
for A in $ARMS; do case $A in tbf-*) JOBS="$JOBS $A:j30" ;; tbs-*) JOBS="$JOBS $A:j90 $A:j60" ;; esac; done
for sz in j60 j90; do for A in $ARMS; do case $A in tbf-*|tbs-*) ;; *) JOBS="$JOBS $A:$sz" ;; esac; done; done

# each arm writes into its own folder; the finished CSV is copied to $RES/RUN_<arm>_<size>.csv when the job ends
run_job(){ # <arm> <size>
  local d="$RES/tb$SFX/$1" lg="$LOGDIR/$1${SFX}_$2.log" csv only
  only=$(arm_list "$1"); [ -n "$ONLY_ALL" ] && only=$ONLY_ALL
  mkdir -p "$d"
  log "  start $1 $2 [spec $(arm_spec "$1") $(arm_extra "$1")]${only:+ only=$(basename "$only")}"
  env $REF RCPSP_CBS_TBSPEC="$(arm_spec "$1")" $(arm_extra "$1") RCPSP_RESULTS_DIR="$d" ${only:+RCPSP_ONLY=$only} \
      ${TIMEOUT_S:+RCPSP_TIMEOUT_S=$TIMEOUT_S} ./HOG2/RCPSP/$BIN_NAME $2 8 > "$lg" 2>&1
  local rc=$?
  csv=$(awk -F'-> ' '/Done ->/{print $2}' "$lg" | tail -1)
  [ -f "$csv" ] && cp "$csv" "$RES/RUN_$1${SFX}_$2.csv"
  log "  end   $1 $2 exit=$rc finished=$( [ -f "$csv" ] && awk -F, 'NR>1 && $13=="True"' "$csv" | wc -l )/$( [ -f "$csv" ] && awk 'NR>1' "$csv" | wc -l )"
}

log "starting: STACK=$STACK MAX_PAR=$MAX_PAR jobs:$JOBS"
for J in $JOBS; do A=${J%:*}; sz=${J#*:}
  while [ "$(jobs -rp | wc -l)" -ge "$MAX_PAR" ]; do wait -n; done
  while [ "$(avail)" -lt "$JOB_AVAIL_GB" ]; do log "MemAvailable $(avail) GB < $JOB_AVAIL_GB, holding $A $sz"; sleep 300; done
  run_job "$A" "$sz" &
  sleep 2
done
wait
log "=== all jobs done ==="

col(){ head -1 "$1" | tr ',' '\n' | grep -nx "$2" | cut -d: -f1; }

echo "=== CORRECTNESS GATE (must print nothing) ==="
for J in $JOBS; do r="${J%:*}${SFX}_${J#*:}"; f="$RES/RUN_$r.csv"; [ -f "$f" ] || continue
  awk -F, -v F="RUN_$r" 'NR>1 && $13=="True" && $5=="False"{print F": grp"$1" ex"$2" mk="$4" opt="$8}' "$f"
done

echo "=== EFFECTIVE FLAGS AND RESULT (rows: plateau 26 / 110, screen 391 / 384, j30 480) ==="
for J in $JOBS; do r="${J%:*}${SFX}_${J#*:}"; f="$RES/RUN_$r.csv"; [ -f "$f" ] || { echo "RUN_$r: MISSING"; continue; }
  fl=""; for c in cbsTbSpec cbsTB cbsSel cbsAdaptBranch hybridT useDR4 cbsTT ttSteps cbsPathmax; do
    i=$(col "$f" $c); [ -n "$i" ] && fl="$fl $c=$(awk -F, -v i="$i" 'NR==2{print $i}' "$f")"; done
  fin=$(awk -F, 'NR>1 && $13=="True"' "$f" | wc -l); tot=$(awk -F, 'NR>1' "$f" | wc -l)
  echo "RUN_$r: finished=$fin/$tot |$fl"
done
log "=== DONE ==="
