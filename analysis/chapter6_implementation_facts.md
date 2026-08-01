# Chapter 6 — Implementation and Engineering: verified facts from code + git

All facts below were verified against the repository on branch `new_herustic`
(2026-08-01) and against `origin/main` (the previous project's final state,
commit b759f4f, 2025-12-20). Citations are `file:line`.

---

## 6.1 Codebase and Tools

**Languages & build.** The solver is C++17, compiled as a single translation
unit: `Driver.cpp` `#include`s `RCPSPState.cpp`, `RCPSP.h`,
`TemplateAStar.h`, and the dominance headers (HOG2/RCPSP/Driver.cpp:22-28).
Release build (from the compile-and-run skill):
`clang++ -std=c++17 -O3 -march=native -DNDEBUG -flto HOG2/RCPSP/Driver.cpp -o build/Driver`.
A root `CMakeLists.txt` also exists (C++20, links Threads+OpenMP), but no
`#pragma omp` appears anywhere in the solver — OpenMP is a vestigial link; the
actual benchmark binaries were built with the plain clang++ command. The only
external library is the vendored single-header **nlohmann/json**
(HOG2/RCPSP/json.hpp). Python 3 (+pandas/matplotlib notebooks) is used for
instance preprocessing and result analysis, not for solving.

**HOG2 usage.** Yes — the solver is still built on HOG2, but only a thin
slice of it:

| HOG2 component | Used? | Notes |
|---|---|---|
| `generic/TemplateAStar.h` (A* engine) | Yes, heavily modified by us | +224 lines: new tie-break comparator, configurable timeout, 4 pruning hooks |
| `algorithms/AStarOpenClosed.h` (open list = binary heap + hash table) | Yes, unmodified | |
| `search/SearchEnvironment.h` (domain interface) | Yes, unmodified | all three models subclass it |
| `RCPSP/FPUtil` (float comparisons) | Yes | |
| `generic/BAE.h` (bidirectional) | Present, **not working / not used** (Driver.cpp:865 "currently not working") | |
| Everything else (grids, GUI, maps, …) | Unused; built with `-DNO_OPENGL` | |

**Preprocessing.** Still the previous project's Python pipeline — **not
reimplemented**. `extract_problems/extract_problem.py` +
`RCPSP_modeling/rcpsp_petri_net.py` parse PSPLIB `.sm` files and emit
per-instance `petri.json` + `rcpsp.json` into `json_outputs_<set>/`. Zero
commits on our branch touch `RCPSP_modeling/` or `extract_problems/`, and the
`json_outputs_*` directories are tracked in `origin/main` (inherited). The C++
side (`readPetri.cpp:16,143`) only loads the pre-generated JSON — it never
reads `.sm` directly. (No "Iyar Zacks" credit string exists in the repo; the
credit lives only in the previous book.)

**Repo layout (main modules).**
- `HOG2/RCPSP/Driver.cpp` — CLI harness: flag parsing, per-instance loop, CSV output, optimum validation, `--diagnose`.
- `HOG2/RCPSP/RCPSP.h` — the three search environments: `RCPSP` (TP), `RCPSP_TT` (TTPN, absolute dates), `RCPSP_TT2` (TTPNR, relative delays); successor generation, goal test, heuristic dispatch, state hashing, DR1/DR2/DR4 generation-time pruning.
- `HOG2/RCPSP/RCPSPState.{h,cpp}` — state classes + all heuristic implementations (CP-DP, LBCS, LBCC, LBIP0, LBRC/LBMAX, h_res), SGS upper bound, per-instance caches.
- `HOG2/RCPSP/DominanceTT2.h` / `DominanceTT.h` — cutset dominance tables for TT2 / TT.
- `HOG2/RCPSP/HeuristicTypes.h` — heuristic enum + string mapping for the CLI.
- `HOG2/RCPSP/readPetri.cpp`, `petriclasses.h` — JSON instance loader and net classes.
- `HOG2/generic/TemplateAStar.h` — the A* engine (open/closed, tie-breaking, reopening, pruning hooks).
- `RCPSP_modeling/`, `extract_problems/` — inherited Python .sm→Petri-net JSON pipeline.
- `json_outputs_{j30,j60,j90,j120}/` — pre-built instance JSONs (inherited).
- `data/`, `data/real_data/`, `logs/` — benchmark CSVs and run logs.
- `scripts/` — validation/comparison tooling (ours): `compare_invariant.sh`, `compare_heuristics.py`, benchmark-union and validation notebooks.
- `analysis/` — book results pipeline (separate session).

