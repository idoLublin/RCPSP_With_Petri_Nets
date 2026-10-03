#!/usr/bin/env bash
# run_benchmark_j90_resume.sh
#
# Resumes the two crashed j90 benchmark runs in parallel:
#   cfg6 (Prio+MDA)  — resume from (14,4)
#   cfg7 (H+MDA)     — resume from (13,6)
#
# Usage:
#   TMUX= bash run_benchmark_j90_resume.sh   # start
#   tmux attach -t j90_resume                # reattach
#   tail -f log_j90_resume_cfg6.txt          # watch cfg6
#   tail -f log_j90_resume_cfg7.txt          # watch cfg7

SESSION="j90_resume"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

EXE="$SCRIPT_DIR/Driver_bench"
if [[ ! -f "$EXE" ]]; then
    echo "ERROR: Driver_bench not found in $SCRIPT_DIR"; exit 1
fi

mkdir -p "$SCRIPT_DIR/new_results"

tmux kill-session -t "$SESSION" 2>/dev/null || true
tmux new-session -d -s "$SESSION" -x 220 -y 50

CMD='
set -uo pipefail
SCRIPT_DIR="'"$SCRIPT_DIR"'"
EXE="'"$EXE"'"
cd "$SCRIPT_DIR"

echo "========================================"
echo " j90 resume started: $(date)"
echo " cfg6 from (14,4) | cfg7 from (13,6)"
echo "========================================"

PIDS=()
"$EXE" resume j90 6 14 4 > log_j90_resume_cfg6.txt 2>&1 & PIDS+=($!)
"$EXE" resume j90 7 13 6 > log_j90_resume_cfg7.txt 2>&1 & PIDS+=($!)
echo "  cfg6 pid=${PIDS[0]} -> log_j90_resume_cfg6.txt"
echo "  cfg7 pid=${PIDS[1]} -> log_j90_resume_cfg7.txt"
echo ""

wait "${PIDS[0]}"; echo "  [done] cfg6"
wait "${PIDS[1]}"; echo "  [done] cfg7"

echo ""
echo "========================================"
echo " j90 resume done: $(date)"
echo " Results: $SCRIPT_DIR/new_results/"
echo "========================================"
'

tmux send-keys -t "$SESSION" "$CMD" Enter

echo "tmux session '$SESSION' started."
echo "  Reattach : tmux attach -t $SESSION"
echo "  Watch    : tail -f $SCRIPT_DIR/log_j90_resume_cfg6.txt"
