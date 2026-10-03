#!/bin/bash
# ─────────────────────────────────────────────────────────────────────────────
# Server run: the valid CBS, one reference configuration and one change at a time. Prepared 2026-10-02 from
# run_server_cpcbs.sh. VALID = no UB of any kind, no root solver, no SetDelay, no non-minimal delay (both delay
# rules lose optima: 62/24/9 and 5/3/0 wrong makespans in the 13-arm run).
#
# Where CBS stands (13-arm run): base 426 / 335 / 249. On J90, 231 are unsolved: 106 that NO solver proves in
# 300 s, 105 where CBS already has provenLB = optimum at the root (the f = C* plateau: a blind search for a
# conflict-free schedule) and 20 with a small bound gap. So the whole reachable J90 gap is the plateau.
# Local spot check on the 26 J60 plateau instances, 20 s cap, 0 wrong against Gurobi/SCIP-proven optima:
#   base 0 | + tie-break 8 | + T=0.3, DR4 off 13 | + propagation, pathmax (= REF) 18, most in < 0.1 s
#   REF with DR4 on 14 | REF at T=0.5 17 | REF at T=0.1 18 | REF with tie-break off 7, tie-break 1 14, tie-break 2 18
#
# BASE = cfg8 (prioritisation + HCBS + MDA), DR5 both, DR4, hybrid T=0.5, skyline, lazy
# REF  = BASE + tie-break 3 (deepest among equal f) + deadline propagation (16 steps) + pathmax + T=0.3 + DR4 off
#
#   arm            sizes        differs from REF by            the question it answers
#   cb-ref         j90 j60 j30  -                              the reference: predicted best on J60/J90
#   cb-ref-noprop  j90 j60      propagation + pathmax off      is the per-node propagation worth its cost at scale
#   cb-ref-dr4     j90 j60      DR4 on                         DR4 on or off in the final configuration
#   cb-ref-t01     j90 j60      T=0.1                          J90 went 239 / 249 / 270 for T = 0.7 / 0.5 / 0.3: lower still?
#   cb-j30-t11     j30          T=1.1, DR4 on                  J30 prefers a HIGH threshold (local: half the expansions)
#   cb-base        j30          BASE                           load canary: must be 428. Fewer = starved batch.
#
# Measured and NOT run: no-MDA branching (local J30 11/26 vs 25/26); min-cut bound (its whole lower-bound gain
# sits on the 106 instances nobody solves, 0-1 raised where an optimum is known); tie-break off / mode 2 on the
# plateau (answered by the spot check above: 7 and 18 = mode 3); insert-time pathmax, MDA refutation,
# conflict-selection modes, energetic check (no gain).
#
# 7 threads, one core each. Peak memory from measured peak RSS (J90 20.4, J60 15.1, J30 11.7 GB per thread):
#   first ~5 h   4 x 20.4 + three J30 threads 30.8 = 113 GB   (worst case, every thread at its peak at once)
#   to ~17 h     82 GB (J90 pass)                 to ~27 h   60 GB (J60 pass)
# J90 runs FIRST: the answer that matters is in after ~15 h, and the J60 tail can be killed if the box is needed.
#
# ARMS="..." picks arms. MIN_AVAIL_GB / WAIT_FOR gate the start as in the template.
# Spot check: SIZES="j60" ONLY=<file> TIMEOUT_S=10 overrides every arm's sizes / instance list / budget.
# UPLOAD  HOG2/RCPSP: *.h *.cpp run_server_cbsladder.sh
# ─────────────────────────────────────────────────────────────────────────────
set -u
REPO=${REPO:-$HOME/RCPSP_With_Petri_nets}
BIN_NAME=${BIN_NAME:-Driver_cbsladder}
cd "$REPO/HOG2/RCPSP" || { echo "cd failed"; exit 1; }

# Optional delayed start: WAIT_FOR = pgrep -f pattern of processes that must be gone first (e.g. the probe2 sweep),
# then wait until MemAvailable >= MIN_AVAIL_GB. Sources are built only after the wait.
WAIT_FOR=${WAIT_FOR:-}
MIN_AVAIL_GB=${MIN_AVAIL_GB:-0}
avail(){ awk '/MemAvailable/ {printf "%d", $2/1048576}' /proc/meminfo; }
if [ -n "$WAIT_FOR" ]; then
  echo "[$(date +%F_%H:%M:%S)] waiting for processes to finish: $WAIT_FOR"
  while pgrep -f "$WAIT_FOR" > /dev/null; do sleep 300; done
fi
while [ "$(avail)" -lt "$MIN_AVAIL_GB" ]; do echo "[$(date +%F_%H:%M:%S)] MemAvailable $(avail) GB < $MIN_AVAIL_GB"; sleep 300; done
echo "[$(date +%F_%H:%M:%S)] starting, MemAvailable $(avail) GB"

