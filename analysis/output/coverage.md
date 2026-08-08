# Results coverage report

Source rows: 24960 across 52 files.
Per-instance timeout: 300 s (overshoot up to ~10 s occurs because the limit is checked between node expansions).

## Coverage matrix

| set | model | config | heuristic | machine | rows | solved | timeouts | max time (s) | status |
|---|---|---|---|---|---:|---:|---:|---:|---|
| j30 | TT | base | CP | laptop | 480 | 369 | 111 | 1225.3 | complete |
| j30 | TT | base | CP | university | 480 | 391 | 89 | 316.1 | complete |
| j30 | TT | base | LBCC | laptop | 480 | 361 | 119 | 1229.1 | complete |
| j30 | TT | base | LBCC | university | 480 | 388 | 92 | 303.6 | complete |
| j30 | TT | base | LBCS | laptop | 480 | 381 | 99 | 1161.0 | complete |
| j30 | TT | base | LBCS | university | 480 | 396 | 84 | 307.6 | complete |
| j30 | TT | base | LBIP0 | laptop | 480 | 387 | 93 | 1266.8 | complete |
| j30 | TT | base | LBIP0 | university | 480 | 391 | 89 | 309.1 | complete |
| j30 | TT | base | LBMAX | university | 480 | 391 | 89 | 309.1 | complete |
| j30 | TT | ttdr | CP | university | 480 | 392 | 88 | 300.0 | complete |
| j30 | TT | ttdr | LBCC | university | 480 | 391 | 89 | 300.4 | complete |
| j30 | TT | ttdr | LBCS | university | 480 | 402 | 78 | 300.0 | complete |
| j30 | TT | ttdr | LBIP0 | university | 480 | 391 | 89 | 300.0 | complete |
| j30 | TT | ttdr | LBMAX | university | 480 | 392 | 88 | 300.4 | complete |
| j30 | TT2 | base | CP | university | 480 | 476 | 4 | 300.0 | complete |
| j30 | TT2 | base | LBCC | university | 480 | 476 | 4 | 300.0 | complete |
| j30 | TT2 | base | LBCS | university | 480 | 477 | 3 | 300.0 | complete |
| j30 | TT2 | base | LBIP0 | university | 480 | 476 | 4 | 300.0 | complete |
| j30 | TT2 | base | LBMAX | university | 480 | 476 | 4 | 300.0 | complete |
| j30 | TT2 | dom | LBCS | university | 480 | 480 | 0 | 25.8 | complete |
| j30 | TT2 | dom | LBMAX | university | 480 | 480 | 0 | 24.3 | complete |
| j30 | TT2 | lber_d3 | LBER | university | 480 | 477 | 3 | 300.0 | complete |
| j60 | TT | base | CP | university | 480 | 192 | 288 | 308.8 | complete |
| j60 | TT | base | LBCC | university | 480 | 266 | 214 | 308.9 | complete |
| j60 | TT | base | LBCS | university | 480 | 189 | 291 | 312.4 | complete |
| j60 | TT | ttdr | CP | university | 480 | 277 | 203 | 303.0 | complete |
| j60 | TT | ttdr | LBCC | university | 480 | 253 | 227 | 302.2 | complete |
| j60 | TT | ttdr | LBCS | university | 480 | 277 | 203 | 301.0 | complete |
| j60 | TT | ttdr | LBIP0 | university | 480 | 261 | 219 | 301.0 | complete |
| j60 | TT | ttdr | LBMAX | university | 480 | 277 | 203 | 300.9 | complete |
| j60 | TT2 | base | CP | university | 480 | 241 | 239 | 307.5 | complete |
| j60 | TT2 | base | LBCC | university | 480 | 236 | 244 | 302.9 | complete |
| j60 | TT2 | base | LBCS | university | 480 | 240 | 240 | 306.4 | complete |
| j60 | TT2 | base | LBIP0 | university | 480 | 241 | 239 | 309.2 | complete |
| j60 | TT2 | base | LBMAX | university | 480 | 241 | 239 | 307.9 | complete |
| j60 | TT2 | dom | LBCS | university | 480 | 299 | 181 | 302.6 | complete |
| j60 | TT2 | dom | LBMAX | university | 480 | 301 | 179 | 306.5 | complete |
| j60 | TT2 | lber_d3 | LBER | university | 480 | 242 | 238 | 305.1 | complete |
| j90 | TT | base | LBCC | university | 480 | 233 | 247 | 302.7 | complete |
| j90 | TT | base | LBCS | university | 480 | 56 | 424 | 304.3 | complete |
| j90 | TT | ttdr | CP | university | 480 | 161 | 319 | 301.9 | complete |
| j90 | TT | ttdr | LBCC | university | 480 | 135 | 345 | 301.2 | complete |
| j90 | TT | ttdr | LBCS | university | 480 | 158 | 322 | 300.9 | complete |
| j90 | TT | ttdr | LBIP0 | university | 480 | 140 | 340 | 301.5 | complete |
| j90 | TT | ttdr | LBMAX | university | 480 | 161 | 319 | 302.0 | complete |
| j90 | TT2 | base | CP | university | 480 | 182 | 298 | 309.8 | complete |
| j90 | TT2 | base | LBCC | university | 480 | 179 | 301 | 303.9 | complete |
| j90 | TT2 | base | LBCS | university | 480 | 181 | 299 | 304.7 | complete |
| j90 | TT2 | base | LBIP0 | university | 480 | 183 | 297 | 308.9 | complete |
| j90 | TT2 | base | LBMAX | university | 480 | 182 | 298 | 307.1 | complete |
| j90 | TT2 | dom | LBCS | university | 480 | 208 | 272 | 302.8 | complete |
| j90 | TT2 | dom | LBMAX | university | 480 | 209 | 271 | 306.6 | complete |

## Duplicate files (byte-identical, one kept)

- `2026-07-07_j60_g1-48_e1-10_tt2_lbcs_j60 (2).csv` == `2026-07-07_j60_g1-48_e1-10_tt2_lbcs_j60 (1).csv` (dropped duplicate)

## Unrecognized files (skipped)

- none

## Missing combinations

- j30 / dom / CP: not run
- j30 / dom / LBCC: not run
- j30 / dom / LBER: not run
- j30 / dom / LBIP0: not run
- j60 / dom / CP: not run
- j60 / dom / LBCC: not run
- j60 / dom / LBER: not run
- j60 / dom / LBIP0: not run
- j90 / dom / CP: not run
- j90 / dom / LBCC: not run
- j90 / dom / LBER: not run
- j90 / dom / LBIP0: not run
- j90 / lber_d3 / LBER: not run
- j30 / ttdr / CP: not run
- j30 / ttdr / LBCC: not run
- j30 / ttdr / LBCS: not run
- j30 / ttdr / LBER: not run
- j30 / ttdr / LBIP0: not run
- j30 / ttdr / LBMAX: not run
- j60 / ttdr / CP: not run
- j60 / ttdr / LBCC: not run
- j60 / ttdr / LBCS: not run
- j60 / ttdr / LBER: not run
- j60 / ttdr / LBIP0: not run
- j60 / ttdr / LBMAX: not run
- j90 / ttdr / CP: not run
- j90 / ttdr / LBCC: not run
- j90 / ttdr / LBCS: not run
- j90 / ttdr / LBER: not run
- j90 / ttdr / LBIP0: not run
- j90 / ttdr / LBMAX: not run
