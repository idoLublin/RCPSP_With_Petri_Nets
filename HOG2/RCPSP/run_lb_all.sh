#!/bin/bash
# ─────────────────────────────────────────────────────────────────────────────
# New-lower-bound experiment, 6 PARALLEL TRACKS (server not pressured):
#   CBS j30 | CBS j60 | CBS j90 | TT2 j30 | TT2 j60 | TT2 j90
# Each track runs its arms SEQUENTIALLY (so same-size jobs never collide on the
# shared output base -> clean capture). Arms:
#   CBS: control | sr(+single-res) | mc(+min-cut)     [RS-gated, low-RS heuristic]
#   TT2: control | sr(+single-res)
# Peak memory is bounded: within a track only ONE job runs at a time, so at most one
# CBS-j90 process exists. Judge on solved/480 and the RS<=0.5 subset. Run in tmux.
# ─────────────────────────────────────────────────────────────────────────────
set -u
ROOT=~/RCPSP_With_Petri_nets
RD="$ROOT/HOG2/RCPSP"
cd "$RD" || { echo "cd failed"; exit 1; }

for tok in RCPSP_CBS_SINGLERES RCPSP_CBS_MINCUT RCPSP_CBS_RSADAPT RCPSP_TT2_SINGLERES RCPSP_TT2_RSADAPT; do
  grep -q "$tok" Driver.cpp || { echo "ABORT: '$tok' not in Driver.cpp — re-upload sources first."; exit 1; }
done
echo "source check OK (new bounds present in Driver.cpp)"

echo "=== BUILD ($(date)) ==="
g++ -std=c++20 -fopenmp -O3 -I../.. -I../gui/STUB Driver.cpp -o Driver_lb 2> build_lb_err.txt \
  && echo "BUILD OK" || { echo "BUILD FAILED"; tail -40 build_lb_err.txt; exit 1; }
BIN="$RD/Driver_lb"
L="$RD/logs"; mkdir -p "$L"

CBSBASE="RCPSP_DR5=1 RCPSP_DOM_RULE=dr5s RCPSP_HYBRID=1 RCPSP_UB=1 RCPSP_SKYLINE=1 RCPSP_LAZY=1 RCPSP_SETDELAY=1 RCPSP_DR4=1"
TT2BASE="RCPSP_TT2_DR5=1 RCPSP_TT2_DR4=1 RCPSP_TT2_BATCH=1"
SR_CBS="RCPSP_CBS_RSADAPT=1 RCPSP_CBS_RS_THRESH=0.6 RCPSP_CBS_SINGLERES=1"
MC_CBS="RCPSP_CBS_RSADAPT=1 RCPSP_CBS_RS_THRESH=0.6 RCPSP_CBS_MINCUT=1"
SR_TT2="RCPSP_TT2_RSADAPT=1 RCPSP_TT2_RS_THRESH=0.6 RCPSP_TT2_SINGLERES=1"

cbs_track() {             # $1 size  $2 workdir
  local size=$1 wd=$2 tag extra
  for a in "control|" "sr|$SR_CBS" "mc|$MC_CBS"; do
    tag=${a%%|*}; extra=${a#*|}
    ( cd "$wd" && env $CBSBASE $extra "$BIN" "$size" 8 > "$L/cbs_${tag}_${size}.log" 2>&1 )
    cp "$(ls -t "$wd/new_results"/output_${size}_cfg8_*.csv 2>/dev/null | head -1)" "$wd/new_results/CBSLB_${tag}_${size}.csv" 2>/dev/null
    [ -n "$extra" ] && { grep -qiE 'single-resource max LB ON|min-cut resource LB ON' "$L/cbs_${tag}_${size}.log" \
        && echo "[ok] CBS $tag $size ACTIVATED" || echo "[WARN] CBS $tag $size NOT activated"; }
  done
  echo "=== CBS $size coverage (solved/480, RS<=0.5) ==="
  for tag in control sr mc; do f="$wd/new_results/CBSLB_${tag}_${size}.csv";
    [ -f "$f" ] && awk -F, 'NR>1{n++;g=$1+0;rs=(((g-1)%4+4)%4);if($13=="True"){s++;if(rs<=1)slo++}if(rs<=1)nlo++}
      END{printf "  CBS %-4s %-8s %d/%d  (RS<=0.5 %d/%d)\n","'"$size"'","'"$tag"'",s,n,slo,nlo}' "$f"; done
}

tt2_track() {             # $1 size  $2 workdir
  local size=$1 wd=$2 tag extra
  for a in "control|" "sr|$SR_TT2"; do
    tag=${a%%|*}; extra=${a#*|}
    ( cd "$wd" && env $TT2BASE $extra "$BIN" "tt2_$size" > "$L/tt2_${tag}_${size}.log" 2>&1 )
    cp "$(ls -t "$wd/new_results"/output_tt2_${size}_*.csv 2>/dev/null | head -1)" "$wd/new_results/TT2LB_${tag}_${size}.csv" 2>/dev/null
    [ -n "$extra" ] && { grep -qiE 'single-resource max LB ON' "$L/tt2_${tag}_${size}.log" \
        && echo "[ok] TT2 $tag $size ACTIVATED" || echo "[WARN] TT2 $tag $size NOT activated"; }
  done
  echo "=== TT2 $size coverage (solved/480, RS<=0.5) ==="
  for tag in control sr; do f="$wd/new_results/TT2LB_${tag}_${size}.csv";
    [ -f "$f" ] && awk -F, 'NR>1{n++;g=$1+0;rs=(((g-1)%4+4)%4);if($4=="True"){s++;if(rs<=1)slo++}if(rs<=1)nlo++}
      END{printf "  TT2 %-4s %-8s %d/%d  (RS<=0.5 %d/%d)\n","'"$size"'","'"$tag"'",s,n,slo,nlo}' "$f"; done
}

echo "=== launching 6 tracks ($(date)) ==="
cbs_track j30 "$RD"   &
cbs_track j60 "$ROOT" &
cbs_track j90 "$ROOT" &
tt2_track j30 "$RD"   &
tt2_track j60 "$ROOT" &
tt2_track j90 "$ROOT" &
wait
echo "=== ALL DONE ($(date)) ==="
