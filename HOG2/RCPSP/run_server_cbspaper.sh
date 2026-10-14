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
#   p-bap        cfg 1 with NO flags at all: the plain baseline (earliest conflict, one child per member, f = makespan)
#   -- Bell & Park (1990) AS PUBLISHED (2026-10-04), on cfg 1 with nothing else --
#   p-bap-dr     p-bap + their two pruning rules: state dominance on the scheduled set, both directions
#                (RCPSP_DR5 bp + SETKEY + BIDIR), and the left-shift rule (RCPSP_LEFTSHIFT=2)
#   p-bap-full   p-bap-dr + everything else in the paper: minimal resource violating sets (RCPSP_BAP_MRVS), the
#                Descendants recursion over intermediate states (RCPSP_INLINE), the upper bound from schedules found
#                during the search (RCPSP_UB=1 RCPSP_UB_SEED=0), ties broken by the highest RVST (RCPSP_CBS_TB=1)
#   p-bap-x-<f>  p-bap-full without one feature: mrvs, inline, ub, tb, ls (left shift), dom (state dominance),
#                bidir, setkey (dominance needs the same RVST too), ls1 (one-unit left shift only)
#   !! p-bap-dr and p-bap-full are NOT exact: the left-shift rule and the state dominance lose the optimum when used
#      together (12-activity counter-example: 55 instead of 52; each rule alone is exact). Use for reported numbers:
#   p-bap-sound  everything in the paper EXCEPT the left-shift rule (= p-bap-x-ls)
#   p-bap-sound-x-<f>   p-bap-sound without one feature: inline, mrvs, ub, tb, bidir, setkey
#   -- the two dominance changes that carry over to our solver (each exact on its own) --
#   p-final-setkey / p-final-bidir / p-god11-setkey / p-god11-bidir : FINAL (0.3, GcD) or single GoD/1.1, plus
#                RCPSP_DOM_BP_SETKEY=1 (compare on the scheduled set alone) or RCPSP_BIDIR=1 (a new state retires the
#                stored states it dominates, which are then not expanded)
#   p-t01 p-t05 p-t11 (threshold 0.1 / 0.5 / 1.1)   p-tbref (the older tie-break GTD)
#   -- schedules chosen from the tuning grid (2026-10-04: 6 thresholds x 4 tie-breaks, offline, tuned on odd exams) --
#   p-rs-rule       10 s GcD/0.1, 30 s Gcs/0.1, rest Gcs/1.1   the pre-registered rule; predicted 452 / 366 / 357
#   p-rs-prio       10 s GcD/0.1, 30 s Gcs/0.1, rest Gcs/1.0   prioritisation in every stage; predicted 441 / 367 / 359
#   p-rs-two        60 s GoD/0.1, rest GoD/1.1                 two stages, one tie-break; predicted 450 / 367 / 354
#   p-god11 p-god10 single search, tie-break GoD, threshold 1.1 / 1.0; predicted 452 / 363 / 345 and 442 / 365 / 346
#   p-rs-120-t01    120 s GoD/0.1, rest GoD/1.1   the UNIFORM rule: one tie-break, the two extreme thresholds (prioritise
#                   everything, then nothing); predicted 450 / 367 / 355. Any first stage of 10-240 s is within 6 of it.
#   p-rs-120-t03    the same with 0.3 (the threshold the paper already states) in the first stage; predicted 450 / 367 / 354
#   -- RCPSP_CBS_FASTFIRST (2026-10-04): at a threshold above 1 do not scan what the earliest-conflict pick never uses --
#   p-god11-ff      p-god11 + FASTFIRST=1        compare with p-god11 (grid: 452 / 363 / 345)
#   p-rs-120-t01-ff p-rs-120-t01 + FASTFIRST=1   compare with p-rs-120-t01 (it only acts in the 1.1 stage)
#   p-rs-120-t03-ff p-rs-120-t03 + FASTFIRST=1   CANDIDATE FINAL ROW: 120 s prioritised at the paper's 0.3, then the
#                   earliest-conflict search with the light scan; one tie-break (GoD) throughout
#   -- the MAIN (long) stage prioritised; simulated with the full scan: --
#   p-rs-11first-t03-ff  120 s earliest-conflict (fast scan) FIRST, then the rest prioritised at 0.3   (446 / 367 / 355)
#   p-rs-120-t03-t10     no unprioritised stage at all: 120 s at 0.3, then the rest at 1.0             (439 / 368 / 355)
#   -- ANY arm + a suffix adds the two Bell & Park dominance changes (each exact on its own, cleared at a restart): --
#   <arm>-sk   + RCPSP_DOM_BP_SETKEY=1   the B&P table compares on the scheduled set alone (no same-RVST requirement)
#   <arm>-bd   + RCPSP_BIDIR=1           a new state retires the stored states it dominates; they are not expanded
#   <arm>-bp   both
#   -- ANY arm + "+modifier" (several allowed, e.g. p-final+od+t11+ff+mir), applied left to right, a later one wins: --
#   +klb keep the proven bound over restarts   +lrn learned conflict selection   +sk +bd +bp the B&P dominance changes
#   +ff fast scan at thresholds above 1        +mir solve the mirrored project    +tt64 64 propagation steps
#   +er energetic reasoning                    +mdaref refute by the MDAs' latest starts      +pmi queue children at the parent's f
#   +nmdp non-minimal delay with BOTH children (the sound relative of SetDelay)
#   +od +cd +cs +td tie-break (GoD / GcD / Gcs / GTD)          +t01 +t03 +t05 +t07 +t10 +t11 threshold
#   +tb<KEYS> any tie-break order, e.g. +tbGmD (fewest children, then deeper), +tbGxD (smallest excess), +tbGomD
#   +inl expand intermediate children in place (B&P Descendants)   +sel1 +sel2 order among prioritised conflicts
#   +dombp +domdr5s one dominance table instead of both            +tt4 +tt8 +tt32 propagation steps
#   +lrnp2 +lrnp32 learned selection with prior 2 / 32 (default 8)
#   +pf faster propagation code (RCPSP_PROP_FAST=1): the same search, less time per node     +pfchk = run both, abort on a difference
#   +frx extra "brother" children: one more child per MDA child, as few conflicts as possible without a higher f
#        (RCPSP_CBS_FREEFIX; +frx1 cheaper, +frxnp control without the contradiction test). Server run 2026-10-11:
#        ARMS="p-final p-final+frx p-rs-mir4+lrn+bp+pf+cfg6 p-rs-mir4+lrn+bp+pf+cfg6+frx" TAG=frx ./run_server_cbspaper.sh
#        (result: frx finished fewer in every set). +frxr / +frxo = the cheap gated form; run 2026-10-12:
#        ARMS="p-final p-final+frxr p-rs-mir4+lrn+bp+pf+cfg6 p-rs-mir4+lrn+bp+pf+cfg6+frxr" TAG=frxr ./run_server_cbspaper.sh
#   Run 2026-10-14 (one launch, 5 arms, ~14 h at MAX_PAR=12): the brother under a weak evaluation (A = h_CBS without
#   the deadline propagation, B = earliest conflict, no heuristic; their no-brother rows are from earlier runs), and
#   the warm-started propagation against +inc (same search: compare solved and time, TIME SPLIT for tt):
#        ORDER=mix ARMS="p-rs-mir4+lrn+bp+notp+frxr p-rs-e2+lrn+bp+cfg6+notp+ff3+frxr p-rs-mir4+lrn+bp+pf+cfg6+inc
#          p-rs-mir4+lrn+bp+pf+cfg6+inc+warm p-rs-mir4+lrn+bp+pf+cfg6+inc+warmup" TAG=oct14 BIN_NAME=Driver_cbs_oct14
#          ./run_server_cbspaper.sh
#   The two brother arms had already run (TAG=weak: A 441/361/353, B 445/342/321; the final arm 457/376/369), so the
#   replacement launch puts their no-brother controls in their place:
#        ORDER=mix ARMS="p-rs-mir4+lrn+bp+notp p-rs-e2+lrn+bp+cfg6+notp+ff3 p-rs-mir4+lrn+bp+pf+cfg6+inc
#          p-rs-mir4+lrn+bp+pf+cfg6+inc+warm p-rs-mir4+lrn+bp+pf+cfg6+inc+warmup" TAG=oct14b BIN_NAME=Driver_cbs_oct14b
#          ./run_server_cbspaper.sh
#   CXX_EXTRA="-march=native" adds compiler flags to the build (use its own BIN_NAME): same search, faster code.
#   ORDER=mix: the arms' jobs interleaved (size, part, arm) instead of one arm after the other, so arms that are
#   compared run side by side under the same server load (matters when the machine is shared and busy).
#   TAG=<name>: REQUIRED when another launch runs an arm of the same name (or ran it and its files must stay):
#   directories, logs and merged files are then named <arm>.<TAG> (RUN_<arm>.<TAG>_<size>.csv). Without it two
#   launches of one arm share a log and overwrite each other's merged file.
#   -- restarts with a MIRRORED stage (stage "<sec>/<order>/<T>/M" runs on the project with every precedence reversed) --
#   p-rs-mir2   150 s GoD/0.3, rest GoD/0.3 mirrored
#   p-rs-mir3   100 s GoD/0.3, 100 s GoD/0.3 mirrored, rest GoD/1.1 (fast scan)
#   p-rs-mir4   75 s GoD/0.3, 75 s GoD/1.1, 75 s GoD/0.3 mirrored, rest GoD/1.1 mirrored (fast scan)
#   p-rs-mir4p  the same four stages with 1.0 in place of 1.1 (prioritised in every stage)
#   p-rs-fm3    100 s GoD/0.3, 100 s GoD/1.1, rest GoD/1.1 mirrored (fast scan)
#   p-rs-fm3p   100 s GoD/0.3, 100 s GoD/1.0, rest GoD/1.0 mirrored (prioritised in every stage)
#   p-rs-mir4g  p-rs-mir4 without the plateau tie-break (order key G only: larger makespan first, nothing else)
#   p-rs-mir4b  p-rs-mir4 with the two prioritised stages first (forward 0.3, mirrored 0.3, forward 1.1, mirrored 1.1)
#   -- ablation modifiers: +notp no deadline propagation (pathmax kept)   +noprop neither   +nodr5 no cutset dominance
#      +cfg5 no MDA   +cfg6 no h_CBS   +cfg7 no conflict prioritisation (replace the configuration number of the arm)
#      +lrn2 learned selection with the counters cleared at every restart
#   START_GAP_S=<s>: seconds between two job starts (default 2). With a large value and JOB_AVAIL_GB the queue admits
#   jobs by the memory actually free, one at a time - for running beside another launch.
#   gurobi_gap.txt = the 42 instances Gurobi (default, unloaded) proves and the measured restart run does not
#   (9 / 14 / 19): use LIST=$PWD/gurobi_gap.txt PARTS=1 to screen many variants on exactly the gap.
#   SCREEN of the unprioritised search (single search, threshold 1.1; grid controls GoD 452/363/345, GTD 452/355/328):
#   ff1-od          level 1: MDAs of the earliest conflict only, h_CBS from it, conflicts counted; tie-break GoD
#   ff2-od          level 2: + no h_CBS; conflicts still counted; GoD
#   ff3-<keys>      level 3: + no counting, one light sweep to the first conflict; tie-break from what that gives:
#                   td = later first conflict, then deeper      d = deeper      n = newest first (depth-first)
#                   xd / kd / md = smaller excess / fewer activities / fewer alternatives of the first conflict, then deeper
#                   txd = later first conflict, then smaller excess, then deeper
#   LIST=<file> restricts a size to the instances listed for it (size,group,exam), split into PARTS; sizes without
#   lines in the file run all 480. Use LIST=$PWD/screen_j60_j90.txt PARTS=2 to match the tuning grid's instance sets.
#   p-rs-god3       60 s GoD/0.1, 60 s GoD/0.7, rest GoD/1.1   ONE tie-break, only the threshold changes; predicted 450 / 367 / 356
#   p-size-god      a policy PER SIZE, one tie-break (GoD), only the threshold changes; predicted 452 / 368 / 357
#                     J30: single search at 1.1    J60: 60 s 0.1, rest 1.0    J90: 120 s 0.7, rest 0.1
#   p-size-all      a policy PER SIZE from all 24 configurations (in-sample best; predicted 452 / 368 / 361)
#                     J30: single GcD/1.1    J60: 10 s Gcs/0.1, rest GoD/1.0    J90: 10 s Gcs/0.1, 60 s GoD/0.1, rest GcD/0.7
#   -- restarts that KEEP what the abandoned stages learned (2026-10-04; compare with p-restart / p-final) --
#   p-rs-klb        RESTART + RCPSP_CBS_RESTART_KEEPLB=1: the proven lower bound floors f in the next stage
#   p-rs-learn      RESTART + RCPSP_CBS_LEARN=1: conflict selection learns which free delays still raise f;
#                   the counters survive the restarts
#   p-rs-klb-learn  both
#   p-rs-geo        both, on a CP-style schedule: the SAME configuration (GcD, 0.3) restarted after 10, 20, 40
#                   and 80 s, so the stages differ only by the kept bound and the learned selection
#   p-rs-geo-reset  p-rs-geo with the counters cleared at every restart (RCPSP_CBS_LEARN=2): what carrying is worth
#   p-learn         FINAL + RCPSP_CBS_LEARN=1, no restart: what the learned selection is worth inside one search
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
g++ -std=c++20 -fopenmp -O3 ${CXX_EXTRA:-} -I../.. -I../gui/STUB Driver.cpp -o "$BIN_NAME" 2> "build_${BIN_NAME}_err.txt" \
  && echo "BUILD OK (flags: -O3 ${CXX_EXTRA:-})" || { echo "BUILD FAILED"; tail -40 "build_${BIN_NAME}_err.txt"; exit 1; }

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
GEO=${GEO:-"10/GcD/0.3,20/GcD/0.3,40/GcD/0.3,80/GcD/0.3,0/GcD/0.3"}
# Bell & Park as published (see the p-bap-* arms)
BAPF="RCPSP_DR5=1 RCPSP_DOM_RULE=bp RCPSP_DOM_BP_SETKEY=1 RCPSP_BIDIR=1 RCPSP_LEFTSHIFT=2 RCPSP_INLINE=1 RCPSP_UB=1 RCPSP_UB_SEED=0 RCPSP_CBS_TB=1 RCPSP_BAP_MRVS=1"

