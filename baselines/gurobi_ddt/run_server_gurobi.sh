#!/bin/bash
# ─────────────────────────────────────────────────────────────────────────────
# Server run: Gurobi DDT baseline, component ablation. Prepared 2026-09-28.
# Run on dsisco01 from the repo's baselines dir (data in baselines/data/{j30,j60,j90}, 480 .sm each).
#
# Every arm: 1 thread, 300 s per instance WITH the model build inside the budget (RCPSP_GRB_BUILD_IN_BUDGET=1),
# SoftMemLimit SOFTMEM GB, independent schedule check (CSV 'verified'). Model and horizon unchanged.
#   default     Gurobi defaults                           re-baseline under the same time accounting
#   nopresolve  Presolve=0                                the 'core' arm: presolve is the Gurobi counterpart of the
#                                                         root stage CBS runs without (Gurobi exposes no switch for
#                                                         node propagation or conflict analysis, unlike SCIP)
#   nocuts      Cuts=0                                    one-at-a-time ablation (optional)
#   noheur      Heuristics=0 RINS=0 PumpPasses=0 MinRelNodes=0 ZeroObjNodes=0   one-at-a-time ablation (optional)
#   bare        all of the above off + Symmetry=0         LP branch-and-bound only (optional)
# ARMS="..." picks arms (default: default nopresolve). SIZES="..." (default j30 j60 j90).
# MAX_PAR jobs at once (default 3 = what the token server has carried before). ONLY=<file> -> RCPSP_ONLY spot check.
# Starts only when MemAvailable >= MAX_PAR*SOFTMEM + 50 GB (keep 50 GB free at the worst case).
# Output: new_results/gurobi_<size>_ddt-<flags>-budget_vNN.csv, copied to new_results/RUN_gurobi-<arm>_<size>.csv
# UPLOAD  baselines/gurobi_ddt: run_gurobi_ddt.py run_server_gurobi.sh
# ─────────────────────────────────────────────────────────────────────────────
set -u
REPO=~/RCPSP_With_Petri_nets
cd "$REPO/baselines" || { echo "cd failed"; exit 1; }
# The login environment exports GRB_LICENSE_FILE=~/.gurobi/gurobi.lic, which does not exist: use the
# admin's token-server license unless GRB_LICENSE_FILE points at a file that is really there.
if [ -z "${GRB_LICENSE_FILE:-}" ] || [ ! -f "$GRB_LICENSE_FILE" ]; then
  echo "GRB_LICENSE_FILE='${GRB_LICENSE_FILE:-}' missing -> /opt/gurobi/gurobi.lic"
  export GRB_LICENSE_FILE=/opt/gurobi/gurobi.lic
fi
PY=${PY:-python3}
ARMS=${ARMS:-"default nopresolve"}
SIZES=${SIZES:-"j30 j60 j90"}
MAX_PAR=${MAX_PAR:-3}
SOFTMEM=${SOFTMEM:-8}
ONLY=${ONLY:-}
MIN_AVAIL_GB=${MIN_AVAIL_GB:-$(( MAX_PAR * SOFTMEM + 50 ))}
RES="$REPO/new_results"
mkdir -p "$RES" logs

log(){ echo "[$(date +%F_%H:%M:%S)] $*"; }
avail(){ awk '/MemAvailable/ {printf "%d", $2/1048576}' /proc/meminfo; }

# ── pre-flight: data, license, script
for sz in $SIZES; do
  n=$(ls data/$sz/${sz}*_*.sm 2>/dev/null | wc -l)
  [ "$n" -eq 480 ] || { log "data/$sz has $n instance files, expected 480"; exit 1; }
done
$PY -c "import gurobipy as gp; e=gp.Env(); print('license OK, gurobi', gp.gurobi.version()); e.dispose()" \
  || { log "Gurobi license check failed (GRB_LICENSE_FILE=$GRB_LICENSE_FILE)"; exit 1; }
[ -z "$ONLY" ] || [ -f "$ONLY" ] || { log "ONLY=$ONLY not found"; exit 1; }

arm_env(){ case $1 in
  default)    echo "" ;;
  nopresolve) echo "RCPSP_GRB_NOPRESOLVE=1" ;;
  nocuts)     echo "RCPSP_GRB_NOCUTS=1" ;;
  noheur)     echo "RCPSP_GRB_NOHEUR=1" ;;
  bare)       echo "RCPSP_GRB_NOPRESOLVE=1 RCPSP_GRB_NOCUTS=1 RCPSP_GRB_NOHEUR=1 RCPSP_GRB_NOSYMMETRY=1" ;;
  *)          return 1 ;;
esac; }
for A in $ARMS; do arm_env "$A" > /dev/null || { log "unknown arm $A"; exit 1; }; done

while [ "$(avail)" -lt "$MIN_AVAIL_GB" ]; do log "MemAvailable $(avail) GB < $MIN_AVAIL_GB"; sleep 300; done
log "starting: ARMS=[$ARMS] SIZES=[$SIZES] MAX_PAR=$MAX_PAR SOFTMEM=${SOFTMEM}GB MemAvailable=$(avail)GB ${ONLY:+ONLY=$ONLY}"

COMMON="RCPSP_GRB_BUILD_IN_BUDGET=1 RCPSP_GRB_SOFTMEM=$SOFTMEM RCPSP_RESULTS_DIR=$RES ${ONLY:+RCPSP_ONLY=$ONLY}"
run_job(){ # <arm> <size>
  local lg="logs/gurobi_$1_$2.log"
  log "  start $1 $2"
  env $COMMON $(arm_env "$1") $PY gurobi_ddt/run_gurobi_ddt.py "$2" --data-dir "data/$2" > "$lg" 2>&1
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
  csv=$(awk -F'-> ' '/Done ->/{print $2}' "logs/gurobi_${A}_${sz}.log" | tail -1)
  [ -f "$csv" ] && cp "$csv" "$RES/RUN_gurobi-${A}_${sz}.csv" || log "no CSV for $A $sz (see logs/gurobi_${A}_${sz}.log)"
done; done

# column by header name
col(){ head -1 "$1" | tr ',' '\n' | grep -nx "$2" | cut -d: -f1; }

echo "=== CORRECTNESS GATE (must print nothing) ==="
for f in "$RES"/RUN_gurobi-*.csv; do
  [ -f "$f" ] || continue
  o=$(col "$f" optimal); s=$(col "$f" solved); v=$(col "$f" verified); m=$(col "$f" makespan); l=$(col "$f" provenLB)
  awk -F, -v o="$o" -v s="$s" -v v="$v" -v m="$m" -v l="$l" -v F="$(basename "$f")" \
    'NR>1 && $s=="True" && $v!="True" {print F": "$1" schedule failed verification"}
     NR>1 && $o=="True" && $m!=$l      {print F": "$1" optimal but makespan "$m" != bound "$l}' "$f"
done

echo "=== SUMMARY (proven optimal / rows, status counts) ==="
for f in "$RES"/RUN_gurobi-*.csv; do
  [ -f "$f" ] || continue
  o=$(col "$f" optimal); st=$(col "$f" status)
  opt=$(awk -F, -v o="$o" 'NR>1 && $o=="True"' "$f" | wc -l); tot=$(awk 'NR>1' "$f" | wc -l)
  stats=$(awk -F, -v c="$st" 'NR>1{n[$c]++} END{for(k in n) printf "%s=%d ", k, n[k]}' "$f")
  echo "$(basename "$f"): $opt/$tot  $stats"
done
log "=== DONE ==="
