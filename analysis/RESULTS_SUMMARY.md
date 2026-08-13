# RCPSP with Petri Nets — Final Experimental Results Summary

Self-contained summary of all final benchmark results for the project book and
presentation. Generated 2026-08-13 from the master results CSV
(`analysis/output/master_results.csv`); regenerable end-to-end via the scripts
in `analysis/` (see `analysis/README.md`).

## 1. What was measured

The solver finds optimal makespans for RCPSP (Resource-Constrained Project
Scheduling Problem) instances by running A* over a Petri-net formulation of the
problem. Two timed formulations are compared:

- **TT (TTPN)** — Timed-Transition Petri Net; tokens carry absolute
  availability dates.
- **TT2 (TTPNR)** — the improved formulation with **relative-delay tokens** and
  a finished-transition bitset, which makes state identity time-shift-invariant
  so equivalent states merge in the closed list.

Admissible heuristics evaluated (all include the `max(CP, h_res)` floor):
**CP** (critical path), **LBCC**, **LBIP0**, **LBMAX** (= max(CP, LB_RC)),
**LBCS** (cutset-based), **LBER** (evaluated at delta setting d3). Dominance
pruning: for TT2, "+dom" = cutset dominance rule + dominance-pop + SGS
upper-bound pruning, and "+dom+dr4" additionally enables the DR4
delayed-start dominance rule (in the recorded +dom+dr4 runs the UB-pruning
counter is zero — configuration to confirm); for TT, "+dr" = the Liu et al.
dominance rules DR1/DR2/DR5.

## 2. Experimental protocol

- Benchmarks: PSPLIB **j30, j60, j90** — 480 instances each (48 parameter
  groups × 10 instances; full factorial of NC ∈ {1.5,1.8,2.1},
  RF ∈ {0.25,0.5,0.75,1.0}, RS ∈ {0.2,0.5,0.7,1.0}, RS varying fastest).
- **300 s per-instance time limit**, sequential, **single core** of an Intel
  Xeon Gold 6248R @ 3.00 GHz (university compute). One solver process per
  parameter group. Solved makespans validated against published optima
  (`data/j30opt.sm`) / lower-bound files.
- TT results on the university machine: full 5-heuristic × 3-set sweeps with
  the Liu dominance rules ("+dr"), plus plain-TT (no rules) runs: all 5
  heuristics on j30, CP/LBCC/LBCS on j60, and LBCC/LBCS on j90. Plain-TT
  makespans were validated against TT2's proven optima on all commonly
  solved instances (exact match).

## 3. Headline results — instances solved (of 480), university machine

| Configuration | j30 | j60 | j90 |
|---|---:|---:|---:|
| TT CP (no rules) | 391 | 192 | — |
| TT LBCC (no rules) | 388 | 266 | **233** |
| TT LBIP0 (no rules) | 391 | — | — |
| TT LBMAX (no rules) | 391 | — | — |
| TT LBCS (no rules) | 396 | 189 | 56 |
| TT CP +dr | 392 | 277 | 161 |
| TT LBCC +dr | 391 | 253 | 135 |
| TT LBIP0 +dr | 391 | 261 | 140 |
| TT LBMAX +dr | 392 | 277 | **161** |
| TT LBCS +dr | **402** | **277** | 158 |
| TT LBER (d3) | 356 | — | — |
| TT2 CP | 476 | 241 | 182 |
| TT2 LBCC | 476 | 236 | 179 |
| TT2 LBIP0 | 476 | 241 | 183 |
| TT2 LBMAX | 476 | 241 | 182 |
| TT2 LBCS | **477** | 240 | 181 |
| TT2 LBER (d3) | **477** | **242** | not run |
| TT2 LBCS +dom | 480 | 299 | 208 |
| TT2 LBMAX +dom | 480 | 301 | 209 |
| TT2 LBCC +dom+dr4 | 480 | 355 | 293 |
| TT2 LBCS +dom+dr4 | **480** | 357 | **295** |
| TT2 LBMAX +dom+dr4 | **480** | **359** | **295** |

