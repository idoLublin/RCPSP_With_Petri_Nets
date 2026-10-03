#!/bin/bash
# ─────────────────────────────────────────────────────────────────────────────
# Reversed-instance BACKWARD — full benchmark, 3 arms (j30/j60/j90).
# The new backward = the fast forward solver run on the precedence-reversed net
# (same optima). Modes tt2rev_j30 / tt2rev_j60 / tt2rev_j90 (Driver.cpp), each
# writes new_results/output_tt2rev_<size>_*.csv. Same TT2 flags as the forward best.
# Run inside tmux on dsisco01 (after scp to dsihead). 300s/instance (server default).
#
# DIRS: j30 data under HOG2/RCPSP; j60/j90 at repo ROOT — each job cd's accordingly
# (this is why the j60/j90 arms MUST run from $ROOT, not $RD).
# ─────────────────────────────────────────────────────────────────────────────
set -u
ROOT=~/RCPSP_With_Petri_nets
RD="$ROOT/HOG2/RCPSP"
cd "$RD" || { echo "cd failed"; exit 1; }

echo "=== BUILD ($(date)) ==="
g++ -std=c++20 -fopenmp -O3 -I../.. -I../gui/STUB Driver.cpp -o Driver_bwd 2> build_bwd_err.txt \
  && echo "BUILD OK" || { echo "BUILD FAILED"; tail -40 build_bwd_err.txt; exit 1; }
BIN="$RD/Driver_bwd"
L="$RD/logs"; mkdir -p "$L"

TT2="RCPSP_TT2_DR5=1 RCPSP_TT2_DR4=1 RCPSP_TT2_BATCH=1"

echo "=== BACKWARD arms ($(date)) ==="
( cd "$RD"   && env $TT2 "$BIN" tt2rev_j30 > "$L/tt2rev_j30.log" 2>&1 ) &
( cd "$ROOT" && env $TT2 "$BIN" tt2rev_j60 > "$L/tt2rev_j60.log" 2>&1 ) &
( cd "$ROOT" && env $TT2 "$BIN" tt2rev_j90 > "$L/tt2rev_j90.log" 2>&1 ) &
wait

# self-identifying copies of each arm's newest output
cp "$(ls -t "$RD/new_results"/output_tt2rev_j30_*.csv | head -1)"   "$RD/new_results/BWD_tt2rev_j30.csv" 2>/dev/null
cp "$(ls -t "$ROOT/new_results"/output_tt2rev_j60_*.csv | head -1)" "$ROOT/new_results/BWD_tt2rev_j60.csv" 2>/dev/null
cp "$(ls -t "$ROOT/new_results"/output_tt2rev_j90_*.csv | head -1)" "$ROOT/new_results/BWD_tt2rev_j90.csv" 2>/dev/null

echo "=== COVERAGE (solved / 480) ==="
for f in "$RD"/new_results/BWD_tt2rev_j30.csv "$ROOT"/new_results/BWD_tt2rev_j60.csv "$ROOT"/new_results/BWD_tt2rev_j90.csv; do
  [ -f "$f" ] && awk -F, 'NR>1{n++; if($4=="True")s++} END{printf "%-40s solved=%d/%d\n",FILENAME,s,n}' "$f"; done
echo "=== ALL DONE ($(date)) ==="
