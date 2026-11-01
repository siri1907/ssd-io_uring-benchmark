# Milestone 2 - Pasha's tasks (2.3, 2.4, 2.7)

Build (same as Milestone 1):  `make LIBURING=$HOME/liburing-inst`   (or just `make` if liburing is installed)
Run everything on the **local SSD** of Babbage, on one quiet core, e.g. `taskset -c 5 ./pilot ...`

## 2.3  Measure SSD capacity and choose the loads
    ./mkfile /local/ssd/test.bin 1024            # 1 GB test file (same as Milestone 1)
    ./capacity --file /local/ssd/test.bin --depth 32 --out loads.conf
Sends reads as fast as the harness allows (policy 1, 3 repeats), takes the median reads/s as the capacity,
and writes `light_rps` (10% of capacity) and `moderate_rps` (50%). Change the fractions with `--light_frac`
and `--moderate_frac` if the team prefers other values. No device speed is assumed.

## 2.4  Calibration: pick three fixed spin durations
    ./calibrate --file /local/ssd/test.bin --loads loads.conf --out calibration.conf
Runs on its own trace (seed 9001; study traces use other seeds). Policy 1 shows how long reads take;
short/mid/long spin = p50 / p90 / p99 of that, rounded to 5 us. It then runs fixed spin-then-wait at those
and at extra candidates and prints a latency / CPU table (`calibration_table.csv`). Open question for the team:
the plan says Policy 3 has three fixed durations and the adaptive policy has three budgets *including zero*,
so the adaptive candidates are probably 0 plus two of these. Please agree this before Milestone 3.

## 2.7  Pilot of Study A (policies 1 to 3)
    ./pilot --file /local/ssd/test.bin --loads loads.conf --calib calibration.conf --duration 10 --reps 3
Runs wait, poll and the three spin durations at light and moderate load, 3 repeats each, same trace for all,
every read verified. Writes `pilot_results.csv` and prints a mean (min-max) table.

## Stand-ins (replace with the real code)
`src/policy_poll.h` is a stand-in for Medur's task 2.1 and `src/policy_spin.h` for Sirisha's task 2.2, so the
tools above can be tested. One small change to the shared interface: `common.h` now has `g_inflight`
(reads outstanding), which the harness sets before every `policy.wait()`. Policy 2 needs it to poll only while
reads are outstanding. Medur and Sirisha: use it, and keep `Policy::wait` as is.

## Tests
`TESTDIR=/path/on/ssd make test` (add `DIRECT=0` on a filesystem without O_DIRECT).
