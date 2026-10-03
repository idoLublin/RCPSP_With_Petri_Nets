# Single-Resource Sub-Solver — spec & API (for reuse by other heuristics/branches)

**What it computes.** For a search node, the bound is
`max_k  opt( single-resource-k relaxation of the instance, releases = node's committed starts )`
i.e. relax all resources except one to ∞, solve that (still a real RCPSP) to optimum, take the
max over the K resources. **Admissible**: each single-resource problem has *more* resource than
the real one ⇒ its optimum ≤ the real optimum ⇒ a valid lower bound. **Node-dependent**: the
releases are the node's own starts, so it tightens with depth. Returned as an **absolute makespan
LB** (floor `h` with `bound − g`).

All engines return the **same bound** (they solve the same relaxation to optimum) — they differ
only in **speed** and in whether their internal heuristic is **consistent** (matters for caching).

---

## Selector & enable flags

| env var | meaning |
|---|---|
| `RCPSP_CBS_SINGLERES=1` | turn the bound on in the **CBS** main search (gated: RS-adaptive + shallow/near-incumbent) |
| `RCPSP_TT2_SINGLERES=1` | turn the bound on in the **TT2** main search (gated: RS-adaptive) |
| `RCPSP_SUBSOLVER=ext\|cbs\|tt2` | which **engine** solves the sub-problem (`g_subsolver` 0/1/2). Applies to whichever main solver is running |
| `RCPSP_SUBSOLVE_HCBS_OFF=1` | in the **cbs** engine, use consistent `h=0` instead of the inconsistent HCBS (cache-safe) |
| `RCPSP_CBS_SINGLERES_EXPAND` / `RCPSP_TT2_SINGLERES_EXPAND` | per-sub-solve expansion cap; **0 = uncapped** (default, solve to optimum) |
| `RCPSP_SUBSET_DOM=0` | disable DR5 dominance inside the `ext` mini-solver (default on) |
| RS gate | `RCPSP_CBS_RSADAPT`/`RCPSP_TT2_RSADAPT` (default ON) + `..._RS_THRESH` (0.6): expensive bound fires only at RS ≤ thresh |

---

## The three engines

### `ext` — extension mini-solver (`subsetRcpspLB`, SubsetSolver.h)
Standalone best-first A* that builds schedules by **extension** (place one ready activity at its
earliest feasible time). Features now on:
- **DR5** cutset dominance (per-scheduled-set pointwise-start skyline)
- **DR4** left-shift (inherent — `earliestFeasible` left-justifies)
- **RC** resource/energetic term in the heuristic (was CPM-only; now `max(CP, RC)`)
- **UB** pruning (stops when its LB proves it ≥ the caller's incumbent)
- Heuristic `makespanLB` = `max(CPM est+tail, RC)` → **consistent ⇒ cache-safe**.

### `cbs` — nested CBS engine (`singleResViaCBS_CBS`, RCPSP.h)
Nests the **full CBS** (delaying-alternatives) solver from the node's schedule (seed `start_times`
= releases). Uses the **current CBS config** (DR5/DR4/SetDelay/Hybrid/MDA/UB/Lazy). Isolated by
move-swapping the CBS dominance + MDA tables; recursion-guarded (all resource bounds off inside).
Heuristic = **HCBS** → **inconsistent ⇒ NOT cache-safe** unless `RCPSP_SUBSOLVE_HCBS_OFF=1`
(then `h=0`, consistent). Fastest per call in tests (~1 ms on CBS-main nodes).

### `tt2` — nested TT2 engine (`singleResViaTT2`, RCPSP.h)
Nests the **full TT2** (Petri forward-firing + **batch**) solver from the node's own marking.
Config = DR5+DR4+Batch. Heuristic = `max(CP,RC)` → **consistent ⇒ cache-safe**.

*(A CBS-main using the tt2 engine, or vice-versa, needs a state translation. TT2-node→CBS-state is
easy — `singleResViaCBS_fromTT2<N>` / `singleResViaCBS_TT2dispatch` do it. CBS-state→TT2-marking is
hard (Petri replay) and is intentionally not implemented.)*

---

## API (all in `RCPSP.h` unless noted; include `RCPSP.h`)

