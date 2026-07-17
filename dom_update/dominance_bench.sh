#!/usr/bin/env bash
# dominance_bench.sh — students' MAX + CS runs with --dominance-pop.
# Launches all 6 (size x heuristic) sweeps as SEPARATE PARALLEL processes inside
# a detached tmux session (survives SSH disconnect). 6 procs x ~45 GB = ~270 GB.
#
#   bash dominance_bench.sh          # start (self-detaches into tmux)
#   tmux attach -t dom               # watch
#   tail -f dom_j30_max.log          # one job live
# Outputs: data/dom_<size>_<heur>.csv   Logs: dom_<size>_<heur>.log
# Re-run to resume: a sweep whose CSV already exists is skipped.

set -uo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"
SESSION="dom"

EXE="$SCRIPT_DIR/build/Driver"
[[ -f "$EXE" ]] || { echo "ERROR: build/Driver not found — build first"; exit 1; }
mkdir -p data

MEM_SOFT_GB=45                 # in-process RSS soft cap (6 x 45 = 270 GB < 300)
MEM_HARD_KB=$((80*1024*1024))  # ulimit -v backstop (virtual; soft fires first)

# Build the batch command (runs inside tmux). One backgrounded process per sweep.
read -r -d '' CMD <<EOF || true
set -uo pipefail
cd "$SCRIPT_DIR"
export MALLOC_ARENA_MAX=1
echo "dominance sweep started: \$(date)"
PIDS=()
for sz in j30 j60 j90; do
  for h in max lbcs; do
    out="data/dom_\${sz}_\${h}.csv"
    if [[ -s "\$out" ]]; then echo "[skip] \$out exists"; continue; fi
    ( ulimit -v $MEM_HARD_KB
      RCPSP_MEM_LIMIT_GB=$MEM_SOFT_GB "$EXE" --method tt2 --heuristic \$h --dominance-pop \
        --problem-type \$sz --group-start 1 --group-end 48 --exam-start 1 --exam-end 10 \
        --time-limit 300 --output-folder data --output-file dom_\${sz}_\${h}.csv \
    ) > dom_\${sz}_\${h}.log 2>&1 &
    PIDS+=(\$!)
    echo "  launched \${sz} \${h}  pid=\${PIDS[-1]}"
  done
done
echo "all 6 running; waiting..."
for p in "\${PIDS[@]}"; do wait "\$p"; done
echo "dominance sweep DONE: \$(date)"
EOF

tmux kill-session -t "$SESSION" 2>/dev/null || true
tmux new-session -d -s "$SESSION" -x 220 -y 50
tmux send-keys -t "$SESSION" "$CMD" Enter

echo "Launched 6 parallel processes in detached tmux session '$SESSION'."
echo "  attach: tmux attach -t $SESSION"
echo "  logs:   tail -f dom_j30_max.log   (or any dom_<size>_<heur>.log)"
echo "  csvs:   data/dom_<size>_<heur>.csv"
