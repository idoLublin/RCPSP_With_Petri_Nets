# RCPSP with Petri Nets — Final Experimental Results Summary

Self-contained summary of all final benchmark results for the project book and
presentation. Generated 2026-08-05 from the master results CSV
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
upper-bound pruning; for TT, "+dr" = the Liu et al. dominance rules
DR1/DR2/DR5.

## 2. Experimental protocol

- Benchmarks: PSPLIB **j30, j60, j90** — 480 instances each (48 parameter
  groups × 10 instances; full factorial of NC ∈ {1.5,1.8,2.1},
  RF ∈ {0.25,0.5,0.75,1.0}, RS ∈ {0.2,0.5,0.7,1.0}, RS varying fastest).
- **300 s per-instance time limit**, sequential, **single core** of an Intel
  Xeon Gold 6248R @ 3.00 GHz (university compute). One solver process per
  parameter group. Solved makespans validated against published optima
  (`data/j30opt.sm`) / lower-bound files.
- TT results on the university machine: full 5-heuristic × 3-set sweeps with
  the Liu dominance rules ("+dr"), plus plain-TT (no rules) LBCS/LBCC on j30.
  Plain-TT CP/LBIP0 baselines on j30 come from earlier laptop sweeps (same
  protocol; node counts machine-independent, their wall times indicative only).

## 3. Headline results — instances solved (of 480), university machine

| Configuration | j30 | j60 | j90 |
|---|---:|---:|---:|
| TT CP (no rules) | 369\* | — | — |
| TT LBCC (no rules) | 388 | — | — |
| TT LBIP0 (no rules) | 387\* | — | — |
| TT LBCS (no rules) | 396 | — | — |
| TT CP +dr | 392 | 277 | 161 |
| TT LBCC +dr | 391 | 253 | 135 |
| TT LBIP0 +dr | 391 | 261 | 140 |
| TT LBMAX +dr | 392 | 277 | **161** |
| TT LBCS +dr | **402** | **277** | 158 |
| TT2 CP | 476 | 241 | 182 |
| TT2 LBCC | 476 | 236 | 179 |
| TT2 LBIP0 | 476 | 241 | 183 |
| TT2 LBMAX | 476 | 241 | 182 |
| TT2 LBCS | **477** | 240 | 181 |
| TT2 LBER (d3) | **477** | **242** | not run |
| TT2 LBCS +dom | **480** | 299 | 208 |
| TT2 LBMAX +dom | **480** | **301** | **209** |

\* **Placeholder from local (laptop) runs** — plain-TT (no rules) sweeps on
the university machine exist only for LBCS/LBCC on j30; the missing plain-TT
CP and LBIP0 j30 numbers are taken from the earlier laptop sweeps (same 300 s
protocol) until university runs replace them. Plain-TT j60/j90 was not run
anywhere. Solved counts and node counts are machine-independent; only the
wall times of starred entries are not comparable to the rest of the table.

Average time / expanded nodes on solved instances (per set, selected):

| Configuration | j30 time | j30 nodes | j60 time | j60 nodes | j90 time | j90 nodes |
|---|---:|---:|---:|---:|---:|---:|
| TT2 CP | 4.78 s | 642,996 | 16.03 s | 1,246,458 | 8.75 s | 402,611 |
| TT2 LBCS | 4.98 s | 637,239 | 17.98 s | 1,150,488 | 8.98 s | 360,773 |
| TT2 LBER (d3) | 5.46 s | 892,346 | 14.83 s | 1,369,588 | — | — |
| TT2 LBCS +dom | **0.37 s** | **31,363** | 22.10 s | 1,109,477 | 14.67 s | 337,815 |
| TT2 LBMAX +dom | **0.36 s** | **32,953** | 21.10 s | 1,205,343 | 14.20 s | 392,910 |

(Averages are over each configuration's solved set; +dom rows on j60/j90 reach
harder instances, which is why their averages rise while solving more.)

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

The same ablation for TT and the Liu rules (university machine, j30):

| Config | Solved (base, Δ) | Avg time (base) | Avg nodes (base) |
|---|---|---|---|
| TT LBCS +dr | 402 (396, **+6**) | 8.62 s (12.96) | 56,316 (674,546) |
| TT LBCC +dr | 391 (388, **+3**) | 5.43 s (12.47) | 37,919 (529,237) |

The Liu rules cut TT's expansions by ~12× on solved instances but add only
+3–6 solved — far less than TT2's cutset rules add on j60/j90, and not enough
to reach even TT2's no-dominance counts.

## 5. TT vs TT2 (model comparison)

**No dominance rules, j30** (LBCS/LBCC: both models on the university machine,
so time ratios are real; CP/LBIP0: TT side from laptop runs — solved/node
figures valid, time ratio indicative only):

| Heuristic | TT solved | TT2 solved | Both | Node ratio TT/TT2 (geo-mean) | Time ratio TT/TT2 (geo-mean) |
|---|---:|---:|---:|---:|---:|
| LBCS | 396 | 477 | 394 | 1.10 | 3.19 |
| LBCC | 388 | 476 | 386 | 0.53 | 3.21 |
| CP\* | 369 | 476 | 367 | 0.88 | 2.36 |
| LBIP0\* | 387 | 476 | 385 | 0.43 | 3.67 |

