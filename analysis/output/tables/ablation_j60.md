### J60 ablation — dominance pruning on vs off (base value and delta in parentheses)

| Configuration | Solved | Avg time on solved (s) | Avg expanded on solved |
|---|---:|---:|---:|
| TT CP +dr | 277 (192, +85) | 11.13 (23.64) | 180,413 (531,391) |
| TT LBCC +dr | 253 (266, -13) | 16.94 (11.01) | 205,275 (225,363) |
| TT LBCS +dr | 277 (189, +88) | 12.08 (24.62) | 179,633 (440,024) |
| TT2 LBMAX +dom | 301 (241, +60) | 21.10 (16.21) | 1,205,343 (1,246,458) |
| TT2 LBMAX +dom+dr4 | 359 (301, +58) | 12.80 (21.10) | 1,015,987 (1,205,343) |
| TT2 LBCS +dom | 299 (240, +59) | 22.10 (17.98) | 1,109,477 (1,150,488) |
| TT2 LBCS +dom+dr4 | 357 (299, +58) | 13.76 (22.10) | 956,989 (1,109,477) |

_Averages are over each configuration's solved instances only; configurations solving different subsets are not directly comparable on time/nodes. Time/node columns show the with-dominance value and the no-dominance value in parentheses._
