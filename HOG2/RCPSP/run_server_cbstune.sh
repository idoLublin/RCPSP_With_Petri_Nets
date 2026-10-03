#!/bin/bash
# ─────────────────────────────────────────────────────────────────────────────
# Server run: TUNING GRID for the restart schedule of the valid CBS. Prepared 2026-10-03 from run_server_cpcbs.sh.
#
# WHY: the schedule of the first paper run (threshold 0.3 with two tie-breaks, then earliest-conflict selection)
# was picked from the few single runs that happened to exist - tie-breaks only ever measured at threshold 0.3,
# earliest-conflict selection only with one tie-break, threshold 1.0 (the classic "cardinal conflicts first"
# rule) never measured with the current stack. This run measures EVERY combination as a plain single search:
#   threshold  0.1  0.3  0.5  0.7  1.0  1.1      (>= threshold: pick by cardinal score; below: earliest conflict.
#                                                 1.0 = fully cardinal conflicts first; 1.1 = always earliest)
#   tie-break  cd = fewest conflicts, then deeper          cs = fewest conflicts, then least total delay
#              td = later first-conflict time, then deeper od = least total overload, then deeper
# = 24 configurations, each a 300 s single search with no restart. Everything else is the FINAL configuration:
# cfg8, lazy, DR5 both + skyline, DR4 off, deadline propagation (16 steps) + pathmax.
#
# Because every row records the instance's solve time, ANY restart schedule over these configurations can then
# be evaluated exactly offline (the first schedule was predicted this way: 366 / 357, measured 366 / 357). The
# schedule is chosen by an explicit rule on half of the instances and checked on the other half, and the same
# files give the threshold-sensitivity table of the paper.
#
# Instances: J30 all 480; J60 / J90 on screen_j60_j90.txt (391 / 384: everything except the instances no solver
# or CBS arm has ever closed). Every (configuration, size) is cut into PARTS (default 2) interleaved lists that
# run as separate jobs and are merged when the last part ends.
# ADMISSION BY MEASURED PEAK: a job starts only if, were EVERY driver this user is running to reach its peak
# RSS, KEEP_FREE_GB (50) would still be free and the new job's own peak would fit. Peaks: this configuration
# 8 / 11 / 14 GB (J30 / J60 / J90; measured 7.2 / 10.3 / 13.4); any other CBS driver still running 20 GB (the
# no-propagation arm measured 19.4); Petri drivers 63 GB. So it starts as wide as memory allows and widens by
# itself as other runs finish. MAX_PAR (default 28) only caps the number of cores used.
# Time: about 250 core-hours in total, i.e. roughly 10-12 h once 20+ jobs fit.
# Output: new_results/tune/<arm>/..., merged to new_results/RUN_tn-t<TT>-<tb>_<size>.csv per finished pair.
# THRESHOLDS="..." TIEBREAKS="..." SIZES="..." pick a sub-grid; TIMEOUT_S / ONLY=<file> for a spot check.
# UPLOAD  HOG2/RCPSP: *.h *.cpp run_server_cbstune.sh screen_j60_j90.txt
# ─────────────────────────────────────────────────────────────────────────────
set -u
REPO=${REPO:-$HOME/RCPSP_With_Petri_nets}
BIN_NAME=${BIN_NAME:-Driver_cbstune}
cd "$REPO/HOG2/RCPSP" || { echo "cd failed"; exit 1; }

# Optional delayed start: WAIT_FOR = pgrep -f pattern of processes that must be gone first (e.g. the probe2 sweep),
# then wait until MemAvailable >= MIN_AVAIL_GB. Sources are built only after the wait.
WAIT_FOR=${WAIT_FOR:-}
MAX_PAR=${MAX_PAR:-28}
MIN_AVAIL_GB=${MIN_AVAIL_GB:-0}
avail(){ awk '/MemAvailable/ {printf "%d", $2/1048576}' /proc/meminfo; }
if [ -n "$WAIT_FOR" ]; then
  echo "[$(date +%F_%H:%M:%S)] waiting for processes to finish: $WAIT_FOR"
  while pgrep -f "$WAIT_FOR" > /dev/null; do sleep 300; done
fi
while [ "$(avail)" -lt "$MIN_AVAIL_GB" ]; do echo "[$(date +%F_%H:%M:%S)] MemAvailable $(avail) GB < $MIN_AVAIL_GB"; sleep 300; done
echo "[$(date +%F_%H:%M:%S)] starting, MemAvailable $(avail) GB"

echo "=== BUILD ($(date)) ==="
g++ -std=c++20 -fopenmp -O3 -I../.. -I../gui/STUB Driver.cpp -o "$BIN_NAME" 2> build_cbstune_err.txt \
  && echo "BUILD OK" || { echo "BUILD FAILED"; tail -40 build_cbstune_err.txt; exit 1; }

