#!/bin/bash
# ─────────────────────────────────────────────────────────────────────────────
# Server run: FEATURE SCREEN on the valid CBS. Prepared 2026-10-02 from run_server_cpcbs.sh.
# Every arm = REF + ONE feature, so each row answers one question. Compare against cb-ref of
# run_server_cbsladder.sh (same flags as REF) restricted to the same instance list.
#
# REF = cfg8 (prioritisation + HCBS + MDA), DR5 both, skyline, lazy, tie-break 3, deadline propagation (16 steps),
#       pathmax, hybrid T=0.3, DR4 off. VALID: no UB, no root solver, no SetDelay, no plain non-minimal delay.
#
# Instances: screen_j60_j90.txt = every J60/J90 instance EXCEPT those out of reach of any 300 s CBS variant
# (base did not solve it AND no optimum is known or the base bound is more than 5 below it): 391 of J60, 384 of J90.
# Kept: what base solves (regressions show), the plateau (26 / 110), bound gaps of 1..5 (30 / 25). The dropped
# 89 / 96 instances cost 300 s each and no arm of the 13-arm run, Gurobi or SCIP solved any of them.
#
#   arm          REF plus                               why it is here
#   -- ablation of REF itself (the paper's table needs these at scale) --
#   sc-tb0       tie-break off                          the plateau lever alone (local J60 plateau: 7 vs 18 of 26)
#   sc-nodr5     DR5 off (no dominance at all)          what cutset dominance is worth in the final configuration
#   sc-nomda     cfg5: singular branching, no MDA       the paper's ablation has no-MDA ahead on J90; never run valid
#   sc-t11       T=1.1 (always the earliest conflict)   cardinal prioritisation off
#   sc-t05       T=0.5                                  the old default threshold, with the new stack
#   -- candidates that were only ever measured on J30, where they lost (J30 does not predict J60/J90) --
#   sc-nmdprec   non-minimal delay, dual emission       the SOUND form of the delay idea: minimal child kept as well
#   sc-er        energetic check after propagation      J30: -4% expansions at 3.7x node cost
#   sc-ng        nogood learning                        J30: no effect
#   sc-sel1/2    conflict-selection modes 1 / 2         J30: +3% / +19% expansions
#   sc-adapt     adaptive singular branching            J30: much worse
#   sc-dr5s      DR5S instead of both                   13-arm run without the stack: 426/335/252 vs 426/335/249
#   sc-tt2       propagation at 2 steps instead of 16   cheaper per node
#   -- bounds, now behind a depth gate that works (they ran at EVERY node in the 13-arm run and lost on speed) --
#   sc-mincut    min-cut bound, depth <= 10             raised the bound on 41 / 14 small-gap instances, reached C* on 0
#   sc-subset    conflict-subset bound, depth <= 10     raised it on 4 / 2
#
# Jobs = (arm, size); every arm's J90 first, then J60; MAX_PAR at once (default 6), one core each.
# Memory: 20.4 GB per job (measured CBS J90 peak RSS; REF-type arms have been running at 3.5-9 GB) =>
# MAX_PAR x 20.4 = 122 GB worst case. A new job also waits for MemAvailable >= JOB_AVAIL_GB (default 75).
# Time: an arm is about 6 h on J90 + 3.5 h on J60 (worst case 11 + 5); 15 arms at 6 in parallel ~ 24 h.
# Each job's CSV is copied to new_results/RUN_<arm>_<size>.csv AS SOON AS THAT JOB ENDS.
# ARMS="..." picks arms, SIZES="j90 j60", MAX_PAR, ONLY=<file>, TIMEOUT_S for a spot check.
# UPLOAD  HOG2/RCPSP: *.h *.cpp run_server_cbsscreen.sh screen_j60_j90.txt
# ─────────────────────────────────────────────────────────────────────────────
set -u
REPO=${REPO:-$HOME/RCPSP_With_Petri_nets}
BIN_NAME=${BIN_NAME:-Driver_cbsscreen}
cd "$REPO/HOG2/RCPSP" || { echo "cd failed"; exit 1; }