All table entries are university-machine measurements (no placeholders
remain). Study bests per set: **480/480 (100%) on j30, 359/480 (75%) on
j60, 295/480 (61%) on j90 — all by TT2 +dom+dr4**. (LBER on TT was also
measured: 356 at d3 / 345 at d4 on j30 — uncompetitive on that model.
Plain TT LBCC's 233 on j90, remarkable in its own right (§5), held the j90
record until DR4 landed.)

Average time / expanded nodes on solved instances (per set, selected):

| Configuration | j30 time | j30 nodes | j60 time | j60 nodes | j90 time | j90 nodes |
|---|---:|---:|---:|---:|---:|---:|
| TT2 CP | 4.78 s | 642,996 | 16.03 s | 1,246,458 | 8.75 s | 402,611 |
| TT2 LBCS | 4.98 s | 637,239 | 17.98 s | 1,150,488 | 8.98 s | 360,773 |
| TT2 LBER (d3) | 5.46 s | 892,346 | 14.83 s | 1,369,588 | — | — |
| TT2 LBCS +dom | 0.37 s | 31,363 | 22.10 s | 1,109,477 | 14.67 s | 337,815 |
| TT2 LBMAX +dom | 0.36 s | 32,953 | 21.10 s | 1,205,343 | 14.20 s | 392,910 |
| TT2 LBCS +dom+dr4 | **0.15 s** | **15,329** | 13.76 s | 956,989 | **5.83 s** | **244,877** |

(Averages are over each configuration's solved set; +dom rows on j60/j90 reach
harder instances, which is why their averages rise while solving more.)

## 3b. The project's progression — j30 as the yardstick

Every milestone measured on the same benchmark (j30, 480 instances), same
300 s protocol, same heuristic family; rows 3–7 on the same university
machine with LBCS, so times are directly comparable. "Sweep time" = wall
time to run all 480 instances sequentially (timeouts counted at 300 s).

| Milestone | Solved | Avg time on solved | Full-sweep time | vs final |
|---|---:|---:|---:|---:|
| TPPN, previous project (10 min, laptop) | 219 (46%) | — | — | — |
| TTPN, previous project (10 min, laptop) | 354 (74%) | — | — | — |
| TT plain (this project) | 396 (82%) | 12.96 s | 8.4 h | 421× slower |
| TT + Liu rules | 402 (84%) | 8.62 s | 7.5 h | 373× slower |
| TT2 (relative-delay states) | 477 (99%) | 4.98 s | 55 min | 45× slower |
| TT2 + cutset/UB pruning (+dom) | 480 (100%) | 0.37 s | 3.0 min | 2.5× slower |
| **TT2 + dom + DR4 (final)** | **480 (100%)** | **0.15 s** | **1.2 min** | — |

Reading: over the two project generations, j30 went from 46% solved in a
10-minute-per-instance budget to **100% solved with the entire benchmark
completed in 1.2 minutes** — within this project alone, a 421× reduction in
total compute and an ~86× per-instance speedup over the plain TT baseline
(12.96 → 0.15 s), with +84 more instances solved. Each step contributed:
the model change (TT→TT2) bought +75–81 instances and ~9× sweep time; the
cutset rules bought the last 3 and ~18×; DR4 another ~2.5×. The same
ordering holds on j60 (189 → 277 → 240 → 301 → **359**) and j90
(56 → 158 → 181 → 209 → **295**), where the gains grow instead of
saturating.

## 4. Dominance-pruning ablation (with vs without, same heuristic)

