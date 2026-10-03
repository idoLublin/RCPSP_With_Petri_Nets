#!/usr/bin/env bash
# Root-node LP bound-quality experiment on the lab server, j30 / j60 / j90.
# Edit the paths below, then:  bash run_server.sh
set -euo pipefail
cd "$(dirname "$0")"

# --- EDIT THESE ------------------------------------------------------------
DATA30="${DATA30:-$HOME/rcpsp/j30.sm}"     # dir of j30*_*.sm files
DATA60="${DATA60:-$HOME/rcpsp/j60.sm}"     # dir of j60*_*.sm files
DATA90="${DATA90:-$HOME/rcpsp/j90.sm}"     # dir of j90*_*.sm files
OPTIMA="${OPTIMA:-./known_optima.csv}"     # scp'd alongside this folder
BACKEND="${BACKEND:-gurobi}"               # gurobi (needs license) | scipy (HiGHS)
JOBS="${JOBS:-$(nproc)}"                   # parallel instances
SUBSET="${SUBSET:-all}"                    # all | 2 (per param, ~96) | 1 (~48)
TIMELIMIT="${TIMELIMIT:-120}"             # per-LP-solve cap (s); 0 = none
PY="${PY:-python3}"
# ---------------------------------------------------------------------------

echo "backend=$BACKEND jobs=$JOBS subset=$SUBSET timelimit=$TIMELIMIT"
$PY -c "import sys;sys.path.insert(0,'.');import rcpsp_lp as R;print('resolved backend:',R.available_backend('$BACKEND'))"

run() {  # size datadir
  local size="$1" data="$2"
  if [[ ! -d "$data" ]]; then echo "!! skip $size: no dir $data"; return; fi
  echo "=================== $size ($data) ==================="
  $PY run.py --data "$data" --size "$size" --optima "$OPTIMA" \
      --backend "$BACKEND" --jobs "$JOBS" --subset "$SUBSET" \
      --cuts 1 --maxsize 3 --rounds 8 --timelimit "$TIMELIMIT" \
      2>&1 | tee "log_${size}_${BACKEND}_${SUBSET}.txt"
}

run j30 "$DATA30"
run j60 "$DATA60"
run j90 "$DATA90"
echo "done. CSVs: lp_rootbound_*.csv   logs: log_*.txt"
