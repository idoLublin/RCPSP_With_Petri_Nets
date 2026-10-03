#!/bin/bash
# ─────────────────────────────────────────────────────────────────────────────
# 3 CONFIGURATIONS ON J30, J60, J90 — ALL 9 JOBS RUN CONCURRENTLY:
#   1. CBS main + CBS subsolver   (sr_cbs, HCBS off)
#   2. TT2 main + TT2 subsolver   (sr_tt2)
#   3. TT2 main + CBS subsolver   (sr_cbs, HCBS off)
# Each arm writes its OWN CSV via RCPSP_OUT (no filename race between the two
# concurrent TT2 arms of a size). Solver is single-threaded, and the 300s deadline
# is WALL-CLOCK, so give each job its own core: with <9 cores the jobs time-slice,
# wall time inflates, and instances time out unfairly. Warned below.
# ─────────────────────────────────────────────────────────────────────────────
set -u
ROOT=~/RCPSP_With_Petri_nets; RD="$ROOT/HOG2/RCPSP"; cd "$RD" || exit 1

for tok in RCPSP_SUBSOLVER RCPSP_SUBSOLVE_HCBS_OFF RCPSP_CBS_SINGLERES RCPSP_TT2_SINGLERES RCPSP_OUT; do
  grep -q "$tok" Driver.cpp || { echo "ABORT: '$tok' not in Driver.cpp — re-upload sources."; exit 1; }
done
echo "source check OK"
echo "=== BUILD ($(date)) ==="
g++ -std=c++20 -fopenmp -O3 -I../.. -I../gui/STUB Driver.cpp -o Driver_var 2> build_var_err.txt \
  && echo "BUILD OK" || { echo "BUILD FAILED"; tail -30 build_var_err.txt; exit 1; }
BIN="$RD/Driver_var"; L="$RD/logs"; mkdir -p "$L"

NPROC=$(nproc 2>/dev/null || echo 0)
[ "$NPROC" -ge 9 ] 2>/dev/null && echo "cores=$NPROC (>=9: wall-clock deadline is fair)" \
  || echo "WARNING: cores=$NPROC < 9 concurrent jobs — jobs will time-slice, inflating wall time and causing UNFAIR 300s timeouts. Consider running sizes in sequence."

# Base configurations
CBS="RCPSP_DR5=1 RCPSP_DOM_RULE=dr5s RCPSP_HYBRID=1 RCPSP_UB=1 RCPSP_SKYLINE=1 RCPSP_LAZY=1 RCPSP_SETDELAY=1 RCPSP_DR4=1 RCPSP_CBS_RSADAPT=1"
TT2="RCPSP_TT2_DR5=1 RCPSP_TT2_DR4=1 RCPSP_TT2_BATCH=1 RCPSP_TT2_RSADAPT=1"

# Target variants
CBS_CBS="RCPSP_CBS_SINGLERES=1 RCPSP_SUBSOLVER=cbs RCPSP_SUBSOLVE_HCBS_OFF=1"
TT2_TT2="RCPSP_TT2_SINGLERES=1 RCPSP_SUBSOLVER=tt2"
TT2_CBS="RCPSP_TT2_SINGLERES=1 RCPSP_SUBSOLVER=cbs RCPSP_SUBSOLVE_HCBS_OFF=1"

# cd "$wd" so the Driver finds the size's instances; RCPSP_OUT is an ABSOLUTE path
# into ONE shared folder ($OUT) so all 9 CSVs land together regardless of size.
OUT="$RD/new_results"; mkdir -p "$OUT"
run_cbs(){ local size=$1 tag=$2 ex=$3 wd=$4
  ( cd "$wd" && env $CBS $ex RCPSP_OUT="$OUT/VAR_cbs_${tag}_${size}.csv" "$BIN" "$size" 8 > "$L/cbs_${tag}_${size}.log" 2>&1 ); }
run_tt2(){ local size=$1 tag=$2 ex=$3 wd=$4
  ( cd "$wd" && env $TT2 $ex RCPSP_OUT="$OUT/VAR_tt2_${tag}_${size}.csv" "$BIN" "tt2_$size" > "$L/tt2_${tag}_${size}.log" 2>&1 ); }

echo "=== LAUNCHING ALL 9 JOBS IN PARALLEL ($(date)) ==="
for size in j30 j60 j90; do
  wd="$ROOT"; [ "$size" = j30 ] && wd="$RD"
  run_cbs "$size" "sr_cbs" "$CBS_CBS" "$wd" &
  run_tt2 "$size" "sr_tt2" "$TT2_TT2" "$wd" &
  run_tt2 "$size" "sr_cbs" "$TT2_CBS" "$wd" &
done

echo "Waiting for all 9 jobs to finish..."
wait
echo "=== ALL DONE ($(date)) ==="

echo "=== SUMMARY (solved overall; RS<=.5 subset) ==="
for size in j30 j60 j90; do
  f="$OUT/VAR_cbs_sr_cbs_${size}.csv"
  [ -f "$f" ] && awk -F, 'NR>1{n++;g=$1+0;rs=(((g-1)%4+4)%4);if($13=="True"){s++;if(rs<=1)lo++}if(rs<=1)tl++}END{printf "  CBS %-4s %-8s %d/%d (RS<=.5 %d/%d)\n","'"$size"'","sr_cbs",s,n,lo,tl}' "$f"
  for tag in sr_tt2 sr_cbs; do
    f="$OUT/VAR_tt2_${tag}_${size}.csv"
    [ -f "$f" ] && awk -F, 'NR>1{n++;g=$1+0;rs=(((g-1)%4+4)%4);if($4=="True"){s++;if(rs<=1)lo++}if(rs<=1)tl++}END{printf "  TT2 %-4s %-8s %d/%d (RS<=.5 %d/%d)\n","'"$size"'","'"$tag"'",s,n,lo,tl}' "$f"
  done
done
