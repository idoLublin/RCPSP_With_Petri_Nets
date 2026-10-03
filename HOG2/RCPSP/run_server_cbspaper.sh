#!/bin/bash
# ─────────────────────────────────────────────────────────────────────────────
# Server run: PAPER RUN of the valid CBS. Prepared 2026-10-03 from run_server_cpcbs.sh.
# Full sets (480 x J30, J60, J90), 300 s per instance, one core per job, one binary, every flag in every CSV row.
# VALID: no upper bound of any kind, no root solver, no SetDelay, no plain non-minimal delay.
#
# FINAL (single configuration; measured 430 / 358 / 349, 0 wrong):
#   conflict prioritisation (cardinal score, hybrid threshold 0.3) + h_CBS heuristic + MDA branching   (cfg 8)
#   lazy node evaluation                                   RCPSP_LAZY=1
#   cutset dominance, both rules, skyline storage          RCPSP_DR5=1 RCPSP_DOM_RULE=both RCPSP_SKYLINE=1
#   left-shift dominance OFF                               RCPSP_DR4=0
#   per-node deadline propagation (16 steps) + pathmax     RCPSP_CBS_TT=1 RCPSP_CBS_TT_STEPS=16 RCPSP_CBS_PATHMAX=1
#   plateau tie-break: fewest conflicts, then deeper       RCPSP_CBS_TBSPEC=GcD
# RESTART = FINAL + RCPSP_CBS_RESTART=30/GcD/0.3,30/Gcs/0.3,0/GTD/1.1 : three complete A* runs in sequence -
#   30 s fewest-conflicts-then-deeper, 30 s fewest-conflicts-then-least-delay, then the remaining 240 s branching
#   on the EARLIEST conflict (threshold 1.1). Every stage is a full admissible A*, so any path it returns is
#   optimal. Estimate from the single-order server runs (in-sample): J60 366, J90 357.
#
#   arm          what it is                                   table it feeds
#   p-restart    RESTART                                      main result row (if the restart is adopted)
#   p-final      FINAL                                        main row without restarts / "restart off"
#   p-restart-t03   RESTART with threshold 0.3 in ALL three stages (only the tie-break changes between stages)
#   p-restart-t11   RESTART with earliest-conflict selection in ALL three stages (prioritisation never used)
#   p-rs-noprop     RESTART without deadline propagation and without pathmax   the configuration with no CP-style bound
#   p-rs-noprop-pm  RESTART without deadline propagation, pathmax kept         pathmax is an A* device and works on h_CBS alone
#                   (arms without propagation peak near 19 GB per job: use MAX_PAR accordingly)
#   -- not in the default list; launch with ARMS="..." once the configuration is fixed --
#   p-noCP p-noH p-noMDA (cfg 7 / 6 / 5)   p-noDR5   p-withDR4   p-noprop   p-notb   p-nolazy      leave-one-out
#   p-onlyCP p-onlyH p-onlyMDA p-none (cfg 2 / 3 / 4 / 1 with every other FINAL feature on)        rest of the grid
#   p-bap        cfg 1 with NO flags at all: the Bell & Park baseline as the paper defines it
#   p-t01 p-t05 p-t11 (threshold 0.1 / 0.5 / 1.1)   p-tbref (the older tie-break GTD)
#
# SPLIT: every (arm, size) is cut into PARTS (default 4) interleaved instance lists that run as separate jobs;
# when the last part ends they are merged (sorted by group, exam) into new_results/RUN_<arm>_<size>.csv.
# Jobs = (arm, size, part), in the order of ARMS. MAX_PAR at once (default 12), one core each - keep MAX_PAR at
# or below the number of FREE cores, an oversubscribed run is slower and under-reports coverage.
# Memory: FINAL-type jobs measured 8-10 GB peak; arms without propagation ~20 GB. A job waits for
# MemAvailable >= JOB_AVAIL_GB (default 60). Time: 300 s per unsolved instance, so a quarter of J90 is ~2.7 h.
# ARMS="..." picks arms, SIZES, PARTS, MAX_PAR; TIMEOUT_S / ONLY=<file> (forces PARTS=1) for a spot check.
# UPLOAD  HOG2/RCPSP: *.h *.cpp run_server_cbspaper.sh
# ─────────────────────────────────────────────────────────────────────────────
set -u
REPO=${REPO:-$HOME/RCPSP_With_Petri_nets}
BIN_NAME=${BIN_NAME:-Driver_cbspaper}
cd "$REPO/HOG2/RCPSP" || { echo "cd failed"; exit 1; }

# Optional delayed start: WAIT_FOR = pgrep -f pattern of processes that must be gone first (e.g. the probe2 sweep),
# then wait until MemAvailable >= MIN_AVAIL_GB. Sources are built only after the wait.
WAIT_FOR=${WAIT_FOR:-}
MAX_PAR=${MAX_PAR:-12}
MIN_AVAIL_GB=${MIN_AVAIL_GB:-$(( MAX_PAR * 10 + 50 ))}
avail(){ awk '/MemAvailable/ {printf "%d", $2/1048576}' /proc/meminfo; }
if [ -n "$WAIT_FOR" ]; then
  echo "[$(date +%F_%H:%M:%S)] waiting for processes to finish: $WAIT_FOR"
  while pgrep -f "$WAIT_FOR" > /dev/null; do sleep 300; done
