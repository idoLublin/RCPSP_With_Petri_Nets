#!/bin/bash
# ─────────────────────────────────────────────────────────────────────────────
# New-baseline runs — best beneficial rules, WITH vs WITHOUT the max-heuristic (Θ).
# Prepared 2026-08-22. Run inside tmux on dsisco01 (after scp to dsihead).
#
# Only rules that HELP are on. Excluded (verified no-op / negative on our solvers):
#   order-swap (RCPSP_ORDERSWAP), immediate-selection (RCPSP_TT2_IMMSEL), TT2-sym.
#
# Two configs, each = 6 parallel jobs (CBS×3 sizes + TT2×3 sizes):
#   CONFIG A "with max-heuristic (Θ)"      : Θ ON  on both solvers
#   CONFIG B "without max-heuristic (Θ)"   : Θ OFF on both solvers
# ("max heuristic" = the Θ-tree max-combined bound: RCPSP_TT2_THETA on Petri,
#  its CBS counterpart RCPSP_CBS_THETA — the Θ floor max'd with HCBS.)
#
# FLAGS -----------------------------------------------------------------------
#  CBS common : RCPSP_DR5=1 RCPSP_DOM_RULE=dr5s RCPSP_HYBRID=1 RCPSP_UB=1
#               RCPSP_SKYLINE=1 RCPSP_LAZY=1 RCPSP_SETDELAY=1 RCPSP_DR4=1
#  CBS Θ      : RCPSP_CBS_THETA=1 (config A) / =0 (config B)
#  TT2 common : RCPSP_TT2_DR5=1 RCPSP_TT2_DR4=1 RCPSP_TT2_BATCH=1
#  TT2 Θ      : RCPSP_TT2_THETA=1 (config A) / =0 (config B)
#
# WARNING: 6 memory-heavy jobs in parallel (CBS j90 peaked ~25 GB). If dsisco01 is
# tight, comment out the j90 lines and run them as a second wave. Configs run
# sequentially (A then B) so at most 6 jobs are live at once.
#
# DIRS: j30 data under HOG2/RCPSP; j60/j90 at repo root — each job cd's accordingly.
# ─────────────────────────────────────────────────────────────────────────────
set -u
ROOT=~/RCPSP_With_Petri_nets
RD="$ROOT/HOG2/RCPSP"
cd "$RD" || { echo "cd failed"; exit 1; }

echo "=== BUILD ($(date)) ==="
g++ -std=c++20 -fopenmp -O3 -I../.. -I../gui/STUB Driver.cpp -o Driver_best 2> build_best_err.txt \
  && echo "BUILD OK" || { echo "BUILD FAILED"; tail -40 build_best_err.txt; exit 1; }
BIN="$RD/Driver_best"
L="$RD/logs"; mkdir -p "$L"

CBS="RCPSP_DR5=1 RCPSP_DOM_RULE=dr5s RCPSP_HYBRID=1 RCPSP_UB=1 RCPSP_SKYLINE=1 RCPSP_LAZY=1 RCPSP_SETDELAY=1 RCPSP_DR4=1"
TT2="RCPSP_TT2_DR5=1 RCPSP_TT2_DR4=1 RCPSP_TT2_BATCH=1"

run_config() {            # $1 = tag (thetaON|thetaOFF)  $2 = CBS_THETA  $3 = TT2_THETA
  local tag=$1 ct=$2 tt=$3
  echo "=== CONFIG $tag : CBS_THETA=$ct TT2_THETA=$tt ($(date)) ==="
  ( cd "$RD"   && env $CBS RCPSP_CBS_THETA=$ct "$BIN" j30 8   > "$L/cbs_j30_$tag.log" 2>&1 ) &
  ( cd "$ROOT" && env $CBS RCPSP_CBS_THETA=$ct "$BIN" j60 8   > "$L/cbs_j60_$tag.log" 2>&1 ) &
  ( cd "$ROOT" && env $CBS RCPSP_CBS_THETA=$ct "$BIN" j90 8   > "$L/cbs_j90_$tag.log" 2>&1 ) &
  ( cd "$RD"   && env $TT2 RCPSP_TT2_THETA=$tt "$BIN" tt2_j30 > "$L/tt2_j30_$tag.log" 2>&1 ) &
  ( cd "$ROOT" && env $TT2 RCPSP_TT2_THETA=$tt "$BIN" tt2_j60 > "$L/tt2_j60_$tag.log" 2>&1 ) &
  ( cd "$ROOT" && env $TT2 RCPSP_TT2_THETA=$tt "$BIN" tt2_j90 > "$L/tt2_j90_$tag.log" 2>&1 ) &
  wait
  # self-identifying copies of each arm's newest output
  cp "$(ls -t "$RD/new_results"/output_j30_cfg8_*.csv | head -1)"   "$RD/new_results/BEST_cbs_${tag}_j30.csv" 2>/dev/null
  cp "$(ls -t "$ROOT/new_results"/output_j60_cfg8_*.csv | head -1)" "$ROOT/new_results/BEST_cbs_${tag}_j60.csv" 2>/dev/null
  cp "$(ls -t "$ROOT/new_results"/output_j90_cfg8_*.csv | head -1)" "$ROOT/new_results/BEST_cbs_${tag}_j90.csv" 2>/dev/null
  cp "$(ls -t "$RD/new_results"/output_tt2_j30_*.csv | head -1)"    "$RD/new_results/BEST_tt2_${tag}_j30.csv" 2>/dev/null
  cp "$(ls -t "$ROOT/new_results"/output_tt2_j60_*.csv | head -1)"  "$ROOT/new_results/BEST_tt2_${tag}_j60.csv" 2>/dev/null
  cp "$(ls -t "$ROOT/new_results"/output_tt2_j90_*.csv | head -1)"  "$ROOT/new_results/BEST_tt2_${tag}_j90.csv" 2>/dev/null
  echo "=== CONFIG $tag done ($(date)) ==="
}

run_config thetaON  1 1
run_config thetaOFF 0 0

echo "=== CBS correctness gate (must print nothing) ==="
awk -F, 'NR>1 && $13=="True" && $5=="False"{print FILENAME": g"$1" e"$2" mk="$4" opt="$8}' \
   "$RD"/new_results/BEST_cbs_*_j30.csv "$ROOT"/new_results/BEST_cbs_*_j*0.csv 2>/dev/null
echo "=== COVERAGE (solved / 480) ==="
for f in "$RD"/new_results/BEST_cbs_*_j30.csv "$ROOT"/new_results/BEST_cbs_*_j60.csv "$ROOT"/new_results/BEST_cbs_*_j90.csv; do
  [ -f "$f" ] && awk -F, 'NR>1{n++; if($13=="True")s++} END{printf "%-48s solved=%d/%d\n",FILENAME,s,n}' "$f"; done
for f in "$RD"/new_results/BEST_tt2_*_j30.csv "$ROOT"/new_results/BEST_tt2_*_j60.csv "$ROOT"/new_results/BEST_tt2_*_j90.csv; do
  [ -f "$f" ] && awk -F, 'NR>1{n++; if($4=="True")s++} END{printf "%-48s solved=%d/%d\n",FILENAME,s,n}' "$f"; done
echo "=== ALL DONE ($(date)) ==="