log(){ echo "[$(date +%F_%H:%M:%S)] $*"; }
# <cfg> | <env on top of FINAL; a later assignment wins>.  p-bap drops FINAL altogether.
# arm_def <arm> [<size>]: the size matters only for the p-size-* arms.
arm_base(){ case $1 in
  p-god11-ff|ff1-od) echo "8|RCPSP_HYBRID_T=1.1 RCPSP_CBS_TBSPEC=GoD RCPSP_CBS_FASTFIRST=1" ;;
  ff2-od)         echo "8|RCPSP_HYBRID_T=1.1 RCPSP_CBS_TBSPEC=GoD RCPSP_CBS_FASTFIRST=2" ;;
  ff3-td)         echo "8|RCPSP_HYBRID_T=1.1 RCPSP_CBS_TBSPEC=GTD RCPSP_CBS_FASTFIRST=3" ;;
  ff3-d)          echo "8|RCPSP_HYBRID_T=1.1 RCPSP_CBS_TBSPEC=GD RCPSP_CBS_FASTFIRST=3" ;;
  ff3-n)          echo "8|RCPSP_HYBRID_T=1.1 RCPSP_CBS_TBSPEC=GN RCPSP_CBS_FASTFIRST=3" ;;
  ff3-xd)         echo "8|RCPSP_HYBRID_T=1.1 RCPSP_CBS_TBSPEC=GxD RCPSP_CBS_FASTFIRST=3" ;;
  ff3-kd)         echo "8|RCPSP_HYBRID_T=1.1 RCPSP_CBS_TBSPEC=GkD RCPSP_CBS_FASTFIRST=3" ;;
  ff3-md)         echo "8|RCPSP_HYBRID_T=1.1 RCPSP_CBS_TBSPEC=GmD RCPSP_CBS_FASTFIRST=3" ;;
  ff3-txd)        echo "8|RCPSP_HYBRID_T=1.1 RCPSP_CBS_TBSPEC=GTxD RCPSP_CBS_FASTFIRST=3" ;;
  p-rs-120-t01-ff) echo "8|RCPSP_CBS_RESTART=120/GoD/0.1,0/GoD/1.1 RCPSP_CBS_FASTFIRST=1" ;;
  p-rs-120-t03-ff) echo "8|RCPSP_CBS_RESTART=120/GoD/0.3,0/GoD/1.1 RCPSP_CBS_FASTFIRST=1" ;;
  p-rs-11first-t03-ff) echo "8|RCPSP_CBS_RESTART=120/GoD/1.1,0/GoD/0.3 RCPSP_CBS_FASTFIRST=1" ;;
  p-rs-120-t03-t10) echo "8|RCPSP_CBS_RESTART=120/GoD/0.3,0/GoD/1.0" ;;
  p-rs-mir2)      echo "8|RCPSP_CBS_RESTART=150/GoD/0.3,0/GoD/0.3/M" ;;
  p-rs-mir3)      echo "8|RCPSP_CBS_RESTART=100/GoD/0.3,100/GoD/0.3/M,0/GoD/1.1 RCPSP_CBS_FASTFIRST=1" ;;
  p-rs-mir4)      echo "8|RCPSP_CBS_RESTART=75/GoD/0.3,75/GoD/1.1,75/GoD/0.3/M,0/GoD/1.1/M RCPSP_CBS_FASTFIRST=1" ;;
  # threshold sensitivity of the final portfolio (2026-10-07): p-rs-mir4 with 0.1 / 0.5 / 0.7 in place of 0.3 in the two
  # prioritised configurations; nothing else changes. Run as p-rs-mir4-t0X+lrn+bp+pf+cfg6 and compare with the final row.
  p-rs-mir4-t01)  echo "8|RCPSP_HYBRID_T=0.1 RCPSP_CBS_RESTART=75/GoD/0.1,75/GoD/1.1,75/GoD/0.1/M,0/GoD/1.1/M RCPSP_CBS_FASTFIRST=1" ;;
  p-rs-mir4-t05)  echo "8|RCPSP_HYBRID_T=0.5 RCPSP_CBS_RESTART=75/GoD/0.5,75/GoD/1.1,75/GoD/0.5/M,0/GoD/1.1/M RCPSP_CBS_FASTFIRST=1" ;;
  p-rs-mir4-t07)  echo "8|RCPSP_HYBRID_T=0.7 RCPSP_CBS_RESTART=75/GoD/0.7,75/GoD/1.1,75/GoD/0.7/M,0/GoD/1.1/M RCPSP_CBS_FASTFIRST=1" ;;
  # the final portfolio WITHOUT conflict prioritisation, the clean variant (2026-10-07): every configuration selects the
  # earliest conflict, so the portfolio is one forward and one backward configuration of 150 s each. Run as
  # p-rs-e2+bp+pf+cfg6. (+cfg4 does NOT measure this: with the prioritisation flag off the scan keeps the first conflict it
  # visits, resource by resource - not the earliest one - the threshold is unused, so configurations 2 and 4 repeat 1 and 3.)
  p-rs-e2)        echo "8|RCPSP_HYBRID_T=1.1 RCPSP_CBS_RESTART=150/GoD/1.1,0/GoD/1.1/M RCPSP_CBS_FASTFIRST=1" ;;
  # tie-breaking comparison on the final portfolio (2026-10-07): the same four configurations with another order among
  # equal-f nodes. p-rs-mir4g (larger g only) says what having a plateau order is worth; these two say what OUR order is
  # worth against the rule Bell & Park already use:
  #   p-rs-mir4gt    larger g, then the later earliest conflict (Bell & Park's rule = RCPSP_CBS_TB=1)
  #   p-rs-mir4gtd   the same, then the deeper node (= RCPSP_CBS_TB=3, the order CBS used before GoD)
  # Run as p-rs-mir4gt+lrn+bp+pf+cfg6 / p-rs-mir4gtd+lrn+bp+pf+cfg6.
  p-rs-mir4gt)    echo "8|RCPSP_CBS_RESTART=75/GT/0.3,75/GT/1.1,75/GT/0.3/M,0/GT/1.1/M RCPSP_CBS_FASTFIRST=1" ;;
  p-rs-mir4gtd)   echo "8|RCPSP_CBS_RESTART=75/GTD/0.3,75/GTD/1.1,75/GTD/0.3/M,0/GTD/1.1/M RCPSP_CBS_FASTFIRST=1" ;;
  p-rs-mir4p)     echo "8|RCPSP_CBS_RESTART=75/GoD/0.3,75/GoD/1.0,75/GoD/0.3/M,0/GoD/1.0/M" ;;
  p-rs-fm3)       echo "8|RCPSP_CBS_RESTART=100/GoD/0.3,100/GoD/1.1,0/GoD/1.1/M RCPSP_CBS_FASTFIRST=1" ;;
  p-rs-fm3p)      echo "8|RCPSP_CBS_RESTART=100/GoD/0.3,100/GoD/1.0,0/GoD/1.0/M" ;;
  p-rs-mir4g)     echo "8|RCPSP_CBS_RESTART=75/G/0.3,75/G/1.1,75/G/0.3/M,0/G/1.1/M RCPSP_CBS_FASTFIRST=1" ;;
  p-rs-mir4b)     echo "8|RCPSP_CBS_RESTART=75/GoD/0.3,75/GoD/0.3/M,75/GoD/1.1,0/GoD/1.1/M RCPSP_CBS_FASTFIRST=1" ;;
  p-rs-120-t01)   echo "8|RCPSP_CBS_RESTART=120/GoD/0.1,0/GoD/1.1" ;;
  p-rs-120-t03)   echo "8|RCPSP_CBS_RESTART=120/GoD/0.3,0/GoD/1.1" ;;
  p-rs-god3)      echo "8|RCPSP_CBS_RESTART=60/GoD/0.1,60/GoD/0.7,0/GoD/1.1" ;;
  p-size-god)     case ${2:-} in
                    j30) echo "8|RCPSP_HYBRID_T=1.1 RCPSP_CBS_TBSPEC=GoD" ;;
                    j60) echo "8|RCPSP_CBS_RESTART=60/GoD/0.1,0/GoD/1.0" ;;
                    *)   echo "8|RCPSP_CBS_RESTART=120/GoD/0.7,0/GoD/0.1" ;;
                  esac ;;
  p-size-all)     case ${2:-} in
                    j30) echo "8|RCPSP_HYBRID_T=1.1" ;;
                    j60) echo "8|RCPSP_CBS_RESTART=10/Gcs/0.1,0/GoD/1.0" ;;
                    *)   echo "8|RCPSP_CBS_RESTART=10/Gcs/0.1,60/GoD/0.1,0/GcD/0.7" ;;
                  esac ;;
  p-restart) echo "8|RCPSP_CBS_RESTART=$RESTART" ;;
  p-restart-t03)  echo "8|RCPSP_CBS_RESTART=30/GcD/0.3,30/Gcs/0.3,0/GTD/0.3" ;;
  p-restart-t11)  echo "8|RCPSP_CBS_RESTART=30/GcD/1.1,30/Gcs/1.1,0/GTD/1.1 RCPSP_HYBRID_T=1.1" ;;
  p-rs-noprop)    echo "8|RCPSP_CBS_RESTART=$RESTART RCPSP_CBS_TT=0 RCPSP_CBS_PATHMAX=0" ;;
  p-rs-noprop-pm) echo "8|RCPSP_CBS_RESTART=$RESTART RCPSP_CBS_TT=0" ;;
  p-rs-rule)      echo "8|RCPSP_CBS_RESTART=10/GcD/0.1,30/Gcs/0.1,0/Gcs/1.1" ;;
  p-rs-prio)      echo "8|RCPSP_CBS_RESTART=10/GcD/0.1,30/Gcs/0.1,0/Gcs/1.0" ;;
  p-rs-two)       echo "8|RCPSP_CBS_RESTART=60/GoD/0.1,0/GoD/1.1" ;;
  p-god11)        echo "8|RCPSP_HYBRID_T=1.1 RCPSP_CBS_TBSPEC=GoD" ;;
  p-god10)        echo "8|RCPSP_HYBRID_T=1.0 RCPSP_CBS_TBSPEC=GoD" ;;
  p-rs-klb)       echo "8|RCPSP_CBS_RESTART=$RESTART RCPSP_CBS_RESTART_KEEPLB=1" ;;
  p-rs-learn)     echo "8|RCPSP_CBS_RESTART=$RESTART RCPSP_CBS_LEARN=1" ;;
  p-rs-klb-learn) echo "8|RCPSP_CBS_RESTART=$RESTART RCPSP_CBS_RESTART_KEEPLB=1 RCPSP_CBS_LEARN=1" ;;
  p-rs-geo)       echo "8|RCPSP_CBS_RESTART=$GEO RCPSP_CBS_RESTART_KEEPLB=1 RCPSP_CBS_LEARN=1" ;;
  p-rs-geo-reset) echo "8|RCPSP_CBS_RESTART=$GEO RCPSP_CBS_RESTART_KEEPLB=1 RCPSP_CBS_LEARN=2" ;;
  p-learn)        echo "8|RCPSP_CBS_LEARN=1" ;;
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
  p-bap-dr)        echo "1|PLAIN RCPSP_DR5=1 RCPSP_DOM_RULE=bp RCPSP_DOM_BP_SETKEY=1 RCPSP_BIDIR=1 RCPSP_LEFTSHIFT=2" ;;
  p-bap-full)      echo "1|PLAIN $BAPF" ;;
  p-bap-sound)          echo "1|PLAIN $BAPF RCPSP_LEFTSHIFT=0" ;;
  p-bap-sound-x-inline) echo "1|PLAIN $BAPF RCPSP_LEFTSHIFT=0 RCPSP_INLINE=0" ;;
  p-bap-sound-x-mrvs)   echo "1|PLAIN $BAPF RCPSP_LEFTSHIFT=0 RCPSP_BAP_MRVS=0" ;;
  p-bap-sound-x-ub)     echo "1|PLAIN $BAPF RCPSP_LEFTSHIFT=0 RCPSP_UB=0" ;;
  p-bap-sound-x-tb)     echo "1|PLAIN $BAPF RCPSP_LEFTSHIFT=0 RCPSP_CBS_TB=0" ;;
  p-bap-sound-x-bidir)  echo "1|PLAIN $BAPF RCPSP_LEFTSHIFT=0 RCPSP_BIDIR=0" ;;
  p-bap-sound-x-setkey) echo "1|PLAIN $BAPF RCPSP_LEFTSHIFT=0 RCPSP_DOM_BP_SETKEY=0" ;;
  p-final-setkey)       echo "8|RCPSP_DOM_BP_SETKEY=1" ;;
  p-final-bidir)        echo "8|RCPSP_BIDIR=1" ;;
  p-god11-setkey)       echo "8|RCPSP_HYBRID_T=1.1 RCPSP_CBS_TBSPEC=GoD RCPSP_DOM_BP_SETKEY=1" ;;
  p-god11-bidir)        echo "8|RCPSP_HYBRID_T=1.1 RCPSP_CBS_TBSPEC=GoD RCPSP_BIDIR=1" ;;
  p-bap-x-mrvs)    echo "1|PLAIN $BAPF RCPSP_BAP_MRVS=0" ;;
  p-bap-x-inline)  echo "1|PLAIN $BAPF RCPSP_INLINE=0" ;;
  p-bap-x-ub)      echo "1|PLAIN $BAPF RCPSP_UB=0" ;;
  p-bap-x-tb)      echo "1|PLAIN $BAPF RCPSP_CBS_TB=0" ;;
  p-bap-x-ls)      echo "1|PLAIN $BAPF RCPSP_LEFTSHIFT=0" ;;
  p-bap-x-ls1)     echo "1|PLAIN $BAPF RCPSP_LEFTSHIFT=1" ;;
  p-bap-x-dom)     echo "1|PLAIN $BAPF RCPSP_DR5=0 RCPSP_BIDIR=0" ;;
  p-bap-x-bidir)   echo "1|PLAIN $BAPF RCPSP_BIDIR=0" ;;
  p-bap-x-setkey)  echo "1|PLAIN $BAPF RCPSP_DOM_BP_SETKEY=0" ;;
  *)         return 1 ;;
