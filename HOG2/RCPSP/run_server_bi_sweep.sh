#!/bin/bash
# ─────────────────────────────────────────────────────────────────────────────
# BIDIRECTIONAL SWEEP: j30 (every arm, in parallel) -> gate -> j60 then j90 (priority slots). 300 s, all 480.
#
# ARMS (HBAE shared: MEET_NORM=1, DR5 both + DR4, batch off, NIP=0, DRAINCERT=1, ASSERT=1 fail-fast, PROF=1)
#   tt2fwd       forward TT2 (DR5+DR4+batch)                        same search as before, faster code
#   tt2bwd       backward TT2 (reversed net, same flags)            same search as before, faster code
#   bi-f         HBAE f order                                       same search as before, faster code
#   bi-b         HBAE b order (j30 only)                            same search as before, faster code
#   bi-fbatch    HBAE f order + batch
#   bi-fmax      HBAE f + stop on max(min f_F, min f_B)             local j30: -33% expansions, never more
#   bi-fmaxmeet  HBAE f + max stop + maximum meeting
#   bi-fmeet     HBAE f + maximum meeting: dominance + shift + DR5-pruned states
#   bi-fmaxalt   HBAE f + max stop + strict alternation             <= ~2x the better direction, local j30 -11%
#   bi-fall      HBAE f + max stop + alternation + maximum meeting (j30 only)
#   bi-bmax      HBAE b + max stop                                  local j30: -21% expansions, never more
#   bi-fmaxtb    bi-fmax + RCPSP_BI_ACTIVETB=1: plateau tie-break prefers MORE active transitions, as TT2's
#                comparator does in code (HBAE preferred fewer). Local: j90 3:4 37920 -> 845 expansions.
#   w-bi-guar1tb explicit-wait model with guaranteed meeting (DR4 on the reversed side, dominance/shift meets,
#                meet-only nodes forward, audit) + the plateau tie-break fix -- as w-bi-guar1 in run_server_wait_sweep.sh
#   (all HBAE arms) RCPSP_BI_HFRESH=1 + RCPSP_BI_DZFIRST=1 are now the defaults: zero-delay children get a fresh,
#                consistent h (they were scored h = 0) and are expanded first on ties -> same search, true f values.
#   tt2fwd-root  tt2fwd + RootBounds.h (RCPSP_ROOT_DLB=1 RCPSP_ROOT_SEED=1): destructive root LB (time-table,
#                energetic, shaving) + SGS/justification seed; closes at root when seed <= LB (closedAtRoot=1)
#   bi-fmaxtb-root bi-fmaxtb + RootBounds.h: HBAE starts with U = seed and stops once U <= root LB (stop=rootfloor).
#                Local: sound on j30 480 + j90 g1-12 (0 LB > C*, 0 seed < C*); j30 closes 340/480 at root, ~5 ms each.
#   tt2fwd-probe / bi-fmaxtb-probe  the -root arms + RCPSP_ROOT_PROBE=1: schedule-or-postpone DFS under deadline D = root LB
#                (then incumbent-1 downwards), restarts, 2 s budget, plus RCPSP_ROOT_SEED_ITERS=30000 (SGS + justification).
#                Local, 37 instances with root LB = C* but no C* seed: probe 10 s alone 9, 30000 seeds alone 13, both 17.
#
# STAGE 1  j30: all 11 arms at once (small memory, minutes). GATE before j60/j90:
#          every arm 480 rows, exit 0, no FALSE RESULT, no WRONG, no impossible incumbent; and the four
#          "same search" arms must reproduce the previous CSV of the same name row for row (FORCE=1 skips only this).
# STAGE 2  j60 then j90 for the three most important arms, one slot each (inside a slot: j60 -> j90):
#          S1 bi-fmax      S2 bi-fmaxmeet      S3 bi-fmeet
#          More arms later, once memory use is known, WITHOUT rerunning j30 (a new logs folder is created):
#            SKIP_STAGE1=1 SLOTS_SPEC="bi-fbatch tt2fwd|bi-bmax tt2bwd|bi-fmaxalt bi-f" bash run_server_bi_sweep.sh
#          ("|" separates slots; the arms inside one slot run one after another)
#          arm@size runs that arm on that size only, e.g. SLOTS_SPEC="bi-fmaxtb@j60|bi-fmaxtb@j90" (sizes in parallel)
#          J30_ARMS="..." limits stage 1 to those arms, e.g. J30_ARMS="tt2fwd tt2bwd bi-fmax bi-fmaxtb"
# MEMORY   no cap: nothing is ever killed for memory. An arm only waits to START while MemAvailable < MIN_FREE_GB
#          (default 45). About 1 KB per expansion was measured locally; the faster code fits more expansions in 300 s.
# SAFETY   a FALSE RESULT in any log stops every arm at once.
#
# UPLOAD   HOG2/RCPSP: Driver.cpp BAE_RCPSP.h DominanceTT2.h RCPSPState.cpp RCPSPState.h readPetri.cpp petriclasses.h
#                      Globals.h RCPSP.h SubsetSolver.h RootBounds.h run_server_bi_sweep.sh ;  repo root: known_optima.csv
# RUN      cd ~/RCPSP_With_Petri_nets/HOG2/RCPSP && bash run_server_bi_sweep.sh 2>&1 | tee bi_sweep.txt
# ANALYSIS ANALYZE_ONLY=1 RUN=<logs dir name> bash run_server_bi_sweep.sh
# ─────────────────────────────────────────────────────────────────────────────
set -u
ROOT=${ROOT:-~/RCPSP_With_Petri_nets}
RD="$ROOT/HOG2/RCPSP"
OPT=${OPT:-"$ROOT/known_optima.csv"}
NR=${NR:-"$ROOT/new_results"}
RUN=${RUN:-bi_sweep_$(date +%Y%m%d-%H%M%S)}
LR=${LR:-"$RD/logs/$RUN"}
MAN="$LR/arms.txt"                       # stage|slot|label|size|csv|log|exitfile
CAP=${CAP_S:-300}
MIN_FREE_GB=${MIN_FREE_GB:-45}
FORCE=${FORCE:-0}
SIZES_BIG=${SIZES_BIG:-"j60 j90"}
TEST_WALL=${TEST_WALL:-0}                # local smoke test only: kill each arm after this many seconds
WATCH_S=${WATCH_S:-60}
BIN_NAME=${BIN_NAME:-Driver_bisweep}
BIN="$RD/$BIN_NAME"
mkdir -p "$LR" "$NR"
export RCPSP_RESULTS_DIR="$NR"

