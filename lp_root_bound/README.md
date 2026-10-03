# Root-node LP bound-quality experiment (j30 / j60 / j90)

Offline study: at the **root** of each PSPLIB instance, compare the current TT2
**max heuristic** `max(CPM, volume)` against a time-indexed **DT LP relaxation**
and the same **LP tightened by minimal-forbidden-set (MDA) cuts**. Reports win
rate, gap-to-optimum, root closures, and complementarity.

This is a **bound-quality** measurement, **not** a per-node search heuristic —
the per-node cost (LP solve/node) was shown to be 10^3–10^5x the max heuristic,
so it is not integrated into the solver. See memory `project_hseq_dt_lp_result`.
The firing-count state-equation form (Bonet hSEQ) is inadmissible for makespan
(objective = sum of durations); this uses the admissible time-indexed relaxation.

## Files
- `rcpsp_lp.py` — parser, CPM + volume (the max-heuristic parts), DT LP, MDA cuts.
  Backends: **gurobi** (warm-started separation; needs a real license) or
  **scipy**/HiGHS (free; rebuilds each round). Verified numerically identical.
- `run.py` — parallel driver, incremental crash-safe CSV, summary.
- `run_server.sh` — edit the data paths, then `bash run_server.sh`.

## Requirements
```
pip install scipy numpy            # scipy>=1.9 bundles HiGHS
pip install gurobipy               # only for --backend gurobi
```
Gurobi needs a full license (the pip trial caps models at 2000 vars — too small
for j30+). On the lab server point `GRB_LICENSE_FILE` at the token server; if it
is down, use `--backend scipy` (slower per solve, but free and parallel).

## Data
Directories of PSPLIB `.sm` files named `<size><param>_<inst>.sm`
(e.g. `j3025_1.sm`). Not shipped here — extract `j30/j60/j90.sm.tgz` from PSPLIB
on the server and point the script at those dirs. `known_optima.csv` (in the repo
root) supplies optima/best-known for j30 (480), j60 (451), j90 (403).

## Run
```bash
# quick pilot (2 instances per parameter group, ~96 each):
BACKEND=scipy SUBSET=2 bash run_server.sh
# full sets with Gurobi:
DATA30=/path/j30.sm DATA60=/path/j60.sm DATA90=/path/j90.sm \
  BACKEND=gurobi SUBSET=all bash run_server.sh
```
Or one size directly:
```bash
python run.py --data /path/j60.sm --size j60 --optima known_optima.csv \
    --backend gurobi --jobs 32 --subset all --cuts 1 --maxsize 3
```

## Output
One CSV row per instance (`lp_rootbound_<size>_<backend>_cuts1_sz3_<subset>_<ts>.csv`)
with every flag recorded: `cpm, volume, maxHeur, lp_plain, lp_cuts, ncuts,
nforbidden, lp_beats_max, volume_binding, closed_root, gap_max, gap_lp,
solve_sec, backend, cuts, maxsize, rounds, timelimit`. Summary (per size) prints
win rate vs maxHeur, ties→wins from cuts, avg gap-to-opt maxHeur vs max(maxHeur,LP+cuts),
root closures, and the complementarity breakdown (share of wins that are
volume-weak / where the bound is loose).

## j30 reference (already measured locally, HiGHS)
- precedence-only LP == CPM (construction sanity: passes)
- LP+cuts > maxHeur on ~33% (subset); ties→wins broad; avg gap 5.59→4.89;
  a few root closures; wins are ~all volume-weak (resource-timing info
  orthogonal to CPM and volume). Expect j60/j90 to extend this trend.
