#!/bin/bash
# ─────────────────────────────────────────────────────────────────────────────
# Speed check (2026-10-08): why was the 8 Oct run (Driver_dly) twice as slow per unit of work as the paper run?
# Runs the PAPER configuration (every new flag off) on four J30 instances with each binary, one after the other,
# on this machine, right now. The search is deterministic, so the expansions must be identical; only the time
# can differ.
#   new binary slower than the old one here  -> the build is the cause (fix the code / compare compilers)
#   all binaries equally fast                -> the machine was slower during the 8 Oct run
# Reference, paper run of 5 Oct:  46:3  4.21 s 136371 exp | 14:2  5.82 s 51783 exp | 41:5  7.54 s 81943 exp |
#                                 14:3 24.65 s 506758 exp.   The 8 Oct control needed 8.09 / 12.14 / 16.27 / 43.10 s.
# Run on dsisco01:  cd ~/RCPSP_With_Petri_nets/HOG2/RCPSP && bash speed_check.sh
# BINS="..." picks the binaries (default: the three built on 7 and 8 Oct). Takes about 3 minutes.
# ─────────────────────────────────────────────────────────────────────────────
set -u
REPO=${REPO:-$HOME/RCPSP_With_Petri_nets}
cd "$REPO" || { echo "cd failed"; exit 1; }
OUT="$REPO/new_results/speedcheck"; rm -rf "$OUT"; mkdir -p "$OUT"
printf 'j30,46,3\nj30,14,2\nj30,41,5\nj30,14,3\n' > "$OUT/only.txt"
ENVV="RCPSP_LAZY=1 RCPSP_DR5=1 RCPSP_DOM_RULE=both RCPSP_SKYLINE=1 RCPSP_DR4=0 RCPSP_HYBRID=1 RCPSP_HYBRID_T=0.3 RCPSP_CBS_TT=1 RCPSP_CBS_TT_STEPS=16 RCPSP_CBS_PATHMAX=1 RCPSP_CBS_TB=0 RCPSP_CBS_TBSPEC=GcD RCPSP_CBS_RESTART=75/GoD/0.3,75/GoD/1.1,75/GoD/0.3/M,0/GoD/1.1/M RCPSP_CBS_FASTFIRST=1 RCPSP_CBS_LEARN=1 RCPSP_DOM_BP_SETKEY=1 RCPSP_BIDIR=1 RCPSP_PROP_FAST=1"
echo "machine now: $(nproc) cores | $(uptime | sed 's/.*load average/load average/')"
mine=$(pgrep -u "$USER" -c Driver 2>/dev/null); echo "processes using more than half a core, all users: $(ps -eo pcpu --no-headers | awk '$1 > 50' | wc -l) | my solver processes: ${mine:-0}"
echo "solver binaries here: $(cd HOG2/RCPSP && ls -1t Driver_* 2>/dev/null | grep -v '\.' | head -12 | tr '\n' ' ')"
echo
for B in ${BINS:-Driver_theta Driver_nolazy Driver_dly}; do
  [ -x "HOG2/RCPSP/$B" ] || { echo "$B: not on this machine, skipped"; continue; }
  d="$OUT/$B"; mkdir -p "$d"
  env $ENVV RCPSP_RESULTS_DIR="$d" RCPSP_ONLY="$OUT/only.txt" ./HOG2/RCPSP/$B j30 6 > "$d/log.txt" 2>&1
  f=$(ls "$d"/*.csv 2>/dev/null | head -1)
  [ -n "$f" ] || { echo "$B: no result file (see $d/log.txt)"; continue; }
  echo "$B   (built $(date -r "HOG2/RCPSP/$B" '+%F %H:%M'))"
  awk -F, 'NR==1{for(i=1;i<=NF;i++)c[$i]=i; next}
           {printf "    %2d:%-2d  time %7.2f s   expansions %8d   heuristic %6.2f s for %9d calls = %5.1f us per call\n", $c["group"], $c["exam"], $c["time"], $c["expandNumber"], $c["ttSec"], $c["ttCalls"], ($c["ttCalls"] > 0 ? 1e6 * $c["ttSec"] / $c["ttCalls"] : 0); T += $c["time"]}
           END{printf "    total %.2f s   (paper run: 42.22 s; control of 8 Oct: 79.60 s)\n", T}' "$f"
done
echo
echo "machine after: $(uptime | sed 's/.*load average/load average/')"
