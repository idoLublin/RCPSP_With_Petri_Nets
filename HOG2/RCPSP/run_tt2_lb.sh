#!/bin/bash
# ─────────────────────────────────────────────────────────────────────────────
# TT2 new-lower-bound experiment. 2 arms on the default TT2 config:
#   control : default (DR5+DR4+Batch)
#   sr      : + single-resource max LB (RS-gated, low-RS heuristic by default)
# SIZE-OUTER: j30 (both arms) first => verification in ~1 min, then j60, then j90.
# Arms sequential within a size for clean output capture. Judge on solved/480. tmux.
# ─────────────────────────────────────────────────────────────────────────────
set -u
ROOT=~/RCPSP_With_Petri_nets
RD="$ROOT/HOG2/RCPSP"
cd "$RD" || { echo "cd failed"; exit 1; }

for tok in RCPSP_TT2_SINGLERES RCPSP_TT2_RSADAPT; do
  grep -q "$tok" Driver.cpp || { echo "ABORT: '$tok' not in Driver.cpp — re-upload sources first."; exit 1; }
done
echo "source check OK (new bounds present in Driver.cpp)"

echo "=== BUILD ($(date)) ==="
g++ -std=c++20 -fopenmp -O3 -I../.. -I../gui/STUB Driver.cpp -o Driver_tt2lb 2> build_tt2lb_err.txt \
  && echo "BUILD OK" || { echo "BUILD FAILED"; tail -40 build_tt2lb_err.txt; exit 1; }
BIN="$RD/Driver_tt2lb"
L="$RD/logs"; mkdir -p "$L"

DEF="RCPSP_TT2_DR5=1 RCPSP_TT2_DR4=1 RCPSP_TT2_BATCH=1"
declare -A ARM=( [control]="" [sr]="RCPSP_TT2_RSADAPT=1 RCPSP_TT2_RS_THRESH=0.6 RCPSP_TT2_SINGLERES=1" )
ORDER=(control sr)

run_one() {               # $1 size(j30..)  $2 workdir  $3 tag
  local size=$1 wd=$2 tag=$3 extra="${ARM[$3]}"
  echo "--- $tag $size ($(date)) ---"
  ( cd "$wd" && env $DEF $extra "$BIN" "tt2_$size" > "$L/tt2lb_${tag}_${size}.log" 2>&1 )
  cp "$(ls -t "$wd/new_results"/output_tt2_${size}_*.csv 2>/dev/null | head -1)" "$wd/new_results/TT2LB_${tag}_${size}.csv" 2>/dev/null
  if [ -n "$extra" ]; then
    grep -qiE 'single-resource max LB ON' "$L/tt2lb_${tag}_${size}.log" \
      && echo "  [ok] $tag $size: bound ACTIVATED" || echo "  [WARN] $tag $size: bound NOT activated!"
  fi
}

for size in j30 j60 j90; do
  wd="$ROOT"; [ "$size" = j30 ] && wd="$RD"
  echo "===== SIZE $size ($(date)) ====="
  for tag in "${ORDER[@]}"; do run_one "$size" "$wd" "$tag"; done
  echo "=== $size coverage (solved/480, RS<=0.5 subset) ==="
  for tag in "${ORDER[@]}"; do f="$wd/new_results/TT2LB_${tag}_${size}.csv";
    [ -f "$f" ] && awk -F, 'NR>1{n++; g=$1+0; rs=(((g-1)%4+4)%4);
        if($4=="True"){s++; if(rs<=1)slo++} if(rs<=1)nlo++}
      END{printf "  %-8s solved=%d/%d  (RS<=0.5: %d/%d)\n","'"$tag"'",s,n,slo,nlo}' "$f"; done
done
echo "=== ALL DONE ($(date)) ==="
