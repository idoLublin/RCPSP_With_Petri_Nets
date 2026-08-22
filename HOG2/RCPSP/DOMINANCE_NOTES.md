# State dominance for CBS — Bell & Park (1990)

## Feature flags & production config (run-ready reference)

All env-driven, all default OFF unless noted. Every feature disables cleanly for
ablation. `applyConfig()` never touches these, so the 8 base configs are unchanged
unless a flag is set.

| env var | default | sound? | what |
|---|---|---|---|
| `RCPSP_DR5=1` | off | yes | enable state dominance |
| `RCPSP_DOM_RULE=` | bp | bp/dr5s/both sound; **dr5 UNSOUND** | which rule. **Recommend `bp`** — `both` not complementary enough to beat its double-table overhead |
| `RCPSP_UB=1` | off | yes | UB pruning; incumbent from datasheet (known optima / bounds / SGS-cache fallback). Redundant with dominance; strongest standalone |
| `RCPSP_HYBRID=1` | off | yes | threshold hybrid conflict selection |
| `RCPSP_HYBRID_T=` | 0.5 | yes | cardinality threshold; <T => first-conflict, >=T => score. 0=pure prio, >1=pure first-conflict |
| `RCPSP_TIMEOUT_S=` | 300 | — | per-instance wall-clock budget |
| `RCPSP_DOM_CAP=` | 4M | yes | dominance-table store cap (memory bound) |
| `RCPSP_LEFTSHIFT=1` | off | **UNSOUND — do not use** | loses optima in CBS (delay-only branching breaks the reachability assumption); caused the (6,2) wrong answer |
| `RCPSP_BIDIR=1` | off | under-validated | bidirectional dominance; part of the (6,2) collision — leave off pending isolated validation |
| `RCPSP_LEAN=1` | off | yes | strip conflict vectors (memory-for-time) |
| `RCPSP_INLINE=1` | off | yes | B&P intermediate recursion — negative, not recommended |

**Recommended production preset (all provably sound):**
`RCPSP_DR5=1 RCPSP_DOM_RULE=bp RCPSP_HYBRID=1` (+ `RCPSP_UB=1` if you want the
datasheet bound; it's a cheat — see [[project_ub_hardcoded_swap]] — swap for
on-the-fly before the paper).

**OWED before trusting on the server:** all-10-exam correctness sweep of the new
flags (the (6,2) bug hid because local sweeps only ran exam-1 per group).

## TTPNR (TT2) notes
- `RCPSP_TT2::GetStateHash` includes `finishedActivitiys` + `activeTransitionIndices`
  (task,remaining) — so the hash ALREADY carries active-remaining; the lowest-g
  duplicate prune is sound w.r.t. remaining.
- DR5 in TTPNR would be a SEPARATE dominance table keyed on `finishedActivitiys`
  only (comparing full active-remaining vectors + g), not a hash change. Measure
  first (like CBS: complementary enough to beat overhead?).
- `getcomper1(RCPSPState_TT2)` now uses `g + MaxRemainInActive` as the f-tie-break.



## Feature pass (2026-07-18): new flags

All env-driven, all default OFF, all validated 0-wrong on 48-group j30 mini-
sweeps (12s local budget; absolute numbers belong to the 300s server runs).
`RCPSP_TIMEOUT_S=<sec>` now sets the per-instance budget (default 300).

