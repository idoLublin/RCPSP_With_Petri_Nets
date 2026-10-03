#!/bin/bash
# ─────────────────────────────────────────────────────────────────────────────
# VIACBS experiment: single-resource bound solved by the NESTED CBS engine (fast,
# tight) vs control. The payoff question: does the tight bound solve hard RS<=0.5
# instances control times out on? 4 tracks (CBS control/viacbs, TT2 control/viacbs).
# VIACBS is heavy per node, so j30+j60 first (add j90 once it looks worthwhile).
# ─────────────────────────────────────────────────────────────────────────────
set -u
ROOT=~/RCPSP_With_Petri_nets; RD="$ROOT/HOG2/RCPSP"; cd "$RD" || exit 1
for tok in RCPSP_CBS_SINGLERES_VIACBS RCPSP_TT2_SINGLERES_VIACBS; do
  grep -q "$tok" Driver.cpp || { echo "ABORT: '$tok' not in Driver.cpp — re-upload sources."; exit 1; }
done
echo "source check OK"
echo "=== BUILD ($(date)) ==="
g++ -std=c++20 -fopenmp -O3 -I../.. -I../gui/STUB Driver.cpp -o Driver_viacbs 2> build_viacbs_err.txt \
  && echo "BUILD OK" || { echo "BUILD FAILED"; tail -30 build_viacbs_err.txt; exit 1; }
BIN="$RD/Driver_viacbs"; L="$RD/logs"; mkdir -p "$L"

CBS="RCPSP_DR5=1 RCPSP_DOM_RULE=dr5s RCPSP_HYBRID=1 RCPSP_UB=1 RCPSP_SKYLINE=1 RCPSP_LAZY=1 RCPSP_SETDELAY=1 RCPSP_DR4=1"
TT2="RCPSP_TT2_DR5=1 RCPSP_TT2_DR4=1 RCPSP_TT2_BATCH=1"
CBSVC="RCPSP_CBS_SINGLERES=1 RCPSP_CBS_SINGLERES_VIACBS=1"
TT2VC="RCPSP_TT2_SINGLERES=1 RCPSP_TT2_SINGLERES_VIACBS=1"

cbs_track(){ local size=$1 wd=$2;
  for a in "control|" "viacbs|$CBSVC"; do tag=${a%%|*}; ex=${a#*|};
    ( cd "$wd" && env $CBS $ex "$BIN" "$size" 8 > "$L/cbs_${tag}_${size}.log" 2>&1 )
    cp "$(ls -t "$wd/new_results"/output_${size}_cfg8_*.csv 2>/dev/null|head -1)" "$wd/new_results/VC_cbs_${tag}_${size}.csv" 2>/dev/null; done
  for tag in control viacbs; do f="$wd/new_results/VC_cbs_${tag}_${size}.csv"; [ -f "$f" ] && awk -F, 'NR>1{n++;g=$1+0;rs=(((g-1)%4+4)%4);if($13=="True"){s++;if(rs<=1)lo++}if(rs<=1)tl++}END{printf "  CBS %-4s %-8s %d/%d (RS<=.5 %d/%d)\n","'"$size"'","'"$tag"'",s,n,lo,tl}' "$f"; done; }
tt2_track(){ local size=$1 wd=$2;
  for a in "control|" "viacbs|$TT2VC"; do tag=${a%%|*}; ex=${a#*|};
    ( cd "$wd" && env $TT2 $ex "$BIN" "tt2_$size" > "$L/tt2_${tag}_${size}.log" 2>&1 )
    cp "$(ls -t "$wd/new_results"/output_tt2_${size}_*.csv 2>/dev/null|head -1)" "$wd/new_results/VC_tt2_${tag}_${size}.csv" 2>/dev/null; done
  for tag in control viacbs; do f="$wd/new_results/VC_tt2_${tag}_${size}.csv"; [ -f "$f" ] && awk -F, 'NR>1{n++;g=$1+0;rs=(((g-1)%4+4)%4);if($4=="True"){s++;if(rs<=1)lo++}if(rs<=1)tl++}END{printf "  TT2 %-4s %-8s %d/%d (RS<=.5 %d/%d)\n","'"$size"'","'"$tag"'",s,n,lo,tl}' "$f"; done; }

for size in j30 j60; do
  wd="$ROOT"; [ "$size" = j30 ] && wd="$RD"
  echo "===== SIZE $size ($(date)) ====="
  cbs_track "$size" "$wd" & tt2_track "$size" "$wd" & wait
done
echo "=== ALL DONE ($(date)) ==="