esac; }
# one "+modifier" -> the environment it adds
arm_mod(){ case $1 in
  klb) echo "RCPSP_CBS_RESTART_KEEPLB=1" ;;   lrn) echo "RCPSP_CBS_LEARN=1" ;;
  sk)  echo "RCPSP_DOM_BP_SETKEY=1" ;;        bd)  echo "RCPSP_BIDIR=1" ;;      bp) echo "RCPSP_DOM_BP_SETKEY=1 RCPSP_BIDIR=1" ;;
  ff)  echo "RCPSP_CBS_FASTFIRST=1" ;;        mir) echo "RCPSP_CBS_MIRROR=1" ;;
  ff2) echo "RCPSP_CBS_FASTFIRST=2" ;;        ff3) echo "RCPSP_CBS_FASTFIRST=3" ;;
  pf)  echo "RCPSP_PROP_FAST=1" ;;            pfchk) echo "RCPSP_PROP_FAST=2" ;;
  # RCPSP_CBS_DELAY (Globals.h; not in the paper): the delay is remembered as a relation. dctl = the plain delay
  # under the same dominance restrictions (B&P table only, no reverse direction) - the row to compare the three with.
  dprec) echo "RCPSP_CBS_DELAY=prec RCPSP_DOM_RULE=bp RCPSP_BIDIR=0" ;;
  dpair) echo "RCPSP_CBS_DELAY=pair RCPSP_DOM_RULE=bp RCPSP_BIDIR=0" ;;
  dwait) echo "RCPSP_CBS_DELAY=wait RCPSP_DOM_RULE=bp RCPSP_BIDIR=0" ;;
  dctl)  echo "RCPSP_DOM_RULE=bp RCPSP_BIDIR=0" ;;
  # dfar (2026-10-09): delay further where that is safe. A node is still just its schedule, so the dominance
  # rules stay as they are; the row to compare it with is the same arm without the modifier.
  dfar)  echo "RCPSP_CBS_DELAY=far" ;;
  # inc (2026-10-09): the propagation of the heuristic computed incrementally - same search, less time (needs pf).
  inc)   echo "RCPSP_PROP_INC=1" ;;      incchk) echo "RCPSP_PROP_INC=2" ;;
  # warm (2026-10-14): a child's deadline propagation at its parent's deadline starts from the parent's fixpoint there
  # (CpCBS.h, RCPSP_CBS_TT_WARM) and redoes only what its later start times change. Same search (needs pf; use with
  # inc). Local, 31 instances the final arm needs 2-40 s for (16 J30 / 5 J60 / 10 J90), +inc vs +inc+warm: propagation
  # -14% / -19% / -26%, total -3% / -10% / -5%, expansions identical. warmup = also the deadlines above the parent's
  # (its fixpoint there computed once and shared by its children): J90 -30% / -10% total but J30 -9%, J60 -12%.
  # warmchk = check every warm call against the cold one and abort on any difference.
  warm)  echo "RCPSP_CBS_TT_WARM=1" ;;
  warmup) echo "RCPSP_CBS_TT_WARM=1 RCPSP_CBS_TT_WARM_UP=1" ;;  warmchk) echo "RCPSP_CBS_TT_WARM=2 RCPSP_CBS_TT_WARM_UP=1" ;;
  # frx (2026-10-11): extra "brother" children (RCPSP_CBS_FREEFIX, Globals.h). For every MDA child c, one more child
  # c+ = c with as few conflicts as possible and f(c+) <= max(g(c), f(parent)) <= f(c): conflicts resolved in time
  # order by the MDA leaving the fewest overloaded resource-time units that the CP contradiction test does not
  # refute at that bound. The MDA branching itself is unchanged (still exact). Compare with the same arm without it.
  #   frx1  only the smallest zero-delta MDA is tried per conflict (cheaper)
  #   frxnp precedence test only, no contradiction test (measured to raise f: control row)
  frx)   echo "RCPSP_CBS_FREEFIX=1" ;;
  frx1)  echo "RCPSP_CBS_FREEFIX=1 RCPSP_CBS_FREEFIX_TRIES=1" ;;
  frxnp) echo "RCPSP_CBS_FREEFIX=1 RCPSP_CBS_FREEFIX_PROP=0" ;;
  # frxr / frxo (2026-10-12): the cheap form of frx. The extra child is built when a node is EXPANDED (its f known),
  # as its child, resolves only the free prefix of its conflicts (stops at the first one without a free fix), and is
  # tried only from nodes near a schedule, relative to the root: frxr = at most 20% of the root's conflicts, frxo = at
  # most 20% of its overloaded resource-time units. Local test (cfg8, 71 instances the control needs >= 1 s for,
  # 60 s): frxr J30 37/40 (control 36), J90 28/29 (27), J60 2/2, nothing lost, same time. Plain frx lost 4-23 per set.
  frxr)  echo "RCPSP_CBS_FREEFIX=1 RCPSP_CBS_FREEFIX_LAZY=1 RCPSP_CBS_FREEFIX_STOP=1 RCPSP_CBS_FREEFIX_RELCONF=0.2" ;;
  frxo)  echo "RCPSP_CBS_FREEFIX=1 RCPSP_CBS_FREEFIX_LAZY=1 RCPSP_CBS_FREEFIX_STOP=1 RCPSP_CBS_FREEFIX_RELOVER=0.2" ;;
  nolazy) echo "RCPSP_LAZY=0" ;;             # eager A*: every generated node is evaluated at once (the portfolio runs on it too)
  notp) echo "RCPSP_CBS_TT=0" ;;              noprop) echo "RCPSP_CBS_TT=0 RCPSP_CBS_PATHMAX=0" ;;
  nodr5) echo "RCPSP_DR5=0" ;;                lrn2) echo "RCPSP_CBS_LEARN=2" ;;
  domcap16) echo "RCPSP_DOM_CAP=16000000" ;;  # dominance table may store 16M states per table instead of 4M (faster code fills it sooner)
  tt64) echo "RCPSP_CBS_TT_STEPS=64" ;;       er)  echo "RCPSP_CBS_TT_ER=1" ;;  mdaref) echo "RCPSP_CBS_MDAREF=1" ;;
  pmi) echo "RCPSP_CBS_PATHMAX_INSERT=1" ;;   nmdp) echo "RCPSP_NMD=1 RCPSP_NMD_PREC=1" ;;
  inl) echo "RCPSP_INLINE=1" ;;               sel1) echo "RCPSP_CBS_SEL=1" ;;   sel2) echo "RCPSP_CBS_SEL=2" ;;
  dombp) echo "RCPSP_DOM_RULE=bp" ;;          domdr5s) echo "RCPSP_DOM_RULE=dr5s" ;;
  tt4) echo "RCPSP_CBS_TT_STEPS=4" ;;  tt8) echo "RCPSP_CBS_TT_STEPS=8" ;;  tt32) echo "RCPSP_CBS_TT_STEPS=32" ;;
  lrnp2) echo "RCPSP_CBS_LEARN=1 RCPSP_CBS_LEARN_PRIOR=2" ;;  lrnp32) echo "RCPSP_CBS_LEARN=1 RCPSP_CBS_LEARN_PRIOR=32" ;;
  tb[A-Za-z]*) echo "RCPSP_CBS_TBSPEC=${1#tb}" ;;
  od)  echo "RCPSP_CBS_TBSPEC=GoD" ;;  cd) echo "RCPSP_CBS_TBSPEC=GcD" ;;  cs) echo "RCPSP_CBS_TBSPEC=Gcs" ;;  td) echo "RCPSP_CBS_TBSPEC=GTD" ;;
  t01) echo "RCPSP_HYBRID_T=0.1" ;;  t03) echo "RCPSP_HYBRID_T=0.3" ;;  t05) echo "RCPSP_HYBRID_T=0.5" ;;
  t07) echo "RCPSP_HYBRID_T=0.7" ;;  t10) echo "RCPSP_HYBRID_T=1.0" ;;  t11) echo "RCPSP_HYBRID_T=1.1" ;;
  *)   return 1 ;;
