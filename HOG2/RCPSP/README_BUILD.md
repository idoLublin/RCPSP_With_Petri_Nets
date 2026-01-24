# RCPSP Solver - Build & Run Guide

## Overview

This is an A* search-based RCPSP (Resource-Constrained Project Scheduling Problem) solver using Petri Nets modeling. The current branch (`DP-approach`) includes Dynamic Programming optimizations for the heuristic function.

## Prerequisites

- **macOS** with Apple Silicon (M-series) or Intel
- **clang++** (comes with Xcode Command Line Tools)
- **C++17** or later
- **VS Code** with C/C++ extension (optional, for tasks)

## Building with VS Code Tasks

Two build tasks are available in `.vscode/tasks.json`:

### Debug Build
```
Task: "C/C++: clang build Driver (Debug)"
```
- Includes debug symbols (`-g`)
- No optimizations
- Good for debugging with breakpoints

### Release Build (Default) ⭐
```
Task: "C/C++: clang build Driver (Release)"
```
- Full optimizations (`-O3 -march=native -DNDEBUG -flto`)
- Best performance for actual runs
- **This is the default build task**

### How to Run Tasks in VS Code

1. **Keyboard Shortcut**: `Cmd+Shift+B` (runs default build task)
2. **Command Palette**: `Cmd+Shift+P` → "Tasks: Run Task" → Select task
3. **Terminal Menu**: Terminal → Run Build Task

Output binary: `build/Driver`

## Building from Terminal

### Release Build (Recommended)
```bash
cd /path/to/RCPSP_With_Petri_Nets
mkdir -p build
clang++ -std=c++17 -O3 -march=native -DNDEBUG -flto HOG2/RCPSP/Driver.cpp -o build/Driver
```

### Debug Build
```bash
clang++ -std=c++17 -g HOG2/RCPSP/Driver.cpp -o build/Driver
```

## Running the Solver

### Basic Usage
```bash
./build/Driver --help
```

### Command Line Options

| Option | Description | Default |
|--------|-------------|---------|
| `--group-start N` | Starting group number | 1 |
| `--group-end N` | Ending group number | 1 |
| `--exam-start N` | Starting exam number | 1 |
| `--exam-end N` | Ending exam number | 10 |
| `--problem-type T` | Problem type: `j30`, `j60`, `j90`, `j120` | j30 |
| `--method M` | Solving method: `tp`, `tt`, `all` | all |
| `--output-folder F` | Output folder path | data |
| `--output-file F` | Output filename | auto-generated |
| `--tag TAG` | Tag to append to filename | (none) |
| `--time-limit N` | Time limit per problem (seconds) | 300 |
| `--use-cs` | Enable CS optimization | true |
| `--no-cs` | Disable CS optimization | |
| `--no-sort` | Disable result sorting | |
| `--no-header` | Don't write CSV header | |
| `--help, -h` | Show help message | |

### Methods

- **TP (Timed Petri)**: Uses timed Petri net representation
- **TT (Time-Tabled)**: Uses time-tabled approach with more constrained search
- **all**: Runs both TP and TT methods

### Example Commands

```bash
# Run groups 1-16, exams 1-10 with TP method
./build/Driver --group-start 1 --group-end 16 --method tp --tag my_test

# Run j60 problems with 5-minute timeout
./build/Driver --problem-type j60 --time-limit 300

# Run specific group with TT method
./build/Driver --group-start 16 --group-end 16 --method tt --tag tt_test

# Run all methods on j30 with custom output
./build/Driver --output-file results.csv --method all
```

### Environment Variables

You can also set defaults via environment variables:
```bash
export RCPSP_GROUP_START=1
export RCPSP_GROUP_END=16
export RCPSP_METHOD=tp
export RCPSP_TIME_LIMIT=300
export RCPSP_TAG=dp_test

./build/Driver  # Uses environment variable defaults
```

## Output

Results are written to CSV files in the `data/` folder (or specified output folder).

**CSV Columns:**
- `group`, `exam` - Problem identification
- `time` - Solve time in seconds
- `finished` - Whether solution was found (True/False)
- `makespan` - Optimal makespan (0 if not finished)
- `expand_number` - Number of nodes expanded
- `generated_number` - Number of nodes generated
- `depth` - Solution depth
- `PetriType` - Method used (TP or TT)
- `SetType` - Problem set (j30, j60, j90, j120)
- `UseCS` - Whether CS optimization was enabled

## DP-Approach Branch Features

This branch includes **Dynamic Programming optimizations** for the heuristic function:

1. **Precomputed Arrays**: Uses topological sort to precompute earliest finish times
2. **O(1) Heuristic Lookups**: Instead of computing heuristic on-the-fly, uses precomputed values
3. **Thread-Local Storage**: Safe for parallel execution
4. **MAX_ACTIVITIES Constant**: Supports up to 128 activities (j30/j60/j90/j120)

## Problem Sets

JSON problem files are located in:
- `json_outputs_j30/` - 30-activity problems (groups 1-48)
- `json_outputs_j60/` - 60-activity problems
- `json_outputs_j90/` - 90-activity problems
- `json_outputs_j120/` - 120-activity problems

Each group has 10 exam instances (e.g., `j301_1`, `j301_2`, ..., `j301_10`).

## Troubleshooting

### Build Warnings
Some deprecated warnings (sprintf, missing override) are expected and don't affect functionality.

### Timeout Issues
If solver times out frequently, try:
- Increasing `--time-limit`
- Using TT method (often faster but more constrained)
- Running on smaller problem sets (j30)

### Memory Issues
For large problems (j90, j120), ensure sufficient RAM available.
