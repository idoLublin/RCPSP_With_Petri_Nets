#!/bin/bash
# ─────────────────────────────────────────────────────────────────────────────
# Server run: NEW heuristic bounds, j30 + j60 + j90. 9 PARALLEL single-threaded arms
# (3 feature configs × 3 sizes), one core each. Prepared 2026-08-19. Run in tmux.
#
# The solver has NO threads/OpenMP, so each arm is single-core => per-instance timings
# stay the paper's single-core metric; only wall-clock shrinks (arms run side-by-side).
#
# Feature configs (on top of the ACTUAL best base; base arms dropped — compare vs the
# existing DR5+batch / cfg8+lazy baselines). All bounds ADMISSIBLE => makespans must be
# unchanged; the run measures COVERAGE + Σexpand/Σtime + feature telemetry:
#   tt2_theta  = TT2  DR5 + batch          + Θ-tree ECT bound     (RCPSP_TT2_THETA)
#   cbs_theta  = CBS  cfg8 sound + lazy     + Θ resource floor     (RCPSP_CBS_THETA)
#   cbs_subset = CBS  cfg8 sound + lazy     + conflict-subset LB   (RCPSP_CBS_SUBSET, EXPAND=50)
# × sizes {j30, j60, j90}.  Default 300s/instance.
#
# ISOLATION: each arm runs in .par/<arm>/ with its own new_results/ + a symlink to the
# right data dir (j30 under HOG2/RCPSP/, j60/j90 at the REPO ROOT), so concurrent
# auto-named CSVs never collide. Collected into new_results/NH_<arm>.csv.
# hSEQ excluded (off-tree prototype); batch already benchmarked so it's the TT2 base.
# NOTE: needs >=9 cores. j90 CBS arms (esp. cbs_subset_j90) are the long pole — many
# RS=0.2 instances burn the full 300s; expect a day-scale wall-clock on those.
# ─────────────────────────────────────────────────────────────────────────────
set -u
REPOROOT=~/RCPSP_With_Petri_nets
ROOT="$REPOROOT/HOG2/RCPSP"
cd "$ROOT" || { echo "cd failed"; exit 1; }
RES="$ROOT/new_results"; mkdir -p "$RES"
PAR="$ROOT/.par"; rm -rf "$PAR"; mkdir -p "$PAR"

echo "=== BUILD ($(date)) ==="
g++ -std=c++20 -fopenmp -O3 -I../.. -I../gui/STUB Driver.cpp -o Driver_newheur 2> build_server_err.txt \
  && echo "BUILD OK" || { echo "BUILD FAILED"; tail -40 build_server_err.txt; exit 1; }

echo "=== UNIT TESTS (Θ-tree + subset solver) ==="
g++ -std=c++20 -O2 test_theta.cpp  -o t_theta  2>/dev/null && ./t_theta  | tail -1 && rm -f t_theta
g++ -std=c++20 -O2 test_subset.cpp -o t_subset 2>/dev/null && ./t_subset | tail -1 && rm -f t_subset

log(){ echo "[$(date +%F_%H:%M:%S)] $*"; }
CBS_SOUND="RCPSP_DR5=1 RCPSP_HYBRID=1 RCPSP_UB=1 RCPSP_SKYLINE=1 RCPSP_DOM_RULE=dr5s RCPSP_LAZY=1"

datadir(){ echo "$ROOT/json_outputs_$1"; }   # on the server ALL sizes live under HOG2/RCPSP/

