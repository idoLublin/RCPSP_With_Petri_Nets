# Handoff — Lazy A* (CBS) & Multiple-Choice-Set / Batch (TTPNR) vs. prior runs

*Paste this whole file into a fresh chat to continue. Written 2026-07-27.*

## Who / goal
- User: **Ido Lublin**, first author of the SoCS-2026 TTPNR paper. Targeting HICSS/SoCS-2026.
- Repo: `C:\Users\idolu\CLionProjects\RCPSP_With_Petri_nets`, branch `ido`. These are the user's
  **own exact RCPSP solvers** (A*/CBS + TT2/TTPNR Petri encoding), **not** a B&B. Never edit HOG2
  library files without permission; `TemplateAStar.h` only with explicit permission. Everything we
  add is **flag-gated, default-off, reversible, and sound**; every flag used is recorded in the run CSV.
- Benchmark: PSPLIB j30/j60/j90, 480 instances each, 300 s/instance single-core, on lab server
  (`dsihead.lnx.biu.ac.il` login → `ssh dsisco01` compute node; shared home).

Both features below are the user's own ideas (built 2026-07-19). Two independent levers on two
different solvers:

---

## Feature A — Lazy A* in CBS  (`RCPSP_LAZY=1`, `g_use_lazy`)
**File:** `HOG2/RCPSP/LazyAStarCBS.h` (new, based on HOG2 DelayedHeuristicAStar; library untouched).
`Driver.cpp solveRCPSP_CBS_impl` branches between eager `TemplateAStar` (default) and `LazyAStarCBS`.

**What / why.** In CBS, `HCost == compute_h_and_RVS()` is the single most expensive per-node op
(scans all resources for conflicts + builds the branching pool). Eager A* runs it at **insertion**
for every OPEN node (`TemplateAStar.h:650`), but the pool is only needed at **expansion**. On hard
instances OPEN ≫ CLOSED, so most scans are wasted. Lazy inserts children with **h=0** and computes
the real scan only at **pop** (cached on `h_cached`), reinserting if corrected `f` grew.
**Sound:** h=0 is admissible + reinsertion keeps true-f order ⇒ provably optimal (validated 0-wrong).

**Results vs previous run (eager, no lazy).** Two different metrics — don't conflate:
- **Generation speed** (nodes generated/sec, full 480, `_4` lazy vs `_2` no-lazy) — the defensible view:
  | config | j30 | j60 | j90 |
  |---|---|---|---|
  | cfg8 (all features, h>0) | **2.1×** | **3.65×** | **4.67×** |
  | cfg5 | 2.0× | ~2.4× | 2.85× |
  | cfg1 (no MDA/heuristic, h=0) | 1.3× | ~1.35× | 1.4× |
  cfg8 gains MOST because it pays the expensive MDA/conflict scan per node, so deferring it saves most;
  cfg1 was already cheap to generate.
- **Expansion throughput** (earlier single-instance, MISLEADING): cfg1 +48%, cfg8 only +7% — because
  h>0 makes ~53% of pops reinsert, eating the scan savings.
- **Coverage gain < generation gain**: A* is expansion-bound; cfg8's reinserts cap *expansion*
  throughput even though *generation* flies. So new-instances-solved is smaller than 4.7×.

**Caveats / owed.** `_4` also carries dr5s+hybrid+ub, but lazy is the only per-generation-cost reducer
so it's the driver. Correctness **not yet fully validated** (only ~26 instances + (6,2)); needs an
all-10-exam sweep and a clean cfg8 lazy-on/off (with the `useLazy` CSV column) to isolate it before a
paper number. Diagnostics: console `Lazy A*: hcost_evals reinserts` (`g_lazy_hcost_evals`,
`g_lazy_reinserts`).

---

## Feature B — Multiple-Choice-Set / Batch expansion in TTPNR  (`RCPSP_TT2_BATCH=1`, `g_tt2_batch`)
**Where:** flag-gated inside `RCPSP_TT2::GetSuccessors` (RCPSP.h) — NOT a new class (state type / HCost /
hash / GoalTest unchanged). Cap `RCPSP_TT2_BATCH_CAP` (default 200000).