fi
while [ "$(avail)" -lt "$MIN_AVAIL_GB" ]; do echo "[$(date +%F_%H:%M:%S)] MemAvailable $(avail) GB < $MIN_AVAIL_GB"; sleep 300; done
echo "[$(date +%F_%H:%M:%S)] starting, MemAvailable $(avail) GB"

echo "=== BUILD ($(date)) ==="
g++ -std=c++20 -fopenmp -O3 -I../.. -I../gui/STUB Driver.cpp -o "$BIN_NAME" 2> build_cbspaper_err.txt \
  && echo "BUILD OK" || { echo "BUILD FAILED"; tail -40 build_cbspaper_err.txt; exit 1; }

cd "$REPO" || { echo "cd repo failed"; exit 1; }
RES=${RES:-$REPO/new_results}
LOGDIR=${LOGDIR:-$REPO/logs}
mkdir -p "$RES/paper/lists" "$LOGDIR"
ONLY=${ONLY:-}
[ -z "$ONLY" ] || [ -f "$ONLY" ] || { echo "ONLY=$ONLY not found"; exit 1; }
TIMEOUT_S=${TIMEOUT_S:-}
JOB_AVAIL_GB=${JOB_AVAIL_GB:-60}
SIZES=${SIZES:-"j90 j60 j30"}
PARTS=${PARTS:-4}; [ -n "$ONLY" ] && PARTS=1
ARMS=${ARMS:-"p-restart p-final"}
FINAL="RCPSP_LAZY=1 RCPSP_DR5=1 RCPSP_DOM_RULE=both RCPSP_SKYLINE=1 RCPSP_DR4=0 RCPSP_HYBRID=1 RCPSP_HYBRID_T=0.3 RCPSP_CBS_TT=1 RCPSP_CBS_TT_STEPS=16 RCPSP_CBS_PATHMAX=1 RCPSP_CBS_TB=0 RCPSP_CBS_TBSPEC=GcD"
RESTART=${RESTART:-"30/GcD/0.3,30/Gcs/0.3,0/GTD/1.1"}

log(){ echo "[$(date +%F_%H:%M:%S)] $*"; }
# <cfg> | <env on top of FINAL; a later assignment wins>.  p-bap drops FINAL altogether.
arm_def(){ case $1 in
  p-restart) echo "8|RCPSP_CBS_RESTART=$RESTART" ;;
  p-restart-t03)  echo "8|RCPSP_CBS_RESTART=30/GcD/0.3,30/Gcs/0.3,0/GTD/0.3" ;;
  p-restart-t11)  echo "8|RCPSP_CBS_RESTART=30/GcD/1.1,30/Gcs/1.1,0/GTD/1.1 RCPSP_HYBRID_T=1.1" ;;
  p-rs-noprop)    echo "8|RCPSP_CBS_RESTART=$RESTART RCPSP_CBS_TT=0 RCPSP_CBS_PATHMAX=0" ;;
  p-rs-noprop-pm) echo "8|RCPSP_CBS_RESTART=$RESTART RCPSP_CBS_TT=0" ;;
  p-final)   echo "8|" ;;
  p-noCP)    echo "7|" ;;
  p-noH)     echo "6|" ;;
  p-noMDA)   echo "5|" ;;
  p-onlyMDA) echo "4|" ;;
  p-onlyH)   echo "3|" ;;
  p-onlyCP)  echo "2|" ;;
  p-none)    echo "1|" ;;
  p-noDR5)   echo "8|RCPSP_DR5=0" ;;
  p-withDR4) echo "8|RCPSP_DR4=1" ;;
  p-noprop)  echo "8|RCPSP_CBS_TT=0 RCPSP_CBS_PATHMAX=0" ;;
  p-notb)    echo "8|RCPSP_CBS_TBSPEC=" ;;
  p-nolazy)  echo "8|RCPSP_LAZY=0" ;;
  p-t01)     echo "8|RCPSP_HYBRID_T=0.1" ;;
  p-t05)     echo "8|RCPSP_HYBRID_T=0.5" ;;
  p-t11)     echo "8|RCPSP_HYBRID_T=1.1" ;;
  p-tbref)   echo "8|RCPSP_CBS_TBSPEC=GTD" ;;
  p-bap)     echo "1|PLAIN" ;;
  *)         return 1 ;;
esac; }
for A in $ARMS; do arm_def "$A" > /dev/null || { log "unknown arm $A"; exit 1; }; done

