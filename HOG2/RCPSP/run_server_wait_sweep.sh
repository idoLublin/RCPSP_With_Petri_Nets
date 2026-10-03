#!/bin/bash
# ─────────────────────────────────────────────────────────────────────────────
# EXPLICIT-WAIT MODEL SWEEP (RCPSP_TT2_WAIT=1): j30 (every arm, in parallel) -> gate -> j60 then j90 (priority slots).
# 300 s cap, all 480 instances, no memory cap.
#
# MODEL  A transition fires only when every input token is ready NOW; a separate WAIT edge advances the clock one unit.
#        Every integer schedule is then a path on the forward AND on the reversed net, so at every time t both searches
#        contain the cut of a common schedule: the frontiers cannot pass each other without a common cut (true BAE*
#        meeting). The native TT2 model only has nodes at its own event times (forward: starts, reversed: ends), which is
#        how the two frontiers could cross without meeting.
#        DR4 becomes a horizon on a wait chain (waiting past min_i d_i + max(1, p_i) with nothing started is dominated);
#        DR5 exempts a state's own wait ancestors (a wait child is always dominated by its parent). A chain where
#        nothing can start jumps in one edge (RCPSP_TT2_WAITJUMP=1, default); HBAE re-creates the skipped nodes as
#        closed meet-only nodes, so every time keeps its cut.
# GUARANTEE  holds with DR5 on both sides and DR4 on ONE side (with DR4 on both, the forward side keeps left-shifted and
#        the reversed side right-shifted schedules, which need not share one). Locally DR4 on the backward side costs
#        about what DR4 on both does (j30 2:2 10.5K vs 10.5K expansions, 6:2 211K vs 254K; forward-only 375K / 638K).
#        Stop rule: max(min f_F, min f_B) (sound per side). NOT HOG2's b-bound: our h_opp is the EST bound, not the other
#        side's heuristic, and the b-bound certified 65 / 63 on j30 13:2 (optimum 62) locally.
#        Meet-only nodes are needed on ONE side only (RCPSP_BI_MEETONLY=fwd|bwd): that side holds the common schedule's
#        cut at every time, and the other side's progress point is a real node. Locally identical makespans and
#        expansions, 10-15% less time on hard j30 (13:2 9.8 s -> 8.4 s); holes then show only on the other side.
# AUDIT  RCPSP_BI_AUDIT=1 rebuilds the schedule of the best join, checks it is feasible with that makespan (an invalid
#        one is a FALSE RESULT), and counts for every time t whether each side holds its cut: auditFwdExact/BwdExact/
#        BothExact, holes inside each side's covered range, auditMissed (both held a common cut before the incumbent
#        reached C*). Locally: wait model 0 holes on the found schedule; native 14-31 holes per side.
#
# ARMS   (HBAE shared: MEET_NORM=1, DR5 both, NIP=0, DRAINCERT=1, f order, STOP=max, ASSERT=1 fail-fast, AUDIT=1, PROF=1)
#   w-tt2fwd      forward TT2, wait model (DR5+DR4+batch)
#   w-tt2bwd      backward TT2 (reversed net), wait model (DR5+DR4+batch)
#   w-bi-guar     HBAE wait: DR4 on the backward side only + dominance / shift / DR5-pruned-state meets   (guaranteed)
#   w-bi-guar1    w-bi-guar with meet-only nodes on the forward side only (faster, still guaranteed)
#   w-bi-exact    HBAE wait: DR4 on the backward side only, exact meets only                              (guaranteed cuts)
#   w-bi-dr4both  HBAE wait: DR4 on both sides, exact meets only                                          (no guarantee)
#   w-bi-nojump   w-bi-guar without the pass-through jump: must give the same proven makespans (j30 only)
#   n-bi-fmax     native HBAE (bi-fmax of the last sweep) with the audit, for the coverage comparison (j30 only)
# STAGE 1  j30: all arms at once. GATE before j60/j90: every arm 480 rows, exit 0, no FALSE RESULT, no WRONG, no
#          impossible incumbent, no "AUDIT MISSED MEET", and w-bi-nojump == w-bi-guar == w-bi-guar1 on every row both prove.
# STAGE 2  j60 then j90, one slot each (inside a slot: j60 -> j90):
#          S1 w-bi-guar1    S2 w-bi-dr4both     S3 w-tt2fwd then w-tt2bwd
#          More later WITHOUT rerunning j30:  SKIP_STAGE1=1 SLOTS_SPEC="w-bi-exact|w-bi-guar" bash run_server_wait_sweep.sh
# MEMORY   no cap. An arm only waits to START while MemAvailable < MIN_FREE_GB (default 45).
# SAFETY   a FALSE RESULT in any log stops every arm at once.
#
# UPLOAD   HOG2/RCPSP: Driver.cpp BAE_RCPSP.h DominanceTT2.h RCPSPState.cpp RCPSPState.h readPetri.cpp petriclasses.h
#                      Globals.h RCPSP.h SubsetSolver.h run_server_wait_sweep.sh ;  repo root: known_optima.csv
# RUN      cd ~/RCPSP_With_Petri_nets/HOG2/RCPSP && bash run_server_wait_sweep.sh 2>&1 | tee wait_sweep.txt
# ANALYSIS ANALYZE_ONLY=1 RUN=<logs dir name> bash run_server_wait_sweep.sh
# ─────────────────────────────────────────────────────────────────────────────
set -u
ROOT=${ROOT:-~/RCPSP_With_Petri_nets}
RD="$ROOT/HOG2/RCPSP"
OPT=${OPT:-"$ROOT/known_optima.csv"}
NR=${NR:-"$ROOT/new_results"}
RUN=${RUN:-wait_sweep_$(date +%Y%m%d-%H%M%S)}
LR=${LR:-"$RD/logs/$RUN"}
MAN="$LR/arms.txt"                       # stage|slot|label|size|csv|log|exitfile
CAP=${CAP_S:-300}
MIN_FREE_GB=${MIN_FREE_GB:-45}
SIZES_BIG=${SIZES_BIG:-"j60 j90"}
TEST_WALL=${TEST_WALL:-0}                # local smoke test only: kill each arm after this many seconds
WATCH_S=${WATCH_S:-60}
BIN_NAME=${BIN_NAME:-Driver_waitsweep}
BIN="$RD/$BIN_NAME"
mkdir -p "$LR" "$NR"
export RCPSP_RESULTS_DIR="$NR"

