#!/usr/bin/env bash
# Run a TT (TTPN) benchmark sweep under the same protocol as the final TT2
# runs: 300 s per-instance timeout, single core, one solver process per
# parameter group (fresh-process memory hygiene), solver-native CSV schema so
# the results merge straight into analysis/build_master.py via --extra-dir.
#
# Usage:
#   analysis/run_tt_sweep.sh <j30|j60|j90> <heuristic...> [options]
#     heuristics: cp lbcs lbcc lbip0 lbmax
#   options:
#     --time-limit N     seconds per instance (default 300)
#     --output-folder D  results folder (default data/tt_final_results)
#     --binary PATH      solver binary (default build/Driver)
#     --dry-run          print the commands without running anything
#
# Examples:
#   analysis/run_tt_sweep.sh j30 cp lbcs lbcc lbip0 lbmax
#   analysis/run_tt_sweep.sh j60 cp lbcs --output-folder data/tt_final_results
#
# Intended to run inside tmux with a log:
#   tmux new -s tt_sweep
#   analysis/run_tt_sweep.sh j30 cp lbcs 2>&1 | tee logs/tt_j30_$(date +%F).log

set -euo pipefail

cd "$(dirname "$0")/.."   # repo root: solver resolves json_outputs_* from here

SET=""
HEURISTICS=()
TIME_LIMIT=300
OUTPUT_FOLDER="data/tt_final_results"
BINARY="build/Driver"
DRY_RUN=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        j30|j60|j90) SET="$1" ;;
        cp|lbcs|lbcc|lbip0|lbmax) HEURISTICS+=("$1") ;;
        --time-limit) TIME_LIMIT="$2"; shift ;;
        --output-folder) OUTPUT_FOLDER="$2"; shift ;;
        --binary) BINARY="$2"; shift ;;
        --dry-run) DRY_RUN=1 ;;
        *) echo "unknown argument: $1" >&2; exit 1 ;;
    esac
    shift
done

[[ -n "$SET" ]] || { echo "missing set (j30|j60|j90)" >&2; exit 1; }
[[ ${#HEURISTICS[@]} -gt 0 ]] || { echo "missing heuristics" >&2; exit 1; }
if [[ $DRY_RUN -eq 0 && ! -x "$BINARY" ]]; then
    echo "solver binary $BINARY not found; build it first:" >&2
    echo "  clang++ -std=c++17 -O3 -march=native -DNDEBUG -flto HOG2/RCPSP/Driver.cpp -o build/Driver" >&2
    exit 1
fi

mkdir -p "$OUTPUT_FOLDER"

for HEUR in "${HEURISTICS[@]}"; do
    TAG="tt_final_${HEUR}"
    # one output file per heuristic; each group appends via --no-header after
    # the first (the solver truncates its output file per invocation, so every
    # group gets its own file and we concatenate at the end)
    SWEEP_DIR="$OUTPUT_FOLDER/${SET}_${HEUR}_parts"
    mkdir -p "$SWEEP_DIR"
    echo "=== TT sweep: set=$SET heuristic=$HEUR time-limit=${TIME_LIMIT}s ==="
    for GROUP in $(seq 1 48); do
        # solver writes to <output-folder>/<output-file>; pass basename only
        CMD=("$BINARY"
             --method tt --heuristic "$HEUR" --problem-type "$SET"
             --group-start "$GROUP" --group-end "$GROUP"
             --exam-start 1 --exam-end 10
             --time-limit "$TIME_LIMIT"
             --output-folder "$SWEEP_DIR" --output-file "g${GROUP}.csv"
             --tag "$TAG")
        if [[ $DRY_RUN -eq 1 ]]; then
            echo "${CMD[*]}"
        else
            echo "[$(date +%T)] $SET $HEUR group $GROUP/48"
            "${CMD[@]}"
        fi
    done
    MERGED="$OUTPUT_FOLDER/$(date +%F)_${SET}_g1-48_e1-10_tt_${TAG}.csv"
    if [[ $DRY_RUN -eq 1 ]]; then
        echo "merge: header of g1 + data rows of g1..g48 -> $MERGED"
    else
        head -1 "$SWEEP_DIR/g1.csv" > "$MERGED"
        for GROUP in $(seq 1 48); do
            tail -n +2 "$SWEEP_DIR/g${GROUP}.csv" >> "$MERGED"
        done
        echo "merged 48 group files -> $MERGED"
    fi
done

echo "done. Merge into the master CSV with:"
echo "  python3 analysis/build_master.py --extra-dir $OUTPUT_FOLDER"
