# RCPSP / CBS / TTPNR — Project & Session Handoff

Read this first in a new session. It captures the project, the codebase, what was
built, the results, the constraints, and what's still owed. Companion files:
`DOMINANCE_NOTES.md` (dominance deep-dive + full flag table) and the persistent
memory under `.claude/.../memory/`.

---

## 1. What the project is

**Goal:** a research paper (targeting HICSS / SoCS 2026) presenting the user's own
exact solvers for the **Resource-Constrained Project Scheduling Problem (RCPSP)**
and comparing them against baselines.

**The user's methods** (all their own code, in `HOG2/RCPSP/`):
- **CBS** — Conflict-Based Search: A* over a tree of partial schedules; each node
  is a set of delay decisions, its schedule is the earliest-start schedule under
  them, branching resolves the earliest/among resource conflicts by delaying
  activities. This is where nearly all of this session's work happened.
- **TT2 / TTPNR** — a Petri-net forward search with relative-delay tokens and
  hash-based duplicate pruning. Separate solver, lightly touched this session.

**Benchmark:** PSPLIB — sizes **j30 / j60 / j90 / j120**, each = **48 parameter
groups × 10 exams = 480 instances**. Protocol: single-core, **300s timeout**.
Parameters per group: **NC** (network complexity 1.5/1.8/2.1), **RF** (resource
factor 0.25/0.5/0.75/1.0), **RS** (resource strength 0.2/0.5/0.7/1.0). Same 48-cell
grid across all sizes. **RS=0.2 = hardest, RS=1.0 = trivial** (0 nodes). Low-RS +
high-RF is the hardest region.

**Baselines:**
- **HICSS** = the user's plain CBS, the 8 configs, no dominance/extensions. This is
  the "before" reference. Result CSVs are the `_1` files.
- **Baseline 4** = Liu, Jin, Zhou & Hu (2023) C&OR 151:106097 branch-and-bound —
  published results only, no runnable source. DR5 dominance comes from this paper's
  rule set (originally Demeulemeester & Herroelen 1992).
- **Baseline 3** = Gurobi — blocked on a lab token server (see memory).

---

## 2. Codebase map (`HOG2/RCPSP/`)

| file | contents |
|---|---|
| `Driver.cpp` | main; benchmark modes; `solveRCPSP_CBS_impl<N>`; TT2 modes; env-flag parsing; `serialSGS_makespan()`, `getDatasheetUB()`; diagnostic modes `sweep`/`dumpdom`/`verifydom`; the 8 `applyConfig` presets |
| `RCPSPState.h/.cpp` | `RCPSPState_CBS<N>` (CBS state), `RCPSPState_TT2` (Petri state); `compute_h_and_RVS()` (conflict detection + scoring + heuristic); `compute_first_conflict()` (light earliest-conflict scan); the 3 successor constructors; `propagate`; `left_shift_prunable()`; `greedy_dive_makespan()` |
| `RCPSP.h` | search environments `RCPSP_CBS<N>`, `RCPSP_TT2`; `GetSuccessors` (child gen + dominance/UB/leftshift gate + inline); `HCost` (h-cache); `GCost`; `GetStateHash` |
| `Globals.h` | `CBSConfig setting`; all `g_use_*` feature flags; `resource_info`; `g_sink_id` |
| `DominanceCBS.h` | the dominance table: B&P / DR5 / DR5S / BOTH rules, Line-8 key, store cap, bidirectional kill set |
| `../generic/TemplateAStar.h` | the A* **library** — modified ONLY for: configurable timeout (`astar_timeout_seconds`) and the TT2 `getcomper1` tie-break. Otherwise off-limits. |

**Templates:** `RCPSPState_CBS<N>` with N = 32 (j30), 62 (j60), 92 (j90), 122 (j120).

**The 8 CBS configs** (`applyConfig(prio, first, heuristic, mda)`):
- **cfg1** = Baseline: first-conflict, no heuristic, no MDA. (`RS` note: this is the
  simplest; dominance helps it MOST because first-conflict always advances t*.)
- cfg2=Prio, cfg3=H, cfg4=MDA, **cfg5**=Prio+H (no MDA), cfg6=Prio+MDA, cfg7=H+MDA,
  **cfg8**=All (Prio+H+MDA).
- **The user cares most about cfg1, cfg5, cfg8.** `use_strong_constraints` is
  hardcoded false in applyConfig, so every config uses the WEAK (MDA/single-delay)
  branching — this matters for dominance soundness (Bell & Park condition 2 is
  vacuous under weak constraints).

---

## 3. What was built this session

