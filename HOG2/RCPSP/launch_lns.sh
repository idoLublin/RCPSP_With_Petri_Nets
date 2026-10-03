#!/bin/bash
# ─────────────────────────────────────────────────────────────────────────────
# One-command launch of the RCPSP_ROOT_LNS server runs, only on the instances no model has proven yet
# (unsolved_union_j60_j90.txt: sound union, j60 79 + j90 116, passed to the Driver as RCPSP_ONLY).
#   cd ~/RCPSP_With_Petri_nets/HOG2/RCPSP && bash launch_lns.sh
#
# Memory plan: >= 50 GB free even with every arm at its measured peak (server CSVs: TT2 j90 63.1 GB, HBAE j60 45.0 /
# j90 55.9 GB, CBS j90 11.0 GB; other users ~150 GB; the pair sweep still holds a TT2 and an HBAE arm):
#   now    HBAE-LNS j60 + CBS-LNS            150.4 + 63.1 + 55.9 + 45.0 + 11.0 = 325.4 -> 50.6 GB free
#   queued HBAE-LNS j90 + TT2-LNS j60->j90   waits until no Driver_bisweep process is left (the pair sweep)
# Each arm also waits to START until MemAvailable >= 50 GB + its own peak.
# Logs: bi_lns_j60.txt  cpcbs_lns.txt  bi_lns_q.txt
# ─────────────────────────────────────────────────────────────────────────────
cd ~/RCPSP_With_Petri_nets/HOG2/RCPSP || exit 1
ONLY="$PWD/unsolved_union_j60_j90.txt"
[ -s "$ONLY" ] || { echo "missing $ONLY"; exit 1; }
export RCPSP_ONLY="$ONLY"

MIN_FREE_GB=95 BIN_NAME=Driver_lns SKIP_STAGE1=1 SLOTS_SPEC="bi-fmaxtb-probe-lns@j60" \
  nohup bash run_server_bi_sweep.sh > bi_lns_j60.txt 2>&1 &
echo "HBAE-LNS j60         pid $!  -> bi_lns_j60.txt"

MIN_AVAIL_GB=61 ARMS="cbs-roottt-lns" PASSES=0 BIN_NAME=Driver_cpcbs_lns \
  nohup bash run_server_cpcbs.sh > cpcbs_lns.txt 2>&1 &
echo "CBS-LNS j30-j60-j90  pid $!  -> cpcbs_lns.txt"

WAIT_FOR=Driver_bisweep MIN_FREE_GB=114 BIN_NAME=Driver_lns_q SKIP_STAGE1=1 SLOTS_SPEC="bi-fmaxtb-probe-lns@j90|tt2fwd-probe-lns" \
  nohup bash run_server_bi_sweep.sh > bi_lns_q.txt 2>&1 &
echo "queued HBAE-LNS j90 + TT2-LNS  pid $!  -> bi_lns_q.txt (starts after the pair sweep)"