echo "=== BUILD ($(date)) ==="
g++ -std=c++20 -fopenmp -O3 -I../.. -I../gui/STUB Driver.cpp -o "$BIN_NAME" 2> build_cbsladder_err.txt \
  && echo "BUILD OK" || { echo "BUILD FAILED"; tail -40 build_cbsladder_err.txt; exit 1; }

cd "$REPO" || { echo "cd repo failed"; exit 1; }
RES=${RES:-$REPO/new_results}
LOGDIR=${LOGDIR:-$REPO/logs}
mkdir -p "$RES" "$LOGDIR"
BASE="RCPSP_DR5=1 RCPSP_DOM_RULE=both RCPSP_DR4=1 RCPSP_HYBRID=1 RCPSP_HYBRID_T=0.5 RCPSP_SKYLINE=1 RCPSP_LAZY=1"
PROP="RCPSP_CBS_TT=1 RCPSP_CBS_TT_STEPS=16 RCPSP_CBS_PATHMAX=1"
REF="RCPSP_CBS_TB=3 $PROP RCPSP_HYBRID_T=0.3 RCPSP_DR4=0"
ARMS=${ARMS:-"cb-ref cb-ref-noprop cb-ref-dr4 cb-ref-t01 cb-j30-t11 cb-base"}
SIZES=${SIZES:-}
ONLY=${ONLY:-}
TIMEOUT_S=${TIMEOUT_S:-}
[ -z "$ONLY" ] || [ -f "$ONLY" ] || { echo "ONLY=$ONLY not found"; exit 1; }

log(){ echo "[$(date +%F_%H:%M:%S)] $*"; }
RUNS=""
# one thread: <name> <"sizes in order"> <env on top of BASE; a later assignment wins>
thread(){ local name=$1 sizes=${SIZES:-$2} e=$3 sz
  for sz in $sizes; do RUNS="$RUNS ${name}_${sz}"; done
  ( for sz in $sizes; do
      log "  $name $sz"
      env $BASE $e RCPSP_RESULTS_DIR="$RES" ${ONLY:+RCPSP_ONLY=$ONLY} ${TIMEOUT_S:+RCPSP_TIMEOUT_S=$TIMEOUT_S} \
          ./HOG2/RCPSP/$BIN_NAME $sz 8 > "$LOGDIR/${name}_${sz}.log" 2>&1
    done ) &
  log "launched $name [$sizes] [$e] pid $!"; sleep 2; }

for A in $ARMS; do case $A in
  cb-ref)        thread cb-ref        "j90 j60" "$REF"
                 [ -z "$SIZES" ] && thread cb-ref "j30" "$REF" ;;
  cb-ref-noprop) thread cb-ref-noprop "j90 j60" "RCPSP_CBS_TB=3 RCPSP_HYBRID_T=0.3 RCPSP_DR4=0" ;;
  cb-ref-dr4)    thread cb-ref-dr4    "j90 j60" "$REF RCPSP_DR4=1" ;;
  cb-ref-t01)    thread cb-ref-t01    "j90 j60" "$REF RCPSP_HYBRID_T=0.1" ;;
  cb-j30-t11)    thread cb-j30-t11    "j30"     "RCPSP_CBS_TB=3 $PROP RCPSP_HYBRID_T=1.1" ;;
  cb-base)       thread cb-base       "j30"     "" ;;
  *) log "unknown arm $A"; exit 1 ;;
esac; done

log "waiting on all threads..."
wait
log "=== all threads done ==="

for r in $RUNS; do
  csv=$(awk -F'-> ' '/Done ->/{print $2}' "$LOGDIR/$r.log" | tail -1)
  [ -f "$csv" ] && cp "$csv" "$RES/RUN_$r.csv" || log "no CSV for $r (see $LOGDIR/$r.log)"
done

col(){ head -1 "$1" | tr ',' '\n' | grep -nx "$2" | cut -d: -f1; }

echo "=== CORRECTNESS GATE (must print nothing) ==="
for r in $RUNS; do f="$RES/RUN_$r.csv"; [ -f "$f" ] || continue
  awk -F, -v F="RUN_$r" 'NR>1 && $13=="True" && $5=="False"{print F": grp"$1" ex"$2" mk="$4" opt="$8}' "$f"
done

echo "=== EFFECTIVE FLAGS AND RESULT (rows must be 480) ==="
for r in $RUNS; do f="$RES/RUN_$r.csv"; [ -f "$f" ] || continue
  fl=""; for c in hybridT useDR4 cbsTT ttSteps cbsPathmax cbsTB timeoutS; do
    i=$(col "$f" $c); [ -n "$i" ] && fl="$fl $c=$(awk -F, -v i="$i" 'NR==2{print $i}' "$f")"; done
  fin=$(awk -F, 'NR>1 && $13=="True"' "$f" | wc -l); tot=$(awk -F, 'NR>1' "$f" | wc -l)
  echo "RUN_$r: finished=$fin/$tot |$fl"
done
log "=== DONE ==="
