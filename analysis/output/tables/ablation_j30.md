### J30 ablation — dominance pruning on vs off (base value and delta in parentheses)

| Configuration | Solved | Avg time on solved (s) | Avg expanded on solved |
|---|---:|---:|---:|
| TT CP +dr | 392 (391, +1) | 5.33 (11.93) | 37,880 (720,638) |
| TT LBCC +dr | 391 (388, +3) | 5.43 (12.47) | 37,919 (529,237) |
| TT LBIP0 +dr | 391 (391, +0) | 5.70 (11.23) | 43,720 (602,342) |
| TT LBMAX +dr | 392 (391, +1) | 5.59 (11.94) | 37,900 (720,637) |
| TT LBCS +dr | 402 (396, +6) | 8.62 (12.96) | 56,316 (674,546) |
| TT2 LBMAX +dom | 480 (476, +4) | 0.36 (4.80) | 32,953 (642,996) |
| TT2 LBCS +dom | 480 (477, +3) | 0.37 (4.98) | 31,363 (637,239) |

_Averages are over each configuration's solved instances only; configurations solving different subsets are not directly comparable on time/nodes. Time/node columns show the with-dominance value and the no-dominance value in parentheses._