**What / why.** Default TT2 fires **ONE** transition per edge (serial SGS); to start *k* now-available
(τ=0) activities you walk a chain of *k* zero-cost edges + all prefix markings. Batch mode instead
emits per node: {τ>0 single firings} ∪ **{every resource-feasible NON-EMPTY subset of the τ=0-available
activities, started in one edge}** — the "multiple-choice set." Subset feasibility = combined demand ≤
τ=0 resource tokens; enumerated by incremental firing with backtracking. Trades depth for branching.
Singletons are included ⇒ batch graph is a **superset** of single-firing ⇒ **optimal** (Prop.1 preserved).
Best paired with `RCPSP_TT2_DR5=1` (DR5 collapses redundant subset-states, kills any 2^|A0| blowup).

**Results vs previous run (`_2` = DR5, no batch → `_3` = batch+DR5).** Server, 300 s:
| size | DR5 only | batch+DR5 | gain |
|---|---|---|---|
| j30 | 480/480 | 480/480 | tie (batch −23% nodes, −6% time) |
| j60 | 295/480 (61.5%) | **325/480 (67.7%)** | **+30** |
| j90 | 204/480 (42.5%) | **282/480 (58.75%)** | **+78** |

Also: avg time −31%/−37% (j60/j90), total expansions −38%/−42%, avg depth 63→38 and 93→44 (path length
~halved). vs the SoCS paper's published TTPNR (no dominance: J30 98.96, J60 47.71, J90 36.04 %) this is
**+20 pp J60, +22.8 pp J90**. The `depth` CSV column identifies configs: no-batch depth is always
n+3 (33/63/93); batch is lower/variable.

**Sound:** makespans agree with no-batch on ALL 974 instances both solved (j30 480, j60 292, j90 202),
0 mismatches, none below LB. Not strictly per-instance dominant (batch misses 3 on j60, 2 on j90 —
plateau/tie-break noise) but hugely net positive. Corrects an earlier "neutral on tight RS=0.2 / only
helps loose resources" call that was drawn from **j30 only** (little concurrency there); at j60/j90
scale far more activities are simultaneously available, so batching is decisive.
Diagnostics: console `TT2 DR5: pruned/checks/inserts/thinned/comparisons/buckets/maxBucket`.

---

## Baselines these build on (context, already shipped/sound)
- **CBS dominance** (`RCPSP_DR5=1 RCPSP_DOM_RULE=both`, DominanceCBS.h): B&P + DR5S cutset, −79…95 %
  expansions, 0-wrong. `both` beats `bp` on the hard RS=0.2 region. j60 cfg8 was NET −4 coverage
  (table overhead on tiny searches). See DOMINANCE_NOTES.md.
- **TT2 DR5** (`RCPSP_TT2_DR5=1`, DominanceTT2.h): cutset dominance, 10–143× fewer nodes, sound.
  **Full j30 = 480/480 proven-optimal** (even groups 13 & 29, RS=0.2/RF=1.0, that CBS leaves 100 %
  unsolved). This is the baseline batch is measured against.
- **Skyline** (`RCPSP_SKYLINE=1`): search-identical Pareto bucket thinning; **memory win, speed-neutral**.
  Leave off unless memory-bound.

## One-line comparison
- **Lazy** = per-node *cost* reducer for CBS (generation 2–4.7× on the heavy cfg8; coverage gain
  smaller, expansion-bound; correctness sweep still owed).
- **Batch** = *branching-structure* change for TTPNR that, on top of DR5, converts j60 +30 and
  j90 +78 new optimal solves (server-confirmed, sound).

## Open items / TODO
1. **Lazy:** full 10-exam correctness sweep + isolated cfg8 lazy-on/off with `useLazy` column → clean paper number.
2. **Batch:** already server-confirmed; ready for the paper's tables. Always pair with `RCPSP_TT2_DR5=1`.
3. **UB pruning in CBS still cheats** (hardcoded known-optima) — must switch to on-the-fly SGS UB before the paper.
4. TT2 memory: `g_pre` dead, ~7 heap allocs/state — optional optimization, parked.

## Repo pointers
- CBS solver: `HOG2/RCPSP/` (RCPSP.h, RCPSPState.*, AStarCompare.h, Driver.cpp, Globals.h flags).
- TTPNR solver: `RCPSP_TT2::` in RCPSP.h; DominanceTT2.h; readPetri.cpp.
- Flags live in `Globals.h` (g_use_lazy, g_tt2_batch, g_tt2_batch_cap, g_tt2_dr5, g_dom_skyline, g_use_ub…).
- Single-instance test: `./Driver tt2one <size> <group> <exam>`.
- Env selects everything; every flag is written into the run CSV header+row (self-documenting).
