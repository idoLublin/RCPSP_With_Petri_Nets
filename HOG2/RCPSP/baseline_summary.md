# Baseline Summary — TTPNR & CBS features vs. baselines

*PSPLIB, 480 instances/size, 300 s/instance, single-core, lab server (dsisco01). Updated 2026-07-27.*
SR = success rate (proven-optimal solved / 480). All feature configs are sound (0 makespan mismatches
on every commonly-solved instance; none below LB).

## 1. TTPNR — Multiple-Choice-Set / Batch expansion vs. DR5 baseline

Success rate (solved / 480, SR %):

| Config | j30 | j60 | j90 |
|---|---|---|---|
| SoCS-2026 published TTPNR (no dominance) | 475/480 (98.96 %) | 229/480 (47.71 %) | 173/480 (36.04 %) |
| TTPNR + DR5 (baseline) | 480/480 (100 %) | 295/480 (61.5 %) | 204/480 (42.5 %) |
| **TTPNR + DR5 + Batch (new)** | **480/480 (100 %)** | **325/480 (67.7 %)** | **282/480 (58.75 %)** |
| **Δ Batch vs. DR5** | +0 | **+30 (+6.2 pp)** | **+78 (+16.3 pp)** |
| Δ Batch vs. SoCS published | +5 | +96 (+20.0 pp) | +109 (+22.7 pp) |

Average time on success (batch vs. DR5 baseline):

| Size | DR5 baseline | Batch + DR5 | Δ time on success |
|---|---|---|---|
| j30 | _(server CSV)_ | _(server CSV)_ | **−6 %** |
| j60 | _(server CSV)_ | _(server CSV)_ | **−31 %** |
| j90 | _(server CSV)_ | _(server CSV)_ | **−37 %** |

<!-- Absolute avg-seconds live in the server _2 (DR5) / _3 (batch+DR5) full-run CSVs; deltas above are
     from the confirmed 2026-07-22 run. Also: total expansions −38 %/−42 % (j60/j90); avg solution
     depth 63→38 (j60) and 93→44 (j90), path length ~halved. Batch is not strictly per-instance
     dominant (loses 3 on j60, 2 on j90 to plateau/tie-break noise) but is hugely net positive.
     Always pair batch with RCPSP_TT2_DR5=1. -->

**Takeaway:** batch's advantage **grows with instance size** — tie at j30 (already 100 %), +30 at j60,
+78 at j90 — because larger instances have far more simultaneously-available activities to bundle into
one edge. This overturns the earlier "neutral on tight resources" read, which was j30-only.

## 2. CBS — Lazy A* (generation-cost reducer)

<!-- Lazy (RCPSP_LAZY=1, LazyAStarCBS.h) defers compute_h_and_RVS from insertion to pop. Its headline
     is GENERATION speed, not SR (A* is expansion-bound, so coverage gain is smaller and the full
     10-exam correctness sweep is still owed before it earns an SR row):
       nodes generated/sec vs eager (_4 lazy vs _2 no-lazy):
         cfg8 (all features, h>0): 2.1x (j30) / 3.65x (j60) / 4.67x (j90)  <- gains most
         cfg5:                     2.0x / ~2.4x / 2.85x
         cfg1 (h=0, no MDA):       1.3x / ~1.35x / 1.4x
     cfg8 gains most because it pays the expensive MDA/conflict scan per node. Sound (h=0 admissible
     + reinsertion => optimal, 0-wrong so far). -->

Not yet an SR row — lazy speeds *generation* (see comment above: 2.1×/3.65×/4.67× on cfg8 across
j30/j60/j90); coverage effect is smaller (expansion-bound) and awaits a full correctness sweep.

## 3. Visualization idea
Compare the **improvement across all sizes** visually: a grouped-bar (or slope) chart of SR % at
j30 / j60 / j90 for {SoCS published, DR5, Batch+DR5}. The story is the **widening gap** — the three
lines fan out as size grows, making "batch scales with problem size" immediate. A second small-multiple
panel could overlay the CBS-lazy generation speedup (1.4× → 4.7×) to show the same size-scaling trend
on the CBS side. *(Offered — say the word and I'll generate it.)*
