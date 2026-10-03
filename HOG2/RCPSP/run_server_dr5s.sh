#!/bin/bash
# ─────────────────────────────────────────────────────────────────────────────
# Server run: decisive bp-vs-dr5s j30 coverage comparison with the sound stack.
# Prepared 2026-07-19. Run inside tmux on dsihead. Sound preset (NO leftshift/bidir):
#   RCPSP_DR5=1 RCPSP_DOM_RULE={bp|dr5s} RCPSP_HYBRID=1 RCPSP_UB=1 RCPSP_SKYLINE=1
#   (+ RCPSP_LAZY=1 for h=0 configs like cfg1)
# The CSV now records domRule/useLazy/useUB/... + per-rule prune counts, so the two
# cfg8 runs (bp vs dr5s) are distinguishable by the domRule column.
# NOTE: RCPSP_UB uses the datasheet UB (a known "cheat" — must go on-the-fly before
# the paper); it is applied identically to both rules so the comparison stays fair.
# ─────────────────────────────────────────────────────────────────────────────
set -u
cd ~/RCPSP_With_Petri_nets/HOG2/RCPSP || { echo "cd failed"; exit 1; }

echo "=== BUILD ($(date)) ==="
g++ -std=c++20 -fopenmp -O3 -I../.. -I../gui/STUB Driver.cpp -o Driver_dr5s 2> build_server_err.txt \
  && echo "BUILD OK" || { echo "BUILD FAILED"; tail -40 build_server_err.txt; exit 1; }

log(){ echo "[$(date +%F_%H:%M:%S)] $*"; }
SOUND="RCPSP_DR5=1 RCPSP_HYBRID=1 RCPSP_UB=1 RCPSP_SKYLINE=1"

# ---- j30 (the coverage goal). Size-increasing: j30 first; add j60/j90 later. ----
log "j30 cfg8 DR5S  (candidate best)"; env $SOUND RCPSP_DOM_RULE=dr5s ./Driver_dr5s j30 8
log "j30 cfg8 BP    (comparison baseline)"; env $SOUND RCPSP_DOM_RULE=bp ./Driver_dr5s j30 8
log "j30 cfg5 DR5S"; env $SOUND RCPSP_DOM_RULE=dr5s ./Driver_dr5s j30 5
log "j30 cfg1 DR5S + LAZY (h=0 config)"; env $SOUND RCPSP_DOM_RULE=dr5s RCPSP_LAZY=1 ./Driver_dr5s j30 1

echo "=== CORRECTNESS GATE (must print nothing) ==="
awk -F, 'NR>1 && $13=="True" && $5=="False"{print FILENAME": grp"$1" ex"$2" mk="$4" opt="$8}' \
    new_results/output_j30_cfg*_*.csv 2>/dev/null

echo "=== COVERAGE SUMMARY (finished per file) ==="
for f in new_results/output_j30_cfg1_*.csv new_results/output_j30_cfg5_*.csv new_results/output_j30_cfg8_*.csv; do
  [ -f "$f" ] || continue
  fin=$(awk -F, 'NR>1 && $13=="True"' "$f" | wc -l)
  rule=$(awk -F, 'NR==2{print $27}' "$f")   # domRule column
  echo "$f: finished=$fin/480 domRule=$rule"
done
log "=== DONE ==="