cd "$REPO" || { echo "cd repo failed"; exit 1; }
RES=${RES:-$REPO/new_results}
LOGDIR=${LOGDIR:-$REPO/logs}
mkdir -p "$RES/tune/lists" "$LOGDIR"
SCREEN="$REPO/HOG2/RCPSP/screen_j60_j90.txt"
ONLY=${ONLY:-}
[ -z "$ONLY" ] || [ -f "$ONLY" ] || { echo "ONLY=$ONLY not found"; exit 1; }
[ -f "$SCREEN" ] || { echo "missing $SCREEN"; exit 1; }
TIMEOUT_S=${TIMEOUT_S:-}
KEEP_FREE_GB=${KEEP_FREE_GB:-50}
PEAK_J30=${PEAK_J30:-8}; PEAK_J60=${PEAK_J60:-11}; PEAK_J90=${PEAK_J90:-14}
PEAK_OTHER_CBS=${PEAK_OTHER_CBS:-20}; PEAK_PETRI=${PEAK_PETRI:-63}
SIZES=${SIZES:-"j90 j60 j30"}
THRESHOLDS=${THRESHOLDS:-"0.3 1.0 1.1 0.5 0.7 0.1"}
TIEBREAKS=${TIEBREAKS:-"cd cs td od"}
PARTS=${PARTS:-2}; [ -n "$ONLY" ] && PARTS=1
FINAL="RCPSP_LAZY=1 RCPSP_DR5=1 RCPSP_DOM_RULE=both RCPSP_SKYLINE=1 RCPSP_DR4=0 RCPSP_HYBRID=1 RCPSP_CBS_TT=1 RCPSP_CBS_TT_STEPS=16 RCPSP_CBS_PATHMAX=1 RCPSP_CBS_TB=0"

log(){ echo "[$(date +%F_%H:%M:%S)] $*"; }
tb_spec(){ case $1 in cd) echo GcD ;; cs) echo Gcs ;; td) echo GTD ;; od) echo GoD ;; *) return 1 ;; esac; }
for b in $TIEBREAKS; do tb_spec "$b" > /dev/null || { log "unknown tie-break $b"; exit 1; }; done
arm_name(){ echo "tn-t$(echo "$1" | tr -d .)-$2"; }      # 0.3 cd -> tn-t03-cd
peak_of(){ case $1 in j30) echo $PEAK_J30 ;; j60) echo $PEAK_J60 ;; *) echo $PEAK_J90 ;; esac; }

