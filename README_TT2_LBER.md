# RCPSP Solver — TT2 search + LBER/CP heuristics

A\* solver for the Resource-Constrained Project Scheduling Problem (RCPSP),
using the **TT2** forward search (relative-time state model) with selectable
lower-bound heuristics. This documents how to **compile** and **run** it.

---

## 1. Requirements

- A C++17 compiler — `g++` (tested: GCC 16) or `clang++`.
- The PSPLIB instance data + bound files under `data/` (already in the repo).
- **Optional:** [OR-Tools](https://developers.google.com/optimization) — only
  needed for the exact IP feasibility tiers of LBER at **depth ≥ 5** (EER/GER).
  Depths 1–4 build and run without it.

---

## 2. Build

### Default (single command, no OR-Tools) — recommended

The project is a unity build: compiling `Driver.cpp` pulls in everything.

```bash
mkdir -p build
g++ -std=c++17 -O3 -march=native -DNDEBUG -flto \
    HOG2/RCPSP/Driver.cpp -o build/Driver
```

(Swap `g++` for `clang++` if preferred — same flags.)

This supports LBER **depths 1–4**. Depths 5–6 (EER/GER) silently degrade to
depth 4 in this build, because those tiers are guarded by `USE_ORTOOLS`.

### With OR-Tools (enables LBER depth 5/6 exact IP tiers)

Requires OR-Tools installed and discoverable by CMake. A `CMakeLists.txt` is
provided that sets `-DUSE_ORTOOLS` and links `ortools::ortools`:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

### Debug build

```bash
g++ -std=c++17 -g HOG2/RCPSP/Driver.cpp -o build/Driver_dbg
```

---

## 3. Run

```bash
./build/Driver [options]
```

### Options

| Option | Meaning | Default |
|---|---|---|
| `--group-start N` / `--group-end N` | Instance group range | 1 / 1 |
| `--exam-start N` / `--exam-end N` | Instance (exam) range | 1 / 10 |
| `--problem-type T` | `j30`, `j60`, `j90`, `j120` | `j30` |
| `--method M` | `tp`, `tt`, `tt2`, `all` (`all` = tp+tt) | `all` |
| `--heuristic H` | `cp`, `lbcs`, `lbcc`, `lber`, `lbrc` | `cp` |
| `--lber-depth D` | LBER pipeline depth `1..6` (higher = tighter, costlier) | 3 |
| `--lber-mode M` | `root` (once per problem) or `pernode` | `root` |
| `--time-limit N` | Per-instance time limit, seconds | 300 |
| `--dp` / `--no-dp` | DP preprocessing for the heuristic | on |
| `--output-folder F` | Output folder | `data` |
| `--output-file F` | Output CSV name (relative to output-folder) | auto |
| `--tag TAG` | Tag appended to the auto filename | — |
| `--no-sort` / `--no-header` | Skip result sorting / CSV header | — |
| `--help`, `-h` | Show help | — |

> To exercise the **TT2** search specifically, use `--method tt2` with a
> `--heuristic` (see below). `--method all` runs the older `tp`+`tt` models.

### Heuristics (for `--method tt2`)

| `--heuristic` | Bound | Notes |
|---|---|---|
| `cp` | Critical path (a.k.a. lbcp) | **Best on j30** — consistent, no re-opening, fastest. |
| `lbrc` | `max(critical path, resource-work/capacity)` | Consistent; ≥ cp; occasionally prunes better. |
| `lber` | `max(critical path, energetic root bound − g)` | Admissible; needs re-opening (auto-enabled for TT2). Use `--lber-depth`. Costlier; on j30 it expands *more* nodes than `cp`. |
| `lbcs`, `lbcc` | Other lower bounds | Used mainly by the `tp`/`tt` models. |

LBER depth guide: `3` = CER+DFF+SHV (default), `4` = +RER, `5` = +EER,
`6` = +GER. Depths 5–6 require the OR-Tools build (else they act as depth 4).

### Examples

```bash
# TT2 + critical path on j30 group 1, exams 1-10
./build/Driver --method tt2 --heuristic cp --problem-type j30 \
  --group-start 1 --group-end 1 --exam-start 1 --exam-end 10

# TT2 + LBER depth 6 on one instance, 5-minute cap
./build/Driver --method tt2 --heuristic lber --lber-depth 6 \
  --group-start 2 --group-end 2 --exam-start 2 --exam-end 2 --time-limit 300

# TT2 + LBRC across all 480 j30 instances
./build/Driver --method tt2 --heuristic lbrc --problem-type j30 \
  --group-start 1 --group-end 48 --exam-start 1 --exam-end 10 \
  --output-file j30_lbrc.csv
```

---

## 4. Output

Results are appended as CSV to the output folder (default `data/`). Columns:

```
group, exam, time, solved, makespan, expandNumber, generatedNumber,
depth, model, problemType, heuristic
```

- `time` = wall seconds, `solved` = `True`/`False` (False = timed out),
  `makespan` = solution cost, `expandNumber`/`generatedNumber` = A\* node counts.

---

## 5. Makespan validation (the "optimal barrier")

Solved instances are checked against bound files in `data/`:

- **`{type}opt.sm`** (e.g. `j30opt.sm`) — *published optima*: an exact-match
  check; a mismatch is a hard error (`exit 1`).
- **`{type}lb.sm`** (e.g. `j60lb.sm`) — *lower bounds only*: an **admissibility**
  check — makespan below the LB is a hard error; otherwise it passes as
  "≥ LB (no UB published)". Optimality is not asserted (unknown for j60/j90).
- If neither file exists, validation is silently skipped.

(The opt-vs-LB decision is keyed on `problemType == "j30"`.)

---

## 6. Memory (optional debug cap)

The TT2 search can grow large on hard instances. An **opt-in** resident-memory
cap makes a runaway instance abort gracefully (logged unsolved, peak RSS
reported) instead of being OOM-killed:

```bash
RCPSP_MEM_LIMIT_GB=20 ./build/Driver --method tt2 --heuristic lber ...
```

Default is **off** (no cap). For batch runs, also wrap the process in a hard OS
backstop so an overrun can never take down the machine/session:

```bash
export MALLOC_ARENA_MAX=1          # keep virtual ~= resident so the soft cap fires first
( ulimit -v 28000000               # 28 GB hard cap -> child bad_alloc, not a machine OOM
  RCPSP_MEM_LIMIT_GB=20 ./build/Driver --method tt2 --heuristic cp \
    --problem-type j30 --group-start 1 --group-end 48 --exam-start 1 --exam-end 10 \
    --time-limit 300 --no-sort --output-file j30_cp.csv )
```

---

## 7. Notes

- TT2 runs A\* with **node re-opening enabled**, which keeps it optimal for the
  admissible-but-inconsistent LBER bound.
- On **j30**, `cp` is the strongest practical heuristic for TT2 (fewest nodes,
  fastest); LBER's extra machinery doesn't pay off because the critical-path
  bound is already near-optimal there.
- **j60/j90** are effectively out of reach for exhaustive A\* on a ~32 GB machine
  (the search exhausts memory before completing), regardless of heuristic.
