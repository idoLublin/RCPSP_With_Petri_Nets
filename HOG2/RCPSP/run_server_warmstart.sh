#!/bin/bash
# ─────────────────────────────────────────────────────────────────────────────
# Server run: SETDELAY + WARM START on j30/j60/j90 (no baseline arm, no j120).
# Prepared 2026-08-19. Run inside tmux on dsisco01, from the REPO ROOT so getRCPSP
# resolves j30 (under HOG2/RCPSP) and j60/j90 (at repo root) in one invocation.
#
# CSV now records rootF (init f) and provenLB (= rootMk + proven LB; == makespan
# when solved), plus warmstartEngaged/warmstartInflMk — so k/RS are judged on
# nodes/time/bound tightness, not coverage alone. All arms are cfg8 (SETDELAY is
# MDA-only; warm start layered on the same strong config). Pure search changes =>
# optimal: the correctness gate MUST print nothing.
#
# Threads (8 concurrent at peak, all single-threaded full-480 cfg8 runs):
#   3 setdelay   : RCPSP_SETDELAY=1                     , one per size, parallel (j30/j60/j90)
#   4 warmstart  : one thread per RS in {$RSS}, each running j30->j60->j90 SEQUENTIALLY
#   1 dr4        : RCPSP_DR4=1 (active-schedule/left-shift dominance), j30->j60->j90 SEQUENTIAL
# NOTE: we sweep RS TARGETS, not a flat k multiplier. A fixed k gives RS' ~ rho+k*r,
# so k>=2 saturates every RS>=0.5 instance (RS'>=1 => capacity >= CPM peak => the
# warm start degenerates to the resource-BLIND schedule). RS targets stay <1 by
# construction, so they never degenerate. All targets below are < 1 on purpose.
# ─────────────────────────────────────────────────────────────────────────────
set -u
REPO=~/RCPSP_With_Petri_nets
cd "$REPO/HOG2/RCPSP" || { echo "cd failed"; exit 1; }

echo "=== BUILD ($(date)) ==="
g++ -std=c++20 -fopenmp -O3 -I../.. -I../gui/STUB Driver.cpp -o Driver_ws 2> build_ws_err.txt \
  && echo "BUILD OK" || { echo "BUILD FAILED"; tail -40 build_ws_err.txt; exit 1; }

cd "$REPO" || { echo "cd repo failed"; exit 1; }
mkdir -p new_results logs
SOUND="RCPSP_DR5=1 RCPSP_HYBRID=1 RCPSP_UB=1 RCPSP_SKYLINE=1 RCPSP_DOM_RULE=dr5s RCPSP_LAZY=1"
RSS="0.4 0.55 0.7 0.8"   # warm-start RS targets to sweep (all < 1; 0.8 = loose, almost always engages)
WSBUD="RCPSP_WARMSTART_BUDGET_S=0"   # 0 => inflated solve gets the FULL timeout, as a separate budget; its time is recorded (warmstartSec)

log(){ echo "[$(date +%F_%H:%M:%S)] $*"; }
# parallel job: <name> <extra-env> <size>
launch(){ ( env $SOUND $2 ./HOG2/RCPSP/Driver_ws $3 8 > "logs/$1.log" 2>&1 ) &
          log "launched $1 [$3 | $2] pid $!"; sleep 2; }

# ── setdelay: 3 threads, all sizes ──
launch sd_j30 "RCPSP_SETDELAY=1" j30
launch sd_j60 "RCPSP_SETDELAY=1" j60
launch sd_j90 "RCPSP_SETDELAY=1" j90

# ── warm start: one thread PER RS target, each running j30 -> j60 -> j90 sequentially ──
for rs in $RSS; do
  ( for sz in j30 j60 j90; do
      log "  ws RS=$rs $sz"
      env $SOUND RCPSP_WARMSTART=1 RCPSP_WARMSTART_RS=$rs $WSBUD \
          ./HOG2/RCPSP/Driver_ws $sz 8 > "logs/wsrs_${rs}_${sz}.log" 2>&1
    done ) &
  log "launched ws RS=$rs (j30->j60->j90 sequential) pid $!"; sleep 2
done

# ── DR4 (active-schedule / left-shift dominance): one thread, j30->j60->j90 sequential ──
( for sz in j30 j60 j90; do
    log "  dr4 $sz"
    env $SOUND RCPSP_DR4=1 ./HOG2/RCPSP/Driver_ws $sz 8 > "logs/dr4_${sz}.log" 2>&1
  done ) &
log "launched dr4 (j30->j60->j90 sequential) pid $!"; sleep 2

log "waiting on all threads..."
wait
log "=== all threads done ==="

# Archive each job's CSV (exact file is printed as 'Done -> ...' by the run).
grab(){ csv=$(awk -F'-> ' '/Done ->/{print $2}' "logs/$1.log" | tail -1); [ -f "$csv" ] && cp "$csv" "new_results/RUN_$2.csv"; }
for n in sd_j30 sd_j60 sd_j90; do grab "$n" "$n"; done
for rs in $RSS; do for sz in j30 j60 j90; do grab "wsrs_${rs}_${sz}" "wsrs_${rs}_${sz}"; done; done
for sz in j30 j60 j90; do grab "dr4_${sz}" "dr4_${sz}"; done

echo "=== CORRECTNESS GATE (must print nothing) ==="
awk -F, 'NR>1 && $13=="True" && $5=="False"{print FILENAME": grp"$1" ex"$2" mk="$4" opt="$8}' \
    new_results/RUN_*.csv 2>/dev/null

echo "=== SUMMARY (finished/total) ==="
for f in new_results/RUN_*.csv; do
  [ -f "$f" ] || continue
  fin=$(awk -F, 'NR>1 && $13=="True"' "$f" | wc -l); tot=$(awk -F, 'NR>1' "$f" | wc -l)
  echo "$(basename "$f"): finished=$fin/$tot"
done
log "=== DONE ==="
