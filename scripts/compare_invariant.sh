#!/bin/bash
# Bit-identical invariant check: compares solved-instance metrics (group, exam,
# makespan, expand, generated, depth) between a baseline CSV and a candidate CSV.
# Only `time` may differ. Also reports the solved-set delta and geo-mean speedup.
# Usage: compare_invariant.sh <baseline.csv> <candidate.csv>
set -u
BASE="$1"; CAND="$2"

key() { awk -F, 'NR>1 && $4=="True"{print $1","$2","$5","$6","$7","$8}' "$1" | sort; }

if diff <(key "$BASE") <(key "$CAND") > /tmp/inv_diff.txt; then
  echo "INVARIANT OK: solved rows bit-identical ($(key "$BASE" | wc -l | tr -d ' ') rows)"
else
  # Distinguish "solved set grew" (allowed) from genuine metric mismatch (not allowed)
  base_keys=$(awk -F, 'NR>1 && $4=="True"{print $1","$2}' "$BASE" | sort)
  cand_keys=$(awk -F, 'NR>1 && $4=="True"{print $1","$2}' "$CAND" | sort)
  only_base=$(comm -23 <(echo "$base_keys") <(echo "$cand_keys"))
  only_cand=$(comm -13 <(echo "$base_keys") <(echo "$cand_keys"))
  mismatch=0
  while IFS=, read -r g e; do
    [ -z "$g" ] && continue
    b=$(awk -F, -v g="$g" -v e="$e" 'NR>1&&$1==g&&$2==e&&$4=="True"{print $5","$6","$7","$8}' "$BASE")
    c=$(awk -F, -v g="$g" -v e="$e" 'NR>1&&$1==g&&$2==e&&$4=="True"{print $5","$6","$7","$8}' "$CAND")
    if [ -n "$b" ] && [ -n "$c" ] && [ "$b" != "$c" ]; then
      echo "MISMATCH g$g e$e: base[$b] cand[$c]"; mismatch=1
    fi
  done <<< "$(awk -F, 'NR>1 && $4=="True"{print $1","$2}' "$BASE" | sort)"
  [ -n "$only_base" ] && { echo "SOLVED SET SHRANK (was solved in base only):"; echo "$only_base"; mismatch=1; }
  [ -n "$only_cand" ] && { echo "solved set grew (ok, new solves):"; echo "$only_cand"; }
  if [ "$mismatch" -eq 1 ]; then echo "INVARIANT FAILED"; exit 1; else echo "INVARIANT OK (solved set grew)"; fi
fi

# Geo-mean speedup on commonly solved instances with time > 0.01s
join -t, -j1 \
  <(awk -F, 'NR>1&&$4=="True"{print $1"-"$2","$3}' "$BASE" | sort) \
  <(awk -F, 'NR>1&&$4=="True"{print $1"-"$2","$3}' "$CAND" | sort) 2>/dev/null |
awk -F, '$2>0.01 && $3>0 {n++; s+=log($2/$3)} END{if(n>0) printf "geo-mean speedup on %d instances (>10ms): %.2fx\n", n, exp(s/n)}'
