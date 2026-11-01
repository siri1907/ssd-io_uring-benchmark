// policy_poll.h - policy 2: continuous checking (plan task 2.1). STAND-IN written by Pasha so the
// Milestone 2 tools can be tested; Medur's real version replaces this file.
//
// While reads are outstanding it loops on io_uring_peek_cqe (burning CPU on purpose, to show the full
// cost of polling). With nothing outstanding it sleeps with a timed wait until the next arrival.
#pragma once
#include <cerrno>

#include "common.h"
#include "policy_wait.h"

inline int policy_poll_wait(struct io_uring* ring, struct io_uring_cqe** cqe, int64_t timeout_ns) {
    *cqe = nullptr;
    if (g_inflight == 0) return policy_wait_immediately(ring, cqe, timeout_ns);  // nothing to poll for
    const uint64_t deadline = timeout_ns < 0 ? UINT64_MAX : now_ns() + static_cast<uint64_t>(timeout_ns);
    for (;;) {
        int r = io_uring_peek_cqe(ring, cqe);
        if (r == 0) return 0;
        if (r != -EAGAIN) return r;
        if (now_ns() >= deadline) return -ETIME;
    }
}

inline const Policy kPolicyPoll = {"poll", policy_poll_wait};
