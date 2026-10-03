#!/usr/bin/env bash
# run_benchmark_tt2.sh
#
# Launches all 3 TT2 benchmark jobs (j30 / j60 / j90) in parallel
# inside a single tmux session so they survive SSH disconnection.
#
# Usage:
#   bash run_benchmark_tt2.sh            # start fresh run
#   tmux attach -t tt2_bench             # reattach to check progress
#   tail -f log_tt2_j30.txt              # watch one job live
#
# Results land in:  new_results/output_tt2_{size}_*.csv

SESSION="tt2_bench"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Use Driver_bench on Linux, Driver_bench.exe on Windows
if [[ -f "$SCRIPT_DIR/Driver_bench" ]]; then
    EXE="$SCRIPT_DIR/Driver_bench"
else
    EXE="$SCRIPT_DIR/Driver_bench.exe"
fi

if [[ ! -f "$EXE" ]]; then
    echo "ERROR: Driver_bench (or Driver_bench.exe) not found in $SCRIPT_DIR"
    echo "Compile with: g++ -std=c++20 -fopenmp -O3 -I../.. -I../gui/STUB Driver.cpp -o Driver_bench"
    exit 1
fi

mkdir -p "$SCRIPT_DIR/new_results"

# Kill any leftover session from a previous run
tmux kill-session -t "$SESSION" 2>/dev/null || true

# Create a new detached tmux session
tmux new-session -d -s "$SESSION" -x 220 -y 50

CMD='
set -uo pipefail
SCRIPT_DIR="'"$SCRIPT_DIR"'"
EXE="'"$EXE"'"
cd "$SCRIPT_DIR"

echo "========================================"
echo " TT2 benchmark started: $(date)"
echo " 3 jobs: j30 / j60 / j90"
echo "========================================"

PIDS=()
LABELS=()

for SIZE in j30 j60 j90; do
    LOG="$SCRIPT_DIR/log_tt2_${SIZE}.txt"
    "$EXE" "tt2_${SIZE}" > "$LOG" 2>&1 &
    PIDS+=($!)
    LABELS+=("tt2_${SIZE}")
    echo "  started tt2_${SIZE}  (pid=${PIDS[-1]})  -> $LOG"
done

echo ""
echo "All 3 jobs running. Waiting for completion..."
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
echo " TT2 benchmark done: $(date)"
echo " Failed: $FAILED / 3"
echo " Results: $SCRIPT_DIR/new_results/"
echo "========================================"
'

tmux send-keys -t "$SESSION" "$CMD" Enter

echo "tmux session '$SESSION' started."
echo ""
echo "  Reattach anytime : tmux attach -t $SESSION"
echo "  Watch j30        : tail -f $SCRIPT_DIR/log_tt2_j30.txt"
echo "  List results     : ls $SCRIPT_DIR/new_results/output_tt2_*.csv"
echo ""
echo "Safe to close this terminal — jobs run inside tmux."