HB="RCPSP_MEET_NORM=1 RCPSP_BI_DR5=3 RCPSP_BI_DR4=1 RCPSP_BI_BATCH=0 RCPSP_BI_NIP=0 RCPSP_BI_DRAINCERT=1 RCPSP_BI_ASSERT=1 RCPSP_TT2_PROF=1 RCPSP_TIMEOUT_S=$CAP"
TT2="RCPSP_TT2_DR5=1 RCPSP_TT2_DR4=1 RCPSP_TT2_BATCH=1 RCPSP_TT2_PROF=1 RCPSP_TIMEOUT_S=$CAP"
MEET="RCPSP_BI_DOMMEET=1 RCPSP_BI_SHIFTMEET=1 RCPSP_BI_MEET_GHOST=1"
declare -A AMODE AENV
arm(){ AMODE[$1]=$2; AENV[$1]=$3; }
arm tt2fwd      tt2     "$TT2"
arm tt2bwd      tt2rev  "$TT2"
arm bi-f        tt2hbae "$HB RCPSP_BI_PRIO=1"
arm bi-b        tt2hbae "$HB RCPSP_BI_PRIO=0"
arm bi-fbatch   tt2hbae "$HB RCPSP_BI_PRIO=1 RCPSP_BI_BATCH=1"
arm bi-fmax     tt2hbae "$HB RCPSP_BI_PRIO=1 RCPSP_BI_STOP=max"
arm bi-fmaxmeet tt2hbae "$HB RCPSP_BI_PRIO=1 RCPSP_BI_STOP=max $MEET"
arm bi-fmeet    tt2hbae "$HB RCPSP_BI_PRIO=1 $MEET"
arm bi-fmaxalt  tt2hbae "$HB RCPSP_BI_PRIO=1 RCPSP_BI_STOP=max RCPSP_BI_ALT=1"
arm bi-fall     tt2hbae "$HB RCPSP_BI_PRIO=1 RCPSP_BI_STOP=max RCPSP_BI_ALT=1 $MEET"
arm bi-bmax     tt2hbae "$HB RCPSP_BI_PRIO=0 RCPSP_BI_STOP=max"
arm bi-fmaxtb   tt2hbae "$HB RCPSP_BI_PRIO=1 RCPSP_BI_STOP=max RCPSP_BI_ACTIVETB=1"
arm w-bi-guar1tb tt2hbae "RCPSP_TT2_WAIT=1 $HB RCPSP_BI_PRIO=1 RCPSP_BI_STOP=max RCPSP_BI_AUDIT=1 RCPSP_BI_DR4SIDE=bwd $MEET RCPSP_BI_MEETONLY=fwd RCPSP_BI_ACTIVETB=1"
# RootBounds.h (default off): destructive root LB (time-table + energetic + shaving) + SGS/justification seed.
# TT2: closes at root when seed <= LB. HBAE: starts with U = seed, stops when U <= LB floor (stop=rootfloor).
RB="RCPSP_ROOT_DLB=1 RCPSP_ROOT_SEED=1"   # not ROOT: that is the repo path
arm tt2fwd-root   tt2     "$TT2 $RB"
arm bi-fmaxtb-root tt2hbae "$HB RCPSP_BI_PRIO=1 RCPSP_BI_STOP=max RCPSP_BI_ACTIVETB=1 $RB"
# + deadline probe (2 s) + 30000 seed runs: local best on the LB = C* instances (17/37 closed, ~3.4 s root time)
PR="RCPSP_ROOT_PROBE=1 RCPSP_ROOT_PROBE_MS=2000 RCPSP_ROOT_SEED_ITERS=30000"
arm tt2fwd-probe   tt2     "$TT2 $RB $PR"
arm bi-fmaxtb-probe tt2hbae "$HB RCPSP_BI_PRIO=1 RCPSP_BI_STOP=max RCPSP_BI_ACTIVETB=1 $RB $PR"
# retests on the current best: HBAE + batch (only compared before the tie-break fix); immediate selection (never run at scale)
arm bi-probe-batch  tt2hbae "$HB RCPSP_BI_PRIO=1 RCPSP_BI_STOP=max RCPSP_BI_ACTIVETB=1 $RB $PR RCPSP_BI_BATCH=1"
arm bi-probe-immsel tt2hbae "$HB RCPSP_BI_PRIO=1 RCPSP_BI_STOP=max RCPSP_BI_ACTIVETB=1 $RB $PR RCPSP_TT2_IMMSEL=1"
arm tt2fwd-probe-immsel tt2 "$TT2 $RB $PR RCPSP_TT2_IMMSEL=1"
# RCPSP_ROOT_PAIR=1: root transitive reduction + disjunctive pair reasoning (local: j30 root closures 340->343,
# LB improved on 48 j30 / 30 j60 instances by ~1.4, 0 unsound, ~3 ms per instance)
arm tt2fwd-probe-pair tt2     "$TT2 $RB $PR RCPSP_ROOT_PAIR=1"
arm bi-fmaxtb-probe-pair tt2hbae "$HB RCPSP_BI_PRIO=1 RCPSP_BI_STOP=max RCPSP_BI_ACTIVETB=1 $RB $PR RCPSP_ROOT_PAIR=1"
# RCPSP_ROOT_LNS=3: large neighbourhood search on the incumbent after the probe (CP repair on a partial-order
# neighbourhood + SGS re-insertion), 10 s, stops when the incumbent reaches the root LB. Local, the 20 unsolved
# j60/j90 instances whose root LB already equals the known optimum: probe alone closes 0, + LNS closes 15;
# spot-check j30/j60 groups 1-12: root closures 104->107 / 98->100, seed never worse, 0 seed < optimum.
LN="RCPSP_ROOT_LNS=3 RCPSP_ROOT_LNS_MS=10000"
arm tt2fwd-probe-lns tt2     "$TT2 $RB $PR $LN"
arm bi-fmaxtb-probe-lns tt2hbae "$HB RCPSP_BI_PRIO=1 RCPSP_BI_STOP=max RCPSP_BI_ACTIVETB=1 $RB $PR $LN"
# backward (reversed instance) with the same root reasoning as the forward/HBAE arms -- never run before: the
# "TT2-backward + root bounds" rows in the sheet are DERIVED (backward run + instance-level root closures).
# Worth it only on j60: backward adds 2 instances to the union there and 0 on j90 (it is 49 behind forward).
arm tt2bwd-probe-lns tt2rev  "$TT2 $RB $PR $LN"
# RCPSP_TT2_TT=1 (TT2NodeBound.h): per-node deadline refutation raises h inside the SEARCH (the CBS RCPSP_CBS_TT
# mechanism ported to TT2). Local: j30 24 hard instances 5.18M -> 3.63M expansions (-30%), j60 12 finishable
# instances 10/12 -> 11/12 solved with -21% expansions, 0 wrong. Two arms: the search gain on its own (vs
# tt2fwd/bi-fmaxtb) and on top of the root stage (vs the -probe-lns arms).
ND="RCPSP_TT2_TT=1 RCPSP_TT2_TT_STEPS=16"
arm tt2fwd-nodett        tt2     "$TT2 $ND"
arm tt2fwd-probe-lns-nodett tt2  "$TT2 $RB $PR $LN $ND"
arm bi-fmaxtb-nodett     tt2hbae "$HB RCPSP_BI_PRIO=1 RCPSP_BI_STOP=max RCPSP_BI_ACTIVETB=1 $ND"
# backward TT2 with the node bound (compare vs tt2bwd). Local pre-flight 2026-10-02, 26 J30 instances, 0 wrong:
# tt2fwd 470k -> 199k expansions, tt2bwd-nodett 175k, bi-fmaxtb 457k -> 228k.
arm tt2bwd-nodett        tt2rev  "$TT2 $ND"
J30_ARMS=${J30_ARMS:-"tt2fwd tt2bwd bi-f bi-b bi-fbatch bi-fmax bi-fmaxmeet bi-fmeet bi-fmaxalt bi-fall bi-bmax bi-fmaxtb w-bi-guar1tb"}
SAME_SEARCH="tt2fwd tt2bwd bi-f bi-b bi-fmax"
SLOTS_SPEC=${SLOTS_SPEC:-"bi-fmax|bi-fmaxmeet|bi-fmeet"}
IFS='|' read -r -a BIG_SLOTS <<< "$SLOTS_SPEC"
BIG_ARMS=$(echo "$SLOTS_SPEC" | tr '| ' '\n\n' | sed 's/@.*//' | awk 'NF && !s[$0]++' | tr '\n' ' ')
for L in $BIG_ARMS; do [ -n "${AMODE[$L]:-}" ] || { echo "unknown arm in SLOTS_SPEC: $L"; exit 1; }; done
for L in $J30_ARMS; do [ -n "${AMODE[$L]:-}" ] || { echo "unknown arm in J30_ARMS: $L"; exit 1; }; done
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
      if(hb){ ma[$(c["meetAt"])]++; if($(c["dmUpdates"])+0 > 0) dmu++; if($(c["maxStop"])+0 > 0) mx++; if($(c["peakRSSMB"])+0 > rss) rss=$(c["peakRSSMB"])+0 }
      else if(("maxMem" in c) && $(c["maxMem"])/1024 > rss) rss=$(c["maxMem"])/1024 }
    END{ printf "  %-12s rows=%3d proven=%3d WRONG=%d impUB=%d mean=%.1fs peakMem=%.1fGB | RS", L, n, p, wrong+0, imp+0, (n?t/n:0), rss/1024
         for(r in T) printf " %s:%d/%d", r, P[r]+0, T[r]
         if(hb){ printf " | meetAt"; for(m in ma) printf " %s=%d", m, ma[m]; printf " | domInc=%d maxStop=%d", dmu+0, mx+0 }
         printf "\n" }' "$OPT" "$3"; }
