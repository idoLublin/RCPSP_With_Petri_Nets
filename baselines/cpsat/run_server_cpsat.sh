#!/bin/bash
# ─────────────────────────────────────────────────────────────────────────────
# Server run: OR-Tools CP-SAT COMPONENT CHECK. Prepared 2026-10-03 from baselines/scip_ddt/run_server_scip.sh.
# Run on dsisco01 from the repo's baselines dir (data in baselines/data/{j30,j60,j90}, 480 .sm each).
#
# QUESTION: on the low-resource-strength instances CP-SAT proves in seconds and our searches do not, which part
# of CP-SAT does the work? Each arm switches ONE thing (1 worker, 300 s, model build inside the budget, every
# schedule checked independently). What an arm losing instances would tell us:
#   default      CP-SAT defaults                                 control
#   nolp         no LP relaxation (linearization level 0)        the LP / cuts matter
#   nopresolve   presolve off                                    model reductions matter
#   nodisj       no pairwise disjunctive reasoning in cumulative pairwise "cannot overlap" reasoning matters
#   ttef         + time-table edge finding   (gain, not loss)    stronger energy reasoning would pay off
#   overload     + energetic overload checker (gain, not loss)   same
#   fixed        chronological branching instead of the learned  the conflict-driven choice of WHAT to branch on matters
#   norestart    no restarts                                     restarts matter
#   forget       learned clauses deleted almost immediately      keeping what was learned matters (closest CP-SAT
#                                                                gets to "no learning")
#   core/lbtree  CP-SAT's two "prove from below" modes           the DIRECTION of the proof matters (our A* is from below)
#   prove        only refute "makespan <= C* - 1" (C* known)     how hard the optimality proof alone is
#   find         only find a schedule of makespan <= C*          how hard the primal side alone is
#   bare         nolp + nopresolve + nodisj + fixed + norestart  time-table propagation + learning, nothing else
#   full         CP-SAT defaults on ALL 480 instances per size   a server-measured, verified CP-SAT baseline
#                (optional, not in the default list)
# Instances for every arm but `full`: cpsat/cp_focus.txt = proven by CP-SAT or Chuffed and NOT by the valid CBS
# with restarts: 30 on J30, 64 on J60, 43 on J90.
# The CSV also has, per instance, conflicts / branches / restarts, and for the minimising arms the time at which
# the incumbent and the proven bound each reached the final makespan (tIncumbentBest / tBoundBest).
#
# ARMS="..." picks arms. SIZES="..." (default j30 j60 j90). MAX_PAR jobs at once (default 12), one core each.
# Memory is small (CP-SAT, 1 worker: a few GB at most); the start gate keeps 50 GB free.
# Time: a job costs at most 300 s per instance of its list (J60: 64 -> 5.3 h if it solved nothing).
# Output: new_results/cpsat/<arm>/cpsat_<size>_<flags>_vNN.csv, copied to new_results/RUN_cpsat-<arm>_<size>.csv
# AS SOON AS THAT JOB ENDS.
# UPLOAD  baselines/cpsat: run_cpsat.py run_server_cpsat.sh cp_focus.txt   (needs baselines/gurobi_ddt/run_gurobi_ddt.py
#         and <repo>/known_optima.csv, both already on the server)
# ─────────────────────────────────────────────────────────────────────────────
set -u
REPO=${REPO:-$HOME/RCPSP_With_Petri_nets}
cd "$REPO/baselines" || { echo "cd failed"; exit 1; }
PY=${PY:-python3}
ARMS=${ARMS:-"default nolp nopresolve nodisj ttef overload fixed norestart forget core lbtree prove find bare"}
SIZES=${SIZES:-"j30 j60 j90"}
MAX_PAR=${MAX_PAR:-12}
ONLY=${ONLY:-$REPO/baselines/cpsat/cp_focus.txt}
TIMEOUT_S=${TIMEOUT_S:-300}
DATA=${DATA:-data}
MIN_AVAIL_GB=${MIN_AVAIL_GB:-$(( MAX_PAR * 4 + 50 ))}
RES=${RES:-$REPO/new_results}
LOGDIR=${LOGDIR:-$REPO/baselines/logs}
mkdir -p "$RES" "$LOGDIR"

log(){ echo "[$(date +%F_%H:%M:%S)] $*"; }
avail(){ awk '/MemAvailable/ {printf "%d", $2/1048576}' /proc/meminfo; }

# ── pre-flight: data, solver, shared helpers, known optima
for sz in $SIZES; do
  n=$(ls $DATA/$sz/${sz}*_*.sm 2>/dev/null | wc -l)
  [ "$n" -eq 480 ] || { log "$DATA/$sz has $n instance files, expected 480"; exit 1; }
done
[ -f gurobi_ddt/run_gurobi_ddt.py ] || { log "gurobi_ddt/run_gurobi_ddt.py missing (helpers are imported from it)"; exit 1; }
[ -f "$REPO/known_optima.csv" ] || { log "$REPO/known_optima.csv missing (the prove / find arms need it)"; exit 1; }
$PY -c "import ortools; print('ortools', ortools.__version__)" || { log "ortools import failed: pip install --user ortools"; exit 1; }
[ -f "$ONLY" ] || { log "ONLY=$ONLY not found"; exit 1; }

