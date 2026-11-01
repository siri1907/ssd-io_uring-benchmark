#!/bin/sh
# Milestone 2 checks: policies 2 and 3 stand-ins, capacity (2.3), calibration (2.4), pilot (2.7).
set -e
D=${TESTDIR:-./tmp_test_m2}; mkdir -p "$D"
DIRECT=${DIRECT:-1}
fail=0
check() { if [ "$2" = "$3" ]; then echo "ok:   $1"; else echo "FAIL: $1 (got '$2', wanted '$3')"; fail=1; fi; }
conf() { awk -F= -v k="$2" '$1==k {print $2}' "$1"; }
num_lt() { awk -v a="$1" -v b="$2" 'BEGIN{print (a<b)?"yes":"no"}'; }

./mkfile "$D/test.bin" 64 >/dev/null
./trace_gen --mode steady --rate 4000 --duration 1 --seed 5 --file-size 67108864 --out "$D/t.csv" >/dev/null
N=$(($(wc -l < "$D/t.csv") - 1))
get() { echo "$1" | awk -F': *' -v k="$2" '$1 ~ k {print $2}' | head -1 | awk '{print $1}'; }

# Policies 2 and 3 (lifecycle): every read finishes and is verified, including a zero spin budget.
for P in "poll 0" "spin 0" "spin 50" "spin 100000"; do
  set -- $P
  out=$(./run_bench --file "$D/test.bin" --trace "$D/t.csv" --direct $DIRECT --depth 8 --policy $1 --spin_us $2)
  check "policy $1 spin=$2 completes all reads"   "$(get "$out" '^requests')" "$N"
  check "policy $1 spin=$2 no wrong data/errors"  "$(get "$out" '^errors')/$(get "$out" '^verify failures')" "0/0"
done
# depth 1 (one read at a time) still works with polling
out=$(./run_bench --file "$D/test.bin" --trace "$D/t.csv" --direct $DIRECT --depth 1 --policy spin --spin_us 20)
check "spin with depth 1 completes all"  "$(get "$out" '^requests')" "$N"
# an unknown policy is a clear error
set +e; ./run_bench --file "$D/test.bin" --trace "$D/t.csv" --policy nope >/dev/null 2>&1; rc=$?; set -e
check "unknown policy reports an error" "$([ $rc -ne 0 ] && echo yes || echo no)" "yes"

# 2.3 capacity
./capacity --file "$D/test.bin" --direct $DIRECT --requests 5000 --reps 3 --out "$D/loads.conf" >/dev/null
CAP=$(conf "$D/loads.conf" capacity_rps); LT=$(conf "$D/loads.conf" light_rps); MD=$(conf "$D/loads.conf" moderate_rps)
check "capacity found (> 0)"            "$(num_lt 0 "$CAP")" "yes"
check "light < moderate < capacity"     "$(num_lt "$LT" "$MD")/$(num_lt "$MD" "$CAP")" "yes/yes"

# 2.4 calibration
./calibrate --file "$D/test.bin" --direct $DIRECT --rate 3000 --duration 1 --out "$D/calibration.conf" --table "$D/cal.csv" >/dev/null
S1=$(conf "$D/calibration.conf" spin_short_us); S2=$(conf "$D/calibration.conf" spin_mid_us); S3=$(conf "$D/calibration.conf" spin_long_us)
check "three spin durations, increasing" "$(num_lt "$S1" "$S2")/$(num_lt "$S2" "$S3")" "yes/yes"
check "calibration table has a poll row" "$(grep -c '^poll' "$D/cal.csv")" "1"
check "calibration table marks 3 chosen" "$(awk -F, '$7==1' "$D/cal.csv" | wc -l | tr -d ' ')" "3"

# 2.7 pilot (tiny): 5 policies x 2 loads x 2 repeats = 20 rows, nothing failed
./pilot --file "$D/test.bin" --direct $DIRECT --light_rps 1000 --moderate_rps 3000 --calib "$D/calibration.conf" --duration 1 --reps 2 --out "$D/pilot.csv" >/dev/null
check "pilot has one row per run (20)"   "$(($(wc -l < "$D/pilot.csv") - 1))" "20"
check "pilot: no errors or wrong data"   "$(tail -n +2 "$D/pilot.csv" | awk -F, '$12!=0 || $13!=0 || $14!=0' | wc -l | tr -d ' ')" "0"
check "pilot: every run read all requests" "$(tail -n +2 "$D/pilot.csv" | awk -F, '$5==0' | wc -l | tr -d ' ')" "0"

rm -rf "$D"
[ $fail -eq 0 ] && echo "MILESTONE 2 TESTS PASSED" || { echo "MILESTONE 2 TESTS FAILED"; exit 1; }
