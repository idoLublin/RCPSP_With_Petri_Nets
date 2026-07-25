# RCPSP Solver — Mentor Run Guide (Linux)

End-to-end protocol for running the full benchmark battery on the `new_herustic` branch. Tested on Linux with `g++`. All paths assume you start from the repo root.

---

## 1. Get the code

```bash
git clone https://github.com/idoLublin/RCPSP_With_Petri_Nets.git
cd RCPSP_With_Petri_Nets
git checkout new_herustic
git pull origin new_herustic
```

Confirm you're on the right branch:

```bash
git log -1 --oneline
# Expect: a commit on new_herustic with TT2 + LBMAX + j60 LB validation.
```

---

## 2. Build (Linux, g++)

```bash
mkdir -p build
g++ -std=c++17 -O3 -march=native -DNDEBUG -flto \
    HOG2/RCPSP/Driver.cpp -o build/Driver
```

If `-march=native` is rejected on your machine, drop it — the binary will be slightly slower but functionally identical:

```bash
g++ -std=c++17 -O3 -DNDEBUG -flto HOG2/RCPSP/Driver.cpp -o build/Driver
```

Expected: a few `-Winconsistent-missing-override` warnings, no errors, produces `build/Driver`.

---

## 3. Sanity smoke (must pass before launching long sweeps)

```bash
./build/Driver --group-start 16 --group-end 16 --exam-start 1 --exam-end 3 \
    --method tt2 --heuristic cp --problem-type j30 \
    --output-folder data/real_data --tag mentor_smoke
```

Expected console output (last lines):

```
Validated: makespan 51 matches published optimum.
Validated: makespan 48 matches published optimum.
Validated: makespan 36 matches published optimum.
```

If you see `ERROR: Makespan mismatch ...` — STOP. The build is producing wrong results; don't continue.

---

## 4. Full benchmark battery

5 heuristics × 2 methods (TT, TT2) × 2 datasets (j30, j60) = **20 sweeps**.

Each sweep writes one CSV under `data/real_data/`. Filenames are auto-generated and include the date, dataset, group range, method, and tag, e.g. `2026-06-27_j30_g1-48_e1-10_tt2_tt2_mentor_j30_cp.csv`.

### 4.1 j30 (480 instances per sweep)

j30 is the published benchmark with certified optimums. Validation crashes on any mismatch — a clean run means the optimum was reached on every solved instance.

```bash
# j30 + TT2 (5 heuristics)
for h in cp lbcs lbcc lbip0 lbmax; do
    ./build/Driver --group-start 1 --group-end 48 \
        --method tt2 --heuristic $h --problem-type j30 \
        --output-folder data/real_data --tag mentor_j30_$h
done

# j30 + TT (5 heuristics)
for h in cp lbcs lbcc lbip0 lbmax; do
    ./build/Driver --group-start 1 --group-end 48 \
        --method tt --heuristic $h --problem-type j30 \
        --output-folder data/real_data --tag mentor_j30_tt_$h
done
```

### 4.2 j60 (480 instances per sweep, much slower)

j60 has only published lower bounds (no certified optima). Validation only checks `makespan ≥ LB` — a value below LB indicates an admissibility bug and crashes the run. Instances not listed in `data/j60lb.sm` are silently skipped (no validation possible).

```bash
# j60 + TT2 (5 heuristics)
for h in cp lbcs lbcc lbip0 lbmax; do
    ./build/Driver --group-start 1 --group-end 48 \
        --method tt2 --heuristic $h --problem-type j60 \
        --output-folder data/real_data --tag mentor_j60_$h
done

# j60 + TT (5 heuristics)
for h in cp lbcs lbcc lbip0 lbmax; do
    ./build/Driver --group-start 1 --group-end 48 \
        --method tt --heuristic $h --problem-type j60 \
        --output-folder data/real_data --tag mentor_j60_tt_$h
done
```

### 4.3 TT with dominance rules (DR1 + DR2 + DR5)

Same as the TT sweeps in §4.1 / §4.2, with `--tt-dr` added to turn the dominance rules on.