# Optional delayed start: WAIT_FOR = pgrep -f pattern of processes that must be gone first (e.g. the probe2 sweep),
# then wait until MemAvailable >= MIN_AVAIL_GB. Sources are built only after the wait.
WAIT_FOR=${WAIT_FOR:-}
MAX_PAR=${MAX_PAR:-6}
MIN_AVAIL_GB=${MIN_AVAIL_GB:-$(( MAX_PAR * 21 + 50 ))}
avail(){ awk '/MemAvailable/ {printf "%d", $2/1048576}' /proc/meminfo; }
if [ -n "$WAIT_FOR" ]; then
  echo "[$(date +%F_%H:%M:%S)] waiting for processes to finish: $WAIT_FOR"
  while pgrep -f "$WAIT_FOR" > /dev/null; do sleep 300; done
fi
while [ "$(avail)" -lt "$MIN_AVAIL_GB" ]; do echo "[$(date +%F_%H:%M:%S)] MemAvailable $(avail) GB < $MIN_AVAIL_GB"; sleep 300; done
echo "[$(date +%F_%H:%M:%S)] starting, MemAvailable $(avail) GB"

echo "=== BUILD ($(date)) ==="
g++ -std=c++20 -fopenmp -O3 -I../.. -I../gui/STUB Driver.cpp -o "$BIN_NAME" 2> build_cbsscreen_err.txt \
  && echo "BUILD OK" || { echo "BUILD FAILED"; tail -40 build_cbsscreen_err.txt; exit 1; }

cd "$REPO" || { echo "cd repo failed"; exit 1; }
RES=${RES:-$REPO/new_results}
LOGDIR=${LOGDIR:-$REPO/logs}
mkdir -p "$RES" "$LOGDIR"
ONLY=${ONLY:-$REPO/HOG2/RCPSP/screen_j60_j90.txt}
[ -f "$ONLY" ] || { echo "missing $ONLY"; exit 1; }
SIZES=${SIZES:-"j90 j60"}
TIMEOUT_S=${TIMEOUT_S:-}
JOB_AVAIL_GB=${JOB_AVAIL_GB:-75}
ARMS=${ARMS:-"sc-tb0 sc-nmdprec sc-nomda sc-nodr5 sc-t11 sc-t05 sc-er sc-ng sc-sel1 sc-sel2 sc-adapt sc-dr5s sc-tt2 sc-mincut sc-subset"}
REF="RCPSP_DR5=1 RCPSP_DOM_RULE=both RCPSP_HYBRID=1 RCPSP_SKYLINE=1 RCPSP_LAZY=1 RCPSP_CBS_TB=3 RCPSP_CBS_TT=1 RCPSP_CBS_TT_STEPS=16 RCPSP_CBS_PATHMAX=1 RCPSP_HYBRID_T=0.3 RCPSP_DR4=0"
GATE="RCPSP_CBS_GATE_DEPTH=1 RCPSP_CBS_RS_THRESH=1.0"

log(){ echo "[$(date +%F_%H:%M:%S)] $*"; }
# the one change of each arm, on top of REF (a later assignment wins)
arm_env(){ case $1 in
  sc-tb0)     echo "RCPSP_CBS_TB=0" ;;
  sc-nodr5)   echo "RCPSP_DR5=0" ;;
  sc-nomda)   echo "" ;;
  sc-t11)     echo "RCPSP_HYBRID_T=1.1" ;;
  sc-t05)     echo "RCPSP_HYBRID_T=0.5" ;;
  sc-nmdprec) echo "RCPSP_NMD=1 RCPSP_NMD_PREC=1" ;;
  sc-er)      echo "RCPSP_CBS_TT_ER=1" ;;
  sc-ng)      echo "RCPSP_CBS_NG=1" ;;
  sc-sel1)    echo "RCPSP_CBS_SEL=1" ;;
  sc-sel2)    echo "RCPSP_CBS_SEL=2" ;;
  sc-adapt)   echo "RCPSP_CBS_ADAPT_BRANCH=1" ;;
  sc-dr5s)    echo "RCPSP_DOM_RULE=dr5s" ;;
  sc-tt2)     echo "RCPSP_CBS_TT_STEPS=2" ;;
  sc-mincut)  echo "RCPSP_CBS_MINCUT=1 RCPSP_CBS_MINCUT_MAXDEPTH=10 $GATE" ;;
  sc-subset)  echo "RCPSP_CBS_SUBSET=1 RCPSP_CBS_SUBSET_MAXDEPTH=10 RCPSP_SUBSOLVE_H_CACHE=1 $GATE" ;;
  *)          return 1 ;;
