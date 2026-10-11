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

## 3. HDA* as built (`HDAStarTT2.h`, mode `tt2hda_<size>`)

HOG2 has no HDA*, neither this copy nor upstream (June 2026). It has `ParallelIDAStar`
and upstream `ParallelAStarIDAStar`, which run IDA* below an A* frontier with no
duplicate detection and no DR5, so they don't fit TT2. HDA* is therefore assembled
from HOG2 parts, with **no HOG2 file modified**:

- **Per worker:** HOG2's `TemplateAStar`, unchanged, with `SetReopenNodes(true)`.
- **Messages:** HOG2's `SharedQueue` (`utils/SharedQueue.h`), one inbox per worker,
  carrying `shared_ptr` batches.
- **Owner** = hash of `MakeKey`. Inside workers `RCPSP_TT2::GetSuccessors` skips DR5
  (`g_tt2_hda_worker`). The worker env routes each child: owned children go through
  DR5 on the worker's own `thread_local` table into its TemplateAStar; the rest are
  buffered per destination (`RCPSP_HDA_BATCH`, default 64; flushed every 16
  expansions and before idling or stopping).
- **Received states** pass DR5 at the owner, then enter the public `openClosedList`
  with the same updates TemplateAStar makes for its own children: new →
  `AddOpenNode`; lower g → decrease-key on OPEN, `Reopen` on CLOSED.
- **Termination:** shared atomic UB, plus `work` = busy workers + batches in flight.
  `work == 0` means UB is optimal. On the deadline every worker drains its inbox,
  and LB = min over workers of the open-list f_min.
- **Run:** `RCPSP_HDA_THREADS=4 RCPSP_TT2_DR5=1 RCPSP_TT2_DR4=1 RCPSP_TT2_BATCH=1
  RCPSP_TT2_KILL=2 ./Driver tt2hda_j60`
- **Refused flags** (state not per-thread yet): SINGLERES, HIERRS, sub-solve caches,
  TT2_TT, root bounds, DUPPROBE, reverse.

**Build with jemalloc.** glibc malloc is the bottleneck when one thread frees states
another allocated. On j30 13:1 with 4 threads it took 8.6 s, against 3.6 s with
jemalloc. jemalloc also makes the serial solver ~15% faster.

    g++ -std=c++20 -fopenmp -O3 -I../.. -I../gui/STUB Driver.cpp -o Driver -l:libjemalloc.so.2

(or `LD_PRELOAD=/lib/x86_64-linux-gnu/libjemalloc.so.2` on an existing binary)

**Bug found on the way (also affects serial; fix behind `RCPSP_TT2_HDZFIX`, forced on in
tt2hda).** TemplateAStar's decrease-key and reopen replace a node's stored state with
the new copy, whose `h` member was never computed (0). That node's zero-delay
children then inherit h = 0 (f = g) through the `predessesor_h` shortcut. This is
admissible but weak. Serially it changes little (j30 with the fix: +0.7% expansions,
480/480 optimal). In HDA* it produced LBs far below the root h. It stays off by default
so existing serial results reproduce.

## 4. j60 sample (exams 1–2 of every group = 96 instances, 30 s, same flags)

| | off | kill=2 |
|---|---|---|
| solved | 70 | 70 (same set) |
| wrong vs `known_optima.csv` | 0 / 70 | 0 / 70 |
| pops on the 70 solved | 3,637,277 | 3,450,120 (−5.1%) |
| time on the 70 solved | 77.5 s | 82.2 s (+6.1%) |
| LB (maxF) at timeout, 26 unsolved | — | higher on 1, lower on 1, equal on 24 |
| peak memory, j60 41:1 alone | 2.78 GB | 2.84 GB (+2%; 553k dead records) |

Four runs shared one machine, so the times are noisy. An OOM kill seen during the
sweep came from four ~4 GB processes sharing the limit; the baseline also peaks at
about 4 GB there. After a re-run every row is present.

**Verdict.** The dead-node skip is sound: 0 wrong across 576 instances. But as a
side set next to TemplateAStar it does not pay off serially, because building and
looking up the exact record costs about what the skipped expansions save. The idea
belongs in the HDA* worker. There the dominance entry carries the node id, so marking
a node dead is one bit with no extra lookup or memory. Leave `RCPSP_TT2_KILL` off in
production runs; keep it as the measurement hook.

## 5. HDA* results (4 cores, jemalloc; DR5+DR4+BATCH)

Serial = `tt2_<size>` with jemalloc. All makespans were checked against
`known_optima.csv`. No reported LB exceeded a known optimum, and no UB fell below one.

| | j30 (480, 60 s) | j60 sample (96, 30 s) |
|---|---|---|
| solved: serial / HDA* 4 threads | 480 / 480 | 71 / **75** (+(21,2) (37,1) (37,2) (41,1); none lost) |
| wrong makespans | 0 | 0 |
| speed-up, all commonly solved | 2.56× (with KILL=2), 2.03× (without) | 1.12–1.37× |
| speed-up, instances > 1 s serial | 2.77× (KILL=2) | 1.43–1.62× |
| extra expansions vs serial | +8% (KILL=2) | +69–89% |
| LB on instances neither solves (21) | — | higher than serial on 9, equal on 12, lower on 0 |
| 2 threads (j30) | 1.54× overall, 480/480 | — |
| 1 thread | identical expansions to serial | — |
| j30 with a 1 s timeout (LB/UB check) | 476 proven, 4 timed out, all bounds valid | — |

The limit on j60 is search overhead, not speed per node. Without an incumbent, workers
expand nodes above the global f_min, and DR5 sees states out of order, so dominated
states are often expanded before their dominator arrives. Things to try next, all
cheap:
1. seed and share the UB (`RCPSP_TT2_UB=1` already prunes at the generator; in HDA*
   it also bounds the workers that run ahead);
