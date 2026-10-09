#!/bin/bash
# ─────────────────────────────────────────────────────────────────────────────
# Server run: SCIP on the On/Off Event-based MILP (Kone et al. 2011). Prepared 2026-10-06 from run_server_scip.sh.
# Run on dsisco01 from the repo's baselines dir (data in baselines/data/{j30,j60,j90}, 480 .sm each).
#
# Same solver layer and limits as the time-indexed SCIP baseline: 1 thread, 300 s per instance WITH the model build
# inside the budget (RCPSP_SCIP_BUILD_IN_BUDGET=1), limits/memory SOFTMEM GB, independent schedule check ('verified').
# The event-based model is expected to time out on most instances, so each size is split into PARTS lists that run
# side by side (a single job per size would need up to 480 x 300 s = 40 h).
#   SIZES="..." (default j30 j60 j90)   PARTS per size (default 8)   MAX_PAR jobs at once (default 24)
# Starts only when MemAvailable >= MAX_PAR*SOFTMEM + 50 GB (keep 50 GB free at the worst case).
# Output: new_results/parts/scip-ooe_<size>_p<k>.csv, merged into new_results/RUN_scip-ooe_<size>.csv
# UPLOAD  baselines/scip_ooe: run_scip_ooe.py run_server_scip_ooe.sh   (needs baselines/gurobi_ddt/run_gurobi_ddt.py
#         present too -- the parser, the SGS bound and the schedule verifier are imported from it)
# ─────────────────────────────────────────────────────────────────────────────
set -u
REPO=~/RCPSP_With_Petri_nets
cd "$REPO/baselines" || { echo "cd failed"; exit 1; }
PY=${PY:-python3}
SIZES=${SIZES:-"j30 j60 j90"}
PARTS=${PARTS:-8}
MAX_PAR=${MAX_PAR:-24}
SOFTMEM=${SOFTMEM:-8}
MIN_AVAIL_GB=${MIN_AVAIL_GB:-$(( MAX_PAR * SOFTMEM + 50 ))}
RES="$REPO/new_results"
mkdir -p "$RES/parts" logs

log(){ echo "[$(date +%F_%H:%M:%S)] $*"; }
avail(){ awk '/MemAvailable/ {printf "%d", $2/1048576}' /proc/meminfo; }

# ── pre-flight: data, solver, shared helpers
for sz in $SIZES; do
  n=$(ls data/$sz/${sz}*_*.sm 2>/dev/null | wc -l)
  [ "$n" -eq 480 ] || { log "data/$sz has $n instance files, expected 480"; exit 1; }
done
[ -f gurobi_ddt/run_gurobi_ddt.py ] || { log "gurobi_ddt/run_gurobi_ddt.py missing (helpers are imported from it)"; exit 1; }
[ -f scip_ooe/run_scip_ooe.py ] || { log "scip_ooe/run_scip_ooe.py missing"; exit 1; }
$PY -c "from pyscipopt import Model; print('pyscipopt OK, SCIP', Model().version())" \
  || { log "pyscipopt import failed (pip install pyscipopt)"; exit 1; }

# ── part lists: instance (group, exam) goes to part ((group-1)*10 + exam-1) mod PARTS, so every part mixes all groups
for sz in $SIZES; do for p in $(seq 0 $((PARTS - 1))); do
  f="$RES/parts/only_ooe_${sz}_p${p}.txt"; : > "$f.tmp"
  for g in $(seq 1 48); do for e in $(seq 1 10); do
    [ $(( ((g - 1) * 10 + e - 1) % PARTS )) -eq "$p" ] && echo "$sz,$g,$e" >> "$f.tmp"
  done; done
  mv "$f.tmp" "$f"
done; done

while [ "$(avail)" -lt "$MIN_AVAIL_GB" ]; do log "MemAvailable $(avail) GB < $MIN_AVAIL_GB"; sleep 300; done
log "starting: SIZES=[$SIZES] PARTS=$PARTS MAX_PAR=$MAX_PAR SOFTMEM=${SOFTMEM}GB MemAvailable=$(avail)GB"

COMMON="RCPSP_SCIP_BUILD_IN_BUDGET=1 RCPSP_SCIP_SOFTMEM=$SOFTMEM RCPSP_RESULTS_DIR=$RES"
run_job(){ # <size> <part>
  local lg="logs/scip_ooe_$1_p$2.log" out="$RES/parts/scip-ooe_$1_p$2.csv"
  log "  start $1 part $2"
  env $COMMON RCPSP_ONLY="$RES/parts/only_ooe_$1_p$2.txt" $PY scip_ooe/run_scip_ooe.py "$1" --data-dir "data/$1" --out "$out" > "$lg" 2>&1
  local rc=$?
  log "  end   $1 part $2 exit=$rc ($(grep -c '^  \[' "$lg") rows)"
}

for sz in $SIZES; do for p in $(seq 0 $((PARTS - 1))); do
  while [ "$(jobs -rp | wc -l)" -ge "$MAX_PAR" ]; do wait -n; done
  run_job "$sz" "$p" &
  sleep 5
done; done
wait
log "=== all jobs done ==="

# ── merge the parts of each size (header once), sorted by group, exam
col(){ head -1 "$1" | tr ',' '\n' | grep -nx "$2" | cut -d: -f1; }
for sz in $SIZES; do
  first="$RES/parts/scip-ooe_${sz}_p0.csv"
  [ -f "$first" ] || { log "no part files for $sz"; continue; }
  g=$(col "$first" group); e=$(col "$first" exam)
  { head -1 "$first"; for p in $(seq 0 $((PARTS - 1))); do tail -n +2 "$RES/parts/scip-ooe_${sz}_p${p}.csv"; done | sort -t, -k"$g","$g"n -k"$e","$e"n; } > "$RES/RUN_scip-ooe_${sz}.csv.tmp"
  mv "$RES/RUN_scip-ooe_${sz}.csv.tmp" "$RES/RUN_scip-ooe_${sz}.csv"
  log "merged $sz: $(( $(wc -l < "$RES/RUN_scip-ooe_${sz}.csv") - 1 )) rows"
done

echo "=== CORRECTNESS GATE (must print nothing) ==="
for f in "$RES"/RUN_scip-ooe_*.csv; do
  [ -f "$f" ] || continue
  o=$(col "$f" optimal); s=$(col "$f" solved); v=$(col "$f" verified); m=$(col "$f" makespan); l=$(col "$f" provenLB)
  st=$(col "$f" status)
  awk -F, -v o="$o" -v s="$s" -v v="$v" -v m="$m" -v l="$l" -v c="$st" -v F="$(basename "$f")" \
    'NR>1 && $s=="True" && $v!="True"  {print F": "$1" schedule failed verification"}
     NR>1 && $o=="True" && $m!=$l      {print F": "$1" optimal but makespan "$m" != bound "$l}
     NR>1 && $o=="True" && $c!="optimal" {print F": "$1" optimal=True but status "$c}' "$f"
done

echo "=== SUMMARY (proven optimal / rows, status counts) ==="
for f in "$RES"/RUN_scip-ooe_*.csv; do
  [ -f "$f" ] || continue
  o=$(col "$f" optimal); st=$(col "$f" status)
  opt=$(awk -F, -v o="$o" 'NR>1 && $o=="True"' "$f" | wc -l); tot=$(awk 'NR>1' "$f" | wc -l)
  stats=$(awk -F, -v c="$st" 'NR>1{n[$c]++} END{for(k in n) printf "%s=%d ", k, n[k]}' "$f")
  echo "$(basename "$f"): $opt/$tot  $stats"
done
log "=== DONE ==="
