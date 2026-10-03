#!/bin/bash
# ─────────────────────────────────────────────────────────────────────────────
# Server run: CBC DDT baseline, component ablation. Prepared 2026-09-29 from run_server_gurobi.sh.
# Run on dsisco01 from the repo's baselines dir (data in baselines/data/{j30,j60,j90}, 480 .sm each).
#
# WHY RE-RUN CBC: the CBC numbers we have came through PuLP (baselines/mip_ddt/run_ddt_mip.py), whose
# `optimal` flag does not come from CBC, and which exposes NO DUAL BOUND at all -- so no "optimal" row in
# those CSVs could ever be checked. run_cbc_ddt.py reads CBC's own verdict and bound out of its log
# ("Result - Optimal solution found" / "Result - Stopped on time limit" / "Lower bound:"), so the gate
# below can check every optimal row against its own bound.
#
# Every arm: 1 thread, 300 s per instance WITH the model build inside the budget (RCPSP_CBC_BUILD_IN_BUDGET=1),
# independent schedule check (CSV 'verified'). Model and horizon identical to the Gurobi and SCIP baselines.
#   default     CBC defaults              the honest baseline, replacing the PuLP numbers
#   nopresolve  presolve off              the MIP counterpart of the root stage our CBS runs without
#   nocuts      cut generation off        one-at-a-time ablation
#   noheur      primal heuristics off     one-at-a-time ablation
#   bare        presolve+cuts+heur off    LP branch-and-bound only (optional)
# CBC has no switch for node propagation or conflict analysis: for those arms use the SCIP baseline
# (baselines/scip_ddt/run_server_scip.sh, arms noprop / noconf).
# ARMS="..." picks arms (default: default nopresolve). SIZES="..." (default j30 j60 j90).
# MAX_PAR jobs at once (default 4). ONLY=<file> -> RCPSP_ONLY spot check.
# CBC is a fork/exec per instance and is modest on memory; SOFTMEM is only used for the start gate.
# Output: new_results/cbc_<size>_ddt-<flags>-budget_vNN.csv, copied to new_results/RUN_cbc-<arm>_<size>.csv
# UPLOAD  baselines/cbc_ddt: run_cbc_ddt.py run_server_cbc.sh   (needs baselines/gurobi_ddt/run_gurobi_ddt.py
#         present too -- the parser, time windows and schedule verifier are imported from it)
# ─────────────────────────────────────────────────────────────────────────────
set -u
REPO=~/RCPSP_With_Petri_nets
cd "$REPO/baselines" || { echo "cd failed"; exit 1; }
PY=${PY:-python3}
ARMS=${ARMS:-"default nopresolve"}
SIZES=${SIZES:-"j30 j60 j90"}
MAX_PAR=${MAX_PAR:-4}
SOFTMEM=${SOFTMEM:-8}
ONLY=${ONLY:-}
MIN_AVAIL_GB=${MIN_AVAIL_GB:-$(( MAX_PAR * SOFTMEM + 50 ))}
RES="$REPO/new_results"
mkdir -p "$RES" logs

log(){ echo "[$(date +%F_%H:%M:%S)] $*"; }
avail(){ awk '/MemAvailable/ {printf "%d", $2/1048576}' /proc/meminfo; }

# ── pre-flight: data, solver, shared helpers
for sz in $SIZES; do
  n=$(ls data/$sz/${sz}*_*.sm 2>/dev/null | wc -l)
  [ "$n" -eq 480 ] || { log "data/$sz has $n instance files, expected 480"; exit 1; }
done
[ -f gurobi_ddt/run_gurobi_ddt.py ] || { log "gurobi_ddt/run_gurobi_ddt.py missing (helpers are imported from it)"; exit 1; }
$PY -c "import sys; sys.path.insert(0,'cbc_ddt'); import run_cbc_ddt as R; print('cbc:', R.CBC_VERSION); raise SystemExit(0 if R.CBC_CLASS else 1)" \
  || { log "no usable CBC (PuLP<=3 bundles one; PuLP 4 needs a system cbc on PATH, e.g. coinor-cbc)"; exit 1; }
[ -z "$ONLY" ] || [ -f "$ONLY" ] || { log "ONLY=$ONLY not found"; exit 1; }

