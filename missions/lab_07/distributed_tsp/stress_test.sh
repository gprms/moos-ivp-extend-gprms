#!/bin/bash
#----------------------------------------------------------
#  stress_test.sh  ::  Lab 7 Distributed TSP
#  Runs the mission repeatedly in both assignment modes and
#  tallies reliability metrics. Headless (no GUI).
#
#  Usage: ./stress_test.sh [runs_per_mode] [warp] [secs]
#----------------------------------------------------------
RUNS=${1:-5}
WARP=${2:-10}
SECS=${3:-22}
OUT=stress_results.csv

echo "run,mode,view_points,henry_assigned,gilda_assigned,henry_recv,gilda_recv,henry_route_v,gilda_route_v,henry_moved,gilda_moved,result" > $OUT

run_one() {
  local mode=$1     # "alt" or "region"
  local idx=$2
  local regflag=""
  [ "$mode" = "region" ] && regflag="--region"

  pkill -f "targ_shoreside.moos|targ_henry.moos|targ_gilda.moos" 2>/dev/null; sleep 1
  ./clean.sh >/dev/null 2>&1
  ./launch.sh --headless=$SECS $regflag $WARP >/dev/null 2>&1
  pkill -f "targ_shoreside.moos|targ_gilda.moos|targ_henry.moos" 2>/dev/null; sleep 1

  local SH=$(ls -dt LOG_SHORESIDE_*/ 2>/dev/null | head -1); SH=$(ls ${SH}*.alog 2>/dev/null)
  local HEN=$(ls -dt LOG_HENRY_*/ 2>/dev/null | head -1); HEN=$(ls ${HEN}*.alog 2>/dev/null)
  local GIL=$(ls -dt LOG_GILDA_*/ 2>/dev/null | head -1); GIL=$(ls ${GIL}*.alog 2>/dev/null)

  local vp=0 ha=0 ga=0 hr=0 gr=0 hrv=0 grv=0 hm=0 gm=0
  if [ -n "$SH" ]; then
    vp=$(grep -c ' VIEW_POINT ' "$SH")
    ha=$(grep ' VISIT_POINT_HENRY ' "$SH" | grep -c 'x=')
    ga=$(grep ' VISIT_POINT_GILDA ' "$SH" | grep -c 'x=')
  fi
  if [ -n "$HEN" ]; then
    hr=$(grep ' VISIT_POINT ' "$HEN" | grep -c 'x=')
    hrv=$(grep ' WPT_UPDATES ' "$HEN" | grep -i 'pts=' | head -1 | grep -oE '[0-9.-]+,[0-9.-]+' | wc -l | tr -d ' ')
    local hx0=$(grep ' NAV_X ' "$HEN"|head -1|awk '{print $4}')
    local hxn=$(grep ' NAV_X ' "$HEN"|awk '{print $4}'|sort -n|tail -1)
    hm=$(awk -v a="$hx0" -v b="$hxn" 'BEGIN{d=b-a; if(d<0)d=-d; print (d>2)?1:0}')
  fi
  if [ -n "$GIL" ]; then
    gr=$(grep ' VISIT_POINT ' "$GIL" | grep -c 'x=')
    grv=$(grep ' WPT_UPDATES ' "$GIL" | grep -i 'pts=' | head -1 | grep -oE '[0-9.-]+,[0-9.-]+' | wc -l | tr -d ' ')
    local gx0=$(grep ' NAV_X ' "$GIL"|head -1|awk '{print $4}')
    local gxn=$(grep ' NAV_X ' "$GIL"|awk '{print $4}'|sort -n|tail -1)
    gm=$(awk -v a="$gx0" -v b="$gxn" 'BEGIN{d=b-a; if(d<0)d=-d; print (d>2)?1:0}')
  fi

  # PASS if: ~100 assigned total, both vehicles received >0, both generated a route
  local total=$((ha+ga))
  local res="FAIL"
  if [ "$total" -ge 95 ] && [ "$hr" -gt 0 ] && [ "$gr" -gt 0 ] && [ "$hrv" -gt 0 ] && [ "$grv" -gt 0 ]; then
    res="PASS"
  fi
  echo "$idx,$mode,$vp,$ha,$ga,$hr,$gr,$hrv,$grv,$hm,$gm,$res" >> $OUT
  echo "  run $idx [$mode]: assigned=$total recv(h/g)=$hr/$gr route_v(h/g)=$hrv/$grv moved(h/g)=$hm/$gm -> $res"
}

echo "=== Stress test: $RUNS runs/mode, warp $WARP, ${SECS}s each ==="
echo "--- Alternating mode ---"
for i in $(seq 1 $RUNS); do run_one alt $i; done
echo "--- Region (East/West) mode ---"
for i in $(seq 1 $RUNS); do run_one region $i; done

echo
echo "=== SUMMARY ==="
total=$(( RUNS * 2 ))
pass=$(grep -c ',PASS' $OUT)
echo "PASS: $pass / $total"
echo "Results written to $OUT"
pkill -f "targ_shoreside.moos|targ_henry.moos|targ_gilda.moos" 2>/dev/null
