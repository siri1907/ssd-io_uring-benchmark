// policy_spin.h - policy 3: fixed spin-then-wait (plan task 2.2). STAND-IN written by Pasha so the
// Milestone 2 tools can be tested; Sirisha's real version (spin time from the config file) replaces this.
//
// Checks io_uring_peek_cqe in a loop for at most g_spin_ns on a monotonic clock. If nothing arrived
// in that time, it sleeps for the rest of the timeout. A budget of 0 skips the loop and just waits,
// so it still makes progress.
#pragma once
#include <cerrno>

#include "common.h"
#include "policy_wait.h"

inline int64_t g_spin_ns = 0;  // set before the run (from the config file in the real version)

inline int policy_spin_wait(struct io_uring* ring, struct io_uring_cqe** cqe, int64_t timeout_ns) {
    *cqe = nullptr;
    if (g_inflight == 0) return policy_wait_immediately(ring, cqe, timeout_ns);
    const uint64_t t0 = now_ns();
    int64_t budget = g_spin_ns;
    if (timeout_ns >= 0 && timeout_ns < budget) budget = timeout_ns;
    while (static_cast<int64_t>(now_ns() - t0) < budget) {
        int r = io_uring_peek_cqe(ring, cqe);
        if (r == 0) return 0;
        if (r != -EAGAIN) return r;
    }
    int64_t rest = -1;
    if (timeout_ns >= 0) {
        rest = timeout_ns - static_cast<int64_t>(now_ns() - t0);
        if (rest < 0) rest = 0;
    }
    return policy_wait_immediately(ring, cqe, rest);
}

inline const Policy kPolicySpin = {"spin", policy_spin_wait};
