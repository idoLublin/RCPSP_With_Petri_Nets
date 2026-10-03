#!/bin/bash
# ─────────────────────────────────────────────────────────────────────────────
# A/B test of the TT2 sub-solve SUCCESSOR CACHE (RCPSP_SUBSOLVE_SUCC_CACHE) on
# higher-memory server hardware. Both arms: TT2 main + TT2 subsolver (best config).
#   sc_off = successor cache OFF   (control)
#   sc_on  = successor cache ON    (unlimited entries, per-instance cleared)
# Isolated CSVs via RCPSP_OUT (no race). Judge by COVERAGE (solve rate) first, then
# time — the 300s deadline is enforced, so if the cache is slower it shows up as
# fewer solves. Sound (raw children + fresh dominance; 0 makespan disagreements locally).
# ─────────────────────────────────────────────────────────────────────────────
set -u
ROOT=~/RCPSP_With_Petri_nets; RD="$ROOT/HOG2/RCPSP"; cd "$RD" || exit 1
for tok in RCPSP_SUBSOLVE_SUCC_CACHE RCPSP_SUBSOLVER RCPSP_TT2_SINGLERES RCPSP_OUT; do
  grep -q "$tok" Driver.cpp || { echo "ABORT: '$tok' not in Driver.cpp — re-upload sources."; exit 1; }
done
echo "source check OK"
echo "=== BUILD ($(date)) ==="
g++ -std=c++20 -fopenmp -O3 -I../.. -I../gui/STUB Driver.cpp -o Driver_var 2> build_var_err.txt \
  && echo "BUILD OK" || { echo "BUILD FAILED"; tail -30 build_var_err.txt; exit 1; }
BIN="$RD/Driver_var"; L="$RD/logs"; mkdir -p "$L"; OUT="$RD/new_results"; mkdir -p "$OUT"

TT2="RCPSP_TT2_DR5=1 RCPSP_TT2_DR4=1 RCPSP_TT2_BATCH=1 RCPSP_TT2_RSADAPT=1 RCPSP_TT2_SINGLERES=1 RCPSP_SUBSOLVER=tt2"
SC_OFF=""
SC_ON="RCPSP_SUBSOLVE_SUCC_CACHE=1"

run_tt2(){ local size=$1 tag=$2 ex=$3 wd=$4
  ( cd "$wd" && env $TT2 $ex RCPSP_OUT="$OUT/SC_tt2_${tag}_${size}.csv" "$BIN" "tt2_$size" > "$L/sc_${tag}_${size}.log" 2>&1 ); }

echo "=== LAUNCH ($(date)) — sizes sequential, off+on parallel per size ==="
for size in j30 j60; do
  echo "--- $size ---"
  wd="$ROOT"; [ "$size" = j30 ] && wd="$RD"
  run_tt2 "$size" "off" "$SC_OFF" "$wd" &
  run_tt2 "$size" "on"  "$SC_ON"  "$wd" &
  wait
done
echo "=== DONE ($(date)) ==="

echo "=== SUMMARY (solved overall; RS<=.5 subset) ==="
for size in j30 j60; do
  for tag in off on; do
    f="$OUT/SC_tt2_${tag}_${size}.csv"
    [ -f "$f" ] && awk -F, 'NR>1{n++;g=$1+0;rs=(((g-1)%4+4)%4);if($4=="True"){s++;if(rs<=1)lo++}if(rs<=1)tl++; t+=$3}END{printf "  %-4s cache_%-3s %d/%d (RS<=.5 %d/%d)  sumTime=%.0fs\n","'"$size"'","'"$tag"'",s,n,lo,tl,t}' "$f"
  done
done