esac; }
# arm_def <arm> [<size>]: arm_base, plus "+modifier" parts, plus the older -sk / -bd / -bp suffixes
arm_def(){ local d m e
  case $1 in
    *+*) d=$(arm_def "${1%%+*}" "${2:-}") || return 1
         for m in $(echo "${1#*+}" | tr '+' ' '); do
           case $m in cfg[1-8]) d="${m#cfg}|${d#*|}"; continue ;; esac      # +cfgN: another configuration number
           e=$(arm_mod "$m") || return 1; d="$d $e"
         done
         echo "$d"; return 0 ;;
  esac
  case $1 in
    *-bp) d=$(arm_base "${1%-bp}" "${2:-}") || return 1; echo "$d RCPSP_DOM_BP_SETKEY=1 RCPSP_BIDIR=1" ;;
    *-sk) d=$(arm_base "${1%-sk}" "${2:-}") || return 1; echo "$d RCPSP_DOM_BP_SETKEY=1" ;;
    *-bd) d=$(arm_base "${1%-bd}" "${2:-}") || return 1; echo "$d RCPSP_BIDIR=1" ;;
    *)    arm_base "$1" "${2:-}" ;;
  esac; }
for A in $ARMS; do arm_def "$A" > /dev/null || { log "unknown arm $A"; exit 1; }; done