---

## 6.1b Ownership split (git-authoritative)

**Correction:** there is **no `origin/master`**. The previous project's code is
`origin/main` (tip b759f4f, 2025-12-20). Our branch `new_herustic` is strictly
ahead of it — merge-base = origin/main tip — by **61 commits**.

**Totals** (`git diff --stat origin/main...HEAD`): 71 files, **+24,661 / −2,657**
lines including data. Code-only (HOG2 + scripts + docs + md): 24 files,
**+9,136 / −2,651**. Core C++ alone: 10 files, +4,865 / −2,650:

```
DominanceTT.h     +213 (new)     RCPSPState.cpp  3821 changed
DominanceTT2.h    +177 (new)     RCPSPState.h     139 changed
HeuristicTypes.h  +100 (new)     petriclasses.h    89 changed
Driver.cpp        1778 changed   readPetri.cpp    249 changed
RCPSP.h            725 changed   TemplateAStar.h  224 changed
```

**Files added by us:** DominanceTT.h, DominanceTT2.h, HeuristicTypes.h,
README_BUILD.md, README_LBCC_IMPLEMENTATION.md, docs/LBCS_IMPLEMENTATION_GUIDE.md,
MENTOR_GUIDE.md, scripts/* (compare_invariant.sh, compare_heuristics.py,
compare_cp_lbcs*.py, union_benchmark_files.py, validation notebooks), the
visualizations (petri_net_visualization.html, lb_visualization.html,
cpm_visualization.html), the bounds files data/j30opt.sm + data/j60lb.sm, the
compile-and-run skill, and all benchmark CSVs/notebooks.

**Inherited files we modified:**
- `Driver.cpp` — effectively rewritten: configurable CLI (flags + env vars), TT2 runner, optimum/LB validation with abort, `--diagnose`, dominance/UB wiring, CSV schema.
- `RCPSP.h` — added the whole `RCPSP_TT2` environment, per-heuristic dispatch in all `HCost`s, DR1/DR2 (TT) and DR4 (TT2) generation-time pruning, FNV-1a hashing.
- `RCPSPState.cpp` — all Chapter-4 heuristics (LBCS, LBCC, LBIP0, LBRC, h_res), the DP cache, the SGS upper bound, demand-matrix cache; TT2 state code integrated from branch `origin/ido`.
- `RCPSPState.h` — TT2 state class, fixed-size arrays/bitset, `MAX_ACTIVITIES` build knob.
- `TemplateAStar.h` — tie-break comparator, timeout API, the four pruning hooks.
- `petriclasses.h` / `readPetri.cpp` — integer arc indices, `reset()`, robust path fallback.

**Untouched inherited:** the entire Python pipeline (RCPSP_modeling/,
extract_problems/, algorithms_for_solving_rcpsp/, a_star_solver.py,
RCPSP-AS/, real_projects/, tests/), all json_outputs_*, and all of HOG2
except TemplateAStar.h.

**Verdict on the book's claim** ("pipeline and TTPN encoding build on the
previous solver; TTPNR encoding, every Chapter-4 heuristic and Chapter-5
dominance mechanism implemented by us"): **mostly confirmed, one caveat.**
Pipeline + TP/TT encodings: inherited ✔. All heuristics + all dominance
mechanisms (DR1/DR2/DR4/DR5-cutset, tables, UB prune): ours, confirmed by git ✔.
**Caveat:** the TTPNR (TT2) *core encoding* is marked in code as "sourced
verbatim from origin/ido" (RCPSPState.cpp:2148-2150; RCPSP.h:930-932) — the
base state/successor code came from Ido Lublin's branch (the SoCS 2026 TTPNR
paper). What is ours in TT2: the heuristic dispatch, admissibility-safe hmax
floor, FNV hashing, DR4, dominance store, UB pruning, and integration. The
book should say "the TTPNR encoding was adopted from [SoCS 2026 / the ido
branch] and extended by us", not "implemented by us".

---

## 6.2 System Architecture

```
PSPLIB .sm ──(Python, inherited)──► json_outputs_<set>/<g>_<e>/{petri.json, rcpsp.json}
                                          │
                              readPetri.cpp: getPetri()/getRCPSP()  → globals petri, RCPSPex
                                          │
Driver.cpp runSolver(): per (group,exam): reset caches → pick solver by --method
                                          │
             ┌────────────────────────────┼─────────────────────────────┐
        RCPSP (TP)                  RCPSP_TT (TTPN)               RCPSP_TT2 (TTPNR)
        RCPSP.h:60                  RCPSP.h:577                   RCPSP.h:934
             └────────────────────────────┼─────────────────────────────┘
                        TemplateAStar<state,int,env>::GetPath  (shared engine)
                          │ HCost() dispatches on global activeHeuristic
                          ▼
        heuristic modules (RCPSPState.cpp, shared): CP-DP, LBCS, LBCC, LBIP0, LBRC, h_res
                          │
        dominance stores (per-model): DominanceTT2.h / DominanceTT.h
        hooked via SetGeneratePruner / SetOnOpenAdded / SetExpandPruner / SetFPruner
                          │
        Driver.cpp: CSV row per instance + optimum/LB validation (abort on mismatch)
```

**Model interface** (HOG2 `SearchEnvironment<state,action>`): each model
implements `GetSuccessors`, `GoalTest`, `HCost`, `GCost`, `GetStateHash`,
plus `operator==` on its state class. States are per-model
(`RCPSPState`, `RCPSPState_TT`, `RCPSPState_TT2` in RCPSPState.h).

**Heuristic module interface**: free functions, selected by the
`HeuristicType` enum (HeuristicTypes.h:21) via the global `activeHeuristic`.
A heuristic receives per state: the vector of unfinished activity IDs, the
`activeTransitionIndices` list of (activityID, remaining-duration) pairs, and
the current makespan `g`; it returns a double lower bound, e.g.
`computeLBCS(unfinished, active)` and
`computeCriticalCapacityLB(unfinished, active, g)` (RCPSP.h:28-45). Each
heuristic has a per-instance `initialize*()` precomputation guarded by a
`thread_local bool`, reset per instance in `runSolver` (Driver.cpp:1008-1012).
TT2 always computes `max(cpH, hresH, extraH)` so the paper's consistent hmax
is a floor under every dispatch (RCPSP.h:1058-1087).

---

## 6.3 Engineering: A* tie-breaking and reopening (closes ch. 3 notes)

**OPEN ordering** (TemplateAStar.h:158-179), in order:
1. smaller **f**;
2. larger **g**;
3. more **started/active** activities (TP: started count; TT2: number of
   active transitions; TT: constant 0 — a no-op for TT);
4. more **finished** activities.

**The book's claimed 4th key "generation order" does not exist** — after key 4
the binary heap's order is arbitrary (not FIFO). Correct the book text.

**Reopening:** the engine defaults to no-reopen (TemplateAStar.h:242), but
**all three solvers explicitly enable it** — `astar.SetReopenNodes(true)` at
Driver.cpp:418, 505, 634. The reopen path (TemplateAStar.h:630-644): a closed
child found via a cheaper path gets its parent/g/f updated and is pushed back
to OPEN. The in-code rationale: free when the heuristic is consistent (the
condition never fires) and required when a bound is admissible-but-inconsistent
— observed concretely as TT2+lbip0 returning 50 instead of the optimum 49 on
j30 g1 e9 without reopening.

---

## 6.3 Engineering Optimizations

**1. The DP optimization (mid-presentation).** `initializeHeuristicDP()`
(RCPSPState.cpp:84-116) runs one CPM forward pass per instance, caching the
static earliest-finish time of every activity in `thread_local heuristicDP[]`.
`getForwardHcostDP()` (RCPSPState.cpp:662+) then computes the critical-path
heuristic per state by only correcting for in-progress activities' remaining
durations (an `activeRemaining[]` overlay), instead of re-deriving the full
recursion at every node. Toggle: `--dp` (default) / `--no-dp`.
**Measured effect** (data/dp_vs_no_dp_comparison_results.csv, 85 j30
instances, mostly timeout-bound so nodes-expanded ≈ throughput): DP wins
59/85 instances; mean **+54.8%** nodes expanded in the same time budget
(median +27%). It is a throughput gain, not universal — no-DP won 26/85.

**2. Other named optimizations (one line each).**
- **FNV-1a 64-bit state hash** replacing the 32-bit Boost-style combine — the old one had ~32-bit birthday collisions at 1M+ states (RCPSP.h:1092-1156).
- **Demand-matrix cache**: string-keyed resource-demand map lookups replaced by O(1) integer-indexed `demandByRes[a][k]` in all hot paths (RCPSPState.cpp:118-129).
- **Fixed-size state storage**: `std::array<short, MAX_ACTIVITIES>` + `std::bitset` finished-set (TT2); `-DRCPSP_MAX_ACTIVITIES=64` halves per-state footprint for j30/j60 (RCPSPState.h:13-22).
- **thread_local scratch vectors** in `GetSuccessors`/`HCost` — zero allocation per call (RCPSP.h:610, 968, 1046).
- **h reuse across Δt=0 firings**: TT2 copies the predecessor's h when `isDeltaZero` (RCPSP.h:1040-1043); TP has the analogous `status` shortcut (RCPSP.h:273-275).
- **Prune-before-HCost ordering**: the dominance generate-check runs *before* the (expensive) heuristic; pruned nodes never pay for h (TemplateAStar.h:683-690).
- **Integer arc indices** resolved once at JSON load — token checks are plain array reads, no hashing (readPetri.cpp:122-137; RCPSP.h:139-150).
- **Goal test by popcount** on the finished bitset (RCPSP.h:1031-1034).
- **Pareto-thinned dominance buckets**: entries dominated by a newly inserted node are dropped from the table, keeping buckets small (DominanceTT2.h:151-169).

**3. SGS upper bound & DR4 — status in real runs (critical).**
- **SGS UB cut** (`--ub-prune`, default OFF): `computeSGSUpperBound()`
  (RCPSPState.cpp:2104-2146) runs serial SGS at the root under 5 priority
  rules (LFT, LST, MTS, LPT, GRD) and takes the best makespan as UB; nodes
  with g+h **strictly greater** than UB are pruned at generation
  (Driver.cpp:653-663), so an optimum equal to UB is still found. Safety
  check: open-list exhaustion under UB pruning aborts as a bug
  (Driver.cpp:719-724).
- **DR4 delayed-start dominance** (`--dr4`, default OFF): in TT2 successor
  generation, firing l at delta d_l is pruned when another eligible i has
  d_i < d_l and d_i + p_i ≤ d_l (RCPSP.h:992-1024, with a written soundness
  argument).
- **What was actually enabled where:**
  - **J30 full tt2 sweeps (completed, 480/480):** the 2026-07-10 dominance run used `--dominance-pop` only (UB OFF, DR4 OFF) — 480/480 in 1.8 min total. The 2026-07-15 DR4 run (`--dr4`) also solved 480/480, in 0.8 min total. A 2026-07-08 run combined dominance+UB.
  - **J60 tt2 sweep (NOT completed — paused at g13 e3):** config from the log header (logs/j60_full_dompop_2026-07-11.log): LBCS, DP on, **Dominance+pop ON, UB OFF, DR4 OFF**, 300 s limit. 58/122 attempted instances solved so far.
  - **J90: no runs exist at all.**
  So the book must describe cutset dominance as the mechanism used in the
  large-set runs; UB pruning and DR4 appear only in the J30 pruning study
  (and a single j60 g1 probe with UB ON).

**Running configuration reference** (Driver.cpp:167-208): flags
`--group-start/--group-end/--exam-start/--exam-end`, `--problem-type j30|j60|j90|j120`,
`--method tp|tt|tt2|all`, `--heuristic cp|lbcs|lbcc|lbip0|lbmax`,
`--dp/--no-dp`, `--reopen`, `--dominance`, `--dominance-pop`, `--ub-prune`,
`--dr4`, `--tt-dr` (=DR1+DR2+DR5+pop) and `--tt-dr1/2/5`, `--tt-dr5-pop`,
`--diagnose`, `--time-limit N` (default 300 s), `--output-folder/--output-file/--tag`,
`--no-sort`, `--no-header`; every option also has an `RCPSP_*` environment
variable. Output CSV schema (Driver.cpp:958):
`group,exam,time,finished,makespan,expand_number,generated_number,depth,PetriType,SetType,Heuristic,dominance_pruned,dominance_pop_pruned,ub_pruned,dr4_pruned`
(the last four columns are TT/TT2-only).

---

## 6.4 Correctness Validation

| Mechanism | Where | Behavior |
|---|---|---|
| J30 known-optima check | Driver.cpp:81-122 (loader), 446-467 / 553-574 / 692-713 (all three solvers) | loads `data/j30opt.sm`; any solved j30 instance whose makespan ≠ published optimum prints ERROR and `exit(1)` — a whole sweep aborts on the first wrong answer |
| J60 published-bounds check | same code path, `data/j60lb.sm` fallback | LB-only: aborts only if makespan < LB (proof of inadmissibility); no UB published |
| UB-prune sanity | Driver.cpp:719-724 | open-list exhaustion under UB pruning ⇒ SGS UB below optimum ⇒ abort |
| `--diagnose` mode | Driver.cpp:780-863 | solves one instance with CP, replays the optimal path through every heuristic; flags g+h > opt (inadmissible) and h-consistency violations — this is how the LBCS inadmissibility and the lbip0 reopening bug were pinned down |
| Run-invariant check | scripts/compare_invariant.sh | asserts solved rows (makespan, nodes expanded/generated, depth) are bit-identical between a baseline and candidate run; only time may differ; reports geo-mean speedup |
| tt-vs-tt2 agreement | `data/real_data/*val*` CSV pairs + scripts/validate_and_compare_benchmarks*.ipynb | validation slices run under both encodings and compared |
| Python unit tests | tests/test_critical_path.py, tests/test_modeling.py | inherited; cover the Python modeling layer only |

**Most recent full validation outcome:** j30 full tt2 sweep with
`--dominance-pop` + LBCS (2026-07-10): **480/480 solved, every one matching
the published optimum** (480 "Validated" lines in the log), 1.8 min total.
DR4 run (2026-07-15): 480/480 in 0.8 min. Best no-pruning heuristic: LBCS at
477/480 (CP 476). J60: partial, 58/122 solved before the pause.

---

## Facts that contradict what chapters 3–6 would naturally claim

1. **No `origin/master`** — the previous project's baseline is `origin/main`.
2. **Tie-breaking**: no generation-order tie-break exists; the real order is
   f → larger g → more started/active → more finished, then arbitrary heap
   order. (And for TT the started-count key is a constant no-op.)
3. **"TTPNR encoding implemented by us"** is contradicted by code comments:
   the TT2 core is "sourced verbatim from origin/ido" (RCPSPState.cpp:2150,
   RCPSP.h:932). Ours = the extensions (heuristic dispatch, hashing, DR4,
   dominance, UB) and integration.
4. **No completed J60/J90 tt2 benchmarks** — J60 paused at g13 e3 (58/122
   solved), J90 never run. The book cannot cite finished J60/J90 results yet.
5. **Preprocessing is still the previous project's Python pipeline** — the C++
   solver never parses `.sm`; it loads pre-generated JSON.
6. **Dependencies**: effectively none beyond vendored nlohmann/json; the
   CMake OpenMP requirement is dead weight (no `#pragma omp` in the solver),
   and benchmark binaries were built with the plain clang++ one-liner.
7. **Reopening is ON in production** (all three solvers), not "assumed
   impossible" — and it is load-bearing (lbip0 50-vs-49 case).
8. **DP is a throughput optimization, not a node-count reduction** — it wins
   59/85 j30 instances with mean +54.8% nodes/time; 26 instances ran faster
   without it.
