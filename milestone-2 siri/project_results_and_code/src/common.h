// common.h - the shared interfaces (plan task 1.1). Agree these with Medur and Sirisha.
//
//  1. Policy      : how the harness waits for completions (policy 1, 2, 3, 4 each fill this in)
//  2. Record      : one row per request (the columns of the output CSV)
//  3. Config keys : see config.h (key=value file, command line overrides it)
#pragma once
#include <liburing.h>

#include <cstddef>
#include <cstdint>
#include <ctime>
#include <string>

// ---- 2. Per-request record. CSV column order is exactly this order. ----
struct Record {
    uint64_t id = 0;
    uint64_t offset = 0;
    uint32_t size = 0;
    uint64_t arrival_ns = 0;   // intended arrival (from the trace), relative to run start
    uint64_t submit_ns = 0;    // when the read was handed to the kernel
    uint64_t complete_ns = 0;  // when the harness saw the completion
    int32_t result = 0;        // bytes read, or -errno
    uint8_t verify_ok = 0;     // 1 if the bytes matched the known pattern
};

// ---- 1. Policy interface. ----
// wait() is called whenever the harness has nothing else to do. It must either hand back
// one completion in *cqe (return 0) or return -ETIME / -EINTR with *cqe == nullptr.
//   timeout_ns < 0   : no deadline (block until a completion exists)
//   timeout_ns >= 0  : give up after this long; the next request is due then
// The policy never submits reads and never marks completions seen: the harness does that,
// so every policy uses the same submission path.
struct Policy {
    const char* name;
    int (*wait)(struct io_uring* ring, struct io_uring_cqe** cqe, int64_t timeout_ns);
};

// Number of reads currently outstanding. The harness updates this just before it calls
// Policy::wait(), so a policy can tell "reads are in flight" (polling makes sense) from
// "nothing outstanding, just sleep until the next arrival". Added in Milestone 2.
inline size_t g_inflight = 0;

inline uint64_t now_ns() {
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<uint64_t>(ts.tv_sec) * 1000000000ull + static_cast<uint64_t>(ts.tv_nsec);
}