# instance list of one part: exams dealt round-robin, rotated every 4 groups, so each part gets the same number of
# instances from every parameter class (RS 0.2 / 0.5 / 0.7 / 1.0) and the parts take about the same time
LIST=${LIST:-}
[ -z "$LIST" ] || [ -f "$LIST" ] || { echo "LIST=$LIST not found"; exit 1; }
part_list(){ # <size> <part>
  [ -n "$ONLY" ] && { echo "$ONLY"; return; }
  if [ -n "$LIST" ] && grep -q "^$1," "$LIST"; then          # this size is restricted to the listed instances
    local f="$RES/paper/lists/${1}_p${2}of${PARTS}_$(basename "$LIST" .txt).txt"
    local tmp="$f.tmp$BASHPID"
    [ -s "$f" ] || { grep "^$1," "$LIST" | tr -d '\015' | awk -v p="$2" -v P="$PARTS" '(NR - 1) % P == p - 1' > "$tmp" && mv "$tmp" "$f"; }
    echo "$f"; return
  fi
  local f="$RES/paper/lists/${1}_p${2}of${PARTS}.txt"
  local tmp="$f.tmp$BASHPID"
  [ -s "$f" ] || { awk -v sz="$1" -v p="$2" -v P="$PARTS" 'BEGIN{ for (g = 1; g <= 48; g++) for (e = 1; e <= 10; e++) if (((e - 1) + 2 * int((g - 1) / 4)) % P == p - 1) print sz "," g "," e }' > "$tmp" && mv "$tmp" "$f"; }
  echo "$f"; }

