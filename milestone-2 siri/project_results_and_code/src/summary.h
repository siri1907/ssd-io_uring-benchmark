// summary.h - turn one RunResult into the numbers we compare (shared by the Milestone 2 tools).
#pragma once
#include <algorithm>
#include <vector>

#include "harness.h"

inline double percentile(std::vector<uint64_t> v, double p) {
    if (v.empty()) return 0;
    std::sort(v.begin(), v.end());
    return static_cast<double>(v[static_cast<size_t>(p * (v.size() - 1) + 0.5)]);
}

struct Summary {
    size_t n = 0;
    double wall_s = 0, rps = 0;
    double lat_p50_us = 0, lat_p90_us = 0, lat_p99_us = 0;    // submit -> complete
    double resp_p50_us = 0, resp_p99_us = 0;                   // arrival -> complete
    double cpu_us_per_read = 0;
    uint64_t errors = 0, short_reads = 0, verify_failures = 0, waits = 0;
};

inline Summary summarize(const RunResult& r) {
    Summary s;
    s.n = r.records.size();
    std::vector<uint64_t> lat, resp;
    lat.reserve(s.n); resp.reserve(s.n);
    for (const Record& x : r.records) {
        lat.push_back(x.complete_ns - x.submit_ns);
        resp.push_back(x.complete_ns - x.arrival_ns);
    }
    s.wall_s = r.wall_ns / 1e9;
    s.rps = s.wall_s > 0 ? s.n / s.wall_s : 0;
    s.lat_p50_us = percentile(lat, 0.50) / 1e3;
    s.lat_p90_us = percentile(lat, 0.90) / 1e3;
    s.lat_p99_us = percentile(lat, 0.99) / 1e3;
    s.resp_p50_us = percentile(resp, 0.50) / 1e3;
    s.resp_p99_us = percentile(resp, 0.99) / 1e3;
    s.cpu_us_per_read = s.n ? r.cpu_ns / 1e3 / s.n : 0;
    s.errors = r.errors; s.short_reads = r.short_reads; s.verify_failures = r.verify_failures; s.waits = r.waits;
    return s;
}
