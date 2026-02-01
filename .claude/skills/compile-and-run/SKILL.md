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
| `--output-folder F` | Output folder path | data |
| `--output-file F` | Output filename | auto-generated |
| `--tag TAG` | Tag to append to filename | (none) |
| `--time-limit N` | Time limit per problem (seconds) | 300 |
| `--use-cs` / `--no-cs` | Enable/disable CS optimization | enabled |
| `--help, -h` | Show help | |

### Example Commands

```bash
# Run groups 1-16, exams 1-10 with TP method
./build/Driver --group-start 1 --group-end 16 --method tp --tag my_test

# Run j60 problems with 5-minute timeout
./build/Driver --problem-type j60 --time-limit 300

# Run specific group with TT method
./build/Driver --group-start 16 --group-end 16 --method tt --tag tt_test
```

## Output

Results are written as CSV to the `data/` folder. Use `--output-folder` or `--output-file` to customize.