```bash
# j30 + TT + dominance (5 heuristics)
for h in cp lbcs lbcc lbip0 lbmax; do
    ./build/Driver --group-start 1 --group-end 48 \
        --method tt --heuristic $h --problem-type j30 --tt-dr \
        --output-folder data/real_data --tag mentor_j30_ttdr_$h
done

# j60 + TT + dominance (5 heuristics, slower)
for h in cp lbcs lbcc lbip0 lbmax; do
    ./build/Driver --group-start 1 --group-end 48 \
        --method tt --heuristic $h --problem-type j60 --tt-dr \
        --output-folder data/real_data --tag mentor_j60_ttdr_$h
done
```

To run a single rule instead of all three, swap `--tt-dr` for one of `--tt-dr1`, `--tt-dr2`, `--tt-dr5`, or `--tt-dr5-pop`.

### 4.4 Time-limit knob

Default is **300 seconds per instance**. To give j60 instances more budget at the cost of total wall time:

```bash
./build/Driver ... --time-limit 600   # 10 minutes per instance
```

### 4.5 Running in `tmux` (recommended for long sweeps)

```bash
tmux new -s rcpsp
# inside tmux:
# launch any sweep command from §4.1 or §4.2 here
# detach: Ctrl-b d
# reattach later: tmux attach -t rcpsp
```

---

## 5. What to watch for

- **`Validated: makespan N matches published optimum.`** — j30 result agrees with the certified optimum. Good.
- **`Validated: makespan N >= LB X (no UB published).`** — j60 result is at or above the published lower bound. Good — but unverified above LB.
- **`Path not found or timeout occurred.`** — instance hit the time limit. Recorded as `False` in the `finished` column of the CSV.
- **`ERROR: Makespan mismatch ...`** (j30) or **`ERROR: makespan ... below published LB ...`** (j60) — admissibility bug, solver exits with code 1. Capture the group/exam in the error message and stop the sweep; this needs investigation before any more results are recorded.

---

## 6. Commit and push results

After all sweeps finish (or whenever you have a meaningful batch):

```bash
git add data/real_data/*.csv
git commit -m "mentor benchmark sweep: j30 + j60 × {TT, TT2} × {cp,lbcs,lbcc,lbip0,lbmax}"
git push origin new_herustic
```

If a sweep crashed mid-way with a real bug, **do not commit** the partial CSV until the bug is investigated — a wrong-result row would be hard to spot later.

---

## 7. Useful one-liners

**Count how many instances each sweep solved:**

```bash
for f in data/real_data/*mentor_j30*.csv; do
    solved=$(awk -F, 'NR>1 && $4=="True"' "$f" | wc -l)
    total=$(awk -F, 'NR>1' "$f" | wc -l)
    echo "${f##*/} : $solved / $total"
done
```

**Confirm all solved j30 instances validated (no silent corruption):**

```bash
for f in data/real_data/*mentor_j30*.csv; do
    # Validation runs only when the makespan in CSV came from an actually-solved row.
    # If the run completed without crashing, every solved row matched the optimum.
    # This just confirms no row has zero makespan with finished=True (impossible state).
    awk -F, 'NR>1 && $4=="True" && $5==0' "$f"
done
```

---

## 8. Method / heuristic legend

**Methods:**
- `tp` — Transition Place (the original encoding)
- `tt` — Timed Transition (TP improvements)
- `tt2` — TTPNR, the SoCS 2026 formulation with relative-delay tokens

**Heuristics** (all admissible):
- `cp` — Critical Path + paper's `hres` (resource-load)
- `lbcs` — `cp` + LBCS (Klein & Scholl LB3, workload extension on critical path)
- `lbcc` — `cp` + LBcc (critical capacity, work-arrival simulation)
- `lbip0` — `cp` + LBip0 (Klein & Scholl LB10, incompatible pairs)
- `lbmax` — `cp` + LBrc (Klein & Scholl LB2, resource capacity)

Every TT2 heuristic includes the paper's hmax floor (`max(CP, hres)`); the chosen extra bound only tightens — never weakens — the heuristic.