bad_rows(){ # size csv -> number of WRONG + impossible-UB rows
  awk -F, -v SZ="$1" 'NR==FNR{ if($2==SZ) o[$3","$4]=$5; next }
    FNR==1{ for(i=1;i<=NF;i++) c[$i]=i; hb=("proven" in c); next }
    { k=$1","$2; if(!(k in o)) next; pv = hb ? ($(c["proven"])==1) : ($(c["solved"])=="True")
      if(pv && $(c["makespan"])+0 != o[k]+0) b++; if(hb && $(c["ub"])+0 >= 0 && $(c["ub"])+0 < o[k]+0) b++ }
    END{ print b+0 }' "$OPT" "$2"; }
same_search(){ # label newCSV prevCSV ; exit 1 if any row solved by both differs in expansions or makespan
  awk -F, -v L="$1" 'FNR==1{ fi++; delete c; for(i=1;i<=NF;i++) c[$i]=i; next }
    { k=$1","$2; hb=("proven" in c); pv = hb ? ($(c["proven"])==1) : ($(c["solved"])=="True"); if(!pv) next
      if(fi==1){ NE[k]=$(c["expandNumber"]); NM[k]=$(c["makespan"]); NT[k]=$(c["time"]) } else { PE[k]=$(c["expandNumber"]); PM[k]=$(c["makespan"]); PT[k]=$(c["time"]) } }
    END{ for(k in NE) if(k in PE){ n++; if(NE[k]!=PE[k] || NM[k]!=PM[k]) d++; tn+=NT[k]; tp+=PT[k] }
         printf "  %-12s solved in both=%d  differing=%d  time previous %.0fs -> now %.0fs\n", L, n+0, d+0, tp, tn
         exit (d>0) }' "$2" "$3"; }
vs_union(){ # label variantCSV fwdCSV bwdCSV
  awk -F, -v L="$1" 'FNR==1{ fi++; delete c; for(i=1;i<=NF;i++) c[$i]=i; next }
    { k=$1","$2; hb=("proven" in c); pv = hb ? ($(c["proven"])==1) : ($(c["solved"])=="True"); rs=$(c["instanceRS"])+0
      if(fi==1){ IR[k]=rs; if(pv) V[k]=1 } else if(pv) U[k]=1 }
    END{ for(k in IR){ if((k in V) && !(k in U)){ g++; if(IR[k]<=0.5) gl++ } if(!(k in V) && (k in U)){ l++; if(IR[k]<=0.5) ll++ } }
         printf "  %-12s proves %d that neither TT2 direction solves (%d at RS<=0.5); misses %d that one of them solves (%d at RS<=0.5)\n", L, g+0, gl+0, l+0, ll+0 }' "$2" "$3" "$4"; }
csv_of(){ awk -F'|' -v S="$1" -v L="$2" -v Z="$3" '$1==S && $3==L && $4==Z {print $5}' "$MAN" 2>/dev/null | tail -1; }

analyze_size(){ # stage size labels...
  local st=$1 sz=$2; shift 2
  echo ""; echo "=== ANALYSIS $sz ($(date)) ==="
  for L in "$@"; do f=$(csv_of "$st" "$L" "$sz"); if [ -n "$f" ] && [ -f "$f" ]; then summ "$L" "$sz" "$f"; else echo "  $L: no CSV"; fi; done
  grep -h -A6 "FALSE RESULT" "$LR"/${sz}_*.log 2>/dev/null | head -14
  local F B; F=$(csv_of "$st" tt2fwd "$sz"); B=$(csv_of "$st" tt2bwd "$sz")
  if [ -n "$F" ] && [ -f "$F" ] && [ -n "$B" ] && [ -f "$B" ]; then
    echo "  -- against the union of forward and backward TT2"
    for L in "$@"; do case $L in bi-*) f=$(csv_of "$st" "$L" "$sz"); [ -n "$f" ] && [ -f "$f" ] && vs_union "$L" "$f" "$F" "$B" ;; esac; done
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

# Optional delayed start (same as run_server_cpcbs.sh): WAIT_FOR = pgrep -f pattern of processes that must be gone
# before this sweep builds and starts, e.g. WAIT_FOR=Driver_bisweep queues behind a running sweep.
WAIT_FOR=${WAIT_FOR:-}
if [ -n "$WAIT_FOR" ]; then
  echo "[$(date +%F_%H:%M:%S)] waiting for processes to finish: $WAIT_FOR"
  while pgrep -f "$WAIT_FOR" > /dev/null; do sleep 300; done
  echo "[$(date +%F_%H:%M:%S)] done waiting"
fi

# ── build + preflight ──
BEFORE_ALL=$(ls "$NR"/*.csv 2>/dev/null | sort)
cd "$RD" || { echo "cd failed"; exit 1; }
[ -f "$OPT" ] || { echo "ABORT: $OPT missing"; exit 1; }
if [ -z "${SKIP_BUILD:-}" ]; then
  echo "=== BUILD ($(date)) ==="
  g++ -std=c++20 -fopenmp -O3 -I../.. -I../gui/STUB Driver.cpp -o "$BIN_NAME" 2> "$LR/build_err.txt" \
    && echo "BUILD OK" || { echo "BUILD FAILED"; tail -40 "$LR/build_err.txt"; exit 1; }
fi
echo "=== PREFLIGHT ==="
pf(){ env $HB RCPSP_BI_PRIO=1 RCPSP_BI_STOP=max $MEET RCPSP_TIMEOUT_S=60 RCPSP_BI_ASSERT=0 ${3:-} "$BIN" tt2hbae j30 $1 $2 2>&1 | grep '^HBAE' | tail -1; }
P1=$(pf 3 9); P2=$(pf 2 2); P3=$(RCPSP_BI_ACTIVETB=1 pf 3 9); echo "  $P1"; echo "  $P2"; echo "  activetb: $P3"
if echo "$P1" | grep -q 'makespan=65 .*proven=1' && echo "$P2" | grep -q 'makespan=51 .*proven=1' && echo "$P1" | grep -q 'stopRule=max' \
   && echo "$P3" | grep -q 'makespan=65 .*proven=1'; then echo "  j30 OK"
else echo "  ABORT: preflight did not reproduce 3:9=65 / 2:2=51 with the new flags (stale files?)"; exit 1; fi
EXP=$(awk -F, '$2=="j60" && $3==4 && $4==1 {print $5}' "$OPT")
GOT=$(cd "$ROOT" && env $TT2 RCPSP_TIMEOUT_S=60 "$BIN" tt2one j60 4 1 2>&1 | grep -oE 'optimal makespan=[0-9]+|Final makespan: [0-9]+' | grep -oE '[0-9]+$' | tail -1)
echo "  j60 4:1 makespan=${GOT:-none} (known optimum ${EXP:-?})"
[ -n "$GOT" ] && [ "$GOT" = "$EXP" ] || { echo "  ABORT: j60 data path / result check failed"; exit 1; }
BEFORE_ALL=$(ls "$NR"/*.csv 2>/dev/null | sort)

# ── STAGE 1: j30, all arms at once ──
if [ -z "${SKIP_STAGE1:-}" ]; then
echo ""; echo "=== STAGE 1: j30 — $J30_ARMS ($(date)) ==="
PIDS=()
for L in $J30_ARMS; do ( run_arm 1 "j30-$L" "$L" j30 ) & PIDS+=($!); sleep 2; done
watch_slots "${PIDS[@]}"; WS=$?
analyze_size 1 j30 $J30_ARMS
[ "$WS" -eq 2 ] && { echo "STOPPED: false result on j30"; exit 2; }

echo ""; echo "=== GATE (j30) ==="
FAIL=0; IDFAIL=0
for L in $J30_ARMS; do
  f=$(csv_of 1 "$L" j30); ex="$LR/j30_${L}.exit"
  [ -n "$f" ] && [ -f "$f" ] || { echo "  $L: no CSV"; FAIL=1; continue; }
  rows=$(( $(wc -l < "$f") - 1 )); code=$(cat "$ex" 2>/dev/null || echo "?"); bad=$(bad_rows j30 "$f")
  [ "$TEST_WALL" -gt 0 ] || { [ "$rows" -eq 480 ] || { echo "  $L: rows=$rows (expected 480)"; FAIL=1; }; }
  [ "$TEST_WALL" -gt 0 ] || { [ "$code" = "0" ] || { echo "  $L: exit code $code"; FAIL=1; }; }
  [ "$bad" -eq 0 ] || { echo "  $L: $bad WRONG / impossible-incumbent rows"; FAIL=1; }
done
echo "  -- same-search arms against the previous CSV of the same name"
for L in $SAME_SEARCH; do
  f=$(csv_of 1 "$L" j30); [ -n "$f" ] && [ -f "$f" ] || continue
  base=$(basename "$f" | sed -E 's/_v[0-9]+\.csv$//')
  prev=$(printf '%s\n' "$BEFORE_ALL" | grep "/${base}_v" | sort | tail -1)
  if [ -z "$prev" ]; then echo "  $L: no previous $base CSV to compare"; continue; fi
  same_search "$L" "$f" "$prev" || IDFAIL=1
done
if [ "$FAIL" -ne 0 ]; then echo "GATE FAILED (correctness / completeness): j60 and j90 not started"; exit 1; fi
if [ "$IDFAIL" -ne 0 ] && [ "$FORCE" != "1" ]; then echo "GATE FAILED (a same-search arm changed its search): rerun with FORCE=1 to continue anyway"; exit 1; fi
echo "  GATE PASSED"
else
  echo ""; echo "=== STAGE 1 skipped (SKIP_STAGE1=1): running only $SLOTS_SPEC ==="
fi

# ── STAGE 2: j60 then j90, priority slots ──
echo ""; echo "=== STAGE 2: $SIZES_BIG ($(date)) ==="
PIDS=(); i=0
for slot in "${BIG_SLOTS[@]}"; do
  i=$((i+1))
  ( for tok in $slot; do L=${tok%@*}; szs=$SIZES_BIG; [ "$tok" != "$L" ] && szs=${tok#*@}
      for sz in $szs; do run_arm 2 "S$i" "$L" "$sz" || exit 1; done; done ) & PIDS+=($!)
  sleep 5
done
watch_slots "${PIDS[@]}"; WS=$?
for sz in $SIZES_BIG; do analyze_size 2 $sz $BIG_ARMS; done
[ "$WS" -eq 2 ] && { echo "STOPPED: false result"; exit 2; }
echo ""; echo "=== ALL DONE ($(date)) === logs: $LR"
