#!/bin/bash
# ─────────────────────────────────────────────────────────────────────────────
# Server run: non-minimal delay (sound) vs plain minimal delay, cfg8, j30/j60/j90.
# Prepared 2026-08-20. Run inside tmux on dsisco01 (after scp to dsihead).
#
# WHAT / WHY -------------------------------------------------------------------
# Non-minimal delay = when resolving a resource conflict, delay each MDA member to
# the earliest complement-freeing event where it actually co-fits (not the single
# min-push). Two issues found + handled:
#   bug1 (self-loop): compute_nonminimal_delay built candidates from the member's
#        OWN start, so a participant that started before the conflict never moved.
#        FIXED (candidates now come only from complement finishes).
#   bug2 (jump skips the intermediate re-branch): handled by conditional dual-
#        emission (RCPSP_NMD_PREC=1) — emit the minimal-delay child alongside the
#        non-minimal one ONLY when they differ = completeness anchor, near-free.
# SOUND config = RCPSP_NMD=1 RCPSP_NMD_PREC=1 (verified 0/108 finished-suboptimal
# on j30 groups 1-48 x exams 1-3, and 0 under DR5+UB). Pure RCPSP_NMD=1 alone is
# FAST but UNSOUND (~1/112 wrong) — never ship alone. Only cfg8 (MDA path) carries
# the anchor, so this is cfg8 only.
#
# DIRECTORIES: j30 data lives under HOG2/RCPSP; j60/j90 data lives at the REPO
# ROOT — so each size runs from its own dir (outputs go to that dir's new_results).
# Sizes run smallest-first, so j30 (the coverage goal) lands before the long
# j60/j90. You can Ctrl-C / detach after any size.
# ─────────────────────────────────────────────────────────────────────────────
set -u
ROOT=~/RCPSP_With_Petri_nets
RCPSPDIR="$ROOT/HOG2/RCPSP"
cd "$RCPSPDIR" || { echo "cd failed"; exit 1; }

echo "=== BUILD ($(date)) ==="
g++ -std=c++20 -fopenmp -O3 -I../.. -I../gui/STUB Driver.cpp -o Driver_nmd 2> build_server_err.txt \
  && echo "BUILD OK" || { echo "BUILD FAILED"; tail -40 build_server_err.txt; exit 1; }
BIN="$RCPSPDIR/Driver_nmd"

log(){ echo "[$(date +%F_%H:%M:%S)] $*"; }
SOUND="RCPSP_DR5=1 RCPSP_DOM_RULE=dr5s RCPSP_HYBRID=1 RCPSP_UB=1 RCPSP_SKYLINE=1"

run_arm(){ # <type> <workdir> <extra-env> <tag>
  local type=$1 wd=$2 extra=$3 tag=$4
  cd "$wd" || { echo "cd $wd failed"; return 1; }
  log "$type cfg8 ($tag): $extra   [cwd=$wd]"
  env $SOUND $extra "$BIN" "$type" 8
  local latest; latest=$(ls -t new_results/output_${type}_cfg8_*.csv 2>/dev/null | head -1)
  [ -n "$latest" ] && mv "$latest" "new_results/output_${type}_cfg8_${tag}.csv"
}

# ---- smallest-first: j30 (HOG2/RCPSP), then j60, j90 (repo root) ----
run_arm j30 "$RCPSPDIR" "RCPSP_NMD=0"                  min
run_arm j30 "$RCPSPDIR" "RCPSP_NMD=1 RCPSP_NMD_PREC=1" nmd
run_arm j60 "$ROOT"     "RCPSP_NMD=0"                  min
run_arm j60 "$ROOT"     "RCPSP_NMD=1 RCPSP_NMD_PREC=1" nmd
run_arm j90 "$ROOT"     "RCPSP_NMD=0"                  min
run_arm j90 "$ROOT"     "RCPSP_NMD=1 RCPSP_NMD_PREC=1" nmd

echo ""
echo "=== CORRECTNESS GATE (must print nothing: optimal-known + finished + wrong) ==="
# cols: 1=group 2=exam 4=makespan 5=correct 8=optimalOrLB 13=finished
for d in "$RCPSPDIR/new_results" "$ROOT/new_results"; do
  awk -F, 'NR>1 && $13=="True" && $5=="False"{print FILENAME": grp"$1" ex"$2" mk="$4" opt="$8}' \
      "$d"/output_j*0_cfg8_min.csv "$d"/output_j*0_cfg8_nmd.csv 2>/dev/null
done

echo ""
echo "=== COVERAGE + NODES (finished, total expands) ==="
for d in "$RCPSPDIR/new_results" "$ROOT/new_results"; do
  for f in "$d"/output_j30_cfg8_min.csv "$d"/output_j30_cfg8_nmd.csv \
           "$d"/output_j60_cfg8_min.csv "$d"/output_j60_cfg8_nmd.csv \
           "$d"/output_j90_cfg8_min.csv "$d"/output_j90_cfg8_nmd.csv; do
    [ -f "$f" ] || continue
    awk -F, 'NR>1{ if($13=="True") fin++; ex+=$14 } END{
      printf "%-52s finished=%d  total_expands=%d\n", FILENAME, fin, ex }' "$f"
  done
done
log "=== DONE ==="
