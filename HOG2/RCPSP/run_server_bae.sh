#!/bin/bash
# ─────────────────────────────────────────────────────────────────────────────
# BAE* (proper) — full benchmark, 3 arms (j30/j60/j90), heavily instrumented.
# Forward A* on N interleaved with backward A* on the precedence-reversed N^R,
# meeting on the NORMALISED cut-marking key. Modes tt2bae2_j30/_j60/_j90 write
# new_results/output_tt2bae2_<size>_*.csv. 300s/instance (server default).
#
# WHAT WE ALREADY KNOW GOING IN (j30, measured):
#   * BAE* is CORRECT (8/8 optimal) and the middle meet fires for real.
#   * It is NOT faster than unidirectional forward on j30, and the reason is
#     structural: the reversed direction is 1.3x-238x HARDER than the forward
#     one, BAE* must push BOTH frontiers, while unidirectional gets to use the
#     CHEAP one. It is not DR4, not batch, not the priority (b beats f by up to
#     33%), not the policy (cardinality beats alternation by 6-39%).
#   * So the question this run answers is NOT "is BAE* faster" — it is
#     "does the direction asymmetry NARROW at j60/j90?" If expRatioBF drops
#     toward 1 with size, BAE* has a real shot there. If it widens, it cannot.
#
# CONFIG (recorded in every row): DR5 both sides, DR4 OFF (it erases the shared
# cut states), batch OFF (it halves meetability), BAE* priority b=2g+h-h_opp,
# Pohl cardinality side-choice, normalised meet key.
#
# READING THE CSV — the columns that matter:
#   solved      True ONLY when optimality was PROVEN. A deadline exit still
#               reports its incumbent in `ub`, so unsolved rows carry a valid UB.
#   ubSource    meet    = genuinely bidirectional (the middle meet set the bound)
#               goal    = a side completed => it degenerated to unidirectional
#               trivial = CPM/EST schedule was already resource-feasible
#   expRatioBF  expandedB/expandedF = THE DIRECTION ASYMMETRY (the headline).
#   bestMeetDepth  where the meet that set the bound sits, as #started/NT.
#               0 = source, 1 = goal, 0.5 = TRUE MIDDLE. Depth, not g/opt: one
#               long activity can eat the makespan while the search barely moves.
#   meetDepthMin/Mean/Max, firstMeetFrac, bestMeetGF/GB (the g split at the meet).
#
# DIRS: j30 data under HOG2/RCPSP; j60/j90 at repo ROOT — each arm cd's accordingly.
# ─────────────────────────────────────────────────────────────────────────────
set -u
ROOT=~/RCPSP_With_Petri_nets
RD="$ROOT/HOG2/RCPSP"
cd "$RD" || { echo "cd failed"; exit 1; }

echo "=== BUILD ($(date)) ==="
g++ -std=c++20 -fopenmp -O3 -I../.. -I../gui/STUB Driver.cpp -o Driver_bae2 2> build_bae2_err.txt \
  && echo "BUILD OK" || { echo "BUILD FAILED"; tail -40 build_bae2_err.txt; exit 1; }
BIN="$RD/Driver_bae2"
L="$RD/logs"; mkdir -p "$L"

BAE="RCPSP_MEET_NORM=1 RCPSP_BI_DR4=0 RCPSP_BI_BATCH=0 RCPSP_BI_PRIO=0 RCPSP_BI_ALT=0"

echo "=== BAE2 arms ($(date)) ==="
( cd "$RD"   && env $BAE "$BIN" tt2bae2_j30 > "$L/tt2bae2_j30.log" 2>&1 ) &
( cd "$ROOT" && env $BAE "$BIN" tt2bae2_j60 > "$L/tt2bae2_j60.log" 2>&1 ) &
( cd "$ROOT" && env $BAE "$BIN" tt2bae2_j90 > "$L/tt2bae2_j90.log" 2>&1 ) &
wait

cp "$(ls -t "$RD/new_results"/output_tt2bae2_j30_*.csv | head -1)"   "$RD/new_results/BAE2_j30.csv" 2>/dev/null
cp "$(ls -t "$ROOT/new_results"/output_tt2bae2_j60_*.csv | head -1)" "$ROOT/new_results/BAE2_j60.csv" 2>/dev/null
cp "$(ls -t "$ROOT/new_results"/output_tt2bae2_j90_*.csv | head -1)" "$ROOT/new_results/BAE2_j90.csv" 2>/dev/null

