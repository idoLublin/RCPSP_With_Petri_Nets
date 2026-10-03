#!/bin/bash
# ─────────────────────────────────────────────────────────────────────────────
# Server run: CP elements for CBS (CpCBS.h) on j30/j60/j90. Prepared 2026-09-13 from run_server_warmstart.sh.
# Run inside tmux on dsisco01; builds in HOG2/RCPSP, runs from the REPO ROOT (j30 under HOG2/RCPSP, j60/j90 at root).
#
# Base = cfg8 + DR5 both + DR4 + hybrid + skyline + lazy, NO SETDELAY and NO datasheet UB:
#   SetDelay loses optima (j30 wrong answers without UB) and the datasheet UB hides it by reporting the datasheet
#   value when OPEN empties — an honest incumbent would inherit that, so every arm here uses a complete base.
# 4 threads, each j30 -> j60 -> j90:
#   cbs-noub    base                        honest control, no incumbent
#   cbs-root    base + ROOT, ROOTFLOOR=0    step 1: incumbent = verified root seed (30000 SGS + 2 s probe),
#                                           closed at root when seed <= LB, prune makespan >= incumbent and f >= incumbent
#                                           (the root-LB h floor is kept for the TT arms, where it no longer costs nodes)
# Local, 10 searched hard j30 instances, 20 s: expansions noub 1646k | root+floor 2276k | root nofloor 1555k |
#   roottt 668k | roottt nofloor 691k | overload-only TT (RCPSP_CBS_TT_FULL=0) 2196k; 0 wrong everywhere; no extra finish at 20 s.
#   cbs-roottt  cbs-root + RCPSP_CBS_TT=1   step 2: deadline time-table propagation per node (D = incumbent-1, then f-raise)
#   cbs-rootng  cbs-roottt + RCPSP_CBS_NG=1 step 3: nogoods from time-table overload explanations
# New CBS CSV columns: cbsUbSource,rootDlbOn,rootSeedOn,rootProbeOn,rootLB,seedUB,rootSec,closedAtRoot,cbsTT,cbsNG,
# ttSteps,ttCalls,ttRaised,ttPrunedAtExpand,ttSec,ngLearned,ngHits,ngStored. Tags: rootdlb,seedub,probe,tt,ng.
#   cbs-roottt-lns  cbs-roottt + RCPSP_ROOT_LNS=3 (10 s large neighbourhood search on the root incumbent)
# ARMS="..." picks arms (default: cbs-noub cbs-root cbs-roottt); PASSES=0 skips the root-only rootbounds passes.
# UPLOAD  HOG2/RCPSP: Driver.cpp RCPSP.h CpCBS.h RootBounds.h run_server_cpcbs.sh
# ─────────────────────────────────────────────────────────────────────────────
set -u
REPO=~/RCPSP_With_Petri_nets
BIN_NAME=${BIN_NAME:-Driver_cpcbs}
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
g++ -std=c++20 -fopenmp -O3 -I../.. -I../gui/STUB Driver.cpp -o "$BIN_NAME" 2> build_cpcbs_err.txt \
  && echo "BUILD OK" || { echo "BUILD FAILED"; tail -40 build_cpcbs_err.txt; exit 1; }

cd "$REPO" || { echo "cd repo failed"; exit 1; }
mkdir -p new_results logs
BASE="RCPSP_DR5=1 RCPSP_DOM_RULE=both RCPSP_DR4=1 RCPSP_HYBRID=1 RCPSP_SKYLINE=1 RCPSP_LAZY=1"
ROOT="RCPSP_ROOT_DLB=1 RCPSP_ROOT_SEED=1 RCPSP_ROOT_PROBE=1 RCPSP_ROOT_PROBE_MS=2000 RCPSP_ROOT_SEED_ITERS=30000"
LN="RCPSP_ROOT_LNS=3 RCPSP_ROOT_LNS_MS=10000"
ARMS=${ARMS:-"cbs-noub cbs-root cbs-roottt"}
PASSES=${PASSES:-1}

log(){ echo "[$(date +%F_%H:%M:%S)] $*"; }
# one thread per arm: <name> <extra-env>, j30 -> j60 -> j90 sequentially
arm(){ ( for sz in j30 j60 j90; do
           log "  $1 $sz"
           env $BASE $2 ./HOG2/RCPSP/$BIN_NAME $sz 8 > "logs/$1_${sz}.log" 2>&1
         done ) &
       log "launched $1 [$2] pid $!"; sleep 2; }

for A in $ARMS; do case $A in
  cbs-noub)       arm cbs-noub   "" ;;
  cbs-root)       arm cbs-root   "$ROOT RCPSP_CBS_ROOTFLOOR=0" ;;
  cbs-roottt)     arm cbs-roottt "$ROOT RCPSP_CBS_TT=1" ;;
  cbs-roottt-lns) arm cbs-roottt-lns "$ROOT RCPSP_CBS_TT=1 $LN" ;;
  *) log "unknown arm $A"; exit 1 ;;
esac; done
# cbs-root: local ablation: the root-LB h floor alone RAISES expansions (plateau order); without it step 1 <= control
# cbs-rootng (RCPSP_CBS_NG=1) dropped for memory: locally identical expansions to cbs-roottt

# Root-only passes (no search, 1 thread, a few MB) replacing the stopped TT2 root/probe arms of the bi sweeps:
# their TT2 result = seq3 tt2fwd + instances closed at the root (ROOTB field 11). Output new_results/ROOTB_<size>_<setting>.csv
[ "$PASSES" = 1 ] && ( for spec in "root j60" "root j90" "probe j60" "probe j90"; do set -- $spec
    if [ "$1" = probe ]; then E="$ROOT"; else E="RCPSP_ROOT_DLB=1 RCPSP_ROOT_SEED=1"; fi
    log "  rootbounds $1 $2"
    env $E ./HOG2/RCPSP/$BIN_NAME rootbounds $2 1 48 2>/dev/null | grep '^ROOTB' > "new_results/ROOTB_$2_$1.csv"
  done ) &
[ "$PASSES" = 1 ] && log "launched rootbounds passes pid $!"

log "waiting on all threads..."
wait
log "=== all threads done ==="

# Archive each job's CSV (exact file is printed as 'Done -> ...' by the run).
grab(){ csv=$(awk -F'-> ' '/Done ->/{print $2}' "logs/$1.log" | tail -1); [ -f "$csv" ] && cp "$csv" "new_results/RUN_$1.csv"; }
for a in $ARMS; do for sz in j30 j60 j90; do grab "${a}_${sz}"; done; done

echo "=== CORRECTNESS GATE (must print nothing) ==="
awk -F, 'NR>1 && $13=="True" && $5=="False"{print FILENAME": grp"$1" ex"$2" mk="$4" opt="$8}' \
    new_results/RUN_cbs-*.csv 2>/dev/null

echo "=== SUMMARY (finished/total) ==="
for f in new_results/RUN_cbs-*.csv; do
  [ -f "$f" ] || continue
  fin=$(awk -F, 'NR>1 && $13=="True"' "$f" | wc -l); tot=$(awk -F, 'NR>1' "$f" | wc -l)
  echo "$(basename "$f"): finished=$fin/$tot"
done
log "=== DONE ==="
