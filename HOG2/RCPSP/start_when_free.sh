#!/bin/bash
# ─────────────────────────────────────────────────────────────────────────────
# Wait until dsisco01 has room, then start the run with the usual launch line (2026-10-09).
# Room = the load of everybody else plus our MAX_PAR jobs fits on the PHYSICAL cores (48 of the 96 shown):
#   1-minute and 5-minute load average both <= PHYS - MAX_PAR, with none of our solver processes running,
#   on NEED consecutive checks EVERY seconds apart (so a short dip does not start it).
# Above that, two jobs share one core and every job runs at about half speed: a 300 s limit then means something
# else and the result cannot be compared with the paper run.
# Run on dsisco01:
#   cd ~/RCPSP_With_Petri_nets/HOG2/RCPSP && nohup bash start_when_free.sh > far3_wait_log.txt 2>&1 &
# Watch:  tail -3 far3_wait_log.txt        Give up waiting:  pkill -u $USER -f start_when_free.sh
# DRY=1 prints the launch line instead of running it.
# ─────────────────────────────────────────────────────────────────────────────
set -u
cd "$(dirname "$0")" || exit 1
MAX_PAR=${MAX_PAR:-12}
PHYS=${PHYS:-48}
NEED=${NEED:-3}
EVERY=${EVERY:-300}
TAG=${TAG:-far3}
LIMIT=$((PHYS - MAX_PAR))
ok=0
while :; do
  read -r l1 l5 _ < /proc/loadavg
  mine=$(pgrep -u "$USER" -c Driver || true)
  if [ "${mine:-0}" -eq 0 ] && awk -v a="$l1" -v b="$l5" -v L="$LIMIT" 'BEGIN{exit !(a <= L && b <= L)}'; then ok=$((ok + 1)); else ok=0; fi
  echo "$(date '+%F %T') load $l1 (1 min) $l5 (5 min), limit $LIMIT, my solver processes ${mine:-0}, passed $ok/$NEED"
  [ "$ok" -ge "$NEED" ] && break
  sleep "$EVERY"
done
echo "$(date '+%F %T') room found: starting TAG=$TAG"
if [ -n "${DRY:-}" ]; then echo "DRY: would launch run_server_cbspaper_far.sh with TAG=$TAG MAX_PAR=$MAX_PAR"; exit 0; fi
TAG=$TAG BIN_NAME=Driver_far2 MAX_PAR=$MAX_PAR PARTS=4 JOB_AVAIL_GB=80 SIZES="j30 j60 j90" ARMS="p-rs-mir4+lrn+bp+pf+cfg6+dfar" nohup bash run_server_cbspaper_far.sh > "${TAG}_log.txt" 2>&1 &
echo "$(date '+%F %T') launched (pid $!), log ${TAG}_log.txt"