arm_env(){ case $1 in
  default)    echo "" ;;
  nopresolve) echo "RCPSP_CBC_NOPRESOLVE=1" ;;
  nocuts)     echo "RCPSP_CBC_NOCUTS=1" ;;
  noheur)     echo "RCPSP_CBC_NOHEUR=1" ;;
  bare)       echo "RCPSP_CBC_NOPRESOLVE=1 RCPSP_CBC_NOCUTS=1 RCPSP_CBC_NOHEUR=1" ;;
  *)          return 1 ;;
esac; }
for A in $ARMS; do arm_env "$A" > /dev/null || { log "unknown arm $A"; exit 1; }; done

while [ "$(avail)" -lt "$MIN_AVAIL_GB" ]; do log "MemAvailable $(avail) GB < $MIN_AVAIL_GB"; sleep 300; done
log "starting: ARMS=[$ARMS] SIZES=[$SIZES] MAX_PAR=$MAX_PAR MemAvailable=$(avail)GB ${ONLY:+ONLY=$ONLY}"

COMMON="RCPSP_CBC_BUILD_IN_BUDGET=1 RCPSP_RESULTS_DIR=$RES ${ONLY:+RCPSP_ONLY=$ONLY}"
run_job(){ # <arm> <size>
  local lg="logs/cbc_$1_$2.log"
  log "  start $1 $2"
  env $COMMON $(arm_env "$1") $PY cbc_ddt/run_cbc_ddt.py "$2" --data-dir "data/$2" > "$lg" 2>&1
  local rc=$?
  log "  end   $1 $2 exit=$rc ($(grep -c '^  \[' "$lg") rows)"
}

# j30 of every arm first, then j60, then j90: early, comparable feedback
for sz in $SIZES; do for A in $ARMS; do
  while [ "$(jobs -rp | wc -l)" -ge "$MAX_PAR" ]; do wait -n; done
  run_job "$A" "$sz" &
  sleep 5
done; done
wait
log "=== all jobs done ==="

for sz in $SIZES; do for A in $ARMS; do
  csv=$(awk -F'-> ' '/Done ->/{print $2}' "logs/cbc_${A}_${sz}.log" | tail -1)
  [ -f "$csv" ] && cp "$csv" "$RES/RUN_cbc-${A}_${sz}.csv" || log "no CSV for $A $sz (see logs/cbc_${A}_${sz}.log)"
done; done

# column by header name
col(){ head -1 "$1" | tr ',' '\n' | grep -nx "$2" | cut -d: -f1; }

echo "=== CORRECTNESS GATE (must print nothing) ==="
for f in "$RES"/RUN_cbc-*.csv; do
  [ -f "$f" ] || continue
  o=$(col "$f" optimal); s=$(col "$f" solved); v=$(col "$f" verified); m=$(col "$f" makespan)
  l=$(col "$f" provenLB); st=$(col "$f" status)
  awk -F, -v o="$o" -v s="$s" -v v="$v" -v m="$m" -v l="$l" -v c="$st" -v F="$(basename "$f")" \
    'NR>1 && $s=="True" && $v!="True"    {print F": "$1" schedule failed verification"}
     NR>1 && $o=="True" && $m!=$l        {print F": "$1" optimal but makespan "$m" != bound "$l}
     NR>1 && $o=="True" && $c!="optimal" {print F": "$1" optimal=True but status "$c}
     NR>1 && $c=="unknown"               {print F": "$1" CBC log gave no verdict"}' "$f"
done

echo "=== SUMMARY (proven optimal / rows, status counts) ==="
for f in "$RES"/RUN_cbc-*.csv; do
  [ -f "$f" ] || continue
  o=$(col "$f" optimal); st=$(col "$f" status)
  opt=$(awk -F, -v o="$o" 'NR>1 && $o=="True"' "$f" | wc -l); tot=$(awk 'NR>1' "$f" | wc -l)
  stats=$(awk -F, -v c="$st" 'NR>1{n[$c]++} END{for(k in n) printf "%s=%d ", k, n[k]}' "$f")
  echo "$(basename "$f"): $opt/$tot  $stats"
done
log "=== DONE ==="
