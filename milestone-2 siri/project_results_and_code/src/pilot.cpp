// pilot - plan task 2.7: a small run of Study A (steady load) with policies 1 to 3, end to end.
//
// Policies: wait (1), poll (2), spin at the three calibrated durations (3). Loads: light and moderate.
// Every run replays the SAME trace (same seed), is repeated --reps times, and every read is verified.
// Runs go round-robin (repeat 1 of everything, then repeat 2, ...) so slow drift hits all policies alike.
//
//   ./pilot --file test.bin --loads loads.conf --calib calibration.conf --duration 10 --reps 3
#include <cmath>
#include <cstdio>
#include <map>

#include "conf.h"
#include "policies.h"
#include "summary.h"

struct Variant { std::string label, policy; double spin_us; };

int main(int argc, char** argv) {
    try {
        Args a(argc, argv, {"loads", "calib"});
        HarnessOptions opt;
        opt.file = a.str("file");
        opt.depth = static_cast<unsigned>(a.num("depth", a.has("depth") ? a.num("depth") : 32));
        opt.direct = a.num("direct", 1) != 0;
        const double dur = a.num("duration", 10);
        const int reps = static_cast<int>(a.num("reps", 3));
        const uint64_t seed = static_cast<uint64_t>(a.num("seed", 1));
        const std::string out = a.str("out", "pilot_results.csv");
        std::vector<std::pair<std::string, double>> loads = {{"light", a.num("light_rps")}, {"moderate", a.num("moderate_rps")}};
        std::vector<Variant> vars = {{"1 wait", "wait", 0}, {"2 poll", "poll", 0}};
        const char* names[3] = {"spin_short_us", "spin_mid_us", "spin_long_us"};
        for (int i = 0; i < 3; i++) {
            double us = a.num(names[i]);
            char lab[64]; std::snprintf(lab, sizeof lab, "3 spin %.0fus", us);
            vars.push_back({lab, "spin", us});
        }

        wl::Config wc;
        wc.seed = seed;
        wc.file_size = file_size_of(opt.file);
        std::map<std::string, std::vector<wl::Request>> traces;
        for (auto& l : loads) { traces[l.first] = wl::generate_steady(wc, l.second, dur); }

        FILE* f = std::fopen(out.c_str(), "w");
        if (!f) throw std::runtime_error("cannot write " + out);
        std::fprintf(f, "load,rate_rps,policy,repeat,reads,reads_per_s,lat_p50_us,lat_p99_us,resp_p50_us,resp_p99_us,cpu_us_per_read,errors,short_reads,verify_failures\n");
        struct Acc { std::vector<Summary> v; };
        std::map<std::string, Acc> acc;  // key: load|policy
        bool bad = false;
        for (int r = 1; r <= reps; r++)
            for (auto& l : loads)
                for (auto& v : vars) {
                    const Policy* pol = pick_policy(v.policy, v.spin_us);
                    Summary s = summarize(run_harness(opt, traces[l.first], *pol));
                    std::printf("repeat %d  %-8s %-14s  p50 %7.1f  p99 %7.1f us  CPU %6.2f us/read%s\n", r, l.first.c_str(), v.label.c_str(),
                                s.lat_p50_us, s.lat_p99_us, s.cpu_us_per_read, (s.errors || s.verify_failures) ? "  ** PROBLEM **" : "");
                    std::fflush(stdout);
                    std::fprintf(f, "%s,%.0f,%s,%d,%zu,%.0f,%.1f,%.1f,%.1f,%.1f,%.2f,%llu,%llu,%llu\n", l.first.c_str(), l.second, v.label.c_str(), r, s.n,
                                 s.rps, s.lat_p50_us, s.lat_p99_us, s.resp_p50_us, s.resp_p99_us, s.cpu_us_per_read,
                                 (unsigned long long)s.errors, (unsigned long long)s.short_reads, (unsigned long long)s.verify_failures);
                    if (s.errors || s.short_reads || s.verify_failures) bad = true;
                    acc[l.first + "|" + v.label].v.push_back(s);
                }
        std::fclose(f);

        std::printf("\n===== Study A pilot: mean over %d repeats (min-max in brackets) =====\n", reps);
        for (auto& l : loads) {
            std::printf("\n%s load: %.0f reads/s for %.0f s\n%-14s %20s %20s %20s\n", l.first.c_str(), l.second, dur, "policy", "p50 us", "p99 us", "CPU us/read");
            for (auto& v : vars) {
                auto& xs = acc[l.first + "|" + v.label].v;
                auto stat = [&](double Summary::*m, char* buf, size_t n) {
                    double lo = 1e18, hi = -1e18, sum = 0;
                    for (auto& s : xs) { lo = std::min(lo, s.*m); hi = std::max(hi, s.*m); sum += s.*m; }
                    std::snprintf(buf, n, "%.1f (%.1f-%.1f)", sum / xs.size(), lo, hi);
                };
                char b1[64], b2[64], b3[64];
                stat(&Summary::lat_p50_us, b1, 64); stat(&Summary::lat_p99_us, b2, 64); stat(&Summary::cpu_us_per_read, b3, 64);
                std::printf("%-14s %20s %20s %20s\n", v.label.c_str(), b1, b2, b3);
            }
        }
        std::printf("\nwrote %s  (%s)\n", out.c_str(), bad ? "SOME READS FAILED - see ** PROBLEM ** lines" : "all reads correct");
        return bad ? 1 : 0;
    } catch (const std::exception& e) { std::fprintf(stderr, "error: %s\n", e.what()); return 1; }
}