2. a tighter batch / flush cadence;
3. a larger j60/j90 run on the server.

## 6. With compact states (`RCPSP_TT2_COMPACT=1`, merged from `ido` 37c3ea0)

Merge commit `0b4fdfb` also carries the HDA* integration. tt2hda accepts compact;
`tt2CompactPrepare` runs on the main thread before the workers start, the rebuild only
reads that table, and its counters are `thread_local`. States routed between workers
stay stripped.

Validation: serial j30 with compact 0/1/2 gives identical expansions (7,156,039), all
optimal. HDA* j30 with compact 1/2 is 480/480 optimal, and check mode (=2) found no
mismatches.

| 4 threads, KILL=2 | serial | HDA* | HDA* + compact |
|---|---|---|---|
| j30 speed-up (all 480) | 1× | 2.67× | **3.02×** |
| j30 peak memory | — | 909 MB | **539 MB** |
| j60 sample solved (96, 30 s) | 71 | 75 | **75** |
| j60 speed-up, 71 commonly solved | 1× | 1.42× | **1.97×** |
| j60 speed-up, > 1 s serial | 1× | 1.92× | **2.53×** |
| j60 peak memory | 5.0 GB | 11.8 GB | **5.8 GB** |
| j60 LB where neither solves (21) | — | higher on 9 | higher on 10, never lower |

Compact halves HDA*'s memory (back to serial level) and makes it ~19% faster: smaller
states mean cheaper routing and fewer allocations, and allocation is HDA*'s bottleneck.

## 7. HOG2 duplicate detection = hash identity

`AStarOpenClosed::Lookup` is `unordered_map<uint64_t,uint64_t>::find(GetStateHash(s))`.
States are never compared, so the 64-bit hash IS the node identity. Under
TemplateAStar the map can't be dropped: decrease-key and reopen find nodes through it.
A collision silently merges two states, so any cheaper hash must stay full 64-bit
quality. DR5 stays non-strict (≥): it is sound because a pruned state is never stored,
and a strict rule would only hand exact duplicates to the hash check.

## 8. Serial speed, same search (TT2 and CBS)

None of these changes alters the search. Each was checked against the same configuration
without it: same solved / makespan / expanded / generated and every DR5 counter, on all
480 j30.

| Change | Where | Switch |
|---|---|---|
| Batch subset children are built once (moved into `neighbors`, no copy, no `std::function`) | `RCPSP.h` | always |
| The firing constructor reserves exactly the room its appends need | `RCPSPState.cpp` | always |
| **DR5 is tested before a child is built** | `RCPSP.h`, `DominanceTT2.h` | `RCPSP_TT2_PRECHECK=1` (2 = check mode) |
| CBS `propagateInc`: the earliest-start scan stops at the last blocked unit | `RootBounds.h` | always |

**Pre-check.** About 72% of TT2 children used to be built in full (token lists and all)
and then dropped by DR5. DR5 reads only the scheduled set, g, and the release of each
active activity. For "fire t after delay d" all three come straight from the parent:
- g' = g + d;
- the scheduled set gains t;
- an active i stays active iff rem_i > d, keeping its release g + rem_i;
- t is active with release g' + p_t if p_t > 0.

So time-advancing children are tested on that view and only the survivors are built.
A batch subset child is still built, because its extensions start from it, but it is
kept only if it survives. The tests run in the order the old DR5 loop ran them, so every
decision is the same.
- Off where that cannot hold: HDA* workers, order-swap, the sub-solve successor
  cache, the wait model, and the prune observer.
- Validated with DR5+DR4+BATCH, DR5+DR4, +KILL=2, +COMPACT=1 and DR5+SYM; check mode
  (=2) found no view mismatch.

Instruction counts (callgrind, exact), TT2 DR5+DR4+BATCH on j30 13:2, 366,354
expansions in every build:

| Build | Instructions | |
|---|---|---|
| before (a250df9) | 15.69 G | |
| + batch move / reserves (4eeaedf) | 14.96 G | −4.7% |
| + `RCPSP_TT2_PRECHECK=1` | 11.18 G | **−28.7%** |

Wall time is noisier here; j30 13:1 (3 runs each) went from 15.5 s to 14.1 s.
CBS (cfg8 + `PROP_FAST=1 PROP_INC=1`, j30 5:1): 28.48 G → 28.11 G (−1.3%), same 171,097
expansions; `RCPSP_PROP_INC=2` found no mismatch on all 480 j30.

After the pre-check the largest TT2 items are DR5 itself (17%; the children that
survive scan their bucket twice), the availability scan (10%), the open list, and
malloc/free (~20%). CBS is dominated by `propagateInc` (~46%), with the cost spread
over its loops.

**Allocator.** This needs no code change. Measured on the final build (pre-check on,
j30 13:1, 3 runs each, same expansions):

| | time | peak memory |
|---|---|---|
| glibc malloc | 13.5 s | 1.04 GB |
| glibc, `GLIBC_TUNABLES=glibc.malloc.tcache_count=65535` | 13.1 s | 1.04 GB |
| jemalloc | **12.3 s (−9%)** | **0.96 GB** |

Before the pre-check jemalloc gave −19% on TT2 (and −10% on CBS 5:1). The pre-check
removed most of the allocations it was saving. For HDA* jemalloc is still essential
(§3). To use it, where `libjemalloc2` is installed (`ldconfig -p | grep jemalloc`):

    LD_PRELOAD=/lib/x86_64-linux-gnu/libjemalloc.so.2 ./Driver ...

or link with `-l:libjemalloc.so.2`. `-march=native` gave no measurable gain.