HB="RCPSP_TT2_WAIT=1 RCPSP_MEET_NORM=1 RCPSP_BI_DR5=3 RCPSP_BI_DR4=1 RCPSP_BI_BATCH=0 RCPSP_BI_NIP=0 RCPSP_BI_DRAINCERT=1 RCPSP_BI_PRIO=1 RCPSP_BI_STOP=max RCPSP_BI_ASSERT=1 RCPSP_BI_AUDIT=1 RCPSP_TT2_PROF=1 RCPSP_TIMEOUT_S=$CAP"
TT2="RCPSP_TT2_WAIT=1 RCPSP_TT2_DR5=1 RCPSP_TT2_DR4=1 RCPSP_TT2_BATCH=1 RCPSP_TT2_PROF=1 RCPSP_TIMEOUT_S=$CAP"
MEET="RCPSP_BI_DOMMEET=1 RCPSP_BI_SHIFTMEET=1 RCPSP_BI_MEET_GHOST=1"
NAT="RCPSP_MEET_NORM=1 RCPSP_BI_DR5=3 RCPSP_BI_DR4=1 RCPSP_BI_BATCH=0 RCPSP_BI_NIP=0 RCPSP_BI_DRAINCERT=1 RCPSP_BI_PRIO=1 RCPSP_BI_STOP=max RCPSP_BI_ASSERT=1 RCPSP_BI_AUDIT=1 RCPSP_TT2_PROF=1 RCPSP_TIMEOUT_S=$CAP"
declare -A AMODE AENV
arm(){ AMODE[$1]=$2; AENV[$1]=$3; }
arm w-tt2fwd     tt2     "$TT2"
arm w-tt2bwd     tt2rev  "$TT2"
arm w-bi-guar    tt2hbae "$HB RCPSP_BI_DR4SIDE=bwd $MEET"
arm w-bi-guar1   tt2hbae "$HB RCPSP_BI_DR4SIDE=bwd $MEET RCPSP_BI_MEETONLY=fwd"
arm w-bi-exact   tt2hbae "$HB RCPSP_BI_DR4SIDE=bwd"
arm w-bi-dr4both tt2hbae "$HB RCPSP_BI_DR4SIDE=both"
arm w-bi-nojump  tt2hbae "$HB RCPSP_BI_DR4SIDE=bwd $MEET RCPSP_TT2_WAITJUMP=0"
arm n-bi-fmax    tt2hbae "$NAT"
J30_ARMS=${J30_ARMS:-"w-tt2fwd w-tt2bwd w-bi-guar w-bi-guar1 w-bi-exact w-bi-dr4both w-bi-nojump n-bi-fmax"}
SLOTS_SPEC=${SLOTS_SPEC:-"w-bi-guar1|w-bi-dr4both|w-tt2fwd w-tt2bwd"}
IFS='|' read -r -a BIG_SLOTS <<< "$SLOTS_SPEC"
BIG_ARMS=$(echo "$SLOTS_SPEC" | tr '|' ' ')
for L in $J30_ARMS $BIG_ARMS; do [ -n "${AMODE[$L]:-}" ] || { echo "unknown arm: $L"; exit 1; }; done
prefix(){ case "${AMODE[$1]}" in tt2) echo TT2fwd ;; tt2rev) echo TT2bwd ;; *) echo HBAE ;; esac; }