TAG=${TAG:-}
nm(){ echo "$1${TAG:+.$TAG}"; }      # name of an arm on disk
JOBS=""
# ORDER=mix: the arms' jobs interleaved (size, part, arm), so all arms of a size run side by side under the same
# server load; the default runs the arms one after the other.
if [ "${ORDER:-arms}" = mix ]; then
  for sz in $SIZES; do for p in $(seq 1 $PARTS); do for A in $ARMS; do JOBS="$JOBS $A:$sz:$p"; done; done; done
else
  for A in $ARMS; do for sz in $SIZES; do for p in $(seq 1 $PARTS); do JOBS="$JOBS $A:$sz:$p"; done; done; done
fi

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
  local n; n=$(nm "$1")
  local d="$RES/paper/$n/$2_p$3" lg="$LOGDIR/${n}_$2_p$3.log" def cfg extra base list
  def=$(arm_def "$1" "$2"); cfg=${def%%|*}; extra=${def#*|}; base=$FINAL
  case "$extra" in PLAIN*) base=""; extra=${extra#PLAIN}; extra=${extra# } ;; esac    # PLAIN [flags]: no FINAL stack underneath
  list=$(part_list "$2" "$3")
  mkdir -p "$d"
  log "  start $1 $2 part $3/$PARTS [cfg$cfg $extra]"
  env $base $extra RCPSP_RESULTS_DIR="$d" RCPSP_ONLY="$list" ${TIMEOUT_S:+RCPSP_TIMEOUT_S=$TIMEOUT_S} \
      ./HOG2/RCPSP/$BIN_NAME $2 $cfg > "$lg" 2>&1
  log "  end   $1 $2 part $3/$PARTS exit=$?"
  merge_if_complete "$n" "$2"
}