F30="$RD/new_results/BAE2_j30.csv"; F60="$ROOT/new_results/BAE2_j60.csv"; F90="$ROOT/new_results/BAE2_j90.csv"

echo ""
echo "=== 1. COVERAGE (proven optimal / total), and anytime UB value ==="
for f in "$F30" "$F60" "$F90"; do [ -f "$f" ] || continue
  awk -F, 'NR>1{n++; if($4=="True")s++; if($4=="False"&&$6!=-1)u++}
    END{printf "  %-30s proven=%d/%d (%.1f%%)  unsolved-but-with-UB=%d\n",
        FILENAME,s,n,(n?100*s/n:0),u}' "$f"; done

echo ""
echo "=== 2. WAS IT ACTUALLY BIDIRECTIONAL? (ubSource) ==="
echo "    meet=middle meet set the bound | goal=a side completed (unidirectional) | trivial=CPM root"
for f in "$F30" "$F60" "$F90"; do [ -f "$f" ] || continue
  awk -F, 'NR>1{c[$7]++; n++}
    END{printf "  %-30s", FILENAME; for(k in c) printf " %s=%d(%.0f%%)",k,c[k],100*c[k]/n; printf "\n"}' "$f"; done

echo ""
echo "=== 3. DIRECTION ASYMMETRY expandedB/expandedF — THE HEADLINE ==="
echo "    BAE* pays for the harder direction; unidirectional picks the cheaper one."
echo "    If this falls toward 1.0 with size, BAE* has a real shot at that size."
for f in "$F30" "$F60" "$F90"; do [ -f "$f" ] || continue
  awk -F, 'NR>1 && $11>0 {a[n++]=$11; s+=$11}
    END{if(!n){printf "  %-30s no data\n",FILENAME; exit}
        asort(a); printf "  %-30s median=%.2f  mean=%.2f  p10=%.2f  p90=%.2f  n=%d\n",
        FILENAME,a[int(n/2)+1],s/n,a[int(n*0.1)+1],a[int(n*0.9)+1],n}' "$f" 2>/dev/null \
  || awk -F, 'NR>1 && $11>0 {s+=$11; n++} END{if(n)printf "  %-30s mean=%.2f n=%d\n",FILENAME,s/n,n}' "$f"; done

echo ""
echo "=== 4. WHERE DO THEY MEET? bestMeetDepth (#started/NT, 0.5 = true middle) ==="
for f in "$F30" "$F60" "$F90"; do [ -f "$f" ] || continue
  awk -F, 'NR>1 && $15>=0 {s+=$15; n++; if($15>=0.4&&$15<=0.6)mid++; if($15<0.25)early++; if($15>0.75)late++}
    END{if(!n){printf "  %-30s no meets\n",FILENAME; exit}
        printf "  %-30s mean=%.3f  in[0.4,0.6]=%d(%.0f%%)  <0.25=%d  >0.75=%d  n=%d\n",
        FILENAME,s/n,mid,100*mid/n,early,late,n}' "$f"; done

echo ""
echo "=== 5. HOW EARLY does the bound arrive? firstMeetFrac (fraction of expansions) ==="
for f in "$F30" "$F60" "$F90"; do [ -f "$f" ] || continue
  awk -F, 'NR>1 && $14>=0 {s+=$14; n++} END{if(n)printf "  %-30s mean=%.3f n=%d\n",FILENAME,s/n,n;
        else printf "  %-30s no meets\n",FILENAME}' "$f"; done

echo ""
echo "=== 6. MEET SPREAD (min/mean/max depth over ALL meets, averaged per instance) ==="
for f in "$F30" "$F60" "$F90"; do [ -f "$f" ] || continue
  awk -F, 'NR>1 && $18>=0 {mn+=$18; me+=$19; mx+=$20; n++}
    END{if(n)printf "  %-30s min=%.3f mean=%.3f max=%.3f n=%d\n",FILENAME,mn/n,me/n,mx/n,n;
        else printf "  %-30s no meets\n",FILENAME}' "$f"; done

echo ""
echo "=== ALL DONE ($(date)) ==="
