# Final-results analysis pipeline (project book & presentation)

Everything here reads the university-compute result CSVs in
`data/university_compute_real_final_resluts/` and regenerates the book's
tables and figures from one master CSV. Nothing modifies the source results.

## Regenerate everything

```bash
python3 analysis/build_master.py      # -> output/master_results.csv + output/coverage.md
python3 analysis/make_tables.py       # -> output/tables/summary_*.{csv,md}, ablation_*.{csv,md}
python3 analysis/make_plots.py        # -> output/figures/*.png (300 dpi) + *.svg
```

Requires Python 3 with matplotlib (tables/master need only the stdlib).

## Files

| file | purpose |
|---|---|
| `psplib_params.py` | group → (NC, RF, RS) factorial mapping; self-validates against `data/TT_ido.csv` (`python3 analysis/psplib_params.py`) |
| `build_master.py` | scans the results tree, dedupes byte-identical files, joins NC/RF/RS, writes the master CSV and the coverage/inconsistency report |
| `make_tables.py` | per-set summary tables (solved / avg time / avg expanded on solved) and dominance-ablation tables, CSV + paste-ready markdown |
| `make_plots.py` | per-set cactus plots and solved-by-RS / solved-by-RF grouped bars; grayscale-safe (identity via line style / hatching), white background, thin black axes |
| `compare_models.py` | TT vs TT2 head-to-head per set: solved counts, geo-mean node/time ratios on commonly-solved, bar + scatter figures |
| `run_tt_sweep.sh` | TT (TTPN) sweep under the identical protocol (300 s, one process per group); output merges via `build_master.py --extra-dir` |
| `estimate_tt_runtime.py` | wall-time estimates for the three TT sweep scopes — run before launching anything |

## Master CSV schema

One row per (instance × model × heuristic × config):
`set, group, exam, instance, NC, RF, RS, model (TT2/TT), heuristic, config
(base|dom|lber_d4), dominance, solved, makespan, time, expand_number,
generated_number, depth, dominance_pruned, dominance_pop_pruned, ub_pruned,
dr4_pruned, run_date, machine, source_file`. Unsolved rows leave
`makespan`/`depth` empty; pruning counters are populated only for `dom` runs.
`machine` is `university` for the final results tree and `laptop` (or the
`--extra-machine` label) for merged extras; `make_tables.py`/`make_plots.py`
filter to `university` by default so book outputs never mix machines.
The four full laptop TT j30 sweeps merge in for the model comparison via:

```bash
python3 analysis/build_master.py \
  --extra-file data/real_data/2026-02-01_j30_g1-48_e1-10_tt_cp_dp.csv \
  --extra-file data/real_data/2026-02-02_j30_g1-48_e1-10_tt_lbcs.csv \
  --extra-file data/real_data/2026-02-09_j30_g1-48_e1-10_tt_lbcc.csv \
  --extra-file data/real_data/2026-05-30_j30_g1-48_e1-10_tt_lbip0.csv
python3 analysis/compare_models.py
```

## Running the TT sweeps later

```bash
tmux new -s tt_sweep
analysis/run_tt_sweep.sh j30 cp lbcs lbcc lbip0 lbmax 2>&1 | tee logs/tt_j30.log
# afterwards:
python3 analysis/build_master.py --extra-dir data/tt_final_results
python3 analysis/make_tables.py && python3 analysis/make_plots.py
```

## Hardware note (book §7.1)

All final experiments ran sequentially on a single core of an Intel Xeon Gold
6248R @ 3.00 GHz with [RAM] GB RAM under [OS], with the solver compiled from a
single C++17 translation unit at `-O3` with [compiler + version], and a 300 s
per-instance time limit. — fill the three placeholders from the university
machine: `lscpu`, `free -h`, `cat /etc/os-release`, `g++ --version` (or
`clang++ --version`).