| Set | Config | Solved (base, Δ) | Avg time (base) | Avg nodes (base) |
|---|---|---|---|---|
| j30 | LBCS +dom | 480 (477, **+3**) | 0.37 s (4.98) | 31,363 (637,239) |
| j30 | LBMAX +dom | 480 (476, **+4**) | 0.36 s (4.80) | 32,953 (642,996) |
| j60 | LBCS +dom | 299 (240, **+59**) | 22.10 s (17.98) | 1,109,477 (1,150,488) |
| j60 | LBMAX +dom | 301 (241, **+60**) | 21.10 s (16.21) | 1,205,343 (1,246,458) |
| j90 | LBCS +dom | 208 (181, **+27**) | 14.67 s (8.98) | 337,815 (360,773) |
| j90 | LBMAX +dom | 209 (182, **+27**) | 14.20 s (8.36) | 392,910 (402,611) |

On j30, dominance pruning **closes the benchmark**: 480/480, every instance in
under 26 s, ~13× faster and ~20× fewer nodes on average.

**Adding DR4 on top of +dom** (delta shown vs the +dom run of the same
heuristic):

| Set | Config | Solved (+dom, Δ) | Avg time (+dom) | Avg nodes (+dom) |
|---|---|---|---|---|
| j30 | TT2 LBCS +dom+dr4 | 480 (480, +0) | 0.15 s (0.37) | 15,329 (31,363) |
| j30 | TT2 LBMAX +dom+dr4 | 480 (480, +0) | 0.14 s (0.36) | 16,035 (32,953) |
| j60 | TT2 LBCS +dom+dr4 | 357 (299, **+58**) | 13.76 s (22.10) | 956,989 (1,109,477) |
| j60 | TT2 LBMAX +dom+dr4 | 359 (301, **+58**) | 12.80 s (21.10) | 1,015,987 (1,205,343) |
| j90 | TT2 LBCS +dom+dr4 | 295 (208, **+87**) | 5.83 s (14.67) | 244,877 (337,815) |
| j90 | TT2 LBMAX +dom+dr4 | 295 (209, **+86**) | 5.10 s (14.20) | 246,843 (392,910) |

DR4 is the second big pruning lever, and it grows with instance size: no new
instances on the saturated j30 (but ~2.5× faster, half the nodes), +58 on
j60, +86–87 on j90. Each +dom+dr4 run is a strict superset of its +dom
counterpart. LBCC+dom+dr4 (480/355/293) sits right beside LBCS/LBMAX —
TT2's rule stack does **not** exhibit the LBCC anomaly the Liu rules show
on TT. All makespans cross-verified against prior optimal runs with zero
mismatches (j30 additionally against the known optima, 480/480).

The same ablation for TT and the Liu rules (university machine):

| Set | Config | Solved (base, Δ) | Avg time (base) | Avg nodes (base) |
|---|---|---|---|---|
| j30 | TT CP +dr | 392 (391, **+1**) | 5.33 s (11.93) | 37,880 (720,638) |
| j30 | TT LBCC +dr | 391 (388, **+3**) | 5.43 s (12.47) | 37,919 (529,237) |
| j30 | TT LBMAX +dr | 392 (391, **+1**) | 5.59 s (11.94) | 37,900 (720,637) |
| j30 | TT LBCS +dr | 402 (396, **+6**) | 8.62 s (12.96) | 56,316 (674,546) |
| j60 | TT CP +dr | 277 (192, **+85**) | 11.13 s (23.64) | 180,413 (531,391) |
| j60 | TT LBCC +dr | 253 (266, **−13**) | 16.94 s (11.01) | 205,275 (225,363) |
| j60 | TT LBCS +dr | 277 (189, **+88**) | 12.08 s (24.62) | 179,633 (440,024) |
| j90 | TT LBCC +dr | 135 (233, **−98**) | 52.49 s (6.87) | 449,977 (110,386) |
| j90 | TT LBCS +dr | 158 (56, **+102**) | 42.33 s (46.42) | 491,876 (589,432) |