log "starting: TAG=${TAG:-none} BIN=$BIN_NAME MAX_PAR=$MAX_PAR PARTS=$PARTS ($(nproc) cores on this machine) jobs:$JOBS"
for J in $JOBS; do IFS=: read -r A sz p <<< "$J"
  while [ "$(jobs -rp | wc -l)" -ge "$MAX_PAR" ]; do wait -n; done
  while [ "$(avail)" -lt "$JOB_AVAIL_GB" ]; do log "MemAvailable $(avail) GB < $JOB_AVAIL_GB, holding $A $sz part $p"; sleep 300; done
  run_job "$A" "$sz" "$p" &
  sleep ${START_GAP_S:-2}
done
wait
log "=== all jobs done ==="

col(){ head -1 "$1" | tr ',' '\n' | grep -nx "$2" | cut -d: -f1; }

echo "=== CORRECTNESS GATE (must print nothing) ==="
for A in $ARMS; do A=$(nm "$A"); for sz in $SIZES; do f="$RES/RUN_${A}_${sz}.csv"; [ -f "$f" ] || continue
  awk -F, -v F="RUN_${A}_${sz}" 'NR>1 && $13=="True" && $5=="False"{print F": grp"$1" ex"$2" mk="$4" opt="$8}' "$f"
done; done

