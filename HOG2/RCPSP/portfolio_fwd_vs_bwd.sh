#!/bin/bash
# ─────────────────────────────────────────────────────────────────────────────
# PORTFOLIO: does the reversed-instance BACKWARD solve anything the FORWARD can't?
# Run on the server AFTER run_server_bwd.sh. Reads the per-instance CSVs in place,
# copies nothing. For each size: forward-solved, backward-solved, UNION (= what a
# racing portfolio would get), and the instances solved ONLY by backward.
#
# Coverage is the metric that matters here, not time: if the union beats the best
# single direction, running both is a real win even though backward alone is not
# stronger than forward alone.
#
# Usage:  bash portfolio_fwd_vs_bwd.sh            # auto-find newest CSVs
#         bash portfolio_fwd_vs_bwd.sh FWD.csv BWD.csv   # or name them explicitly
# ─────────────────────────────────────────────────────────────────────────────
set -u
ROOT=~/RCPSP_With_Petri_nets
RD="$ROOT/HOG2/RCPSP"

newest() { ls -t $1 2>/dev/null | head -1; }

# solved instance keys "group:exam" from a CSV whose col4 is solved(True/False)
keys() { awk -F, 'NR>1 && $4=="True" {print $1":"$2}' "$1" | sort -u; }

one_size() {
  local size=$1 fwd=$2 bwd=$3
  [ -f "$fwd" ] || { echo "  $size: forward CSV not found ($fwd)"; return; }
  [ -f "$bwd" ] || { echo "  $size: backward CSV not found ($bwd)"; return; }
  local F B U ONLYB ONLYF
  F=$(keys "$fwd" | wc -l); B=$(keys "$bwd" | wc -l)
  U=$(cat <(keys "$fwd") <(keys "$bwd") | sort -u | wc -l)
  ONLYB=$(comm -13 <(keys "$fwd") <(keys "$bwd") | tr '\n' ' ')
  ONLYF=$(comm -23 <(keys "$fwd") <(keys "$bwd") | wc -l)
  echo "  === $size ==="
  echo "    forward solved : $F"
  echo "    backward solved: $B"
  echo "    UNION (portfolio): $U      gain over forward: $((U-F))"
  echo "    only-forward: $ONLYF   only-backward: $(echo $ONLYB | wc -w)"
  [ -n "${ONLYB// /}" ] && echo "    instances ONLY backward solves: $ONLYB"
  echo "    fwd=$(basename "$fwd")"
  echo "    bwd=$(basename "$bwd")"
}

if [ $# -eq 2 ]; then one_size "custom" "$1" "$2"; exit 0; fi

echo "=== PORTFOLIO forward vs reversed-backward ($(date)) ==="
one_size j30 "$(newest "$RD/new_results/output_tt2_j30_*.csv")"   "$(newest "$RD/new_results/BWD_tt2rev_j30.csv $RD/new_results/output_tt2rev_j30_*.csv")"
one_size j60 "$(newest "$ROOT/new_results/output_tt2_j60_*.csv")" "$(newest "$ROOT/new_results/BWD_tt2rev_j60.csv $ROOT/new_results/output_tt2rev_j60_*.csv")"
one_size j90 "$(newest "$ROOT/new_results/output_tt2_j90_*.csv")" "$(newest "$ROOT/new_results/BWD_tt2rev_j90.csv $ROOT/new_results/output_tt2rev_j90_*.csv")"
echo "=== DONE ==="