The picture is strongly size- and heuristic-dependent. On j30 the Liu rules
cut expansions ~12–19× but add only +1–6 solved (the set is nearly
saturated). On j60/j90 they are a lifeline for CP and LBCS: **+85, +88 and
+102 solved** — the largest absolute dominance-rule gains measured in this
study (TT2's cutset rules add +59/+60 on j60 and +27 on j90). **But they
actively hurt LBCC: −13 on j60 and −98 on j90** — with the rules on, LBCC
expands *more* nodes (450k vs 110k avg on j90) and runs far slower per
instance. Why the rules interact so badly with LBCC specifically is an open
question worth a diagnosis before the defense. Without rules, CP/LBCS
collapse with size (LBCS 396 → 189 → 56) while LBCC degrades gracefully
(388 → 266 → 233).

## 5. TT vs TT2 (model comparison)

**No dominance rules** (all university machine):

| Set | Heuristic | TT solved | TT2 solved | Both | Node ratio TT/TT2 (geo-mean) | Time ratio TT/TT2 (geo-mean) |
|---|---|---:|---:|---:|---:|---:|
| j30 | CP | 391 | 476 | 389 | 1.06 | 3.58 |
| j30 | LBCC | 388 | 476 | 386 | 0.53 | 3.21 |
| j30 | LBIP0 | 391 | 476 | 389 | 0.45 | 4.39 |
| j30 | LBMAX | 391 | 476 | 389 | 1.06 | 3.76 |
| j30 | LBCS | 396 | 477 | 394 | 1.10 | 3.19 |
| j60 | CP | 192 | 241 | 160 | 6.36 | 0.69 |
| j60 | LBCC | **266** | 236 | 219 | 0.15 | 0.20 |
| j60 | LBCS | 189 | 240 | 156 | 5.69 | 1.09 |
| j90 | LBCC | **233** | 179 | 155 | 0.29 | 0.70 |
| j90 | LBCS | 56 | 181 | 34 | 27.9 | 14.5 |

On j30, TT2 dominates every heuristic (+80–89 solved, ~3× faster). On the
larger sets the picture becomes **heuristic-dependent**: with CP/LBCS the
gap widens in TT2's favor (LBCS: +51 on j60, +125 on j90, node ratio up to
~28×) — but **with LBCC the direction flips**: plain TT solves +30 more
than plain TT2 on j60 (266 vs 236) and +54 more on j90 (233 vs 179),
expanding 3–7× *fewer* nodes and running ~1.4–5× faster per common
instance. LBCC's guidance evidently synergizes with TT's absolute-date
states on large instances; understanding this mechanism is an open question
(same caveat as the LBCC+dr anomaly in §4).

**Each model with its own dominance rules** (TT + Liu DR vs TT2 + cutset/UB,
all university machine):

| Set | Heuristic | TT +dr | TT2 +dom | Node ratio TT/TT2 | Time ratio TT/TT2 |
|---|---|---:|---:|---:|---:|
| j30 | LBCS | 402 | **480** | 1.64 | 7.78 |
| j30 | LBMAX | 392 | **480** | 1.53 | 7.95 |
| j60 | LBCS | 277 | **299** | 3.63 | 0.49 |
| j60 | LBMAX | 277 | **301** | 3.50 | 0.42 |
| j90 | LBCS | 158 | **208** | 56.2 | 17.6 |
| j90 | LBMAX | 161 | **209** | 56.2 | 6.7 |

With DR4 included, TT2's best stack beats TT's best (+dr) by **+78 on j30
(480 vs 402), +82 on j60 (359 vs 277), and +134 on j90 (295 vs 161)**. The
table above (TT2 +dom without DR4) is retained for the like-for-like
rules-vs-rules comparison. One nuance there: on j60's
commonly-solved instances TT+dr is actually ~2× *faster* per instance (time
ratio < 1) despite expanding 3.5× more nodes — TT2's dominance machinery has
a high per-node cost there — but TT2 converts that machinery into 22–24 more
solved instances, and on j90 the node gap explodes to ~56×.

