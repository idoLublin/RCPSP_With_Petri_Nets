# RCPSP with Petri Nets — Final Experimental Results Summary

Self-contained summary of all final benchmark results for the project book and
presentation. Generated 2026-08-01 from the master results CSV
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
pruning ("+dom") = cutset dominance rule + dominance-pop + SGS upper-bound
pruning for TT2.

## 2. Experimental protocol

- Benchmarks: PSPLIB **j30, j60, j90** — 480 instances each (48 parameter
  groups × 10 instances; full factorial of NC ∈ {1.5,1.8,2.1},
  RF ∈ {0.25,0.5,0.75,1.0}, RS ∈ {0.2,0.5,0.7,1.0}, RS varying fastest).
- **300 s per-instance time limit**, sequential, **single core** of an Intel
  Xeon Gold 6248R @ 3.00 GHz (university compute). One solver process per
  parameter group. Solved makespans validated against published optima
  (`data/j30opt.sm`) / lower-bound files.
- The TT baseline numbers in §5 come from earlier full sweeps on a laptop
  (same protocol, 300 s); node counts are machine-independent, wall times
  across machines are indicative only. TT runs on the university machine are
  planned but not yet executed.

## 3. TT2 headline results — instances solved (of 480)

| Configuration | j30 | j60 | j90 |
|---|---:|---:|---:|
| TT2 CP | 476 | 241 | 182 |
| TT2 LBCC | 476 | 236 | 179 |
| TT2 LBIP0 | 476 | 241 | 183 |
| TT2 LBMAX | 476 | 241 | 182 |
| TT2 LBCS | **477** | 240 | 181 |
| TT2 LBER (d3) | **477** | **242** | not run |
| TT2 LBCS +dom | **480** | 299 | 208 |
| TT2 LBMAX +dom | **480** | **301** | **209** |

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

## 5. TT vs TT2 (model comparison, j30, no dominance rules)

TT figures from full laptop sweeps (same 300 s protocol; time ratios are
cross-machine and indicative — node ratios are exact).

| Heuristic | TT solved | TT2 solved | Both | Node ratio TT/TT2 (geo-mean) | Time ratio TT/TT2 (geo-mean) |
|---|---:|---:|---:|---:|---:|
| CP | 369 | 476 | 367 | 0.88 | 2.36 |
| LBCC | 361 | 476 | 359 | 0.39 | 1.96 |
| LBIP0 | 387 | 476 | 385 | 0.43 | 3.67 |
| LBCS | 381 | 477 | 379 | 0.66 | 1.99 |

Reading: TT2 solves **+89 to +115 more instances** per heuristic. On the easy,
commonly-solved instances TT actually expands *fewer* nodes (ratio < 1), yet is
~2–3.7× slower per instance — TT2's node operations are cheaper and its
time-shift-invariant state merging pays off precisely on the hard tail where TT
times out. TT2's advantage is therefore structural (state merging + cheaper
states), not heuristic-driven.

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
   floor dominates).
6. TT2 structurally dominates TT: +~100 solved on j30 and ~2–3.7× faster per
   instance, despite comparable or higher node counts on easy instances.
7. Scaling: TT2+dom solves 100% of j30, ~63% of j60, ~44% of j90 within 300 s.
8. Obvious next experiment: **LBER + dominance** — dominance added +59/+60 on
   j60 for LBCS/LBMAX, and LBER starts from the best base count (242), so
   LBER+dom could plausibly exceed the current best 301.

## 8. Data inventory & caveats

- University-compute TT2 results: `data/university_compute_real_final_resluts/tt2/`
  (24 CSVs; one byte-identical duplicate dropped). Full coverage matrix in
  `analysis/output/coverage.md`.
- Gaps: dominance runs exist only for LBCS/LBMAX (notably missing: LBER+dom);
  LBER d3 complete on j30+j60 but never run on j90 (earlier d4 files were
  replaced by the complete d3 runs); **no TT runs on the university machine yet**
  (estimates: full 5-heuristic × 3-set matrix worst-case 600 h; recommended
  scope 5×j30 + CP,LBCS×j60 worst 280 h / realistic 90–128 h; minimal
  CP,LBCS×j30 realistic ~19 h — runner ready in `analysis/run_tt_sweep.sh`).
- Timeout rows record `time ≈ 300–310 s` (limit checked between expansions).
- Hardware sentence for the book needs RAM/OS/compiler filled in from the
  university machine.

## 9. Artifacts

- Tables (CSV + markdown): `analysis/output/tables/` — `summary_*`,
  `ablation_*`, `tt_vs_tt2_*`.
- Figures (PNG 300 dpi + SVG, grayscale-safe): `analysis/output/figures/` —
  cactus plots, solved-by-RS/RF grouped bars, TT-vs-TT2 solved bars and
  node scatter, per set; plus `scaling_summary` (best configuration per set,
  480→301→209 of 480 — the one-slide takeaway for the presentation).