```cpp
// CBS-state in, absolute-makespan LB out:
template<short N> double singleResViaCBS_CBS(const RCPSPState_CBS<N>& node);
template<short N> double singleResourceMaxLB_CBS(const RCPSPState_CBS<N>& node);   // ext engine

// TT2-state in:
double singleResViaTT2(const RCPSPState_TT2& node);
double singleResourceMaxLB_TT2(const std::vector<short>& tempUnfinished,
                               const std::array<short,128>& abs_start, short g);   // ext engine
double singleResViaCBS_TT2dispatch(const std::array<short,128>& abs_start, short g); // cbs engine from a TT2 node

// The raw mini-solver (SubsetSolver.h) — solve ANY small multi-resource RCPSP sub-instance:
//   P.cap[R], P.demand[n][R], P.release[n], P.preds[n]; ub = caller incumbent (LLONG_MAX = none)
SubsetResult subsetRcpspLB(const SubsetInstance& P, long expandCap /*0=uncapped*/, long ub = LLONG_MAX);
```

**Reuse tip (other heuristics):** the cheapest reusable unit is `subsetRcpspLB` — hand it any
`SubsetInstance` (residual + releases + caps) and it returns a proven LB with DR5/DR4/RC/UB. For a
"solve a relaxation from this node" heuristic, `singleResViaCBS_CBS` / `singleResViaTT2` are the
templates: copy the save/restore-isolate pattern (move-swap the dominance/MDA tables, disable the
resource bounds, seed the root, run, restore).

## Deadline (important for reuse)
Each sub-solve **honors the parent instance's absolute wall-clock budget**, not a fresh one.
Driver sets `g_instance_deadline = now() + astar_timeout_seconds` per instance; the nested
CBS/TT2 GetPath re-points `astar_timeout_seconds` to the *remaining* budget per call, and the
`ext` solver checks `heur_deadline_hit()` in its loop. On expiry the sub-solve **aborts and
returns the current min-f-on-OPEN** — still `≤ opt`, so the bound stays admissible (just weaker).
If you drive the sub-solver from another heuristic, **set `g_instance_deadline`/`g_instance_deadline_set`
yourself** (or leave `g_instance_deadline_set=false` for unlimited) — otherwise an uncapped nested
solve can run for minutes and blow the search's own timeout.

## `subsolver` CSV column
Every solver row (and the initF rows) ends with a `subsolver` column = `ext`/`cbs`/`tt2`
(`subsolver_name()`), so each run's CSV self-identifies which engine produced it.

## Successor (transposition) cache — `RCPSP_SUBSOLVE_SUCC_CACHE=1` (TT2 sub-solve)
Caches each `(marking, k)`'s RAW children (post-DR4/symmetry, PRE stored-state dominance)
so a repeat expansion in a later sub-solve skips the available-scan + batch enumeration;
order-swap/DR5 are always re-applied fresh → **sound** (verified: 0 makespan disagreements).
`RCPSP_SUCC_CACHE_CAP` bounds entries (0 = unlimited; per-instance cleared). **Local result:
net-negative** (j30 1,1: 16s→119s) — the full-state copy + large-map overhead outweighs the
cheap generation it skips, and it doesn't avoid state construction / DR5. Kept flag-gated,
default off; under evaluation on higher-memory server hardware. Shadow reuse (RCPSP_SHADOW,
`(state,k)`-keyed): ~68% TT2 / ~14% CBS.

## Caching guidance
A cross-call exact-distance cache (memoize sub-problem `opt`, back-fill exact `h*` on the proven
path) is **only sound with a consistent sub-solve heuristic**. Cache-safe engines: **ext**, **tt2**,
and **cbs + `RCPSP_SUBSOLVE_HCBS_OFF=1`**. Plain **cbs** (HCBS) is inconsistent — do **not** cache it.

## CSV telemetry (self-documenting)
`singleResCalls`/`singleResBetter` (CBS), `tt2SingleResCalls`/`tt2SingleResBetter` (TT2),
`heurTimeSec` (total time in HCost), `singleResTimeSec` (time in the sub-solve), `rootH`,
`heuristicLowRS`/`heuristicHighRS`. `g_singleres_viacbs_capped` counts nested solves that returned
no goal (timeout ⇒ "unsolvable sub-problem" signal).

*(Init-run heuristic/time comparison vs control is appended below once measured.)*