An interesting crossover: **TT+dr (277) beats TT2-without-dominance (241) on
j60** — good pruning rules on the weaker model outperform the stronger model
without pruning. The plain-TT j60 number explains it: the Liu rules carry TT
from 189 to 277 (+88), more than covering the model gap. But TT+dr never
reaches TT2+dom, and on j90 the model gap is too large for the rules to
close: even +102 only lifts TT from 56 to 158, still below plain TT2 (181).

## 5b. Local (laptop) TT runs — full j30 sweeps, 300 s timeout

All complete 480-instance TT (TTPN, no dominance rules) sweeps run on the
local development laptop with the same protocol. Solved and node counts are
machine-independent; wall times are laptop times and are NOT comparable to
the university-machine tables above.

| Configuration | Solved (of 480) | Avg time on solved (s) | Avg expanded on solved |
|---|---:|---:|---:|
| TT CP (with duplicate pruning) | 369 | 6.77 | 251,266 |
| TT CP (no duplicate pruning) | 368 | 8.15 | 239,170 |
| TT LBCC | 361 | 4.98 | 165,054 |
| TT LBCS | 381 | 5.82 | 226,631 |
| TT LBIP0 | 387 | 8.96 | 505,346 |

Notes: these are now purely historical (every §3 entry is a university
measurement). The university plain-TT runs solve more (CP 391 vs 369, LBCS
396 vs 381, LBCC 388 vs 361, LBIP0 391 vs 387) — faster CPU plus solver
improvements between February and August. Duplicate pruning (DP) barely
matters on TT j30 (+1 solved).

## 5c. Prior published results (context)