echo "=== EFFECTIVE FLAGS AND RESULT (rows must be 480) ==="
for A in $ARMS; do A=$(nm "$A"); for sz in $SIZES; do f="$RES/RUN_${A}_${sz}.csv"; [ -f "$f" ] || { echo "RUN_${A}_${sz}: MISSING"; continue; }
  fl=""; for c in useConflictPrioritization useHeuristic useMDASets useLazy useDR5 useDR4 useHybrid hybridT cbsTT ttSteps cbsPathmax cbsTbSpec cbsRestart cbsRestartKeepLB cbsLearn cbsFastFirst useUB ubSeed useLeftshift leftshiftGlobal useBidir useInline domRule domBpSetKey bapMrvs cbsTB useSetDelay cbsFreeFix freeFixProp freeFixTries freeFixLazy freeFixStop freeFixRelConf freeFixRelOver propFast propInc ttWarm ttWarmUp; do
    i=$(col "$f" $c); [ -n "$i" ] && fl="$fl $c=$(awk -F, -v i="$i" 'NR==2{print $i}' "$f")"; done
  fin=$(awk -F, 'NR>1 && $13=="True"' "$f" | wc -l); tot=$(awk -F, 'NR>1' "$f" | wc -l)
  st=""; i=$(col "$f" restartStage); [ -n "$i" ] && st=" solved-by-stage: $(awk -F, -v i="$i" 'NR>1 && $13=="True"{n[$i]++} END{for(k in n) printf "%s=%d ", k, n[k]}' "$f")"
  echo "RUN_${A}_${sz}: finished=$fin/$tot |$fl |$st"
done; done

# RCPSP_CBS_PROF (Globals.h): where the search time goes, summed over all instances of the merged file, as a share
# of the total search time. expand = generating children; heur = evaluating nodes (incl. hcbs and tt); extra = the
# FREEFIX extra child (incl. its gate); other = A* bookkeeping and everything not timed.
echo "=== TIME SPLIT (share of total search time; RCPSP_CBS_PROF) ==="
for A in $ARMS; do A=$(nm "$A"); for sz in $SIZES; do f="$RES/RUN_${A}_${sz}.csv"; [ -f "$f" ] || continue
  awk -F, -v N="RUN_${A}_${sz}" 'NR==1{ for (i=1;i<=NF;i++) c[$i]=i; next }
    { if (!("profExpandSec" in c)) { nocol=1; exit }
      T+=$(c["time"]); E+=$(c["profExpandSec"]); G+=$(c["profGenRegularSec"]); I+=$(c["profGenInlineSec"]); F+=$(c["profFilterSec"])
      X+=$(c["profExtraSec"]); XG+=$(c["profExtraGateSec"]); XS+=$(c["profExtraScanSec"]); XM+=$(c["profExtraMdaSec"])
      XB+=$(c["profExtraBuildSec"]); XT+=$(c["profExtraTestSec"]); XQ+=$(c["profExtraSgsSec"]); XE+=$(c["profExtraEvalSec"])
      H+=$(c["heurTimeSec"]); HC+=$(c["profHcbsSec"]); TT+=$(c["ttSec"]) }
    END{ if (nocol) { printf "%s: no profiling columns (built before RCPSP_CBS_PROF)\n", N; exit }
      if (T <= 0) { printf "%s: no time\n", N; exit }
      ho = H - XE
      printf "%s: total %.0f s | expand %.1f%% (regular %.1f, inline %.1f, filter %.1f, extra %.1f [gate %.1f scan %.1f mda %.1f build %.1f test %.1f sgs %.1f eval %.1f]) | heur %.1f%% (conflict selection %.1f, propagation %.1f) | other %.1f%%\n",
        N, T, 100*E/T, 100*G/T, 100*I/T, 100*F/T, 100*X/T, 100*XG/T, 100*XS/T, 100*XM/T, 100*XB/T, 100*XT/T, 100*XQ/T, 100*XE/T,
        100*ho/T, 100*HC/T, 100*TT/T, 100*(T-E-ho)/T }' "$f"
done; done
log "=== DONE ==="