esac; }
arm_cfg(){ [ "$1" = sc-nomda ] && echo 5 || echo 8; }
for A in $ARMS; do arm_env "$A" > /dev/null || { log "unknown arm $A"; exit 1; }; done

# Several arms share a result-file name (the name does not carry every flag), so each arm writes into its own
# folder; the finished CSV is copied to $RES/RUN_<arm>_<size>.csv when the job ends.
run_job(){ # <arm> <size>
  local d="$RES/screen/$1" lg="$LOGDIR/$1_$2.log" csv
  mkdir -p "$d"
  log "  start $1 $2 [$(arm_env "$1")]"
  env $REF $(arm_env "$1") RCPSP_RESULTS_DIR="$d" RCPSP_ONLY="$ONLY" ${TIMEOUT_S:+RCPSP_TIMEOUT_S=$TIMEOUT_S} \
      ./HOG2/RCPSP/$BIN_NAME $2 $(arm_cfg "$1") > "$lg" 2>&1
  local rc=$?
  csv=$(awk -F'-> ' '/Done ->/{print $2}' "$lg" | tail -1)
  [ -f "$csv" ] && cp "$csv" "$RES/RUN_$1_$2.csv"
  log "  end   $1 $2 exit=$rc -> ${csv:-NO CSV (see $lg)}"
}

log "starting: ARMS=[$ARMS] SIZES=[$SIZES] MAX_PAR=$MAX_PAR ONLY=$ONLY"
for sz in $SIZES; do for A in $ARMS; do
  while [ "$(jobs -rp | wc -l)" -ge "$MAX_PAR" ]; do wait -n; done
  while [ "$(avail)" -lt "$JOB_AVAIL_GB" ]; do log "MemAvailable $(avail) GB < $JOB_AVAIL_GB, holding $A $sz"; sleep 300; done
  run_job "$A" "$sz" &
  sleep 2
done; done
wait
log "=== all jobs done ==="

col(){ head -1 "$1" | tr ',' '\n' | grep -nx "$2" | cut -d: -f1; }

echo "=== CORRECTNESS GATE (must print nothing) ==="
for sz in $SIZES; do for A in $ARMS; do f="$RES/RUN_${A}_${sz}.csv"; [ -f "$f" ] || continue
  awk -F, -v F="RUN_${A}_${sz}" 'NR>1 && $13=="True" && $5=="False"{print F": grp"$1" ex"$2" mk="$4" opt="$8}' "$f"
done; done

echo "=== EFFECTIVE FLAGS AND RESULT (rows: 384 on j90, 391 on j60) ==="
for sz in $SIZES; do for A in $ARMS; do f="$RES/RUN_${A}_${sz}.csv"; [ -f "$f" ] || { echo "RUN_${A}_${sz}: MISSING"; continue; }
  fl=""; for c in useMDASets useDR5 domRule hybridT useDR4 cbsTT ttSteps cbsPathmax cbsTB cbsSel cbsAdaptBranch cbsTtEr cbsNG useNonMinimalDelay useNmdPrecedence useCbsMinCut useSubsetLB cbsGateDepth; do
    i=$(col "$f" $c); [ -n "$i" ] && fl="$fl $c=$(awk -F, -v i="$i" 'NR==2{print $i}' "$f")"; done
  fin=$(awk -F, 'NR>1 && $13=="True"' "$f" | wc -l); tot=$(awk -F, 'NR>1' "$f" | wc -l)
  echo "RUN_${A}_${sz}: finished=$fin/$tot |$fl"
done; done
log "=== DONE ==="