arm_env(){ case $1 in
  default|full) echo "" ;;
  nolp)       echo "RCPSP_CPSAT_LIN=0" ;;
  nopresolve) echo "RCPSP_CPSAT_NOPRESOLVE=1" ;;
  nodisj)     echo "RCPSP_CPSAT_NODISJ=1" ;;
  ttef)       echo "RCPSP_CPSAT_TTEF=1" ;;
  overload)   echo "RCPSP_CPSAT_OVERLOAD=1" ;;
  fixed)      echo "RCPSP_CPSAT_SEARCH=fixed" ;;
  norestart)  echo "RCPSP_CPSAT_NORESTART=1" ;;
  forget)     echo "RCPSP_CPSAT_FORGET=1" ;;
  core)       echo "RCPSP_CPSAT_OPT=core" ;;
  lbtree)     echo "RCPSP_CPSAT_OPT=lbtree" ;;
  prove)      echo "RCPSP_CPSAT_MODE=prove" ;;
  find)       echo "RCPSP_CPSAT_MODE=find" ;;
  bare)       echo "RCPSP_CPSAT_LIN=0 RCPSP_CPSAT_NOPRESOLVE=1 RCPSP_CPSAT_NODISJ=1 RCPSP_CPSAT_SEARCH=fixed RCPSP_CPSAT_NORESTART=1" ;;
  *)          return 1 ;;
esac; }
for A in $ARMS; do arm_env "$A" > /dev/null || { log "unknown arm $A"; exit 1; }; done

while [ "$(avail)" -lt "$MIN_AVAIL_GB" ]; do log "MemAvailable $(avail) GB < $MIN_AVAIL_GB"; sleep 300; done
log "starting: ARMS=[$ARMS] SIZES=[$SIZES] MAX_PAR=$MAX_PAR ONLY=$(basename "$ONLY") MemAvailable=$(avail)GB"

run_job(){ # <arm> <size>
  local d="$RES/cpsat/$1" lg="$LOGDIR/cpsat_$1_$2.log" only csv
  only="RCPSP_ONLY=$ONLY"; [ "$1" = full ] && only=""
  mkdir -p "$d"
  log "  start $1 $2 [$(arm_env "$1")]"
  env RCPSP_CPSAT_WORKERS=1 RCPSP_CPSAT_TIMEOUT=$TIMEOUT_S RCPSP_RESULTS_DIR="$d" $only $(arm_env "$1") \
      $PY cpsat/run_cpsat.py "$2" --data-dir "$DATA/$2" > "$lg" 2>&1
  local rc=$?
  csv=$(awk -F'-> ' '/Done ->/{print $2}' "$lg" | tail -1)
  [ -f "$csv" ] && cp "$csv" "$RES/RUN_cpsat-$1_$2.csv"
  log "  end   $1 $2 exit=$rc ($(grep -c '^  \[' "$lg") rows)"
}

# J90 of every arm first, then J60, then J30 (the sizes that matter most come back first)
for sz in $(echo $SIZES | tr ' ' '\n' | sort -r | tr '\n' ' '); do for A in $ARMS; do
  while [ "$(jobs -rp | wc -l)" -ge "$MAX_PAR" ]; do wait -n; done
  run_job "$A" "$sz" &
  sleep 2
done; done
wait
log "=== all jobs done ==="

col(){ head -1 "$1" | tr ',' '\n' | grep -nx "$2" | cut -d: -f1; }

echo "=== CORRECTNESS GATE (must print nothing) ==="
for f in "$RES"/RUN_cpsat-*.csv; do
  [ -f "$f" ] || continue
  o=$(col "$f" optimal); s=$(col "$f" solved); v=$(col "$f" verified); m=$(col "$f" makespan)
  k=$(col "$f" knownOpt); st=$(col "$f" status); md=$(col "$f" cpMode)
  awk -F, -v o="$o" -v s="$s" -v v="$v" -v m="$m" -v k="$k" -v c="$st" -v md="$md" -v F="$(basename "$f")" \
    'NR>1 && $s=="True" && $v!="True"                      {print F": "$1" schedule failed verification"}
     NR>1 && $md=="opt" && $o=="True" && $k!="" && $m!=$k   {print F": "$1" optimal "$m" but known optimum "$k}
     NR>1 && $c=="WRONG_KNOWN_OPT"                           {print F": "$1" found a schedule below the known optimum"}
     NR>1 && $c ~ /^ERROR/                                   {print F": "$1" "$c}' "$f"
done

echo "=== SUMMARY: proven / rows | solved within 1 s, 10 s, 60 s | conflicts in total ==="
for sz in $SIZES; do for A in $ARMS; do f="$RES/RUN_cpsat-${A}_${sz}.csv"; [ -f "$f" ] || { echo "RUN_cpsat-${A}_${sz}: MISSING"; continue; }
  o=$(col "$f" optimal); t=$(col "$f" total_s); c=$(col "$f" conflicts)
  awk -F, -v o="$o" -v t="$t" -v c="$c" -v N="cpsat-${A}_${sz}" 'NR>1{ n++; cf+=$c; if ($o=="True") { p++; if ($t<=1) a++; if ($t<=10) b++; if ($t<=60) d++ } }
    END{ printf "%-24s %3d / %3d | %3d %3d %3d | %d\n", N, p, n, a, b, d, cf }' "$f"
done; done
log "=== DONE ==="
