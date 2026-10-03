#!/bin/bash
# ─────────────────────────────────────────────────────────────────────────────
# Server run: Θ-tree (Vilím ECT) resource dual-bound ablation, j30.
# Prepared 2026-08-19. Run inside tmux on dsihead (300s/instance default timeout).
#
# The Θ bound (thetaTreeECT / ThetaTree.h) is a THIRD admissible lower bound,
# resource-contention-aware (respects each activity's earliest start AND capacity
# jointly), max()-combined with the existing bounds. It NEVER changes the makespan
# (independently admissible) — it only tightens h to (try to) cut expansions.
#   • TT2 / TTPNR : RCPSP_TT2_THETA=1  → max(CPM, energy, Θ) in RCPSP_TT2::HCost
#   • CBS         : RCPSP_CBS_THETA=1  → floors HCBS with (Θ_makespan − g), like warmstart
# Both record useThetaBound / thetaBoundBetter in their CSVs.
#
# PURPOSE = quantify the before/after tradeoff: local spot checks show a small,
# SOUND expand-count reduction (largest on low-RS / high-contention instances)
# but a NET WALL-CLOCK LOSS from the flat per-node tree-build cost. This run
# measures that at full-benchmark scale so we can decide (a) keep off, (b) keep
# on, or (c) gate behind a depth window / cheaper recompute before turning on.
# Each arm is a strict A/B: identical config, Θ off vs on, so any coverage/time
# delta is attributable to the bound alone.
# ─────────────────────────────────────────────────────────────────────────────
set -u
cd ~/RCPSP_With_Petri_nets/HOG2/RCPSP || { echo "cd failed"; exit 1; }

echo "=== BUILD ($(date)) ==="
g++ -std=c++20 -fopenmp -O3 -I../.. -I../gui/STUB Driver.cpp -o Driver_theta 2> build_server_err.txt \
  && echo "BUILD OK" || { echo "BUILD FAILED"; tail -40 build_server_err.txt; exit 1; }

# Θ-tree construction unit test (10 hand-computed cases) — gate before benchmarking.
echo "=== Θ-TREE UNIT TEST ==="
g++ -std=c++20 -O2 test_theta.cpp -o test_theta 2>/dev/null \
  && ./test_theta | tail -1 && rm -f test_theta \
  || { echo "unit test build failed"; exit 1; }

log(){ echo "[$(date +%F_%H:%M:%S)] $*"; }
RES=new_results
mkdir -p "$RES"

# Grab the newest CSV matching a glob and rename it to an explicit arm name, so
# the two A/B files are unambiguous regardless of internal filename tagging.
stash(){ # $1 = glob, $2 = destination basename
  local f; f=$(ls -t $1 2>/dev/null | head -1)
  [ -n "$f" ] && mv "$f" "$RES/$2" && echo "  -> $RES/$2"
}

# ── 1. TT2 / TTPNR ablation (full j30, DR5 sound stack) ──────────────────────
log "TT2  j30  DR5           (baseline)"
env RCPSP_TT2_DR5=1                    ./Driver_theta tt2_j30 >/dev/null 2>&1
stash "$RES/output_tt2_j30_*.csv" "ABL_tt2_j30_theta_OFF.csv"

log "TT2  j30  DR5 + THETA    (arm)"
env RCPSP_TT2_DR5=1 RCPSP_TT2_THETA=1  ./Driver_theta tt2_j30 >/dev/null 2>&1
stash "$RES/output_tt2_j30_*.csv" "ABL_tt2_j30_theta_ON.csv"

# ── 2. CBS ablation (full j30, cfg8 sound preset) ────────────────────────────
CBS_SOUND="RCPSP_DR5=1 RCPSP_HYBRID=1 RCPSP_UB=1 RCPSP_SKYLINE=1 RCPSP_DOM_RULE=dr5s"
log "CBS  j30  cfg8 sound     (baseline)"
env $CBS_SOUND                     ./Driver_theta j30 8 >/dev/null 2>&1
stash "$RES/output_j30_cfg8_*.csv" "ABL_cbs_j30_cfg8_theta_OFF.csv"

log "CBS  j30  cfg8 + THETA   (arm)"
env $CBS_SOUND RCPSP_CBS_THETA=1   ./Driver_theta j30 8 >/dev/null 2>&1
stash "$RES/output_j30_cfg8_*.csv" "ABL_cbs_j30_cfg8_theta_ON.csv"