# instance list of one part: exams dealt round-robin, rotated every 4 groups, so each part gets the same number of
# instances from every parameter class (RS 0.2 / 0.5 / 0.7 / 1.0) and the parts take about the same time
part_list(){ # <size> <part>
  [ -n "$ONLY" ] && { echo "$ONLY"; return; }
  local f="$RES/paper/lists/${1}_p${2}of${PARTS}.txt"
  [ -s "$f" ] || awk -v sz="$1" -v p="$2" -v P="$PARTS" 'BEGIN{ for (g = 1; g <= 48; g++) for (e = 1; e <= 10; e++) if (((e - 1) + 2 * int((g - 1) / 4)) % P == p - 1) print sz "," g "," e }' > "$f"
  echo "$f"; }

JOBS=""
for A in $ARMS; do for sz in $SIZES; do for p in $(seq 1 $PARTS); do JOBS="$JOBS $A:$sz:$p"; done; done; done

part_csv(){ awk -F'-> ' '/Done ->/{print $2}' "$LOGDIR/$1_$2_p$3.log" 2>/dev/null | tail -1; }
# merge the parts of (arm, size) once every part has finished: header once, rows sorted by group, exam
merge_if_complete(){ # <arm> <size>
  local p f files=""
  for p in $(seq 1 $PARTS); do f=$(part_csv "$1" "$2" "$p"); [ -n "$f" ] && [ -f "$f" ] || return 0; files="$files $f"; done
  local out="$RES/RUN_$1_$2.csv" tmp="$RES/paper/.merge_$1_$2.$$"
  { head -1 $(echo $files | cut -d' ' -f1); for f in $files; do tail -n +2 "$f"; done | sort -t, -k1,1n -k2,2n; } > "$tmp" && mv "$tmp" "$out"
  log "  MERGED $1 $2 -> $(basename "$out"): finished=$(awk -F, 'NR>1 && $13=="True"' "$out" | wc -l)/$(awk 'NR>1' "$out" | wc -l)"
}

run_job(){ # <arm> <size> <part>
  local d="$RES/paper/$1/$2_p$3" lg="$LOGDIR/$1_$2_p$3.log" def cfg extra base list
  def=$(arm_def "$1"); cfg=${def%%|*}; extra=${def#*|}; base=$FINAL
  [ "$extra" = PLAIN ] && { base=""; extra=""; }
  list=$(part_list "$2" "$3")
  mkdir -p "$d"
  log "  start $1 $2 part $3/$PARTS [cfg$cfg $extra]"
  env $base $extra RCPSP_RESULTS_DIR="$d" RCPSP_ONLY="$list" ${TIMEOUT_S:+RCPSP_TIMEOUT_S=$TIMEOUT_S} \
      ./HOG2/RCPSP/$BIN_NAME $2 $cfg > "$lg" 2>&1
  log "  end   $1 $2 part $3/$PARTS exit=$?"
  merge_if_complete "$1" "$2"
}

log "starting: MAX_PAR=$MAX_PAR PARTS=$PARTS ($(nproc) cores on this machine) jobs:$JOBS"
for J in $JOBS; do IFS=: read -r A sz p <<< "$J"
  while [ "$(jobs -rp | wc -l)" -ge "$MAX_PAR" ]; do wait -n; done
  while [ "$(avail)" -lt "$JOB_AVAIL_GB" ]; do log "MemAvailable $(avail) GB < $JOB_AVAIL_GB, holding $A $sz part $p"; sleep 300; done
  run_job "$A" "$sz" "$p" &
  sleep 2
done
wait
log "=== all jobs done ==="

col(){ head -1 "$1" | tr ',' '\n' | grep -nx "$2" | cut -d: -f1; }

echo "=== CORRECTNESS GATE (must print nothing) ==="
for A in $ARMS; do for sz in $SIZES; do f="$RES/RUN_${A}_${sz}.csv"; [ -f "$f" ] || continue
  awk -F, -v F="RUN_${A}_${sz}" 'NR>1 && $13=="True" && $5=="False"{print F": grp"$1" ex"$2" mk="$4" opt="$8}' "$f"
done; done

echo "=== EFFECTIVE FLAGS AND RESULT (rows must be 480) ==="
for A in $ARMS; do for sz in $SIZES; do f="$RES/RUN_${A}_${sz}.csv"; [ -f "$f" ] || { echo "RUN_${A}_${sz}: MISSING"; continue; }
  fl=""; for c in useConflictPrioritization useHeuristic useMDASets useLazy useDR5 useDR4 useHybrid hybridT cbsTT ttSteps cbsPathmax cbsTbSpec cbsRestart useUB useSetDelay; do
    i=$(col "$f" $c); [ -n "$i" ] && fl="$fl $c=$(awk -F, -v i="$i" 'NR==2{print $i}' "$f")"; done
  fin=$(awk -F, 'NR>1 && $13=="True"' "$f" | wc -l); tot=$(awk -F, 'NR>1' "$f" | wc -l)
  st=""; i=$(col "$f" restartStage); [ -n "$i" ] && st=" solved-by-stage: $(awk -F, -v i="$i" 'NR>1 && $13=="True"{n[$i]++} END{for(k in n) printf "%s=%d ", k, n[k]}' "$f")"
  echo "RUN_${A}_${sz}: finished=$fin/$tot |$fl |$st"
done; done
log "=== DONE ==="