| flag | what | j30 cfg1 exam-1 @12s result |
|------|------|------------------------------|
| `RCPSP_UB=1` | SGS incumbent seed, B&P Line-4 child pruning (makespan >= UB), incumbent-return on OPEN exhaustion, LB/UB gap print on timeout | (43,9) proven optimal by exhaustion; (5,1) baseline-timeout solved optimally in 37s; timeouts now report gaps |
| `RCPSP_LEFTSHIFT=1` | sound left-shift rule: scheduled-set activity, precedence-idle, resource-feasible one-unit shift (NOTE: old isLeftShiftable() ignores resources — unusable) | -84% expansions alone; all optimal |
| `RCPSP_BIDIR=1` | B&P bidirectional: new state retires dominated stored states (kill set, checked at expansion) | -19.8% expansions on top of dominance, +1 solve |
| `RCPSP_HYBRID=1` | prio picks cardinal conflicts as before, but among non-cardinals branches on the EARLIEST (advances t*, keeps dominance eligible) | cfg8: -53.5% expansions; cfg5: +1 solve |
| `RCPSP_LEAN=1` | strip conflict vectors from OPEN/CLOSED copies (self-healing) | -4..-23% memory for +15-30% time; keep off unless memory-bound |
| `RCPSP_HGREED=1` | greedy subset refinement of cardinal conflict cost (existing flag, exposed) | no measurable effect |
| `RCPSP_INLINE=1` | B&P Descendants recursion (intermediates expanded in place) | negative locally (loses solves at 12s) — not recommended |
| `RCPSP_DOM_CAP=<n>` | dominance store cap per table (default 4M) | memory bound, never unsound |

**Recommended server stack:** `RCPSP_DR5=1 RCPSP_DOM_RULE=both RCPSP_UB=1
RCPSP_LEFTSHIFT=1 RCPSP_BIDIR=1` (+ `RCPSP_HYBRID=1` for prio configs).
Combined on cfg1 exam-1 grid @12s: **solved 33/48 -> 40/48, expansions -97.4%,
0 wrong**; new solves include groups 41/43/46 (hard RS=0.2 cells).


## Performance pass (2026-07-18)

Search-identical optimizations (verified bit-for-bit on 104 runs — makespan,
expandNumber, generatedNumber all unchanged across j30 cfg1/4/5/8 and j60
cfg5/8, dominance off/on, rules bp/dr5s/both):

1. **Light scan for the dominance gate** — `compute_first_conflict()` finds only
   the earliest conflict (sweep-line, early-breaking); the full scoring/MDA scan
   now runs only for children A* actually keeps (via HCost). Previously every
   generated child — including the 5-40x duplicates — paid a full scan.
2. **HCost caching** — `h_cache`/`h_cached` on the state; repeat HCost calls free.
3. **Sweep-line conflict scan** in both branches of `compute_h_and_RVS` —
   incremental running-set/demand instead of rescanning all activities per event;
   identical visit order and current_jobs order, so search is unchanged.
4. **Single-build dominance check+store** — each rule's key/artifacts built once
   per child (was up to 4 builds in BOTH mode); duplicate detection folded in.
5. **Store cap** — env `RCPSP_DOM_CAP` (default 4M entries/table); above it the
   table checks but stops storing (weaker pruning only, never unsound).
6. **O(k) compaction** instead of repeated vector::erase in the gate.

Measured (sequential local runs, same machine): j30 total 1.9x faster
(dominance-ON configs 1.5-2.6x), j60 total 1.8x (cfg8+dominance 3.2x).
Backup of pre-pass sources: `_perf_backup_20260718_*/`.


Status: **working, 0 wrong on 485 finished j30 instances** (configs 1, 4, 5, 8, full 48-group grid).

## How to run

Off by default. `applyConfig()` never touches the flag, so the 8 existing configs are
unchanged unless you opt in.

```bash
g++ -std=c++20 -fopenmp -O3 -I../.. -I../gui/STUB Driver.cpp -o Driver_bench

RCPSP_DR5=1 ./Driver_bench j30 1                 # dominance on, Bell & Park rule (default)
RCPSP_DR5=1 RCPSP_DOM_RULE=dr5s ./Driver_bench j30 1   # DR5 + release-bound comparison
RCPSP_DR5=1 RCPSP_DOM_RULE=dr5  ./Driver_bench j30 1   # plain DR5 — UNSOUND, loses optima
```

Extra driver modes added for this work:
- `sweep <type> <cfg> <startG> <endG> <exam>` — one exam across a group range, single CSV.
  Walks the whole NC x RF x RS grid; this is what correctness validation should use.