# ── 3. CBS cfg1 ablation (weak base bound — where Θ has the MOST headroom) ────
# cfg1 has no HCBS heuristic, so Θ becomes the primary lower bound; this is the
# clearest read on the bound's pruning power (expect the biggest node cut here,
# and also the clearest per-node-cost tax). Slower arm — RS=0.2 groups will hit
# the 300s cap; that is expected and identical on both sides of the A/B.
log "CBS  j30  cfg1           (baseline)"
env RCPSP_DR5=1 RCPSP_DOM_RULE=dr5s                    ./Driver_theta j30 1 >/dev/null 2>&1
stash "$RES/output_j30_cfg1_*.csv" "ABL_cbs_j30_cfg1_theta_OFF.csv"

log "CBS  j30  cfg1 + THETA   (arm)"
env RCPSP_DR5=1 RCPSP_DOM_RULE=dr5s RCPSP_CBS_THETA=1  ./Driver_theta j30 1 >/dev/null 2>&1
stash "$RES/output_j30_cfg1_*.csv" "ABL_cbs_j30_cfg1_theta_ON.csv"

# ── CORRECTNESS GATE ─────────────────────────────────────────────────────────
# Θ is admissible, so makespans MUST be unchanged. CBS files carry their own
# `correct` column (col5); flag any solved-but-wrong row. TT2 files have no such
# column, so join solved rows against known_optima.csv (key j30<grp>_<exam>).
echo "=== CORRECTNESS GATE (must print nothing) ==="
# CBS: finished(col13)==True AND correct(col5)==False  =>  solved but WRONG makespan.
awk -F, 'NR>1 && $13=="True" && $5=="False"{print FILENAME": grp"$1" ex"$2" mk="$4" WRONG"}' \
    $RES/ABL_cbs_j30_*_theta_*.csv 2>/dev/null
# TT2: no `correct` column, so join solved(col4) rows to known_optima.csv
# (col1=instance j30<grp>_<exam>, col5=optimum) and flag any makespan(col5) mismatch.
awk -F, '
  FNR==NR { if (FNR>1) opt[$1]=$5; next }             # known_optima: $1=instance, $5=optimum
  FNR==1  { next }
  $4=="True" { k="j30"$1"_"$2; if (k in opt && $5+0 != opt[k]+0)
              print FILENAME": grp"$1" ex"$2" mk="$5" != opt="opt[k] }
' ../../known_optima.csv $RES/ABL_tt2_j30_theta_*.csv 2>/dev/null

# ── ABLATION SUMMARY: coverage + total expand + total time + Θ-fire count ─────
# Θ earns its keep iff Σexpand drops enough to beat the added Σtime. The last
# column (thetaBoundBetter) shows how often it actually improved the bound.
echo "=== ABLATION SUMMARY (finished / Σexpand / Σtime_s / ΣthetaBetter) ==="
summ(){ # $1=file  $2=solvedCol  $3=expandCol  $4=timeCol
  [ -f "$1" ] || { echo "  (missing) $1"; return; }
  awk -F, -v sc="$2" -v ec="$3" -v tc="$4" '
    NR>1 { n++; if ($sc=="True") fin++; xsum+=$ec; tsum+=$tc; th+=$NF }
    END { printf "  %-38s finished=%d/%d  Σexpand=%d  Σt=%.0fs  ΣthetaBetter=%d\n",
                 FN, fin, n, xsum, tsum, th }
  ' FN="$(basename $1)" "$1"
}
# solved/finished col: TT2 solved=4 ; CBS finished=13. expand: TT2=6 CBS=14. time=3 both.
summ "$RES/ABL_tt2_j30_theta_OFF.csv"        4 6 3
summ "$RES/ABL_tt2_j30_theta_ON.csv"         4 6 3
summ "$RES/ABL_cbs_j30_cfg8_theta_OFF.csv"  13 14 3
summ "$RES/ABL_cbs_j30_cfg8_theta_ON.csv"   13 14 3
summ "$RES/ABL_cbs_j30_cfg1_theta_OFF.csv"  13 14 3
summ "$RES/ABL_cbs_j30_cfg1_theta_ON.csv"   13 14 3

log "=== DONE ==="
echo "Arm CSVs: $RES/ABL_*.csv  (scp back for the paper's before/after table)"
# ── PHASE 2 (uncomment once j30 is understood): repeat on j90 ─────────────────
# env RCPSP_TT2_DR5=1                   ./Driver_theta tt2_j90 >/dev/null 2>&1; stash "$RES/output_tt2_j90_*.csv" "ABL_tt2_j90_theta_OFF.csv"
# env RCPSP_TT2_DR5=1 RCPSP_TT2_THETA=1 ./Driver_theta tt2_j90 >/dev/null 2>&1; stash "$RES/output_tt2_j90_*.csv" "ABL_tt2_j90_theta_ON.csv"
