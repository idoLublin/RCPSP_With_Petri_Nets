### J90 ablation — dominance pruning on vs off (base value and delta in parentheses)

| Configuration | Solved | Avg time on solved (s) | Avg expanded on solved |
|---|---:|---:|---:|
| TT LBCC +dr | 135 (233, -98) | 52.49 (6.87) | 449,977 (110,386) |
| TT LBCS +dr | 158 (56, +102) | 42.33 (46.42) | 491,876 (589,432) |
| TT2 LBMAX +dom | 209 (182, +27) | 14.20 (8.36) | 392,910 (402,611) |
| TT2 LBMAX +dom+dr4 | 295 (209, +86) | 5.10 (14.20) | 246,843 (392,910) |
| TT2 LBCS +dom | 208 (181, +27) | 14.67 (8.98) | 337,815 (360,773) |
| TT2 LBCS +dom+dr4 | 295 (208, +87) | 5.83 (14.67) | 244,877 (337,815) |

_Averages are over each configuration's solved instances only; configurations solving different subsets are not directly comparable on time/nodes. Time/node columns show the with-dominance value and the no-dominance value in parentheses._
