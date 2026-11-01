#ifndef POLICY_FIXED_SPIN_H
#define POLICY_FIXED_SPIN_H

#include <liburing.h>
#include <time.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

// Policy 3: Spins for a fixed time budget, then sleeps if no completion arrives.
inline int execute_policy_3_fixed_spin(struct io_uring *ring, uint64_t spin_budget_us) {
    struct io_uring_cqe *cqe = NULL;
    struct timespec start_time, current_time;
    int ret = 0;

    // Record the start time using a high-resolution monotonic clock
    clock_gettime(CLOCK_MONOTONIC, &start_time);

    while (true) {
        // Task 2.5: Non-blocking check to see if our read request has finished
        ret = io_uring_peek_cqe(ring, &cqe);
        if (ret == 0 && cqe != NULL) {
            // Completion found! Verify buffer content/status if needed here.
            io_uring_cqe_seen(ring, cqe);
            return 0; // Success
        }

        // Calculate elapsed time to enforce our fixed spin budget
        clock_gettime(CLOCK_MONOTONIC, &current_time);
        uint64_t elapsed_us = (current_time.tv_sec - start_time.tv_sec) * 1000000ULL +
                              (current_time.tv_nsec - start_time.tv_nsec) / 1000ULL;

        if (elapsed_us >= spin_budget_us) {
            // Spin budget exhausted; fall back to a blocking wait call
            ret = io_uring_wait_cqe(ring, &cqe);
            if (ret == 0 && cqe != NULL) {
                io_uring_cqe_seen(ring, cqe);
                return 0;
            }
            break;
        }
    }

    return ret;
}

#endif // POLICY_FIXED_SPIN_H