# run_arm <name> <size> <mode-words> <env-flags> : isolated single-threaded arm.
run_arm(){
  local name="$1" size="$2" mode="$3" flags="$4"
  local d="$PAR/$name"
  mkdir -p "$d/new_results"
  ln -sfn "$(datadir "$size")" "$d/json_outputs_$size"
  ( cd "$d" && env $flags "$ROOT/Driver_newheur" $mode ) > "$d/run.log" 2>&1
  local f; f=$(ls -t "$d"/new_results/*.csv 2>/dev/null | head -1)
  [ -n "$f" ] && cp "$f" "$RES/NH_${name}.csv" && log "collected $name -> NH_${name}.csv"
}

# Pre-flight: fail loudly if a size's data dir is missing (silent mk=0 rows otherwise).
for SZ in j30 j60 j90; do
  d="$(datadir "$SZ")"
  [ -d "$d" ] && [ -n "$(ls -A "$d" 2>/dev/null)" ] \
    || { echo "FATAL: data for $SZ not found at $d — fix datadir() before running"; exit 1; }
done
echo "data dirs OK for j30/j60/j90"

log "launching 9 single-threaded arms in parallel (3 configs × j30/j60/j90)"
for SZ in j30 j60 j90; do
  run_arm "tt2_theta_$SZ"  "$SZ" "tt2_$SZ" "RCPSP_TT2_DR5=1 RCPSP_TT2_BATCH=1 RCPSP_TT2_THETA=1" &
  run_arm "cbs_theta_$SZ"  "$SZ" "$SZ 8"   "$CBS_SOUND RCPSP_CBS_THETA=1" &
  run_arm "cbs_subset_$SZ" "$SZ" "$SZ 8"   "$CBS_SOUND RCPSP_CBS_SUBSET=1 RCPSP_CBS_SUBSET_EXPAND=50" &
done
wait
log "all arms finished"

# ── CORRECTNESS GATE (admissible => makespans MUST be unchanged) ──────────────
echo "=== CORRECTNESS GATE (must print nothing) ==="
# CBS: finished(13)==True AND correct(5)==False => solved but WRONG.
awk -F, 'NR>1 && $13=="True" && $5=="False"{print FILENAME": grp"$1" ex"$2" mk="$4" WRONG"}' \
    "$RES"/NH_cbs_*.csv 2>/dev/null
# TT2: join solved(4) rows to known_optima.csv. Key = problemType(10)+group(1)+_+exam(2),
# matching known_optima col1 (e.g. j301_1 / j601_1 / j901_1) — size-agnostic.
awk -F, '
  FNR==NR { if (FNR>1) opt[$1]=$5; next }
  FNR==1  { next }
  $4=="True" { k=$10$1"_"$2; if (k in opt && $5+0 != opt[k]+0)
              print FILENAME": "$10" grp"$1" ex"$2" mk="$5" != opt="opt[k] }
' "$REPOROOT/known_optima.csv" "$RES"/NH_tt2_*.csv 2>/dev/null

# ── SUMMARY (finished / Σexpand / Σtime / feature telemetry) ──────────────────
echo "=== SUMMARY ==="
tt2summ(){ [ -f "$1" ] || { echo "  (missing) $(basename $1)"; return; }
  awk -F, 'NR>1{n++;if($4=="True")f++;x+=$6;t+=$3;th+=$NF}
    END{printf "  %-22s finished=%d/%d  Σexp=%d  Σt=%.0fs  ΣΘBetter=%d\n",FN,f,n,x,t,th}' \
    FN="$(basename $1)" "$1"; }
cbssumm(){ [ -f "$1" ] || { echo "  (missing) $(basename $1)"; return; }
  awk -F, 'NR>1{n++;if($13=="True")f++;x+=$14;t+=$3;thB+=$(NF-7);sB+=$(NF-5);sS+=$(NF-4);sCH+=$NF}
    END{printf "  %-22s finished=%d/%d  Σexp=%d  Σt=%.0fs  | ΘBetter=%d subBetter=%d subSolves=%d subCacheHits=%d\n",
               FN,f,n,x,t,thB,sB,sS,sCH}' FN="$(basename $1)" "$1"; }
for SZ in j30 j60 j90; do
  tt2summ "$RES/NH_tt2_theta_$SZ.csv"
  cbssumm "$RES/NH_cbs_theta_$SZ.csv"
  cbssumm "$RES/NH_cbs_subset_$SZ.csv"
done

log "=== DONE ==="
echo "Arm CSVs: $RES/NH_*.csv   (per-arm logs: $PAR/<arm>/run.log)"