### 3a. State dominance (the core contribution) — SOUND, validated
- **Bell & Park (1990)** state dominance + the **Descendants Line-8 gate** (the
  breakthrough: never enqueue/compare an "intermediate" child with the same RVST
  and scheduled-set as its parent — else the parent trivially dominates its own
  subtree and you lose optima). This gate is why dominance works at all.
- **`t_first`** on the state: the EARLIEST conflict, tracked separately from the
  branched-on conflict, so dominance keys correctly under prioritization.
- **DR5** (cutset dominance) — **UNSOUND in CBS** (ignores release bounds; 4–8
  wrong/config). **DR5S** = DR5 + the missing release-bound comparison — SOUND.
  **BOTH** = B&P ∪ DR5S — sound but overhead-heavy (see 6b: not worth it).
- **Validation:** 1,808 instances zero-wrong (all j30 + checkable j60 + 47 j60
  "Unknown" instances proven by independent OR-Tools CP-SAT). Harness: `dumpdom`
  (log every prune) + `verifydom` (re-solve both sides exactly, self-tests first).

### 3b. Performance optimizations — SEARCH-IDENTICAL (bit-verified), ~1.9× faster
- Sweep-line conflict scan (was O(events×K) rescans); light-scan dominance gate
  (full scoring/MDA only for kept children); HCost caching; single-build dominance
  check+store; `RCPSP_DOM_CAP` memory bound. All verified bit-identical (makespan,
  expandNumber, generatedNumber) on a 104-run matrix.

### 3c. New pruning/search features (flag-gated)
- **UB pruning** (`RCPSP_UB`) — SOUND. Drop children with makespan > incumbent
  (strict `>` so a tight datasheet UB keeps the optimal goal). Incumbent from
  `getDatasheetUB()`: known optima → published bounds → `ub_cache.csv` → serial-SGS.
  **This is a deliberate "cheat" — must become on-the-fly before the paper** (memory
  `project_ub_hardcoded_swap`).
- **Threshold hybrid** (`RCPSP_HYBRID`, `RCPSP_HYBRID_T` default 0.5) — SOUND.
  Conflicts with cardinality score ≥ T picked by score; < T picked by earliest
  (first-conflict, advances t*, keeps dominance eligible). Sweep: nodes best at
  T≈0.3–0.4, coverage best at 0.5.
- **Left-shift** (`RCPSP_LEFTSHIFT`) — **UNSOUND, DO NOT USE.** CBS only delays,
  never shifts left, so the "shifted equivalent lives elsewhere" assumption breaks;
  with UB/BIDIR also pruning, both the state and its equivalent can be killed →
  optimum lost. Caused the (6,2) wrong answer.
- **Bidirectional** (`RCPSP_BIDIR`) — under-validated; part of the (6,2) collision.
  Leave off until validated alone.
- **Configurable timeout** (`RCPSP_TIMEOUT_S`, default 300).
- **LEAN** (`RCPSP_LEAN`) — memory-for-time, off by default.

### 3d. TT2 / TTPNR
- `getcomper1(RCPSPState_TT2)` tie-break changed to `g + MaxRemainInActive` (max
  remaining duration among active transitions). Tie-break only — can't affect
  correctness. Benefit unmeasured (needs a TT2 run).

---

## 4. Key results & findings

**j30 coverage (solved/480):** baseline → dominance → new-features
- cfg1: 349 → 402 → 417   cfg5: 363 → 370 → 413   cfg8: 396 → 414 → 436
**j60/j90 (baseline → dominance only):** cfg1/cfg5 gain, **cfg8 REGRESSES** (j60 −4,
j90 −6) — the "overhead paradox."

**Total expansions (matched, j30):** cfg1 105M → 6.4M → 1.5M (−98.6%); similar
cfg5/cfg8. **Generation throughput** (nodes/sec on timeouts): dominance costs a
uniform **~60%** of throughput (the per-node key/hash/scan cost); the perf rewrite
recovered ~⅓–½ of that.

**The overhead paradox (why cfg8 regresses):** dominance cuts expansions ~80% but
is *slower* in wall-clock on cfg8, because MDA already keeps node counts low so the
per-node dominance cost exceeds the savings. **Branching factor** explains it:
dominance RAISES bf (subtree pruning, more children per surviving expansion), UB
LOWERS it (child pruning). UB is the intended fix — it should flip the cfg8
regression positive on a safe-stack rerun.

**The (6,2) bug:** full stack (UB+LEFTSHIFT+BIDIR+HYBRID) returned makespan 53 for
optimum 51 on j30 cfg5 — a genuine unsoundness from LEFTSHIFT. Only surfaced on
cfg5 (larger search); cfg8 got "lucky" (solved in 2.5K nodes before the bug could
fire). **Lesson: validate ALL 10 exams, not just exam-1 — that's how it hid.**

