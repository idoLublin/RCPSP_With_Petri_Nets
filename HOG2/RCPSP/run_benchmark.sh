#!/usr/bin/env bash
# run_benchmark.sh
#
# Launches all 24 CBS benchmark jobs (8 configs × 3 sizes) in parallel
# inside a single tmux session so they survive SSH disconnection.
#
# Usage:
#   bash run_benchmark.sh            # start fresh run
#   tmux attach -t cbs_bench         # reattach to check progress
#   tail -f log_j30_cfg1.txt         # watch one job live
#
# Config numbers:
#   1=Baseline  2=Prio  3=H  4=MDA  5=Prio+H  6=Prio+MDA  7=H+MDA  8=All

SESSION="cbs_bench"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# Use Driver_bench on Linux, Driver_bench.exe on Windows
if [[ -f "$SCRIPT_DIR/Driver_bench" ]]; then
    EXE="$SCRIPT_DIR/Driver_bench"
else
    EXE="$SCRIPT_DIR/Driver_bench.exe"
fi

if [[ ! -f "$EXE" ]]; then
    echo "ERROR: Driver_bench (or Driver_bench.exe) not found in $SCRIPT_DIR"
    echo "Compile with: g++ -std=c++20 -fopenmp -O3 -I../.. -I../gui/STUB Driver.cpp ../gui/STUB/GL/gl.cpp ../gui/STUB/GL/glut.cpp -o Driver_bench"
    exit 1
fi

mkdir -p "$SCRIPT_DIR/new_results"

# Kill any leftover session from a previous run
tmux kill-session -t "$SESSION" 2>/dev/null || true

# Create a new detached tmux session (single window is enough —
# all 24 jobs run as background processes and log to files).
tmux new-session -d -s "$SESSION" -x 220 -y 50

# Build the command string that tmux will execute:
# launches all 24 jobs, waits, prints a final summary.
CMD='
set -uo pipefail
SCRIPT_DIR="'"$SCRIPT_DIR"'"
EXE="'"$EXE"'"
cd "$SCRIPT_DIR"

echo "========================================"
echo " CBS benchmark started: $(date)"
echo " 24 jobs: 8 configs x j30/j60/j90"
echo "========================================"

PIDS=()
LABELS=()

for SIZE in j30 j60 j90; do
    for CFG in 1 2 3 4 5 6 7 8; do
        LOG="$SCRIPT_DIR/log_${SIZE}_cfg${CFG}.txt"
        "$EXE" "$SIZE" "$CFG" > "$LOG" 2>&1 &
        PIDS+=($!)
        LABELS+=("${SIZE}_cfg${CFG}")
        echo "  started ${SIZE} cfg${CFG}  (pid=${PIDS[-1]})  -> $LOG"
    done
done

echo ""
echo "All 24 jobs running. Waiting for completion..."
echo ""

FAILED=0
for i in "${!PIDS[@]}"; do
    wait "${PIDS[$i]}"
    STATUS=$?
    if [[ $STATUS -ne 0 ]]; then
        echo "  [FAIL] ${LABELS[$i]} exited with $STATUS"
        FAILED=$((FAILED+1))
    else
        echo "  [OK]   ${LABELS[$i]}"
    fi
done

echo ""
echo "========================================"
echo " CBS benchmark done: $(date)"
echo " Failed: $FAILED / 24"
echo " Results: $SCRIPT_DIR/new_results/"
echo "========================================"
'

tmux send-keys -t "$SESSION" "$CMD" Enter

echo "tmux session '$SESSION' started."
echo ""
echo "  Reattach anytime : tmux attach -t $SESSION"
echo "  Watch one job    : tail -f $SCRIPT_DIR/log_j30_cfg1.txt"
echo "  List results     : ls $SCRIPT_DIR/new_results/"
echo ""
echo "Safe to close this terminal — jobs run inside tmux."
