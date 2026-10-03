#!/bin/bash
# ─────────────────────────────────────────────────────────────────────────────
# HBAE (HOG2 BAE*, bidirectional TT2) — j30 SERVER RUN. 300s/instance, 4 arms:
#   HBAE-b  BAE* priority  b = 2g + h - h_opp   (RCPSP_BI_PRIO=0, HOG2's own ordering)
#   HBAE-f  plain f order  f = g + h            (RCPSP_BI_PRIO=1)
#   fwd     forward TT2    tt2_j30     DR5 + DR4 + batch
#   bwd     backward TT2   tt2rev_j30  DR5 + DR4 + batch
#
# Shared HBAE settings (correctness first):
#   RCPSP_BI_DR5=3 RCPSP_BI_DR4=1 RCPSP_BI_BATCH=0
#   RCPSP_BI_NIP=0        lazy nipping OFF: ours matches on the many-to-one cut key, not the exact
#                         state as HOG2 does, which leaves a hole in the proof
#   RCPSP_BI_DRAINCERT=1  an empty OPEN counts as proof even with dominance on that side
#                         (the code also requires nipping off + no discard)
#   defaults kept         RCPSP_BI_NOUB=1 (keep f>=U nodes), RCPSP_BI_FBOUND=1 (stop on the
#                         f-bound, whatever the expansion order)
#   RCPSP_BI_ASSERT=1     an arm HALTS on the first certified answer != known optimum
#
# UPLOAD FIRST:  HOG2/RCPSP: Driver.cpp BAE_RCPSP.h DominanceTT2.h RCPSPState.cpp
#                  (+ Globals.h RCPSP.h RCPSPState.h SubsetSolver.h) run_server_hbae.sh
#                repo root:  known_optima.csv   (untracked; the correctness gate reads it)
# RE-RUN ONLY THE ANALYSIS:
#   ANALYZE_ONLY=1 HB0=<b.csv> HB1=<f.csv> FW=<fwd.csv> BW=<bwd.csv> bash run_server_hbae.sh
# ─────────────────────────────────────────────────────────────────────────────
set -u
SET=${SET:-main}   # main = HBAE f / f-batch on j60+j90;  SET=speed = j30 check of the per-node speedups + h fix
                   # SET=unfire = HBAE with the un-fire backward, j30
                   # SET=unfire is KNOWN UNSOUND (local 2026-09-10: j30 1:1 certifies 48, optimum 43; un-fire dead-ends) -- do not run
ROOT=${ROOT:-~/RCPSP_With_Petri_nets}
RD="$ROOT/HOG2/RCPSP"
OPT=${OPT:-"$ROOT/known_optima.csv"}
NR="$ROOT/new_results"   # every CSV of every size lands here (RCPSP_RESULTS_DIR, set below)
L="$RD/logs"
HB0=${HB0:-}; HB1=${HB1:-}; FW=${FW:-}; BW=${BW:-}

# awk expression: did this row prove optimality? (HBAE has `proven`, TT2 has `solved`)
isproven='("proven" in c) ? ($c["proven"]==1) : (("solved" in c) && ($c["solved"]=="True" || $c["solved"]=="1"))'

