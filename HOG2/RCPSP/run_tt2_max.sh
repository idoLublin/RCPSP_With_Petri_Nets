#!/bin/bash
# ─────────────────────────────────────────────────────────────────────────────
# TT2 run: BATCH + DRs (DR4+DR5) + the MAX heuristic.
# "max heuristic" = main-repo TT2's ALWAYS-ON base bound  base = max(CP, RC)
#   CP = getForwardHcost (critical path), RC = getforwardResource (resource capacity)
#   (RCPSP.h). This is the default heuristic — Θ is NOT part of it and is left OFF.
# This is the standing default config (DR4+DR5+Batch) with the max(CP,RC) heuristic.
# Run inside tmux on dsisco01 (after scp to dsihead). All three sizes in parallel.
#
# FLAGS: RCPSP_TT2_DR5=1 RCPSP_TT2_DR4=1 RCPSP_TT2_BATCH=1   (Θ off => h = max(CP,RC))
# DIRS : j30 data under HOG2/RCPSP; j60/j90 at the repo root — each job cd's there.
# ─────────────────────────────────────────────────────────────────────────────
set -u
ROOT=~/RCPSP_With_Petri_nets
RD="$ROOT/HOG2/RCPSP"
cd "$RD" || { echo "cd failed"; exit 1; }

echo "=== BUILD ($(date)) ==="
g++ -std=c++20 -fopenmp -O3 -I../.. -I../gui/STUB Driver.cpp -o Driver_tt2max 2> build_tt2max_err.txt \
  && echo "BUILD OK" || { echo "BUILD FAILED"; tail -40 build_tt2max_err.txt; exit 1; }
BIN="$RD/Driver_tt2max"
L="$RD/logs"; mkdir -p "$L"

TT2="RCPSP_TT2_DR5=1 RCPSP_TT2_DR4=1 RCPSP_TT2_BATCH=1"

echo "=== TT2 batch+DRs+max(CP,RC) — 3 sizes in parallel ($(date)) ==="
( cd "$RD"   && env $TT2 "$BIN" tt2_j30 > "$L/tt2_max_j30.log" 2>&1 ) &
( cd "$ROOT" && env $TT2 "$BIN" tt2_j60 > "$L/tt2_max_j60.log" 2>&1 ) &
( cd "$ROOT" && env $TT2 "$BIN" tt2_j90 > "$L/tt2_max_j90.log" 2>&1 ) &
wait

cp "$(ls -t "$RD/new_results"/output_tt2_j30_*.csv 2>/dev/null | head -1)"    "$RD/new_results/TT2_max_j30.csv" 2>/dev/null
cp "$(ls -t "$ROOT/new_results"/output_tt2_j60_*.csv 2>/dev/null | head -1)"  "$ROOT/new_results/TT2_max_j60.csv" 2>/dev/null
cp "$(ls -t "$ROOT/new_results"/output_tt2_j90_*.csv 2>/dev/null | head -1)"  "$ROOT/new_results/TT2_max_j90.csv" 2>/dev/null

echo "=== COVERAGE (solved / 480) ==="
for f in "$RD"/new_results/TT2_max_j30.csv "$ROOT"/new_results/TT2_max_j60.csv "$ROOT"/new_results/TT2_max_j90.csv; do
  [ -f "$f" ] && awk -F, 'NR>1{n++; if($4=="True")s++} END{printf "%-40s solved=%d/%d\n",FILENAME,s,n}' "$f"; done
echo "=== ALL DONE ($(date)) ==="
