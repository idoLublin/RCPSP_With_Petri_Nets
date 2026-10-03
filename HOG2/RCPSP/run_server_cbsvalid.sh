#!/bin/bash
# ─────────────────────────────────────────────────────────────────────────────
# Server run: the strongest VALID CBS. Prepared 2026-09-28 from run_server_cpcbs.sh (same template).
# VALID here = no UB of any kind (no datasheet, no seed), no root solver, no left-shift. Only the search.
#
# Why: exactly ONE valid CBS configuration has ever been run at full scale — cfg8 + DR5(both) + DR4 + hybrid
# + lazy + skyline, j30 428 / j60 335 / j90 249 (CBS_j*_cfg8-both-hyb-lazy-sky_v01.csv, 0 wrong). Every other
# CBS feature we own has only ever been measured WITH a datasheet UB, with the root, or with SetDelay.
#
# BASE = cfg8 (prioritisation + HCBS heuristic + MDA) + DR5 both + DR4 + hybrid(T=0.5) + skyline + lazy.
#
# PASS 1 (default, 4 threads, peak ~11 GB each at j90 => ~44 GB):
#   cbs-base        BASE                         control, must reproduce 428 / 335 / 249
#   cbs-nodr4       BASE, RCPSP_DR4=0            DR4 on/off, the comparison asked for
#   cbs-sd          BASE + RCPSP_SETDELAY=1      the predicted strongest
#   cbs-sd-nodr4    BASE + SetDelay, no DR4
#   !! SetDelay is UNSOUND without a UB (measured: 4 wrong makespans in the j60 SetDelay run, and j30 4-6 of 24
#   !! in local tests). The correctness gate below WILL flag it. That is the point of running it: with no UB to
#   !! mask the loss, the wrong-makespan count is the measurement.
#
# PASS 2 — sound levers never measured without UB/root (7 threads, ~77 GB):
#   ARMS="cbs-nmd cbs-tt cbs-singleres cbs-dr5s cbs-t03 cbs-t05 cbs-t07" bash run_server_cbsvalid.sh
#   cbs-nmd         + RCPSP_NMD=1                non-minimal delay, post-fix: sound, ~3-4x fewer nodes
#   cbs-tt          + RCPSP_CBS_TT=1             per-node deadline time table (works standalone; rootLB floor = -1)
#   cbs-singleres   + RCPSP_CBS_SINGLERES=1      single-resource max LB
#
# PASS 3 - the two bounds worth a valid run (2 threads, ~22 GB):
#   ARMS="cbs-subset cbs-mincut" bash run_server_cbsvalid.sh
#   cbs-subset      + RCPSP_CBS_SUBSET=1         conflict-subset look-ahead: biggest node cut we ever measured
#   cbs-mincut      + RCPSP_CBS_MINCUT=1         min-cut resource LB - never measured at all
#   Theta and hierRS are NOT here: both were already measured worse, and nothing since would change that.
#
# !! GATING, and why these arms raise MAXDEPTH: each bound fires on `shallow || nearUB`, and nearUB needs a UB.
# !! With no UB the near-incumbent half of the gate is dead, so the defaults (depth <= 3/4/6) would only ever
# !! fire near the root. MAXDEPTH=10 keeps the bound alive deep enough to be worth measuring; the CSV columns
# !! (*Calls / *Better) say whether it actually fired - check those before reading any arm as "no effect".
#   cbs-dr5s        RCPSP_DOM_RULE=dr5s          dominance rule: dr5s instead of both
#   cbs-t03/t05/t07 RCPSP_HYBRID_T=0.3/0.5/0.7   cardinal-score threshold: below it we take the first conflict.
#                                                Every run on disk is T=0.5 (the default), so t05 repeats the
#                                                base config on purpose - a same-batch control for the sweep.
#
# DEFAULT = all 13 arms at once. 13 x 11.0 GB (measured CBS j90 peak) = 143 GB. With 150 GB held by other
# users on a 370 GB box that leaves ~77 GB free at peak, above the 50 GB floor. MIN_AVAIL_GB=195 (143 + the
# 50 GB reserve) makes the run WAIT instead of starting if someone else takes more memory first.
# NEEDS >= 13 FREE CORES: one instance per arm at a time, 300 s wall-clock each. If the box has fewer, the
# arms share cores, every arm gets less CPU inside the same 300 s, and the coverage numbers are not
# comparable to the archived runs. Check with `nproc` and the load average before launching; if it is tight,
# run the passes separately (ARMS=... as listed above).
# Each arm runs j30 -> j60 -> j90 sequentially. MIN_AVAIL_GB / WAIT_FOR gate the start as in the template.
# UPLOAD  HOG2/RCPSP: Driver.cpp RCPSP.h CpCBS.h RootBounds.h run_server_cbsvalid.sh
# ─────────────────────────────────────────────────────────────────────────────
set -u
REPO=~/RCPSP_With_Petri_nets
BIN_NAME=${BIN_NAME:-Driver_cbsvalid}
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
g++ -std=c++20 -fopenmp -O3 -I../.. -I../gui/STUB Driver.cpp -o "$BIN_NAME" 2> build_cbsvalid_err.txt \
  && echo "BUILD OK" || { echo "BUILD FAILED"; tail -40 build_cbsvalid_err.txt; exit 1; }

