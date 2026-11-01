// capacity - plan task 2.3: measure what the SSD can do, then choose the light and moderate loads.
//
// Sends reads as fast as the harness allows (all arrive at time 0, so the outstanding limit is the only
// brake), using policy 1. Repeats the test, takes the median reads/second as the capacity, and writes
// light and moderate request rates as fractions of it. No device speed is assumed.
//
//   ./capacity --file /local/ssd/test.bin --depth 32 --out loads.conf
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <random>

#include "conf.h"
#include "policies.h"
#include "summary.h"

int main(int argc, char** argv) {
    try {
        Args a(argc, argv);
        HarnessOptions opt;
        opt.file = a.str("file");
        opt.depth = static_cast<unsigned>(a.num("depth", 32));
        opt.direct = a.num("direct", 1) != 0;
        opt.verify = a.num("verify", 1) != 0;
        const size_t nreq = static_cast<size_t>(a.num("requests", 100000));
        const int reps = static_cast<int>(a.num("reps", 3));
        const double light_frac = a.num("light_frac", 0.10), mod_frac = a.num("moderate_frac", 0.50);
        const uint64_t seed = static_cast<uint64_t>(a.num("seed", 7));
        const std::string out = a.str("out", "loads.conf");

        const uint64_t slots = file_size_of(opt.file) / 4096;
        if (slots == 0) throw std::runtime_error("test file is smaller than 4 KiB");
        std::mt19937_64 rng(seed);
        std::vector<wl::Request> trace(nreq);
        for (size_t i = 0; i < nreq; i++) trace[i] = {i, 0, (rng() % slots) * 4096, 4096};

        std::printf("capacity test: %zu reads of 4 KiB, depth %u, %d repeats, policy wait\n", nreq, opt.depth, reps);
        std::vector<double> rates;
        for (int r = 0; r < reps; r++) {
            Summary s = summarize(run_harness(opt, trace, kPolicyWaitImmediately));
            if (s.errors || s.verify_failures) throw std::runtime_error("read errors or wrong data during the capacity test");
            std::printf("  repeat %d: %.0f reads/s (%.1f MB/s)\n", r + 1, s.rps, s.rps * 4096 / 1e6);
            rates.push_back(s.rps);
        }
        std::sort(rates.begin(), rates.end());
        const double cap = rates[rates.size() / 2];
        auto round100 = [](double x) { return std::max(100.0, std::floor(x / 100.0) * 100.0); };
        const double light = round100(cap * light_frac), mod = round100(cap * mod_frac);
        std::printf("capacity (median) : %.0f reads/s\nlight load        : %.0f reads/s (%.0f%% of capacity)\n"
                    "moderate load     : %.0f reads/s (%.0f%% of capacity)\n", cap, light, light_frac * 100, mod, mod_frac * 100);

        FILE* f = std::fopen(out.c_str(), "w");
        if (!f) throw std::runtime_error("cannot write " + out);
        std::fprintf(f, "# written by ./capacity - measured on this SSD, not assumed\ndepth=%u\ncapacity_rps=%.0f\n"
                        "light_rps=%.0f\nmoderate_rps=%.0f\n", opt.depth, cap, light, mod);
        std::fclose(f);
        std::printf("wrote %s\n", out.c_str());
        return 0;
    } catch (const std::exception& e) { std::fprintf(stderr, "error: %s\n", e.what()); return 1; }
}
