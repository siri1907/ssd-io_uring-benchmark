// calibrate - plan task 2.4: choose three fixed spin durations, on a SEPARATE calibration trace.
//
// 1. Run policy 1 (wait) on the calibration trace and look at how long reads take to finish
//    (submit -> complete). A spin only helps if the read finishes inside the spin window, so:
//       short = median (p50), mid = p90, long = p99   (rounded to 5 us, forced to be increasing)
// 2. Run fixed spin-then-wait at those durations and at a list of extra candidates, and print a table of
//    latency and CPU per read, so the team can see the trade-off and confirm or change the choice.
// The calibration seed differs from the study seeds, so the final results are not tuned to the test data.
//
//   ./calibrate --file test.bin --loads loads.conf --out calibration.conf
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <set>

#include "conf.h"
#include "policies.h"
#include "summary.h"

static double round5(double us) { return std::max(5.0, std::round(us / 5.0) * 5.0); }

int main(int argc, char** argv) {
    try {
        Args a(argc, argv, {"loads"});
        HarnessOptions opt;
        opt.file = a.str("file");
        opt.depth = static_cast<unsigned>(a.num("depth", 32));
        opt.direct = a.num("direct", 1) != 0;
        const double rate = a.num("rate", a.has("moderate_rps") ? a.num("moderate_rps") : 0);
        if (rate <= 0) throw std::runtime_error("give --loads loads.conf (from ./capacity) or --rate");
        const double dur = a.num("duration", 5);
        const uint64_t seed = static_cast<uint64_t>(a.num("seed", 9001));  // study traces use other seeds
        const std::string out = a.str("out", "calibration.conf"), table = a.str("table", "calibration_table.csv");

        wl::Config wc;
        wc.seed = seed;
        wc.file_size = file_size_of(opt.file);
        auto trace = wl::generate_steady(wc, rate, dur);
        std::printf("calibration trace: %zu reads, %.0f reads/s for %.1f s, seed %llu\n", trace.size(), rate, dur, (unsigned long long)seed);

        Summary w = summarize(run_harness(opt, trace, kPolicyWaitImmediately));
        if (w.errors || w.verify_failures) throw std::runtime_error("read errors or wrong data during calibration");
        double s1 = round5(w.lat_p50_us), s2 = round5(w.lat_p90_us), s3 = round5(w.lat_p99_us);
        if (s2 <= s1) s2 = s1 + 5;
        if (s3 <= s2) s3 = s2 + 5;
        std::printf("wait: latency p50 %.1f  p90 %.1f  p99 %.1f us  ->  spin durations %.0f / %.0f / %.0f us\n",
                    w.lat_p50_us, w.lat_p90_us, w.lat_p99_us, s1, s2, s3);

        std::set<double> cands = {0, s1, s2, s3};
        std::string list = a.str("candidates", "5,10,20,50,100,200");
        for (size_t p = 0; p < list.size();) {
            size_t c = list.find(',', p); if (c == std::string::npos) c = list.size();
            cands.insert(std::atof(list.substr(p, c - p).c_str())); p = c + 1;
        }

        FILE* t = std::fopen(table.c_str(), "w");
        if (!t) throw std::runtime_error("cannot write " + table);
        std::fprintf(t, "policy,spin_us,reads_per_s,lat_p50_us,lat_p99_us,cpu_us_per_read,chosen\n");
        std::printf("\n%-10s %8s %10s %9s %9s %12s\n", "policy", "spin_us", "reads/s", "p50 us", "p99 us", "CPU us/read");
        auto row = [&](const char* name, double spin, const Summary& s, bool chosen) {
            std::printf("%-10s %8.0f %10.0f %9.1f %9.1f %12.2f %s\n", name, spin, s.rps, s.lat_p50_us, s.lat_p99_us, s.cpu_us_per_read, chosen ? " <- chosen" : "");
            std::fprintf(t, "%s,%.0f,%.0f,%.1f,%.1f,%.2f,%d\n", name, spin, s.rps, s.lat_p50_us, s.lat_p99_us, s.cpu_us_per_read, chosen);
        };
        row("wait", 0, w, false);
        for (double c : cands) {
            if (c == 0) continue;
            Summary s = summarize(run_harness(opt, trace, *pick_policy("spin", c)));
            if (s.errors || s.verify_failures) throw std::runtime_error("read errors or wrong data during calibration");
            row("spin", c, s, c == s1 || c == s2 || c == s3);
        }
        Summary p = summarize(run_harness(opt, trace, kPolicyPoll));
        row("poll", 0, p, false);
        std::fclose(t);

        FILE* f = std::fopen(out.c_str(), "w");
        if (!f) throw std::runtime_error("cannot write " + out);
        std::fprintf(f, "# written by ./calibrate on the calibration trace (seed %llu, %.0f reads/s)\n"
                        "# adaptive candidate budgets: 0 plus two of these (team to confirm)\n"
                        "spin_short_us=%.0f\nspin_mid_us=%.0f\nspin_long_us=%.0f\n", (unsigned long long)seed, rate, s1, s2, s3);
        std::fclose(f);
        std::printf("\nwrote %s and %s\n", out.c_str(), table.c_str());
        return 0;
    } catch (const std::exception& e) { std::fprintf(stderr, "error: %s\n", e.what()); return 1; }
}