# GB that would remain above KEEP_FREE_GB if every driver this user is running grew to its measured peak
headroom(){
  ps -u "$(id -un)" -o rss=,args= 2>/dev/null | awk -v avail="$(avail)" -v keep="$KEEP_FREE_GB" -v me="/$BIN_NAME " \
      -v p30="$PEAK_J30" -v p60="$PEAK_J60" -v p90="$PEAK_J90" -v pcbs="$PEAK_OTHER_CBS" -v ptt="$PEAK_PETRI" '
    { rss = $1 / 1048576; pk = 0
      if (index($0, me))                                             pk = ($0 ~ / j30 /) ? p30 : ($0 ~ / j60 /) ? p60 : p90
      else if ($0 ~ /\/Driver_[A-Za-z0-9_]+ +(tt2|tt2rev|tt2hbae)_j/) pk = ptt
      else if ($0 ~ /\/Driver_[A-Za-z0-9_]+ +j[0-9]+ +[0-9]/)        pk = pcbs
      if (pk > rss) g += pk - rss }
    END{ printf "%d", avail - g - keep }'; }

# instance list of one job. J30: exams dealt round-robin, rotated every 4 groups, so each part gets the same
# number of instances from every resource-strength class. J60 / J90: the screen list of that size, dealt round-robin.
job_list(){ # <size> <part>
  [ -n "$ONLY" ] && { echo "$ONLY"; return; }
  local f="$RES/tune/lists/${1}_p${2}of${PARTS}.txt"
  if [ ! -s "$f" ]; then
    if [ "$1" = j30 ]; then
      awk -v p="$2" -v P="$PARTS" 'BEGIN{ for (g = 1; g <= 48; g++) for (e = 1; e <= 10; e++) if (((e - 1) + 2 * int((g - 1) / 4)) % P == p - 1) print "j30," g "," e }' > "$f"
    else
      grep "^$1," "$SCREEN" | awk -v p="$2" -v P="$PARTS" '(NR - 1) % P == p - 1' > "$f"
    fi
  fi
  echo "$f"; }

JOBS=""
for sz in $SIZES; do for T in $THRESHOLDS; do for b in $TIEBREAKS; do
  for p in $(seq 1 $PARTS); do JOBS="$JOBS $T:$b:$sz:$p"; done
done; done; done

part_csv(){ awk -F'-> ' '/Done ->/{print $2}' "$LOGDIR/$1_$2_p$3.log" 2>/dev/null | tail -1; }
merge_if_complete(){ # <arm> <size>
  local p f files=""
  for p in $(seq 1 $PARTS); do f=$(part_csv "$1" "$2" "$p"); [ -n "$f" ] && [ -f "$f" ] || return 0; files="$files $f"; done
  local out="$RES/RUN_$1_$2.csv" tmp="$RES/tune/.merge_$1_$2.$$"
  { head -1 $(echo $files | cut -d' ' -f1); for f in $files; do tail -n +2 "$f"; done | sort -t, -k1,1n -k2,2n; } > "$tmp" && mv "$tmp" "$out"
  log "  DONE $1 $2 -> $(basename "$out"): finished=$(awk -F, 'NR>1 && $13=="True"' "$out" | wc -l)/$(awk 'NR>1' "$out" | wc -l)"
}

run_job(){ # <threshold> <tb> <size> <part>
  local arm; arm=$(arm_name "$1" "$2")
  local d="$RES/tune/$arm/$3_p$4" lg="$LOGDIR/${arm}_$3_p$4.log" list
  list=$(job_list "$3" "$4"); mkdir -p "$d"
  log "  start $arm $3 part $4/$PARTS [T=$1 spec=$(tb_spec "$2")]"
  env $FINAL RCPSP_HYBRID_T=$1 RCPSP_CBS_TBSPEC="$(tb_spec "$2")" RCPSP_RESULTS_DIR="$d" RCPSP_ONLY="$list" \
      ${TIMEOUT_S:+RCPSP_TIMEOUT_S=$TIMEOUT_S} ./HOG2/RCPSP/$BIN_NAME $3 8 > "$lg" 2>&1
  log "  end   $arm $3 part $4/$PARTS exit=$?"
  merge_if_complete "$arm" "$3"
}

for sz in $SIZES; do for p in $(seq 1 $PARTS); do job_list "$sz" "$p" > /dev/null; done; done   # lists made once, up front
log "starting: MAX_PAR=$MAX_PAR ($(nproc) cores) PARTS=$PARTS thresholds=[$THRESHOLDS] tie-breaks=[$TIEBREAKS] sizes=[$SIZES] jobs=$(echo $JOBS | wc -w) headroom=$(headroom)GB"
held=0
for J in $JOBS; do IFS=: read -r T b sz p <<< "$J"
  while :; do
    n=$(jobs -rp | wc -l)
    if [ "$n" -ge "$MAX_PAR" ]; then wait -n; continue; fi
    h=$(headroom)
    [ "$h" -ge "$(peak_of $sz)" ] && break
    [ $(( held % 20 )) -eq 0 ] && log "holding (running $n, headroom $h GB < $(peak_of $sz) GB needed for a $sz job)"
    held=$(( held + 1 )); sleep 30
  done
  run_job "$T" "$b" "$sz" "$p" &
  sleep 6            # let the new driver appear in the process table before the next admission test
done
wait
log "=== all jobs done ==="

col(){ head -1 "$1" | tr ',' '\n' | grep -nx "$2" | cut -d: -f1; }
echo "=== CORRECTNESS GATE (must print nothing) ==="
for f in "$RES"/RUN_tn-*.csv; do [ -f "$f" ] || continue
  awk -F, -v F="$(basename "$f")" 'NR>1 && $13=="True" && $5=="False"{print F": grp"$1" ex"$2" mk="$4" opt="$8}' "$f"
done
echo "=== SOLVED (rows: j30 480, j60 391, j90 384); columns = tie-break, rows = threshold ==="
for sz in $SIZES; do
  printf '%-4s %-5s' "$sz" "T"; for b in $TIEBREAKS; do printf ' %7s' "$b"; done; echo
  for T in $(echo $THRESHOLDS | tr ' ' '\n' | sort -n); do
    printf '%-4s %-5s' "" "$T"
    for b in $TIEBREAKS; do f="$RES/RUN_$(arm_name "$T" "$b")_$sz.csv"
      if [ -f "$f" ]; then
        sp=$(awk -F, -v i="$(col "$f" cbsTbSpec)" 'NR==2{print $i}' "$f"); [ "$sp" = "$(tb_spec "$b")" ] || printf '!'
        printf ' %3d/%-3d' "$(awk -F, 'NR>1 && $13=="True"' "$f" | wc -l)" "$(awk 'NR>1' "$f" | wc -l)"
      else printf ' %7s' "-"; fi
    done; echo
  done
done
log "=== DONE ==="