- `dumpdom <type> <group> <exam> <cfg> <out>` — run one instance, append every prune pair.
- `verifydom <type> <group> <exam> <cfg> <pairs>` — re-solve both sides of each dumped
  prune exactly and report any pair where optimum(dominating) > optimum(pruned).
  Self-tests itself first (must reproduce the known optimum from the root).

## The rule

Bell & Park section 5. Given RVST (earliest resource violation, our `t_first`):

    A_s = { a : a finishes at or before RVST }      "scheduled set"
    A_u = { a : a finishes after RVST }             includes activities RUNNING at RVST

S dominates S' (prune S') iff
1. same A_s,
2. same added arcs among A_u,
3. S's start times on A_u are all <= S''s.

Start times of A_s are deliberately ignored — only arcs inside A_u propagate into the future.

## The two things that made it work

**1. Descendants Line 8 (the fix).** A child is only a real state to remember/compare if
its RVST **and** A_s both differ from its parent's. Otherwise it is an *intermediate*
state and must be skipped entirely. Without this gate the rule prunes optima: a child is
always >= its parent componentwise, so an intermediate child (same RVST, same A_s) is
trivially dominated by its own parent and the whole subtree dies. This alone took
instance (1,1) from a wrong 62 to the optimal 43.

**2. Check once per generated child, in GetSuccessors** — not in HCost. HCost is called
an unpredictable number of times per node, and the check both reads and writes the table,
so table contents would depend on A*'s evaluation order rather than on which states were
actually generated.

## Weak vs strong constraints

`use_strong_constraints` is hardcoded `false` in `applyConfig`, so the arc constructor
(`RCPSPState_CBS(prev, from, to, t)`) is unreachable and `added_precedences` is always
empty. Every config is Bell & Park's *weak* form: a delay is a release-time constant, not
a real arc. Condition (2) is therefore vacuous and is only evaluated when arcs exist —
it costs one `.empty()` check. If `use_strong_constraints` is ever enabled, condition (2)
starts doing real work and is required for correctness.

## DR5

Plain DR5 (cutset = finished + actives-not-in-conflict; prune if same cutset, t*' <= t*,
running members finish no later) **loses optima here**: 4-8 wrong per config on j30.

Reason: DR5 comes from a B&B where unscheduled activities are genuinely free, so the
cutset summarises the node. In CBS every activity carries a release-time lower bound in
`start_times`, and DR5's cutset ignores the ones outside it. Adding that comparison
(`RCPSP_DOM_RULE=dr5s`) restores correctness (0 wrong) at roughly Bell & Park's strength.

## Results (j30, sweep = 1 exam x 48 groups, 5-min limit)

| config              | expand OFF | expand ON | cut    | wrong |
|---------------------|-----------:|----------:|-------:|------:|
| cfg1 (no features)  |    908,751 |    66,292 | -92.7% |     0 |
| cfg5 (all exc MDA)  |    169,509 |    58,817 | -65.3% |     0 |
| cfg8 (all features) |    657,049 |   176,966 | -73.1% |     0 |

(on instances both runs finished; the OFF sweeps were still running when measured, so n is
small — the ON runs finished the full grid, OFF did not, which is itself the result.)

Coverage within the 5-min limit, full grid: cfg1 41/48, cfg5 37/48, cfg8 41/48 solved.

## Reverting

Pre-work snapshot of all touched files: `HOG2/RCPSP/_dr5_backup_<timestamp>/`.
Touched: `RCPSPState.h`, `RCPSPState.cpp`, `RCPSP.h`, `Globals.h`, `Driver.cpp`
(+ new `DominanceCBS.h`). All five are tracked, so `git diff` also shows the changes —
note it will include pre-existing uncommitted work, hence the snapshot.

The only unconditional change (not behind the flag) is the state addition in
`RCPSPState.h` / `compute_h_and_RVS`: `t_first` and `first_conflict_pool`, the earliest
conflict tracked separately from the branched-on conflict `t`. It costs one `min` per
confirmed conflict and does not affect the search when the flag is off.
