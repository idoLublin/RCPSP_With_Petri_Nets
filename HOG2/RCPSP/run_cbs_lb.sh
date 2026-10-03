#!/bin/bash
# ─────────────────────────────────────────────────────────────────────────────
# CBS new-lower-bound experiment. 3 arms on the standing default base:
#   control : default (DR4+DR5+SetDelay+Hybrid+UB+Skyline+Lazy)
#   sr      : + single-resource max LB (RS-gated, low-RS heuristic by default)
#   mc      : + min-cut max-flow LB (RS-gated)
# SIZE-OUTER ordering: j30 (all arms) first => results/verification in ~2 min, then
# j60, then j90. Within a size arms run SEQUENTIALLY so each arm's output is captured
# cleanly (same base filename otherwise). Judge on solved/480, esp. RS<=0.5. tmux.
# ─────────────────────────────────────────────────────────────────────────────
set -u
ROOT=~/RCPSP_With_Petri_nets
RD="$ROOT/HOG2/RCPSP"
cd "$RD" || { echo "cd failed"; exit 1; }

for tok in RCPSP_CBS_SINGLERES RCPSP_CBS_RSADAPT RCPSP_CBS_MINCUT; do
  grep -q "$tok" Driver.cpp || { echo "ABORT: '$tok' not in Driver.cpp — re-upload sources first."; exit 1; }
done
echo "source check OK (new bounds present in Driver.cpp)"

echo "=== BUILD ($(date)) ==="
g++ -std=c++20 -fopenmp -O3 -I../.. -I../gui/STUB Driver.cpp -o Driver_cbslb 2> build_cbslb_err.txt \
  && echo "BUILD OK" || { echo "BUILD FAILED"; tail -40 build_cbslb_err.txt; exit 1; }
BIN="$RD/Driver_cbslb"
L="$RD/logs"; mkdir -p "$L"

BASE="RCPSP_DR5=1 RCPSP_DOM_RULE=dr5s RCPSP_HYBRID=1 RCPSP_UB=1 RCPSP_SKYLINE=1 RCPSP_LAZY=1 RCPSP_SETDELAY=1 RCPSP_DR4=1"

# arm tag -> extra env
declare -A ARM=( [control]="" \
                 [sr]="RCPSP_CBS_RSADAPT=1 RCPSP_CBS_RS_THRESH=0.6 RCPSP_CBS_SINGLERES=1" \
                 [mc]="RCPSP_CBS_RSADAPT=1 RCPSP_CBS_RS_THRESH=0.6 RCPSP_CBS_MINCUT=1" )
ORDER=(control sr mc)

run_one() {               # $1 size  $2 workdir  $3 tag
  local size=$1 wd=$2 tag=$3 extra="${ARM[$3]}"
  echo "--- $tag $size ($(date)) ---"
  ( cd "$wd" && env $BASE $extra "$BIN" "$size" 8 > "$L/cbs_${tag}_${size}.log" 2>&1 )
  cp "$(ls -t "$wd/new_results"/output_${size}_cfg8_*.csv 2>/dev/null | head -1)" "$wd/new_results/CBSLB_${tag}_${size}.csv" 2>/dev/null
  if [ -n "$extra" ]; then
    grep -qiE 'single-resource max LB ON|min-cut resource LB ON' "$L/cbs_${tag}_${size}.log" \
      && echo "  [ok] $tag $size: bound ACTIVATED" || echo "  [WARN] $tag $size: bound NOT activated!"
  fi
}

for size in j30 j60 j90; do
  wd="$ROOT"; [ "$size" = j30 ] && wd="$RD"
  echo "===== SIZE $size ($(date)) ====="
  for tag in "${ORDER[@]}"; do run_one "$size" "$wd" "$tag"; done
  echo "=== $size coverage (solved/480, RS<=0.5 subset) ==="
  for tag in "${ORDER[@]}"; do f="$wd/new_results/CBSLB_${tag}_${size}.csv";
    [ -f "$f" ] && awk -F, 'NR>1{n++; g=$1+0; rs=(((g-1)%4+4)%4);
        if($13=="True"){s++; if(rs<=1)slo++} if(rs<=1)nlo++}
      END{printf "  %-8s solved=%d/%d  (RS<=0.5: %d/%d)\n","'"$tag"'",s,n,slo,nlo}' "$f"; done
done
echo "=== ALL DONE ($(date)) ==="