gate(){ # label csv — every proven/solved row against the known optimum
  awk -F, -v LBL="$1" -v SZ="${3:-j30}" 'NR==FNR{ if($2==SZ) o[$3","$4]=$5; next }
    FNR==1{ for(i=1;i<=NF;i++) c[$i]=i; if(!("makespan" in c)){ nomk=1; exit }; next }
    { n++; k=$1","$2; if(!(k in o)) next
      pv = '"$isproven"'
      if(pv){ p++; if($c["makespan"]+0 != o[k]+0){ b++; if(b<=5) printf "    WRONG %s g%s e%s got=%s opt=%s\n",LBL,$1,$2,$c["makespan"],o[k] } }
      else if(("ub" in c) && $c["ub"]+0!=-1 && $c["ub"]+0<o[k]+0) ub++ }
    END{ if(nomk){ printf "  %-7s CSV has no makespan column -- CANNOT GATE\n",LBL; exit }
         printf "  %-7s rows=%d  proven=%d (%.1f%%)  WRONG=%d  impossible-UB=%d\n",LBL,n,p,(n?100*p/n:0),b+0,ub+0 }' "$OPT" "$2"; }

tstats(){ # label csv — time distribution over ALL rows (a timeout counts as the cap)
  awk -F, 'NR==1{for(i=1;i<=NF;i++)c[$i]=i; if(!("time" in c)) exit; next}{print $c["time"]}' "$2" | sort -g | \
  awk -v LBL="$1" 'function q(p){ return t[int(p*(NR-1))+1] }
    { t[NR]=$1; s+=$1 }
    END{ if(NR) printf "  %-7s mean=%.3fs  median=%.3fs  p90=%.3fs  p99=%.2fs  max=%.1fs\n",LBL,s/NR,q(.5),q(.9),q(.99),t[NR] }'; }

h2h(){ # label hbae.csv — against fwd and bwd, on instances all three proved
  [ -f "$2" ] && [ -f "$FW" ] && [ -f "$BW" ] || { echo "  $1: missing CSV"; return; }
  awk -F, -v LBL="$1" '
    FNR==1{ fi++; delete c; for(i=1;i<=NF;i++) c[$i]=i; if(!("time" in c) || !("expandNumber" in c)) miss=1; next }
    { k=$1","$2; t=$c["time"]+0; e=$c["expandNumber"]+0; ok='"$isproven"'
      if(!ok) next
      if(fi==1){H[k]=t; HE[k]=e} else if(fi==2){F[k]=t; FE[k]=e} else {B[k]=t; BE[k]=e} }
    END{ if(miss){ printf "  %s: a CSV lacks time/expandNumber -- skipped\n", LBL; exit }
         for(k in H) if((k in F)&&(k in B)){ n++; sh+=H[k]; sf+=F[k]; sb+=B[k]; m=(F[k]<B[k]?F[k]:B[k]); sm+=m
           eh+=HE[k]; ef+=FE[k]; eb+=BE[k]; if(H[k]<F[k]) hf++; if(H[k]<m) hm++ }
         printf "  %s vs fwd/bwd, instances all three proved: %d\n", LBL, n
         if(n){ printf "    mean time      %s=%.3fs  fwd=%.3fs  bwd=%.3fs  best-of(fwd,bwd)=%.3fs\n", LBL, sh/n, sf/n, sb/n, sm/n
                printf "    mean expanded  %s=%.0f  fwd=%.0f  bwd=%.0f\n", LBL, eh/n, ef/n, eb/n
                printf "    faster than fwd on %d/%d, faster than the better of fwd/bwd on %d/%d\n", hf+0, n, hm+0, n } }' "$2" "$FW" "$BW"; }

analyze(){
  echo ""; echo "=== 0. CORRECTNESS GATE — read this first; any WRONG invalidates that arm ==="
  for a in "HBAE-b:$HB0" "HBAE-f:$HB1" "fwd:$FW" "bwd:$BW"; do
    f=${a#*:}; if [ -f "$f" ]; then gate "${a%%:*}" "$f"; else echo "  ${a%%:*}: no CSV"; fi; done
  for lg in "$L/j30_hbae_b.log" "$L/j30_hbae_f.log"; do [ -f "$lg" ] && grep -A8 "FALSE RESULT" "$lg" | head -11; done

  echo ""; echo "=== 1. TIME PER INSTANCE (all rows; a timeout counts as the cap) ==="
  for a in "HBAE-b:$HB0" "HBAE-f:$HB1" "fwd:$FW" "bwd:$BW"; do f=${a#*:}; [ -f "$f" ] && tstats "${a%%:*}" "$f"; done

  echo ""; echo "=== 2. HEAD TO HEAD against forward / backward TT2 ==="
  h2h "HBAE-b" "$HB0"; h2h "HBAE-f" "$HB1"

  echo ""; echo "=== 3. HBAE: how did it stop? (bound | fwd/bwd-drained = empty OPEN | deadline | trivial) ==="
  for a in "HBAE-b:$HB0" "HBAE-f:$HB1"; do f=${a#*:}; [ -f "$f" ] && awk -F, -v LBL="${a%%:*}" \
    'NR==1{for(i=1;i<=NF;i++)c[$i]=i; next}{s[$c["stopReason"]]++; n++}
     END{printf "  %-7s", LBL; for(k in s) printf " %s=%d(%.0f%%)",k,s[k],100*s[k]/n; printf "\n"}' "$f"; done

  echo ""; echo "=== 4. HBAE: where do the frontiers meet? meetDepth = #started/NT, 0.5 = true middle ==="
  for a in "HBAE-b:$HB0" "HBAE-f:$HB1"; do f=${a#*:}; [ -f "$f" ] && awk -F, -v LBL="${a%%:*}" \
    'NR==1{for(i=1;i<=NF;i++)c[$i]=i; next}
     $c["stopReason"]!="trivial"{ a[$c["meetAt"]]++; d=$c["meetDepth"]+0; if(d>=0){ s+=d; m++; if(d>=.4&&d<=.6) mid++ } }
     END{ printf "  %-7s meetAt:", LBL; for(k in a) printf " %s=%d",k,a[k]
          if(m) printf "   meetDepth mean=%.3f  in [0.4,0.6]: %d/%d", s/m, mid, m; printf "\n" }' "$f"; done

  echo ""; echo "=== 5. HBAE: WHO DID THE WORK? (rows where the search ran) ==="
  for a in "HBAE-b:$HB0" "HBAE-f:$HB1"; do f=${a#*:}; [ -f "$f" ] || continue
    head -1 "$f" | grep -q expandedF || { echo "  ${a%%:*}: no per-side columns (older binary)"; continue; }
    awk -F, -v LBL="${a%%:*}" 'function v(k){ return (k in c) ? $(c[k])+0 : 0 }
      NR==1{ for(i=1;i<=NF;i++) c[$i]=i; next }
      v("expandNumber")>0 { n++; ef=v("expandedF"); eb=v("expandedB"); EF+=ef; EB+=eb; tf+=v("timeFSec"); tb+=v("timeBSec")
        sh=(ef+eb>0) ? ef/(ef+eb) : 0; if(sh>0.9) fw++; else if(sh<0.1) bw++; else bal++
        if(v("peakRSSMB")>rss) rss=v("peakRSSMB") }
      END{ if(!n || EF+EB<=0) exit
           printf "  %-7s n=%d  expansions fwd/bwd = %.0f%% / %.0f%%   time in Expand fwd/bwd = %.0fs / %.0fs\n", LBL, n, 100*EF/(EF+EB), 100*EB/(EF+EB), tf, tb
           printf "          one side did >90%% of the expansions: forward %d, backward %d; balanced %d   peak RSS %.0f MB\n", fw+0, bw+0, bal+0, rss }' "$f"
  done

  echo ""; echo "=== 6. STOPPING ON max(min f_F, min f_B) INSTEAD OF THE AVERAGE: SAFE? HOW MUCH SOONER? ==="
  echo "    Shadow test (128-expansion resolution); the search itself still stops on the average."
  echo "    WRONG = at the moment max would have stopped, the incumbent was above the known optimum."
  for a in "HBAE-b:$HB0" "HBAE-f:$HB1"; do f=${a#*:}
    if [ -f "$f" ] && head -1 "$f" | grep -q expAtMaxBound; then
      awk -F, -v LBL="${a%%:*}" 'NR==FNR{ if($2=="j30") o[$3","$4]=$5; next }
        FNR==1{ for(i=1;i<=NF;i++) c[$i]=i; next }
        { e=$(c["expAtMaxBound"])+0; if(e<0) next; k=$1","$2; n++; u=$(c["uAtMaxBound"])+0; tot=$(c["expandNumber"])+0
          pr=($(c["proven"])=="True" || $(c["proven"])==1)
          if(k in o){ chk++; if(u>o[k]+0){ bad++; if(bad<=5) printf "    %s WRONG g%s e%s incumbent=%s optimum=%s\n", LBL, $1, $2, u, o[k] } }
          if(pr && tot>0){ pv++; sv+=1-e/tot; if(e<tot) sooner++ } else if(!pr) unp++ }
        END{ printf "  %-7s rows where max would have stopped: %d   checked against known optimum: %d   WRONG: %d\n", LBL, n+0, chk+0, bad+0
             if(pv) printf "          proven rows: max stops sooner on %d of %d, mean expansions saved %.0f%%\n", sooner+0, pv, 100*sv/pv
             printf "          unproven rows max would have closed: %d\n", unp+0 }' "$OPT" "$f"
    else echo "  ${a%%:*}: no CSV with the shadow columns"; fi
  done

  echo ""; echo "=== 7. WHERE THE TIME GOES (rows where the search ran; % of search time) ==="
  echo "    hash = RCPSP_TT2::GetStateHash for TT2; for HBAE the string-built identity and meet hashes"
  for a in "HBAE-b:$HB0" "HBAE-f:$HB1" "fwd:$FW" "bwd:$BW"; do f=${a#*:}; [ -f "$f" ] || continue
    awk -F, -v LBL="${a%%:*}" 'function v(k){ return (k in c) ? $(c[k])+0 : 0 }
      NR==1{ for(i=1;i<=NF;i++) c[$i]=i; if(!("profSuccSec" in c)){ none=1; exit }; bi=("biHashSec" in c); next }
      v("expandNumber")>0 { n++; T+=(bi ? v("time") : v("profAstarSec")); S+=v("profSuccSec"); AV+=v("profAvailSec"); G+=v("profGenSec")
        D+=v("profDR5Sec"); H+=v("heurTimeSec"); CP+=v("profHeurCPSec"); HR+=v("profHeurResSec")
        HS+=(bi ? v("biHashSec")+v("meetHashSec") : v("profHashSec")); E+=v("expandNumber") }
      END{ if(none){ printf "  %-7s no profiling columns\n", LBL; exit }
           if(!n || T<=0) exit
           printf "  %-7s n=%d time=%.0fs | succ %.0f%% (avail %.0f, gen %.0f, DR5 %.0f) | heur %.0f%% (CP %.0f, res %.0f) | hash %.0f%% | other %.0f%% | %.1f us/exp\n", LBL, n, T, 100*S/T, 100*AV/T, 100*G/T, 100*D/T, 100*H/T, 100*CP/T, 100*HR/T, 100*HS/T, 100*(T-S-H-HS)/T, 1e6*T/(E>0?E:1) }' "$f"
  done

  echo ""; echo "=== 8. FLAGS AS ACTUALLY RUN ==="
  for a in "HBAE-b:$HB0" "HBAE-f:$HB1"; do f=${a#*:}; [ -f "$f" ] && awk -F, -v LBL="${a%%:*}" \
    'function v(k){ return (k in c) ? $(c[k]) : "?" } NR==1{for(i=1;i<=NF;i++)c[$i]=i; next} NR==2{printf "  %-7s model=%s prio=%s dr5=%s dr4=%s batch=%s nip=%s noub=%s fbound=%s drainCert=%s cap=%s\n",LBL,v("model"),v("prio"),v("useTT2DR5"),v("dr4"),v("batch"),v("nip"),v("noub"),v("fbound"),v("drainCert"),v("timeoutS"); exit}' "$f"; done
  for a in "fwd:$FW" "bwd:$BW"; do f=${a#*:}; [ -f "$f" ] && awk -F, -v LBL="${a%%:*}" \
    'function v(k){ return (k in c) ? $(c[k]) : "?" } NR==1{for(i=1;i<=NF;i++)c[$i]=i; next} NR==2{printf "  %-7s model=%s dr5=%s dr4=%s batch=%s direction=%s cap=%s\n",LBL,v("model"),v("useTT2DR5"),v("useTT2DR4"),v("useTT2Batch"),v("direction"),v("timeoutS"); exit}' "$f"; done
}

if [ -n "${ANALYZE_ONLY:-}" ]; then analyze; exit 0; fi

cd "$RD" || { echo "cd failed"; exit 1; }
[ -f "$OPT" ] || { echo "ABORT: $OPT missing -- scp known_optima.csv to the repo root"; exit 1; }
mkdir -p "$L" "$NR"
export RCPSP_RESULTS_DIR="$NR"   # the binary writes every CSV here, whatever directory an arm starts in

echo "=== BUILD ($(date)) ==="
g++ -std=c++20 -fopenmp -O3 -I../.. -I../gui/STUB Driver.cpp -o Driver_hbae_$SET 2> build_hbae_${SET}_err.txt \
  && echo "BUILD OK" || { echo "BUILD FAILED"; tail -40 build_hbae_${SET}_err.txt; exit 1; }
BIN="$RD/Driver_hbae_$SET"

HB="RCPSP_MEET_NORM=1 RCPSP_BI_DR5=3 RCPSP_BI_DR4=1 RCPSP_BI_BATCH=0 RCPSP_BI_ALT=0 RCPSP_BI_NIP=0 RCPSP_BI_DRAINCERT=1 RCPSP_BI_ASSERT=1 RCPSP_TT2_PROF=1"
TT2="RCPSP_TT2_DR5=1 RCPSP_TT2_DR4=1 RCPSP_TT2_BATCH=1 RCPSP_TT2_PROF=1"
CAP="RCPSP_TIMEOUT_S=300"

# PREFLIGHT (seconds): proves the uploaded files have the fixes and the flags are honoured.
# j30 3:9 (opt 65) is only proven through an empty OPEN under dominance, so it needs
# DRAINCERT + NIP=0; j30 2:2 (opt 51) was never solved before the fixes.
echo "=== PREFLIGHT ==="
pf(){ env $HB RCPSP_BI_PRIO=1 RCPSP_TIMEOUT_S=60 RCPSP_BI_ASSERT=0 RCPSP_BI_MINF_CHECK=1 "$BIN" tt2hbae j30 $1 $2 2>&1 | grep '^HBAE' | tail -1; }
P1=$(pf 3 9); P2=$(pf 2 2); echo "  $P1"; echo "  $P2"
if echo "$P1" | grep -q 'makespan=65 .*proven=1' && echo "$P2" | grep -q 'makespan=51 .*proven=1' \
   && echo "$P1" | grep -q ' dr4=1' && echo "$P1" | grep -q 'meetSide='; then echo "  OK"
else echo "  ABORT: preflight did not reproduce the gated behaviour (stale files or ignored flags)"; exit 1; fi
if [ "$SET" = unfire ]; then
  P3=$(env $HB RCPSP_BI_BWD=unfire RCPSP_BI_DR5=1 RCPSP_BI_PRIO=1 RCPSP_TIMEOUT_S=60 RCPSP_BI_ASSERT=0 "$BIN" tt2hbae j30 2 2 2>&1 | grep '^HBAE' | tail -1)
  echo "  unfire 2:2: $P3"
  echo "$P3" | grep -q 'bwd=unfire' || { echo "  ABORT: RCPSP_BI_BWD=unfire not honoured (stale Driver.cpp)"; exit 1; }
fi

BEFORE=$(ls "$NR"/*.csv 2>/dev/null | sort)
SIZES=${SIZES:-"j60 j90"}
# Picked from the j30 server run (4 arms x 480, 0 wrong): f order beat b order on time and expansions.
# Batch is active (expansions differ on 345/360 searched j30 rows) but net-neutral on j30; its benefit
# grows with instance size, so j60/j90 test f with and without batch.
# DR settings come from $HB (DR5 both + DR4, batch off); later env assignments win.
ARMS=(
  "f|RCPSP_BI_PRIO=1"                                   # f = g + h order, batch off
  "f-batch|RCPSP_BI_PRIO=1 RCPSP_BI_BATCH=1"            # same, batch on (the default TT2 config)
)
if [ "$SET" = unfire ]; then
  # STEP 1 of the combined model: backward = un-fire on the forward net, one predecessor per activity,
  # every edge re-fired and checked (RCPSP_BWD_FULLVERIFY default 1). Everything else as HBAE.
  # DR5 forward side only (the forward DR5 rule is unsound on un-fire states); a drained backward
  # queue never counts as proof. The correctness gate decides whether step 2 (b-bound) is worth it.
  SIZES=${SIZES_UNFIRE:-j30}
  ARMS=(
    "uf-f|RCPSP_BI_PRIO=1 RCPSP_BI_BWD=unfire RCPSP_BI_DR5=1"
    "uf-b|RCPSP_BI_PRIO=0 RCPSP_BI_BWD=unfire RCPSP_BI_DR5=1"
  )
fi
if [ "$SET" = speed ]; then
  # Per-node speedups (numeric hashes, meet key once per child, no state copies). f and b run the SAME
  # search as the previous j30 f/b runs (v01), so expansions must match those CSVs row for row and the
  # time difference is the pure per-node gain. f-hfuse1 = zero-time children keep the parent's h
  # (RCPSP_BI_HFUSE=1): correct but more expansions on 3 local instances; measured here on all 480.
  SIZES=${SIZES_SPEED:-j30}
  ARMS=(
    "f|RCPSP_BI_PRIO=1"
    "b|RCPSP_BI_PRIO=0"
    "f-hfuse1|RCPSP_BI_PRIO=1 RCPSP_BI_HFUSE=1"
  )
fi
echo "=== BI ARMS ($SET): $SIZES ($(date)) ==="
for sz in $SIZES; do
  dir="$ROOT"; [ "$sz" = j30 ] && dir="$RD"              # j60/j90 data (and j60lb.sm/j90lb.sm) resolve from the repo root
  for a in "${ARMS[@]}"; do
    n=${a%%|*}; e=${a#*|}
    ( cd "$dir" && env $HB $e $CAP "$BIN" tt2hbae_$sz > "$L/${sz}_bi_$n.log" 2>&1 ) &
    sleep 3
  done
done
wait
echo "=== ARMS DONE ($(date)) ==="

for sz in $SIZES; do
for f in $(comm -13 <(echo "$BEFORE") <(ls "$NR"/HBAE_${sz}_*.csv 2>/dev/null | { if [ "$SET" = unfire ]; then grep unfire; else grep -v unfire; fi; } | sort)); do
  echo ""; echo "##### $(basename "$f")   rows=$(( $(wc -l < "$f") - 1 ))"
  gate "gate" "$f" "$sz"; tstats "time" "$f"
  awk -F, 'function v(k){ return (k in c) ? $(c[k]) : "" }
    NR==1{ for(i=1;i<=NF;i++) c[$i]=i; next }
    { st[v("stopReason")]++; if(v("stopReason")=="trivial") next
      n++; at[v("meetAt")]++; sd[v("meetSide")]++
      if(v("meetDepth")+0>=0){ md+=v("meetDepth"); mn++ }
      if(v("meetTimeFrac")+0>=0){ tf+=v("meetTimeFrac"); tn++ }
      dF+=v("maxDepthF"); dB+=v("maxDepthB"); T+=v("time"); FB+=v("fBoundSec"); E+=v("expandNumber") }
    END{ printf "  stop:"; for(k in st) printf " %s=%d", k, st[k]; printf "\n"
         printf "  meetAt:"; for(k in at) printf " %s=%d", k, at[k]; printf "   meetSide:"; for(k in sd) printf " %s=%d", k, sd[k]; printf "\n"
         if(n) printf "  searched=%d  meetDepth(activities) mean=%.2f  meetTimeFrac(gF/U) mean=%.2f  maxDepth F/B mean=%.2f/%.2f  f-bound time %.1f%%  %.1f us/exp\n", n, (mn?md/mn:-1), (tn?tf/tn:-1), dF/n, dB/n, (T>0?100*FB/T:0), (E>0?1e6*T/E:0) }' "$f"
  awk -F, 'NR==1{ for(i=1;i<=NF;i++) c[$i]=i; next } { t[$(c["instanceRS"])]++; if($(c["proven"])==1) p[$(c["instanceRS"])]++ }
    END{ printf "  proven by RS:"; for(k in t) printf " %s=%d/%d", k, p[k]+0, t[k]; printf "\n" }' "$f"
done
done
echo ""; echo "=== FALSE RESULTS (empty = none) ==="
grep -h -A8 "FALSE RESULT" "$L"/*_bi_*.log | head -40
grep -l "Could not open" "$L"/*_bi_*.log 2>/dev/null | sed 's/^/  WARNING reference bounds file not found (fail-fast inactive): /'
echo ""; echo "=== ALL DONE ($(date)) ==="