**Complementarity (6b):** BOTH dominance uses MORE nodes than bp/dr5s alone on
matched instances (double-table overhead, not complementary) → **ship `bp` alone**.

**Timing is noisy** on the shared server (runs done at different loads). Trust the
DETERMINISTIC metrics: expandNumber, generatedNumber, coverage, optima. Report
those; treat wall-clock as a rough guide.

---

## 5. Build & run

**Local (Windows):** g++ is at `C:\msys64\mingw64\bin`; **must add it to PATH**
(`export PATH="/c/msys64/mingw64/bin:$PATH"`) or cc1plus fails silently. Sandbox
sometimes blocks g++ subprocesses.
```
g++ -std=c++20 -fopenmp -O3 -I../.. -I../gui/STUB Driver.cpp -o Driver_bench
```
(NO stub GL sources — Driver.cpp defines `renderScene` itself; adding `glut.cpp`
double-defines it. The `control reaches end of non-void` warning at ~line 1479 is
pre-existing/harmless.)

**Run modes:** `./Driver_bench <size> <cfg>` = full 480-instance config run
(writes `new_results/output_<size>_cfg<N>_K.csv`). `sweep <size> <cfg> <g0> <g1>
<exam>` = one exam across a group range (fast correctness checks). `tt2_<size>` =
TT2. `dumpdom`/`verifydom` = dominance validation.

**Recommended production preset (all provably sound):**
`RCPSP_DR5=1 RCPSP_DOM_RULE=bp RCPSP_HYBRID=1` (+`RCPSP_UB=1` for the datasheet
bound). NOT leftshift, NOT bidir. Full flag table in `DOMINANCE_NOTES.md`.

**Server:** `lublinido@dsihead.lnx.biu.ac.il`, code at `~/RCPSP_With_Petri_nets`.
Upload from PC (scp/PowerShell — WSL is `/mnt/c/...`, and has broken before; use
relative remote paths like `RCPSP_With_Petri_nets/HOG2/RCPSP/` to avoid tilde
expansion). Build there (g++ 11.5/14), launch in tmux, run j30→j60→j90
size-increasing. **Correctness gate:** after j30, `awk -F, 'NR>1 && $5=="False"'
new_results/output_j30_cfg*.csv` must print nothing.

---

## 6. Constraints (do not violate)

- **Only `HOG2/RCPSP/` is free to edit** (+ the authorized `TemplateAStar.h`
  timeout & TT2 `getcomper1` changes). Everything else under `HOG2/` is library —
  never touch without explicit permission.
- The Excel `סיכום תוצאות - שרת מעבדה.xlsx` is **strictly read-only**.
- Gurobi license files — never delete/move/modify.
- Work on COPIES of binaries (`Driver_<tag>.exe`), never overwrite result CSVs
  (`getNextFilename` handles this).
- The exit-on-wrong-answer in Driver is intentionally commented out ("log wrong
  answers and continue") — grep the CSV for `correct==False`, don't rely on abort.

---

## 7. Owed / TODO (priority order)

1. **All-10-exam correctness sweep** of the production preset before any server run
   (the (6,2) lesson).
2. **Safe-stack j60/j90 rerun** (`DR5=1 DOM_RULE=bp UB=1 HYBRID=1`, NOT leftshift/
   bidir) — expected to flip the cfg8 SR regression positive (throughput restored +
   UB coverage). Current server `_2` files are dominance-only (old binary); `_3`
   files are j30-only new-features.
3. **OFF baselines for j60/j90** to complete the base→dom→new comparison.
4. **Switch datasheet UB → on-the-fly** (SGS/dive/TT2) before writing the paper.
5. Decide `both` vs `bp` from a full run; sweep `HYBRID_T` per config/size.
6. **TTPNR:** measure the DR5-opportunity (separate dominance table keyed on
   `finishedActivitiys`, full active-remaining vectors — NOT a hash change; the hash
   already carries active-remaining). Verify TT2 tie-break earns its place. Check
   whether the hash's omission of `resource_nodes` is sound.

## 8. Results data locations
- Server CSVs copied to `C:\Users\idolu\Documents\לימודים\...\CBS\`: `HICSS\`
  (baseline `_1`), `DR\` (dominance `_2`, new-features `_3`).
- `known_optima.csv` (repo root) — 1334 instances, consolidated optima.
- Source backups: `HOG2/RCPSP/_dr5_backup_*/`, `_perf_backup_*/`.
- Diagnostic binaries: `Driver_ub2.exe` is the latest (everything); older tagged
  ones (`Driver_before/after/hy/tt/...`) are intermediate build points.