**SoCS 2026 paper** (Lublin, Atzmon & Cohen, "Petri Net Induced Heuristic
Search for Resource Constrained Scheduling") — TTPNR with the hmax heuristic
vs MIP branch-and-cut baselines, **same machine (Xeon Gold 6248R, single
core) and same 5-minute timeout** as our experiments, so directly comparable:

| Solver | j30 solved | j60 solved | j90 solved | Avg time j30/j60/j90 (s) |
|---|---:|---:|---:|---|
| TTPNR (paper) | 475 (98.96%) | 229 (47.71%) | 173 (36.04%) | 7.97 / 19.33 / 5.97 |
| SCIP | 343 (71.46%) | 42 (8.75%) | 1 (0.21%) | 37.65 / 186.42 / 155.97 |
| CBC | 227 (47.29%) | 135 (28.13%) | 62 (12.92%) | 42.14 / 85.96 / 197.37 |

The paper's TTPNR numbers agree closely with our TT2 base runs (476/241/183)
— an independent consistency check — and provide the MIP baseline for the
book: **TT2+dom solves 480/301/209 where SCIP manages 343/42/1**. The paper
also reports the hardness gradient we reproduce in §6 (RS = 0.2: TTPNR
solves 98% on j30 but 2.5% on j60 and 0% on j90).

**Previous project book** (Danino & Lublin, 2025) — earlier stage of this
project line, **10-minute timeout on a different machine** (trend only, not
directly comparable): TPPN model 219/480 solved on j30 (46%), TTPN model
354/480 (74%), TTPN+LBCS 356/480. The progression across the two projects is
TPPN 219 → TTPN 354 (10 min) → TT+dr 402 → TT2+dom 480 (5 min).

## 6. Hardness structure (RS/RF breakdown)

- Difficulty is concentrated at **low Resource Strength**: on j60/j90 base
  configurations solve almost nothing at RS = 0.2, roughly a quarter at
  RS = 0.5, and nearly all 120 instances at RS = 1.0.
- Dominance-pruning gains come mostly from RS 0.2–0.5 (e.g. j60 RS = 0.5:
  ~33 → 57 of 120).
- High RF (every job uses every resource) compounds the difficulty; the j30
  hard core (the 3–4 base timeouts) sits at low-RS/low-NC groups (g2, g3, g7).

## 7. Findings (one-liners)

1. Dominance pruning is the biggest lever — heuristic choice moves ≤ 5
   instances per set, dominance adds +3/+60/+27 on j30/j60/j90.
2. j30 is solved completely (480/480) by TT2 + dominance, in minutes of total
   compute (max instance 26 s).
3. LBCS and LBER (d3) tie as best pure heuristics on j30 (477); **LBER d3 is
   the single best base heuristic on j60 (242)**. With dominance, LBCS and
   LBMAX are within 2 instances everywhere.
4. **LBER is complementary, not redundant**: it solves j30 g3_9 — one of the
   three hard-core instances no other base heuristic solves — and 2 j60
   instances (g31_4, g42_5) that no other base heuristic solves, raising the
   virtual-best base portfolio from 241 to 243 on j60 and to 478/480 on j30
   (only g2_2 and g7_10 remain out of reach without dominance). On commonly
   solved instances its node count matches LBCS (geo-mean ratio 1.02); its
   higher arithmetic average comes from a heavy tail.
5. LBMAX ≈ CP in search behavior (identical node counts on j30/j90 — the hmax
   floor dominates; the same tie shows up in TT+dr: 277=277 on j60, 161=161
   on j90).
6. TT2 dominates TT on j30 for every heuristic (+80–89 solved, ~3× faster on
   identical hardware) and rules-vs-rules everywhere (TT2+dom beats TT+dr by
   +22 to +78 on every set). But the no-rules comparison on large sets is
   **heuristic-dependent**: with CP/LBCS TT2's lead grows to +125 on j90,
   while **with LBCC plain TT beats plain TT2** (+30 on j60, +54 on j90)
   with 3–7× fewer nodes.
7. Rule-heuristic interaction is strong: the Liu rules add only +1–6 on j30
   but **+85/+88 on j60 and +102 on j90 for CP/LBCS** — yet they **hurt
   LBCC (−13 on j60, −98 on j90)**, inflating rather than shrinking the
   search. This explains the j60 base-crossover (TT+dr 277 > plain TT2 241)
   and flags an open diagnosis question for LBCC. TT2's own rule stack shows
   no such anomaly (LBCC+dom+dr4 355/293, right beside LBCS/LBMAX).
8. **DR4 is the decisive second pruning lever for TT2**: +0/+58/+86–87 over
   +dom, with ~2.5× speedups even where counts saturate. Study bests per
   set — all TT2 +dom+dr4: **480 (100%), 359 (75%), 295 (61%)**. Plain TT
   LBCC's 233 on j90 held the record before DR4 and remains the best
   non-TT2 result and the best no-rules result on that set.
9. Obvious next experiment: **LBER + dominance** — dominance added +59/+60 on
   j60 for LBCS/LBMAX, and LBER starts from the best base count (242), so
   LBER+dom could plausibly exceed the current best 301.

## 8. Data inventory & caveats

- University-compute TT2 results: `data/university_compute_real_final_resluts/tt2/`
  (24 CSVs; one byte-identical duplicate dropped). Full coverage matrix in
  `analysis/output/coverage.md`.
- TT results on the university machine: `.../tt/` — full +dr matrix
  (5 heuristics × 3 sets), plain-TT (all 5 on j30, CP/LBCC/LBCS on j60,
  LBCC/LBCS on j90), and TT LBER d3/d4 on j30.
- TT2 +dom+dr4: LBCC/LBCS/LBMAX × all three sets (complete).

### Missing runs — and whether they matter

| Missing run | Importance | Why |
|---|---|---|
| **TT2 LBER +dom+dr4 (j60, j90)** | **HIGH — the one gap that could change a headline** | LBER is the best/complementary base heuristic (242 on j60, unique hard-core solves); on the winning +dom+dr4 stack it could plausibly exceed 359/295. |
| TT2 LBER base on j90 | Medium | Completes the LBER row in §3; LBER is the only heuristic whose base j90 number is unknown. |
| TT2 CP/LBIP0 +dom+dr4 | Low | The three measured +dom+dr4 heuristics cluster within 2–4 instances; CP ≈ LBMAX throughout, so little new information expected. |
| TT2 LBCC +dom (without DR4) | Low | Only needed for a complete LBCC ablation chain; +dom+dr4 supersedes it in practice. |
| Plain-TT LBIP0/LBMAX on j60/j90, CP on j90 | Low | The TT story (collapse without rules; LBCC exception) is already established by the existing runs. |
| Confirm UB-pruning setting of the +dom+dr4 runs | Documentation | `ub_pruned = 0` in every row — either UB was off or it never fired; §7.1/setup text should state the exact flags. |
- Local (laptop) TT j30 sweeps: `data/real_data/2026-02-0*_tt_*.csv` and
  `2026-05-30_..._tt_lbip0.csv` (§5b).
- Prior-work numbers in §5c are quoted from the SoCS 2026 paper and the 2025
  project book, not from our runs — cite them accordingly.
- Timeout rows record `time ≈ 300–310 s` (limit checked between expansions).
- Hardware sentence for the book needs RAM/OS/compiler filled in from the
  university machine.

## 9. Artifacts

- Tables (CSV + markdown): `analysis/output/tables/` — `summary_*`,
  `ablation_*`, `tt_vs_tt2_*`.
- Figures: `analysis/output/figures/`, every figure as 300-dpi PNG **and**
  SVG, grayscale-safe (white background, thin black axes, identity carried
  by line style / hatching, not color).

### Figure guide — which figure to use where

When the book or slides need a figure, reference it by exact filename:

| Figure file | What it shows | Suggested use |
|---|---|---|
| `scaling_summary` | 3 bars: best TT2 configuration per set — 480/480 (100%), 359/480 (75%), 295/480 (61%) with the config named in each bar | The single takeaway slide; book conclusion |
| `cactus_j30` | Instances solved vs time budget (log x, 0.01–300 s), all 11 TT2 configurations; +dom+dr4 curves leftmost | Book results §: main j30 figure (consider `--configs` to slim to ~6 lines for print) |
| `cactus_j60` | Same for j60; +dom+dr4 curves top out at ~359, clearly above +dom (~301) and base (~241) | Book results §: j60 figure |
| `cactus_j90` | Same for j90; +dom+dr4 ~295 vs +dom ~209 vs base ~183 | Book results §: j90 figure (or merge with j60 in text) |
| `solved_by_RS_j30/j60/j90` | Grouped bars: solved per Resource Strength level (0.2/0.5/0.7/1.0), one bar per configuration | Hardness analysis §6 — the RS gradient; j60 version is the most telling |
| `solved_by_RF_j30/j60/j90` | Same broken down by Resource Factor | Hardness analysis, secondary (RF effect is milder) |
| `tt_vs_tt2_solved_j30/j60/j90` | Paired bars TT vs TT2 per heuristic, no dominance rules (j30: 5 heuristics; j60/j90: LBCS only — 189 vs 240, 56 vs 181) | Model-comparison §5, first half; j90 version shows the widening gap |
| `tt_vs_tt2_nodes_j30/j60/j90` | Log-log scatter, expanded nodes per instance TT vs TT2 (no rules), diagonal reference | Model-comparison §5 — node parity on j30, ~28× gap on j90 |
| `tt_vs_tt2_rules_solved_j30/j60/j90` | Paired bars TT+dr vs TT2+dom (402 vs 480; 277 vs 301; 161 vs 209) | Model-comparison §5, rules-vs-rules; j60 version also illustrates the crossover discussion |
| `tt_vs_tt2_rules_nodes_j30/j60/j90` | Log-log node scatter with each model's rules; j90 shows the ~56× node gap | Supplementary / appendix |

The no-rules comparison figures cover the heuristics run in both models per
set (j30: all five; j60: CP/LBCC/LBCS; j90: LBCC/LBCS) — the j90 solved
chart shows the LBCC flip (233 vs 179). Figures NOT yet generated: any
TT-only cactus, and any figure involving SCIP/CBC (those numbers exist only
as the §5c table quoted from the paper).