TT2 solves **+80 to +107 more instances** per heuristic and is ~3.2× faster
per commonly-solved instance on identical hardware, with roughly comparable
node counts — TT2's states are cheaper to process and its
time-shift-invariant merging wins the hard tail where TT times out.

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

TT2 keeps the solved-count lead everywhere (+22 to +78). One nuance: on j60's
commonly-solved instances TT+dr is actually ~2× *faster* per instance (time
ratio < 1) despite expanding 3.5× more nodes — TT2's dominance machinery has
a high per-node cost there — but TT2 converts that machinery into 22–24 more
solved instances, and on j90 the node gap explodes to ~56×.

An interesting crossover: **TT+dr (277) beats TT2-without-dominance (241) on
j60** — good pruning rules on the weaker model outperform the stronger model
without pruning — but TT+dr never reaches TT2+dom, and on j90 it even trails
plain TT2 (161 vs 183).

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

Notes: these are the source of the starred placeholders in §3. The later
university plain-TT runs solve more (LBCS 396 vs 381, LBCC 388 vs 361) —
faster CPU plus solver improvements between February and August — so the
laptop numbers are conservative lower bounds for TT's ability. Duplicate
pruning (DP) barely matters on TT j30 (+1 solved).

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
6. TT2 structurally dominates TT with or without dominance rules: no-rules
   +80–107 solved on j30 at ~3.2× speed on identical hardware; rules-vs-rules
   TT2+dom beats TT+dr by +22 to +78 on every set.
7. The Liu rules help TT far less than the cutset rules help TT2 (+3–6 on j30
   vs closing the set; ~12× node cut but the hard tail stays out of reach).
   Still, **TT+dr (277) beats plain TT2 (241) on j60** — pruning quality can
   outweigh model quality — while on j90 TT+dr trails even plain TT2.
8. Scaling: TT2+dom solves 100% of j30, ~63% of j60, ~44% of j90 within 300 s;
   TT+dr manages 84% / 58% / 34%.
9. Obvious next experiment: **LBER + dominance** — dominance added +59/+60 on
   j60 for LBCS/LBMAX, and LBER starts from the best base count (242), so
   LBER+dom could plausibly exceed the current best 301.

## 8. Data inventory & caveats

- University-compute TT2 results: `data/university_compute_real_final_resluts/tt2/`
  (24 CSVs; one byte-identical duplicate dropped). Full coverage matrix in
  `analysis/output/coverage.md`.
- TT results on the university machine: `.../tt/` — full +dr matrix
  (5 heuristics × 3 sets) plus plain-TT LBCS/LBCC on j30.
- Gaps: TT2 dominance runs exist only for LBCS/LBMAX (notably missing:
  LBER+dom); LBER d3 never run on j90; **plain-TT (no rules) missing on the
  university machine for CP/LBIP0/LBMAX on j30 and for all of j60/j90** —
  where needed, CP/LBIP0 j30 figures are filled from the old laptop sweeps
  and marked as such (runner ready in `analysis/run_tt_sweep.sh` to close
  this properly).
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
| `scaling_summary` | 3 bars: best configuration per set — 480/480 (100%), 301/480 (63%), 209/480 (44%) with the config named in each bar | The single takeaway slide; book conclusion |
| `cactus_j30` | Instances solved vs time budget (log x, 0.01–300 s), all 8 TT2 configurations; +dom curves dominate 2 orders of magnitude to the left | Book results §: main j30 figure |
| `cactus_j60` | Same for j60; +dom curves separate from the base pack (~301 vs ~241) | Book results §: j60 figure |
| `cactus_j90` | Same for j90; curves plateau low (~209 best) | Book results §: j90 figure (or merge with j60 in text) |
| `solved_by_RS_j30/j60/j90` | Grouped bars: solved per Resource Strength level (0.2/0.5/0.7/1.0), one bar per configuration | Hardness analysis §6 — the RS gradient; j60 version is the most telling |
| `solved_by_RF_j30/j60/j90` | Same broken down by Resource Factor | Hardness analysis, secondary (RF effect is milder) |
| `tt_vs_tt2_solved_j30` | Paired bars TT vs TT2 per heuristic, no dominance rules (396→477 etc.) | Model-comparison §5, first half |
| `tt_vs_tt2_nodes_j30` | Log-log scatter, expanded nodes per instance TT vs TT2 (no rules), diagonal reference | Model-comparison §5 — shows node parity + hard tail |
| `tt_vs_tt2_rules_solved_j30/j60/j90` | Paired bars TT+dr vs TT2+dom (402 vs 480; 277 vs 301; 161 vs 209) | Model-comparison §5, rules-vs-rules; j60 version also illustrates the crossover discussion |
| `tt_vs_tt2_rules_nodes_j30/j60/j90` | Log-log node scatter with each model's rules; j90 shows the ~56× node gap | Supplementary / appendix |

Figures NOT yet generated (would need new runs or new script work): any
TT-only cactus, any j60/j90 no-rules TT comparison (plain TT was never run
there), and any figure involving SCIP/CBC (those numbers exist only as the
§5c table quoted from the paper).