# ── analysis helpers ──
summ(){ # label size csv
  awk -F, -v L="$1" -v SZ="$2" 'NR==FNR{ if($2==SZ) o[$3","$4]=$5; next }
    FNR==1{ for(i=1;i<=NF;i++) c[$i]=i; hb=("proven" in c); next }
    { n++; k=$1","$2; rs=$(c["instanceRS"]); T[rs]++
      pv = hb ? ($(c["proven"])==1) : ($(c["solved"])=="True")
      if(pv){ p++; P[rs]++; if((k in o) && $(c["makespan"])+0 != o[k]+0) wrong++ }
      if(hb && $(c["ub"])+0 >= 0 && (k in o) && $(c["ub"])+0 < o[k]+0) imp++
      t += $(c["time"])
      if(hb){ ma[$(c["meetAt"])]++; if($(c["peakRSSMB"])+0 > rss) rss=$(c["peakRSSMB"])+0 }
      else if(("maxMem" in c) && $(c["maxMem"])/1024 > rss) rss=$(c["maxMem"])/1024 }
    END{ printf "  %-13s rows=%3d proven=%3d WRONG=%d impUB=%d mean=%.1fs peakMem=%.1fGB | RS", L, n, p, wrong+0, imp+0, (n?t/n:0), rss/1024
         for(r in T) printf " %s:%d/%d", r, P[r]+0, T[r]
         if(hb){ printf " | meetAt"; for(m in ma) printf " %s=%d", m, ma[m] }
         printf "\n" }' "$OPT" "$3"; }
audit_summ(){ # label csv : the meeting audit on proven, searched rows
  awk -F, -v L="$1" 'FNR==1{ for(i=1;i<=NF;i++) c[$i]=i; next }
    { if(!("auditStatus" in c)) exit; S[$(c["auditStatus"])]++
      if($(c["auditStatus"])!="ok") next
      ok++; miss+=$(c["auditMissed"]); fh+=$(c["auditFwdHoles"]); bh+=$(c["auditBwdHoles"])
      if($(c["auditFwdHoles"])+$(c["auditBwdHoles"])==0) nohole++
      if($(c["auditBothExact"])>0) both++
      if($(c["maxGF"])+$(c["maxGB"]) >= $(c["makespan"])) { cr++; if($(c["auditBothExact"])>0) crb++ } }
    END{ if(!ok){ printf "  %-13s audit: no ok rows\n", L; exit }
         printf "  %-13s audit ok=%d MISSED=%d | holes per row fwd %.1f bwd %.1f, rows without holes %d | rows with a common exact cut %d | frontiers crossed in time %d, of them with a common exact cut %d | status:", L, ok, miss, fh/ok, bh/ok, nohole+0, both+0, cr+0, crb+0
         for(s in S) printf " %s=%d", s, S[s]; printf "\n" }' "$2"; }
bad_rows(){ # size csv -> number of WRONG + impossible-UB rows
  awk -F, -v SZ="$1" 'NR==FNR{ if($2==SZ) o[$3","$4]=$5; next }
    FNR==1{ for(i=1;i<=NF;i++) c[$i]=i; hb=("proven" in c); next }
    { k=$1","$2; if(!(k in o)) next; pv = hb ? ($(c["proven"])==1) : ($(c["solved"])=="True")
      if(pv && $(c["makespan"])+0 != o[k]+0) b++; if(hb && $(c["ub"])+0 >= 0 && $(c["ub"])+0 < o[k]+0) b++ }
    END{ print b+0 }' "$OPT" "$2"; }
same_makespan(){ # label csvA csvB ; exit 1 if a row proven by both differs in makespan
  awk -F, -v L="$1" 'FNR==1{ fi++; delete c; for(i=1;i<=NF;i++) c[$i]=i; next }
    { if($(c["proven"])!=1) next; k=$1","$2; if(fi==1){ A[k]=$(c["makespan"]); AE[k]=$(c["expandNumber"]); AT[k]=$(c["time"]) } else { B[k]=$(c["makespan"]); BE[k]=$(c["expandNumber"]); BT[k]=$(c["time"]) } }
    END{ for(k in A) if(k in B){ n++; if(A[k]!=B[k]) d++; ea+=AE[k]; eb+=BE[k]; ta+=AT[k]; tb+=BT[k] }
         printf "  %-13s proven by both=%d makespan differs=%d | expansions %d vs %d, time %.0fs vs %.0fs\n", L, n+0, d+0, ea, eb, ta, tb
         exit (d>0) }' "$2" "$3"; }
vs_union(){ # label variantCSV fwdCSV bwdCSV
  awk -F, -v L="$1" 'FNR==1{ fi++; delete c; for(i=1;i<=NF;i++) c[$i]=i; next }
    { k=$1","$2; hb=("proven" in c); pv = hb ? ($(c["proven"])==1) : ($(c["solved"])=="True"); rs=$(c["instanceRS"])+0
      if(fi==1){ IR[k]=rs; if(pv) V[k]=1 } else if(pv) U[k]=1 }
    END{ for(k in IR){ if((k in V) && !(k in U)){ g++; if(IR[k]<=0.5) gl++ } if(!(k in V) && (k in U)){ l++; if(IR[k]<=0.5) ll++ } }
         printf "  %-13s proves %d that neither TT2W direction solves (%d at RS<=0.5); misses %d that one of them solves (%d at RS<=0.5)\n", L, g+0, gl+0, l+0, ll+0 }' "$2" "$3" "$4"; }
csv_of(){ awk -F'|' -v S="$1" -v L="$2" -v Z="$3" '$1==S && $3==L && $4==Z {print $5}' "$MAN" 2>/dev/null | tail -1; }

analyze_size(){ # stage size labels...
  local st=$1 sz=$2; shift 2
  echo ""; echo "=== ANALYSIS $sz ($(date)) ==="
  for L in "$@"; do f=$(csv_of "$st" "$L" "$sz"); if [ -n "$f" ] && [ -f "$f" ]; then summ "$L" "$sz" "$f"; else echo "  $L: no CSV"; fi; done
  grep -h -A8 "FALSE RESULT" "$LR"/${sz}_*.log 2>/dev/null | head -18
  grep -h "AUDIT MISSED" "$LR"/${sz}_*.log 2>/dev/null | head -5
  echo "  -- meeting audit (HBAE arms)"
  for L in "$@"; do case ${AMODE[$L]} in tt2hbae) f=$(csv_of "$st" "$L" "$sz"); [ -n "$f" ] && [ -f "$f" ] && audit_summ "$L" "$f" ;; esac; done
  local F B; F=$(csv_of "$st" w-tt2fwd "$sz"); B=$(csv_of "$st" w-tt2bwd "$sz")
  if [ -n "$F" ] && [ -f "$F" ] && [ -n "$B" ] && [ -f "$B" ]; then
    echo "  -- against the union of forward and backward TT2W"
    for L in "$@"; do case $L in w-bi-*) f=$(csv_of "$st" "$L" "$sz"); [ -n "$f" ] && [ -f "$f" ] && vs_union "$L" "$f" "$F" "$B" ;; esac; done
  fi
}

if [ -n "${ANALYZE_ONLY:-}" ]; then
  analyze_size 1 j30 $J30_ARMS
  for sz in $SIZES_BIG; do analyze_size 2 $sz $BIG_ARMS; done
  exit 0
fi

# ── launching ──
wait_mem(){
  while :; do
    [ -f "$LR/STOP" ] && return 1
    local avail; avail=$(awk '/MemAvailable/ {printf "%d", $2/1048576}' /proc/meminfo)
    [ "${avail:-0}" -ge "$MIN_FREE_GB" ] && return 0
    echo "[$(date +%H:%M:%S)] waiting for memory: ${avail} GB available < $MIN_FREE_GB" >> "$LR/memwait.log"
    sleep 60
  done
}
run_arm(){ # stage slot label size  (blocking)
  local stage=$1 slot=$2 L=$3 sz=$4
  [ -f "$LR/STOP" ] && return 1
  wait_mem || return 1
  local dir="$ROOT"; [ "$sz" = j30 ] && dir="$RD"
  local log="$LR/${sz}_${L}.log" ex="$LR/${sz}_${L}.exit" pre; pre=$(prefix "$L")
  exec 9>"$LR/launch.lock"; flock 9                       # one launch at a time => the new CSV is unambiguous
  local before; before=$(ls "$NR"/${pre}_${sz}_*.csv 2>/dev/null | sort)
  if [ "$TEST_WALL" -gt 0 ]; then
    ( cd "$dir" && exec env ${AENV[$L]} timeout "$TEST_WALL" "$BIN" ${AMODE[$L]}_$sz > "$log" 2>&1 ) &
  else
    ( cd "$dir" && exec env ${AENV[$L]} "$BIN" ${AMODE[$L]}_$sz > "$log" 2>&1 ) &
  fi
  local pid=$! f="" t
  echo "$pid" > "$LR/${sz}_${L}.pid"
  for t in $(seq 1 600); do
    f=$(comm -13 <(printf '%s\n' "$before") <(ls "$NR"/${pre}_${sz}_*.csv 2>/dev/null | sort) | grep -v '^$' | head -1)
    [ -n "$f" ] && break
    kill -0 "$pid" 2>/dev/null || break
    sleep 0.5
  done
  echo "$stage|$slot|$L|$sz|$f|$log|$ex" >> "$MAN"
  flock -u 9
  echo "  [$(date +%H:%M:%S)] started $sz $L (slot $slot) -> $(basename "${f:-NO_CSV}")"
  wait "$pid"; echo $? > "$ex"
  echo "  [$(date +%H:%M:%S)] finished $sz $L exit=$(cat "$ex")"
}
watch_slots(){ # pids... : wait for all; on a FALSE RESULT stop every arm
  local pids=("$@")
  while :; do
    local alive=0 p
    for p in "${pids[@]}"; do kill -0 "$p" 2>/dev/null && alive=$((alive+1)); done
    if grep -l "FALSE RESULT" "$LR"/*.log >/dev/null 2>&1; then
      touch "$LR/STOP"
      echo "!!! FALSE RESULT in: $(grep -l 'FALSE RESULT' "$LR"/*.log | xargs -n1 basename | tr '\n' ' ') -- stopping every arm"
      for pf in "$LR"/*.pid; do kill "$(cat "$pf")" 2>/dev/null; done
      for p in "${pids[@]}"; do kill "$p" 2>/dev/null; done
      return 2
    fi
    [ "$alive" -eq 0 ] && return 0
    sleep "$WATCH_S"
  done
}

# ── build + preflight ──
cd "$RD" || { echo "cd failed"; exit 1; }
[ -f "$OPT" ] || { echo "ABORT: $OPT missing"; exit 1; }
if [ -z "${SKIP_BUILD:-}" ]; then
  echo "=== BUILD ($(date)) ==="
  g++ -std=c++20 -fopenmp -O3 -I../.. -I../gui/STUB Driver.cpp -o "$BIN_NAME" 2> "$LR/build_err.txt" \
    && echo "BUILD OK" || { echo "BUILD FAILED"; tail -40 "$LR/build_err.txt"; exit 1; }
fi
echo "=== PREFLIGHT ==="
pf(){ env ${AENV[w-bi-guar]} RCPSP_TIMEOUT_S=60 RCPSP_BI_ASSERT=0 "$BIN" tt2hbae j30 $1 $2 2>&1 | grep -E '^HBAE|^AUDIT' | tr '\n' ' '; }
P1=$(pf 3 9); P2=$(pf 2 2); echo "  $P1"; echo "  $P2"
if echo "$P1" | grep -q 'makespan=65 .*proven=1' && echo "$P2" | grep -q 'makespan=51 .*proven=1' \
   && echo "$P1" | grep -q 'dr4side=2 wait=1' && echo "$P1" | grep -q 'status=ok'; then echo "  j30 OK (wait model, DR4 backward only, audit ok)"
else echo "  ABORT: preflight did not reproduce 3:9=65 / 2:2=51 with the wait flags (stale files?)"; exit 1; fi
EXP=$(awk -F, '$2=="j60" && $3==4 && $4==1 {print $5}' "$OPT")
GOT=$(cd "$ROOT" && env $TT2 RCPSP_TIMEOUT_S=60 "$BIN" tt2one j60 4 1 2>&1 | grep -oE 'optimal makespan=[0-9]+|Final makespan: [0-9]+' | grep -oE '[0-9]+$' | tail -1)
echo "  j60 4:1 makespan=${GOT:-none} (known optimum ${EXP:-?})"
[ -n "$GOT" ] && [ "$GOT" = "$EXP" ] || { echo "  ABORT: j60 data path / result check failed"; exit 1; }

# ── STAGE 1: j30, all arms at once ──
if [ -z "${SKIP_STAGE1:-}" ]; then
echo ""; echo "=== STAGE 1: j30 — $J30_ARMS ($(date)) ==="
PIDS=()
for L in $J30_ARMS; do ( run_arm 1 "j30-$L" "$L" j30 ) & PIDS+=($!); sleep 2; done
watch_slots "${PIDS[@]}"; WS=$?
analyze_size 1 j30 $J30_ARMS
[ "$WS" -eq 2 ] && { echo "STOPPED: false result on j30"; exit 2; }

echo ""; echo "=== GATE (j30) ==="
FAIL=0
for L in $J30_ARMS; do
  f=$(csv_of 1 "$L" j30); ex="$LR/j30_${L}.exit"
  [ -n "$f" ] && [ -f "$f" ] || { echo "  $L: no CSV"; FAIL=1; continue; }
  rows=$(( $(wc -l < "$f") - 1 )); code=$(cat "$ex" 2>/dev/null || echo "?"); bad=$(bad_rows j30 "$f")
  [ "$TEST_WALL" -gt 0 ] || { [ "$rows" -eq 480 ] || { echo "  $L: rows=$rows (expected 480)"; FAIL=1; }; }
  [ "$TEST_WALL" -gt 0 ] || { [ "$code" = "0" ] || { echo "  $L: exit code $code"; FAIL=1; }; }
  [ "$bad" -eq 0 ] || { echo "  $L: $bad WRONG / impossible-incumbent rows"; FAIL=1; }
done
if grep -l "AUDIT MISSED" "$LR"/j30_*.log >/dev/null 2>&1; then echo "  AUDIT MISSED MEET in: $(grep -l 'AUDIT MISSED' "$LR"/j30_*.log | xargs -n1 basename | tr '\n' ' ')"; FAIL=1; fi
G=$(csv_of 1 w-bi-guar j30); N=$(csv_of 1 w-bi-nojump j30); G1=$(csv_of 1 w-bi-guar1 j30)
echo "  -- same makespan wherever both prove: the pass-through jump against no jump, one-sided meet-only nodes against both"
if [ -n "$G" ] && [ -f "$G" ] && [ -n "$N" ] && [ -f "$N" ]; then same_makespan "guar/nojump" "$G" "$N" || FAIL=1; fi
if [ -n "$G" ] && [ -f "$G" ] && [ -n "$G1" ] && [ -f "$G1" ]; then same_makespan "guar/guar1" "$G" "$G1" || FAIL=1; fi
if [ "$FAIL" -ne 0 ]; then echo "GATE FAILED: j60 and j90 not started"; exit 1; fi
echo "  GATE PASSED"
else
  echo ""; echo "=== STAGE 1 skipped (SKIP_STAGE1=1): running only $SLOTS_SPEC ==="
fi

# ── STAGE 2: j60 then j90, priority slots ──
echo ""; echo "=== STAGE 2: $SIZES_BIG ($(date)) ==="
PIDS=(); i=0
for slot in "${BIG_SLOTS[@]}"; do
  i=$((i+1))
  ( for L in $slot; do for sz in $SIZES_BIG; do run_arm 2 "S$i" "$L" "$sz" || exit 1; done; done ) & PIDS+=($!)
  sleep 5
done
watch_slots "${PIDS[@]}"; WS=$?
for sz in $SIZES_BIG; do analyze_size 2 $sz $BIG_ARMS; done
[ "$WS" -eq 2 ] && { echo "STOPPED: false result"; exit 2; }
echo ""; echo "=== ALL DONE ($(date)) === logs: $LR"
