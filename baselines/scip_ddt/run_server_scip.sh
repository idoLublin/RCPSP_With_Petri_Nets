#!/bin/bash
# ─────────────────────────────────────────────────────────────────────────────
# Server run: SCIP DDT baseline, component ablation. Prepared 2026-09-29 from run_server_gurobi.sh.
# Run on dsisco01 from the repo's baselines dir (data in baselines/data/{j30,j60,j90}, 480 .sm each).
#
# WHY RE-RUN SCIP AT ALL: the SCIP numbers we have came through PuLP (baselines/mip_ddt/run_ddt_mip.py),
# whose `optimal` flag is True whenever a feasible solution exists at the time limit -- TIMEOUTS COUNTED AS
# PROVEN OPTIMA. run_scip_ddt.py takes SCIP's own status string instead and writes the dual bound as provenLB,
# so the correctness gate below can check every "optimal" row against its own bound.
#
# Every arm: 1 thread, 300 s per instance WITH the model build inside the budget (RCPSP_SCIP_BUILD_IN_BUDGET=1),
# limits/memory SOFTMEM GB, independent schedule check (CSV 'verified'). Model and horizon unchanged.
#   default     SCIP defaults                 the honest baseline, replacing the PuLP numbers
#   nopresolve  presolving off                the MIP counterpart of the root stage our CBS runs without
#   noprop      node propagation off          SCIP-only: the counterpart of RCPSP_CBS_TT (per-node deadline
#                                             propagation). Gurobi exposes no such switch, so this arm is the
#                                             one that says how much of SCIP's edge is propagation at nodes.
#   noconf      conflict analysis off         SCIP-only: the counterpart of RCPSP_CBS_NG (nogoods)
#   nocuts      separating off                one-at-a-time ablation (optional)
#   noheur      primal heuristics off         one-at-a-time ablation (optional)
#   bare        presolve+cuts+heur+prop+conf off   LP branch-and-bound only (optional)
# ARMS="..." picks arms (default: default nopresolve noprop noconf). SIZES="..." (default j30 j60 j90).
# MAX_PAR jobs at once (default 4). ONLY=<file> -> RCPSP_ONLY spot check.
# Starts only when MemAvailable >= MAX_PAR*SOFTMEM + 50 GB (keep 50 GB free at the worst case).
# Output: new_results/scip_<size>_ddt-<flags>-budget_vNN.csv, copied to new_results/RUN_scip-<arm>_<size>.csv
# UPLOAD  baselines/scip_ddt: run_scip_ddt.py run_server_scip.sh   (needs baselines/gurobi_ddt/run_gurobi_ddt.py
#         present too -- the parser, time windows and schedule verifier are imported from it)
# ─────────────────────────────────────────────────────────────────────────────
set -u
REPO=~/RCPSP_With_Petri_nets
cd "$REPO/baselines" || { echo "cd failed"; exit 1; }
PY=${PY:-python3}
ARMS=${ARMS:-"default nopresolve noprop noconf"}
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
$PY -c "from pyscipopt import Model; print('pyscipopt OK, SCIP', Model().version())" \
  || { log "pyscipopt import failed (pip install pyscipopt)"; exit 1; }
[ -z "$ONLY" ] || [ -f "$ONLY" ] || { log "ONLY=$ONLY not found"; exit 1; }

arm_env(){ case $1 in
  default)    echo "" ;;
  nopresolve) echo "RCPSP_SCIP_NOPRESOLVE=1" ;;
  noprop)     echo "RCPSP_SCIP_NOPROP=1" ;;
  noconf)     echo "RCPSP_SCIP_NOCONF=1" ;;
  nocuts)     echo "RCPSP_SCIP_NOCUTS=1" ;;
  noheur)     echo "RCPSP_SCIP_NOHEUR=1" ;;
  bare)       echo "RCPSP_SCIP_NOPRESOLVE=1 RCPSP_SCIP_NOCUTS=1 RCPSP_SCIP_NOHEUR=1 RCPSP_SCIP_NOPROP=1 RCPSP_SCIP_NOCONF=1" ;;
  *)          return 1 ;;
esac; }
for A in $ARMS; do arm_env "$A" > /dev/null || { log "unknown arm $A"; exit 1; }; done

while [ "$(avail)" -lt "$MIN_AVAIL_GB" ]; do log "MemAvailable $(avail) GB < $MIN_AVAIL_GB"; sleep 300; done
log "starting: ARMS=[$ARMS] SIZES=[$SIZES] MAX_PAR=$MAX_PAR SOFTMEM=${SOFTMEM}GB MemAvailable=$(avail)GB ${ONLY:+ONLY=$ONLY}"

COMMON="RCPSP_SCIP_BUILD_IN_BUDGET=1 RCPSP_SCIP_SOFTMEM=$SOFTMEM RCPSP_RESULTS_DIR=$RES ${ONLY:+RCPSP_ONLY=$ONLY}"
run_job(){ # <arm> <size>
  local lg="logs/scip_$1_$2.log"
  log "  start $1 $2"
  env $COMMON $(arm_env "$1") $PY scip_ddt/run_scip_ddt.py "$2" --data-dir "data/$2" > "$lg" 2>&1
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
  csv=$(awk -F'-> ' '/Done ->/{print $2}' "logs/scip_${A}_${sz}.log" | tail -1)
  [ -f "$csv" ] && cp "$csv" "$RES/RUN_scip-${A}_${sz}.csv" || log "no CSV for $A $sz (see logs/scip_${A}_${sz}.log)"
done; done

# column by header name
col(){ head -1 "$1" | tr ',' '\n' | grep -nx "$2" | cut -d: -f1; }

echo "=== CORRECTNESS GATE (must print nothing) ==="
for f in "$RES"/RUN_scip-*.csv; do
  [ -f "$f" ] || continue
  o=$(col "$f" optimal); s=$(col "$f" solved); v=$(col "$f" verified); m=$(col "$f" makespan); l=$(col "$f" provenLB)
  st=$(col "$f" status)
  awk -F, -v o="$o" -v s="$s" -v v="$v" -v m="$m" -v l="$l" -v c="$st" -v F="$(basename "$f")" \
    'NR>1 && $s=="True" && $v!="True"  {print F": "$1" schedule failed verification"}
     NR>1 && $o=="True" && $m!=$l      {print F": "$1" optimal but makespan "$m" != bound "$l}
     NR>1 && $o=="True" && $c!="optimal" {print F": "$1" optimal=True but status "$c}' "$f"
done

echo "=== SUMMARY (proven optimal / rows, status counts) ==="
for f in "$RES"/RUN_scip-*.csv; do
  [ -f "$f" ] || continue
  o=$(col "$f" optimal); st=$(col "$f" status)
  opt=$(awk -F, -v o="$o" 'NR>1 && $o=="True"' "$f" | wc -l); tot=$(awk 'NR>1' "$f" | wc -l)
  stats=$(awk -F, -v c="$st" 'NR>1{n[$c]++} END{for(k in n) printf "%s=%d ", k, n[k]}' "$f")
  echo "$(basename "$f"): $opt/$tot  $stats"
done
log "=== DONE ==="
