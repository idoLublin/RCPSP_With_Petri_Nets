---
name: compile-and-run
description: "Compile and run the RCPSP solver. Covers release/debug builds with clang++, CLI options, and running benchmarks. Use when: compile, build, run solver, execute Driver, benchmark."
---

# Compile and Run RCPSP Solver

## Compile

### Release Build (default)
```bash
cd /Users/jonathantoaf/Documents/bar-ilan/final-project/code/RCPSP_With_Petri_Nets
mkdir -p build
clang++ -std=c++17 -O3 -march=native -DNDEBUG -flto HOG2/RCPSP/Driver.cpp -o build/Driver
```

### Debug Build
```bash
cd /Users/jonathantoaf/Documents/bar-ilan/final-project/code/RCPSP_With_Petri_Nets
mkdir -p build
clang++ -std=c++17 -g HOG2/RCPSP/Driver.cpp -o build/Driver
```

## Run

```bash
./build/Driver [options]
```

### Key Options

| Option | Description | Default |
|--------|-------------|---------|
| `--group-start N` | Starting group number | 1 |
| `--group-end N` | Ending group number | 1 |
| `--exam-start N` | Starting exam number | 1 |
| `--exam-end N` | Ending exam number | 10 |
| `--problem-type T` | `j30`, `j60`, `j90`, `j120` | j30 |
| `--method M` | `tp`, `tt`, `all` | all |
| `--heuristic H` | `cp` (Critical Path), `lbcs` (Lower Bound CS), `lbcc` (Lower Bound CC) | cp |
| `--dp` | Use DP preprocessing for heuristic (default) | enabled |
| `--no-dp` | Disable DP preprocessing (use original heuristic) | |
| `--dominance` | Cutset-style dominance pruning at node generation (TT2 only) | disabled |
| `--dominance-pop` | `--dominance` + re-check when a node is popped for expansion | disabled |
| `--ub-prune` | Prune `g+h > UB` using a root serial-SGS schedule (TT2 only) | disabled |
| `--tt-dr` | Enable all Liu-style dominance rules DR1+DR2+DR5 (TT only) | disabled |
| `--tt-dr1` / `--tt-dr2` / `--tt-dr5` / `--tt-dr5-pop` | Individual TT dominance sub-rules (ablation) | disabled |
| `--output-folder F` | Output folder path | data |
| `--output-file F` | Output filename | auto-generated |
| `--tag TAG` | Tag to append to filename | (none) |
| `--time-limit N` | Time limit per problem (seconds) | 300 |
| `--no-sort` | Disable result sorting | |
| `--no-header` | Don't write CSV header | |
| `--help, -h` | Show help | |

### Example Commands

```bash
# Run groups 1-48, exams 1-10 with TT method and CP heuristic (DP enabled)
./build/Driver --group-start 1 --group-end 48 --method tt --heuristic cp --tag cp_dp

# Run without DP preprocessing
./build/Driver --group-start 1 --group-end 48 --method tt --heuristic cp --no-dp --tag cp_nodp

# Run specific group with TT method
./build/Driver --group-start 10 --group-end 10 --exam-start 8 --exam-end 8 --method tt

# Run j60 problems with 5-minute timeout
./build/Driver --problem-type j60 --time-limit 300

# Run TP method on group 1
./build/Driver --group-start 1 --group-end 1 --method tp --tag tp_test
```

## Output

Results are written as CSV to the `data/` folder. Use `--output-folder` or `--output-file` to customize.

CSV columns: `Group, Exam, Time, Solved, Makespan, NodesExpanded, NodesTouched, PathLength, Method, ProblemType, Heuristic` — TT2 rows append `dominance_pruned, dominance_pop_pruned, ub_pruned`; TT rows with `--tt-dr*` append `dr1_pruned, dr2_pruned, dominance_pruned, dominance_pop_pruned`.

Note: each Driver invocation truncates its output CSV at startup — use one output file per invocation when batching runs.

The Heuristic column includes DP status (e.g., `CP_DP` or `CP_NoDP`).

## Makespan Validation

Solved problems are automatically validated against `data/{problemType}opt.sm` (e.g., `data/j30opt.sm`). If a solved problem's makespan doesn't match the known optimal, the solver exits with an error.
