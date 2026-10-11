# TT2 parallel (HDA*) — measurements and Phase 0 inventory

Working notes for parallelising the forward TT2 A* (`solveRCPSP_TT2`). The plan is
hash-distributed A* (HDA*). States are assigned to worker threads by
`TT2DominanceTable::MakeKey` (scheduled set = finished ∪ active). Every DR5
comparison and every duplicate stays inside one worker, because two states can only
dominate or duplicate each other when they share that key. The global lower bound
is the MIN over workers of their open-list f_min, plus messages still in flight.
Termination: UB ≤ that LB.

## 1. Measurements (j30, all 480; `RCPSP_TT2_DR5=1 RCPSP_TT2_DR4=1 RCPSP_TT2_BATCH=1`, 20 s)

New env flags. Both are off by default, and the search is unchanged unless noted:

| flag | what |
|---|---|
| `RCPSP_TT2_DUPPROBE=1` | classifies every child reaching TemplateAStar against its open/closed list (one extra lookup per child). Prints `TT2 DUP:` |
| `RCPSP_TT2_KILL=1` | records DR5 entries retired by skyline thinning as DEAD; counts expansions of dead nodes. Search unchanged. Prints `TT2 KILL:` |
| `RCPSP_TT2_KILL=2` | as 1, and a dead node emits no children (CBS has the same idea as `is_killed`) |

**Duplicate census.** Of 9,093,948 children reaching A*:

| outcome | count |
|---|---|
| new | 8,868,977 |
| on OPEN, lower g | 224,971 (2.5%) |
| on OPEN, not lower | 0 |
| on CLOSED (any) | 0 |

DR5 runs first, inside `GetSuccessors`, and its relation is non-strict. So every
duplicate with g ≥ the stored g is pruned by the table before HOG2's lookup sees it.
HOG2's duplicate check never prunes. Its only remaining job is decrease-key for the
2.5% that arrive with a better g.

**Hash cost.** `GetStateHash` is 4.6% of A* time (3.43 s of 73.75 s). This is the
ceiling for what merging the A* index into the dominance table could save in time.
Memory would also drop.

**Dead-node skip (`RCPSP_TT2_KILL=2` vs off).** 480/480 solved by both, 0 wrong makespans
against `known_optima.csv`.

| | off | kill=2 |
|---|---|---|
| A* pops (TemplateAStar `expandNumber`) | 7,156,039 | 7,010,367 (−2.0%) |
| pops that generated children | 7,156,039 | 6,207,809 (−13.3%) |
| total time | 73.8 s | 73.4 s (−0.5%) |

Dead nodes are 11–13% of pops. Their children were already almost all pruned by
DR5, so skipping them saves little work on j30. j60 sample: see section 4.

## 2. Phase 0: what must change before threads (forward TT2 path only)

These are the globals written on the TT2 search path: `RCPSP.h` 1417–2370,
`RCPSPState.cpp` TT2 ctors, the heuristic helpers, `DominanceTT2.h`, `TT2NodeBound.h`,
`ThetaTree.h` and `RootBounds.h`. Found with a regex scan, so treat the list as a
lower bound.

| class | globals | fix |
|---|---|---|
| per-instance telemetry (~45) | `g_prof_*`, `g_tt2_{dr4_pruned,sym_pruned,immsel_fired,freefire_fired,theta_better,singleres_*,hierrs_*,wait_*,dead_expanded,dup}`, `g_heur_time_sec/calls`, `g_root_h`, `g_ub_pruned`, `g_orderswap_cand`, `g_hcache_*`, `g_succ_*`, `g_shadow_*` | `thread_local`, summed after join |
| per-search tables | `get_tt2_dominance_table()` (static t0/t1), `get_tt2_cache()`, `g_cbs_hierrs_cache` | one per worker (HDA* owns them per partition) |
| hot-path function statics | `resDemandMatrix()` slot cache (`RCPSPState.cpp`), `actDemandSlot()` (`RCPSPState.cpp`) | `thread_local` (they are refilled per instance) |
| flags flipped during search | `g_in_subsolve`, `g_tt2_theta`, `g_tt2_singleres`, `g_subsolve_k`, `g_singleres_viacbs_capped` (sub-solve save/restore, `RCPSP.h` ~2037–2071) | `thread_local`, or disallow SINGLERES/HIERRS sub-solves in parallel mode |
| incumbent / UB | `g_incumbent` | `std::atomic<short>` with CAS-min |
| root-only | `initialstatename`, `finalstatename` (root ctor only) | none: built once before workers start |
| config read during search | all `g_use_*` / `g_tt2_*` flags set from env, `petri`, `RCPSPex`, `resource_info`, `upstream`/`downstream` | none: read-only once workers start. Must NOT become `thread_local` (workers would see defaults) |
| HOG2 | `timeout` and `astar_timeout_seconds` are namespace globals in `TemplateAStar.h` | HDA* workers run their own loop, not TemplateAStar, so no HOG2 change is needed |

`static const bool x = [](){ getenv... }()` function statics are safe: C++11
guarantees thread-safe one-time init.

## 3. HDA* worker design (Phase 1, not started)

- Each worker owns: an open heap, a node store, a `TT2DominanceTable` used as its only
  index, and an inbox.
- The dominance-table entry stores the node id:
  - an exact match with a better g → decrease-key;
  - dominated → prune;
  - thinned → node dead (skip at pop).

  This replaces HOG2's hash index (section 1 shows it only does decrease-key).
- Owner of a state = `mix(MakeKey(s)) % N`. h is computed at the owner, after the
  DR5 check.
- Shared atomic UB. Each worker publishes its f_min. A sent/received counter detects
  messages in flight.

## 4. j60 sample

Running (exams 1–2 of every group, 30 s, both configs). Results will be added here.