cd "$REPO" || { echo "cd repo failed"; exit 1; }
mkdir -p new_results logs
# cfg8 is passed as the config argument (8) below: prioritisation + HCBS heuristic + MDA.
BASE="RCPSP_DR5=1 RCPSP_DOM_RULE=both RCPSP_DR4=1 RCPSP_HYBRID=1 RCPSP_HYBRID_T=0.5 RCPSP_SKYLINE=1 RCPSP_LAZY=1"
ARMS=${ARMS:-"cbs-base cbs-nodr4 cbs-sd cbs-sd-nodr4 cbs-nmd cbs-tt cbs-singleres cbs-dr5s cbs-t03 cbs-t05 cbs-t07 cbs-subset cbs-mincut"}
# The sub-solving bounds (singleres / subset / hierRS) re-solve small RCPSPs per node; the h-value cache is a
# measured ~16% win and is sound, so every arm that sub-solves gets it.
HC="RCPSP_SUBSOLVE_H_CACHE=1"
# RS-adaptive gating exists to switch expensive bounds off at high RS, but with a single heuristic there is
# nothing to do differently per RS - so a bound arm runs on EVERY instance instead of only RS <= 0.6.
UNGATE="RCPSP_CBS_RS_THRESH=1.0"
# Per-node deadline propagation (CpCBS.h) at 16 refutation steps: the largest valid-CBS lever measured
# (j30 edge set: 9.76M -> 0.92M expansions, 0 wrong). The TT ladder arms below build on it.
TT="RCPSP_CBS_TT=1 RCPSP_CBS_TT_STEPS=16"

log(){ echo "[$(date +%F_%H:%M:%S)] $*"; }
# one thread per arm: <name> <extra-env>, j30 -> j60 -> j90 sequentially
arm(){ ( for sz in j30 j60 j90; do
           log "  $1 $sz"
           env $BASE $2 ./HOG2/RCPSP/$BIN_NAME $sz 8 > "logs/$1_${sz}.log" 2>&1
         done ) &
       log "launched $1 [$2] pid $!"; sleep 2; }

for A in $ARMS; do case $A in
  cbs-base)      arm cbs-base      "" ;;
  cbs-nodr4)     arm cbs-nodr4     "RCPSP_DR4=0" ;;
  cbs-sd)        arm cbs-sd        "RCPSP_SETDELAY=1" ;;
  cbs-sd-nodr4)  arm cbs-sd-nodr4  "RCPSP_SETDELAY=1 RCPSP_DR4=0" ;;
  cbs-nmd)       arm cbs-nmd       "RCPSP_NMD=1" ;;
  cbs-tt)        arm cbs-tt        "RCPSP_CBS_TT=1" ;;
  cbs-singleres) arm cbs-singleres "RCPSP_CBS_SINGLERES=1 RCPSP_CBS_SINGLERES_MAXDEPTH=10 $UNGATE $HC" ;;
  cbs-subset)    arm cbs-subset    "RCPSP_CBS_SUBSET=1 RCPSP_CBS_SUBSET_MAXDEPTH=10 $UNGATE $HC" ;;
  cbs-mincut)    arm cbs-mincut    "RCPSP_CBS_MINCUT=1 RCPSP_CBS_MINCUT_MAXDEPTH=10 $UNGATE" ;;
  cbs-dr5s)      arm cbs-dr5s      "RCPSP_DOM_RULE=dr5s" ;;
  cbs-t03)       arm cbs-t03       "RCPSP_HYBRID_T=0.3" ;;
  cbs-t05)       arm cbs-t05       "RCPSP_HYBRID_T=0.5" ;;
  cbs-t07)       arm cbs-t07       "RCPSP_HYBRID_T=0.7" ;;
  # ── TT ladder (2026-09-29): per-node deadline propagation at 16 steps, then the plateau / pathmax features ──
  cbs-tt16)      arm cbs-tt16      "$TT" ;;
  cbs-tt-tb3)    arm cbs-tt-tb3    "$TT RCPSP_CBS_TB=3" ;;
  cbs-tt-pm)     arm cbs-tt-pm     "$TT RCPSP_CBS_TB=3 RCPSP_CBS_PATHMAX=1" ;;
  cbs-tt-pm-sel1) arm cbs-tt-pm-sel1 "$TT RCPSP_CBS_TB=3 RCPSP_CBS_PATHMAX=1 RCPSP_CBS_SEL=1" ;;
  cbs-tt-pm-nmd) arm cbs-tt-pm-nmd "$TT RCPSP_CBS_TB=3 RCPSP_CBS_PATHMAX=1 RCPSP_NMD=1" ;;
  cbs-tt-subset) arm cbs-tt-subset "$TT RCPSP_CBS_TB=3 RCPSP_CBS_PATHMAX=1 RCPSP_CBS_SUBSET=1 RCPSP_CBS_SUBSET_MAXDEPTH=10 RCPSP_CBS_GATE_DEPTH=1 $UNGATE $HC" ;;
  cbs-tt-mincut) arm cbs-tt-mincut "$TT RCPSP_CBS_TB=3 RCPSP_CBS_PATHMAX=1 RCPSP_CBS_MINCUT=1 RCPSP_CBS_MINCUT_MAXDEPTH=10 RCPSP_CBS_GATE_DEPTH=1 $UNGATE" ;;
  *) log "unknown arm $A"; exit 1 ;;
esac; done

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
